// api-sqlite - REST API in C over a SQLite table (Mongoose + SQLite).
//
//   api-sqlite [-p PUERTO] [-b BASE.db] [-d CARPETA]
//     -p  port (default 8001)
//     -b  database (default /home/m5/lecturas.db; created if missing)
//     -d  also serve web pages from this folder (optional)
//
// Table: lecturas(id, sensor, valor, hora)
//   GET    /api/lecturas?limite=N&sensor=S   newest first (limite<=1000)
//   GET    /api/lecturas/ID
//   POST   /api/lecturas      body {"sensor":"temp","valor":23.5}
//   DELETE /api/lecturas/ID
//   GET    /api/resumen       count, average, min and max per sensor
//
// SQLite builds the JSON itself (json_object / json_group_array), so the C
// side only binds parameters and copies text: no SQL injection, no JSON
// escaping by hand.
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mongoose.h"
#include "sqlite3.h"

static volatile sig_atomic_t seguir = 1;
static sqlite3 *db;
static const char *carpeta = NULL;
static const char *JSON = "Content-Type: application/json\r\n";

static void cortar(int s) {
  (void) s;
  seguir = 0;
}

static void error_json(struct mg_connection *c, int code, const char *msg) {
  mg_http_reply(c, code, JSON, "{%m:%m}\n", MG_ESC("error"), MG_ESC(msg));
}

// Run a prepared statement that returns one text column (a JSON document)
// and send it.  Takes ownership of st.
static void responder_json(struct mg_connection *c, sqlite3_stmt *st,
                           int vacio_404) {
  int rc = sqlite3_step(st);
  if (rc == SQLITE_ROW && sqlite3_column_type(st, 0) != SQLITE_NULL) {
    mg_http_reply(c, 200, JSON, "%s\n",
                  (const char *) sqlite3_column_text(st, 0));
  } else if (rc == SQLITE_ROW || rc == SQLITE_DONE) {
    if (vacio_404) error_json(c, 404, "no existe");
    else mg_http_reply(c, 200, JSON, "[]\n");
  } else {
    error_json(c, 500, sqlite3_errmsg(db));
  }
  sqlite3_finalize(st);
}

static sqlite3_stmt *preparar(struct mg_connection *c, const char *sql) {
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) {
    error_json(c, 500, sqlite3_errmsg(db));
    return NULL;
  }
  return st;
}

#define FILA                                                              \
  "json_object('id',id,'sensor',sensor,'valor',valor,'hora',hora)"

