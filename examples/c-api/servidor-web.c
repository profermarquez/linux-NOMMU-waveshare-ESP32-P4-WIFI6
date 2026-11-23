// servidor-web - static web server plus a small JSON API, in C (Mongoose).
//
//   servidor-web [-p PUERTO] [-d CARPETA]
//     -p  port (default 8000)
//     -d  folder with the web pages (default /home/m5/www)
//
//   GET /api/hola          {"mensaje":"hola","uptime_s":...}
//   GET /api/memoria       free RAM and the NOMMU pool, from the kernel
//   POST /api/eco          returns the JSON body it got, plus its size
//   any other path         a file from the folder (index.html for /)
//
// One process, one thread, an event loop: it serves many clients at once
// without fork() (which this NOMMU kernel cannot do).
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include "mongoose.h"

static volatile sig_atomic_t seguir = 1;
static const char *carpeta = "/home/m5/www";

static void cortar(int s) {
  (void) s;
  seguir = 0;
}

// Value of "name N" in /sys/kernel/board/nommu_pool, or -1.
static long pool(const char *name) {
  FILE *f = fopen("/sys/kernel/board/nommu_pool", "r");
  char k[64];
  long v, r = -1;
  if (f == NULL) return -1;
  while (fscanf(f, "%63s %ld", k, &v) == 2) {
    if (strcmp(k, name) == 0) {
      r = v;
      break;
    }
  }
  fclose(f);
  return r;
}

static void atender(struct mg_connection *c, int ev, void *ev_data) {
  if (ev != MG_EV_HTTP_MSG) return;
  struct mg_http_message *hm = (struct mg_http_message *) ev_data;
  const char *json = "Content-Type: application/json\r\n";
  struct sysinfo si;

  if (mg_match(hm->uri, mg_str("/api/hola"), NULL)) {
    sysinfo(&si);
    mg_http_reply(c, 200, json, "{%m:%m,%m:%ld}\n", MG_ESC("mensaje"),
                  MG_ESC("hola desde la ESP32-P4"), MG_ESC("uptime_s"),
                  (long) si.uptime);
  } else if (mg_match(hm->uri, mg_str("/api/memoria"), NULL)) {
    sysinfo(&si);
    mg_http_reply(c, 200, json, "{%m:%lu,%m:%ld,%m:%ld}\n",
                  MG_ESC("ram_libre_kb"),
                  (unsigned long) (si.freeram * si.mem_unit / 1024),
                  MG_ESC("pool_usado_kb"), pool("used") / 1024,
                  MG_ESC("pool_bloque_libre_kb"), pool("largest_free") / 1024);
  } else if (mg_match(hm->uri, mg_str("/api/eco"), NULL)) {
    if (mg_strcmp(hm->method, mg_str("POST")) != 0) {
      mg_http_reply(c, 405, json, "{%m:%m}\n", MG_ESC("error"),
                    MG_ESC("usar POST"));
    } else if (mg_json_get(hm->body, "$", NULL) < 0) {
      mg_http_reply(c, 400, json, "{%m:%m}\n", MG_ESC("error"),
                    MG_ESC("el cuerpo no es JSON"));
    } else {
      mg_http_reply(c, 200, json, "{%m:%.*s,%m:%lu}\n", MG_ESC("recibido"),
                    (int) hm->body.len, hm->body.buf, MG_ESC("bytes"),
                    (unsigned long) hm->body.len);
    }
  } else if (mg_match(hm->uri, mg_str("/api/#"), NULL)) {
    mg_http_reply(c, 404, json, "{%m:%m}\n", MG_ESC("error"),
                  MG_ESC("no existe"));
  } else {
    struct mg_http_serve_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.root_dir = carpeta;
    mg_http_serve_dir(c, hm, &opts);
  }
}

int main(int argc, char **argv) {
  const char *puerto = "8000";
  char url[64];
  struct mg_mgr mgr;
  int o;

  while ((o = getopt(argc, argv, "p:d:h")) != -1) {
    if (o == 'p') puerto = optarg;
    else if (o == 'd') carpeta = optarg;
    else {
      fprintf(stderr, "uso: %s [-p PUERTO] [-d CARPETA]\n", argv[0]);
      return o == 'h' ? 0 : 2;
    }
  }
  snprintf(url, sizeof(url), "http://0.0.0.0:%s", puerto);
  signal(SIGINT, cortar);
  signal(SIGTERM, cortar);
  signal(SIGPIPE, SIG_IGN);

  mg_log_set(MG_LL_ERROR);
  mg_mgr_init(&mgr);
  if (mg_http_listen(&mgr, url, atender, NULL) == NULL) {
    fprintf(stderr, "no se pudo escuchar en %s (puerto ocupado?)\n", url);
    return 1;
  }
  printf("servidor-web: %s en %s  (Ctrl+C para cortar)\n", carpeta, url);
  fflush(stdout);
  while (seguir) mg_mgr_poll(&mgr, 1000);
  mg_mgr_free(&mgr);
  printf("servidor-web: listo\n");
  return 0;
}