static void listar(struct mg_connection *c, struct mg_http_message *hm) {
  char buf[64];
  long limite = 100;
  sqlite3_stmt *st;
  if (mg_http_get_var(&hm->query, "limite", buf, sizeof(buf)) > 0) {
    limite = strtol(buf, NULL, 10);
    if (limite < 1) limite = 1;
    if (limite > 1000) limite = 1000;
  }
  int con_sensor = mg_http_get_var(&hm->query, "sensor", buf, sizeof(buf)) > 0;
  st = preparar(c,
                "SELECT json_group_array(json(j)) FROM (SELECT " FILA
                " AS j FROM lecturas WHERE ?1 IS NULL OR sensor = ?1"
                " ORDER BY id DESC LIMIT ?2)");
  if (st == NULL) return;
  if (con_sensor) sqlite3_bind_text(st, 1, buf, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(st, 2, limite);
  responder_json(c, st, 0);
}

static void una(struct mg_connection *c, long long id) {
  sqlite3_stmt *st = preparar(c, "SELECT " FILA " FROM lecturas WHERE id = ?");
  if (st == NULL) return;
  sqlite3_bind_int64(st, 1, id);
  responder_json(c, st, 1);
}

static void crear(struct mg_connection *c, struct mg_http_message *hm) {
  double valor;
  char *sensor = mg_json_get_str(hm->body, "$.sensor");
  if (sensor == NULL || !mg_json_get_num(hm->body, "$.valor", &valor)) {
    free(sensor);
    error_json(c, 400, "hace falta {\"sensor\": texto, \"valor\": numero}");
    return;
  }
  sqlite3_stmt *st =
      preparar(c, "INSERT INTO lecturas(sensor, valor) VALUES (?, ?)");
  if (st != NULL) {
    sqlite3_bind_text(st, 1, sensor, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 2, valor);
    if (sqlite3_step(st) == SQLITE_DONE) {
      long long id = sqlite3_last_insert_rowid(db);
      sqlite3_finalize(st);
      st = preparar(c, "SELECT " FILA " FROM lecturas WHERE id = ?");
      if (st != NULL) {
        sqlite3_bind_int64(st, 1, id);
        if (sqlite3_step(st) == SQLITE_ROW)
          mg_http_reply(c, 201, JSON, "%s\n",
                        (const char *) sqlite3_column_text(st, 0));
        else
          error_json(c, 500, sqlite3_errmsg(db));
        sqlite3_finalize(st);
      }
    } else {
      error_json(c, 500, sqlite3_errmsg(db));
      sqlite3_finalize(st);
    }
  }
  free(sensor);
}

static void borrar(struct mg_connection *c, long long id) {
  sqlite3_stmt *st = preparar(c, "DELETE FROM lecturas WHERE id = ?");
  if (st == NULL) return;
  sqlite3_bind_int64(st, 1, id);
  if (sqlite3_step(st) != SQLITE_DONE) error_json(c, 500, sqlite3_errmsg(db));
  else if (sqlite3_changes(db) == 0) error_json(c, 404, "no existe");
  else mg_http_reply(c, 200, JSON, "{%m:%lld}\n", MG_ESC("borrado"), id);
  sqlite3_finalize(st);
}

static void resumen(struct mg_connection *c) {
  sqlite3_stmt *st = preparar(
      c,
      "SELECT json_group_array(json_object('sensor',sensor,'cantidad',n,"
      "'media',media,'minimo',mi,'maximo',ma)) FROM (SELECT sensor,"
      " count(*) AS n, round(avg(valor),3) AS media, min(valor) AS mi,"
      " max(valor) AS ma FROM lecturas GROUP BY sensor ORDER BY sensor)");
  if (st != NULL) responder_json(c, st, 0);
}

static void atender(struct mg_connection *c, int ev, void *ev_data) {
  if (ev != MG_EV_HTTP_MSG) return;
  struct mg_http_message *hm = (struct mg_http_message *) ev_data;
  struct mg_str caps[2];
  int get = mg_strcmp(hm->method, mg_str("GET")) == 0;

  if (mg_match(hm->uri, mg_str("/api/lecturas"), NULL)) {
    if (get) listar(c, hm);
    else if (mg_strcmp(hm->method, mg_str("POST")) == 0) crear(c, hm);
    else error_json(c, 405, "usar GET o POST");
  } else if (mg_match(hm->uri, mg_str("/api/lecturas/*"), caps)) {
    long long id = 0;
    if (!mg_str_to_num(caps[0], 10, &id, sizeof(id))) {
      error_json(c, 400, "el id tiene que ser un numero");
    } else if (get) {
      una(c, id);
    } else if (mg_strcmp(hm->method, mg_str("DELETE")) == 0) {
      borrar(c, id);
    } else {
      error_json(c, 405, "usar GET o DELETE");
    }
  } else if (mg_match(hm->uri, mg_str("/api/resumen"), NULL)) {
    resumen(c);
  } else if (carpeta != NULL && !mg_match(hm->uri, mg_str("/api/#"), NULL)) {
    struct mg_http_serve_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.root_dir = carpeta;
    mg_http_serve_dir(c, hm, &opts);
  } else {
    error_json(c, 404, "no existe");
  }
}

int main(int argc, char **argv) {
  const char *puerto = "8001", *base = "/home/m5/lecturas.db";
  char url[64];
  char *err = NULL;
  struct mg_mgr mgr;
  int o;

  while ((o = getopt(argc, argv, "p:b:d:h")) != -1) {
    if (o == 'p') puerto = optarg;
    else if (o == 'b') base = optarg;
    else if (o == 'd') carpeta = optarg;
    else {
      fprintf(stderr, "uso: %s [-p PUERTO] [-b BASE.db] [-d CARPETA]\n",
              argv[0]);
      return o == 'h' ? 0 : 2;
    }
  }
  if (sqlite3_open(base, &db) != SQLITE_OK) {
    fprintf(stderr, "no se pudo abrir %s: %s\n", base, sqlite3_errmsg(db));
    return 1;
  }
  // TRUNCATE + NORMAL: about 1 s per write on this microSD instead of ~5 s
  // with the defaults, and still safe for the data already committed.
  if (sqlite3_exec(db,
                   "PRAGMA journal_mode=TRUNCATE; PRAGMA synchronous=NORMAL;"
                   "CREATE TABLE IF NOT EXISTS lecturas("
                   " id INTEGER PRIMARY KEY, sensor TEXT NOT NULL,"
                   " valor REAL, hora TEXT DEFAULT CURRENT_TIMESTAMP)",
                   NULL, NULL, &err) != SQLITE_OK) {
    fprintf(stderr, "error en la base: %s\n", err);
    return 1;
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
  printf("api-sqlite: %s en %s  (Ctrl+C para cortar)\n", base, url);
  fflush(stdout);
  while (seguir) mg_mgr_poll(&mgr, 1000);
  mg_mgr_free(&mgr);
  sqlite3_close(db);
  printf("api-sqlite: listo\n");
  return 0;
}
