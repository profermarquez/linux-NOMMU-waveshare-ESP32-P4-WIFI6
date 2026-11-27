// radio - play an MP3 internet radio stream on the Waveshare ESP32-P4-WIFI6
// speaker, with a cache against network drops.
//
//   radio [-v VOLUMEN] [-c SEGUNDOS] [-s SEGUNDOS] [URL]
//     URL  http:// MP3 stream (default: RFI Monde, 64 kbps)
//     -v   volume in percent, 1..100 (default 100)
//     -c   cache, in seconds of audio (default 30; 3..120)
//     -s   stop after this many seconds (default: until Ctrl+C)
//
// Plain HTTP only (no TLS: it would not fit the CPU next to the decoder).
//
// Cache: the compressed MP3 is kept in a RAM ring buffer (30 s at 64 kbps =
// 240 KB).  At start the radio waits until the cache is full, so it plays
// that many seconds behind live (the server sends at real-time speed; the
// initial burst makes it a bit faster).  While playing it reads the network
// without blocking after every MP3 frame, so the cache stays full.  If the
// connection drops it keeps playing from the cache and reconnects in the
// background (connection attempts limited to 3 s each).  If the cache runs
// dry anyway, it waits for 3 s of audio and goes on.
//
// The MP3 is decoded with minimp3 (CC0), converted to 16 kHz by linear
// interpolation and written to /dev/ws-audio.  On this board minimp3 runs
// with software floating point: a 64 kbps 22 kHz stream takes ~67 % of the
// CPU (the whole radio ~86 %), so a cache helps against the network, not
// against the CPU being busy with something else.
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "minimp3.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define RFI "http://rfimonde64k.ice.infomaniak.ch/rfimonde-64.mp3"
#ifndef DISPOSITIVO
#define DISPOSITIVO "/dev/ws-audio"
#endif
#define TASA 16000
#define FRAMES 1020           // one 4080-byte block of stereo S16
#define VENTANA (16 * 1024)   // linear window the decoder reads from
#define MINIMO_S 3            // after the cache runs dry, wait this much

static volatile sig_atomic_t cortar = 0;
static void al_cortar(int s) {
  (void) s;
  cortar = 1;
}

static double ahora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

// ---- cache: ring buffer of compressed bytes ----
static unsigned char *cache;
static size_t cap, ini, len;  // capacity, read position, bytes stored

// Contiguous free space at the write end.
static unsigned char *cache_hueco(size_t *n) {
  size_t fin = (ini + len) % cap;
  *n = fin >= ini && len < cap ? cap - fin : cap - len;
  if (len == cap) *n = 0;
  return cache + fin;
}

static size_t cache_sacar(unsigned char *dst, size_t max) {
  size_t n = 0;
  while (n < max && len > 0) {
    size_t tramo = cap - ini;
    if (tramo > len) tramo = len;
    if (tramo > max - n) tramo = max - n;
    memcpy(dst + n, cache + ini, tramo);
    ini = (ini + tramo) % cap;
    len -= tramo;
    n += tramo;
  }
  return n;
}

// ---- network ----
static int separar(const char *url, char *host, size_t hl, char *port, char *path, size_t pl) {
  if (strncmp(url, "http://", 7)) return -1;
  const char *h = url + 7, *slash = strchr(h, '/');
  size_t n = slash ? (size_t) (slash - h) : strlen(h);
  if (n == 0 || n >= hl) return -1;
  memcpy(host, h, n);
  host[n] = 0;
  strcpy(port, "80");
  char *c = strchr(host, ':');
  if (c) {
    *c = 0;
    snprintf(port, 8, "%s", c + 1);
  }
  snprintf(path, pl, "%s", slash ? slash : "/");
  return 0;
}

// Connect (3 s limit) and read the HTTP/ICY headers.  Returns a non-blocking
// socket or -1.  *kbps gets icy-br if the server sends it.
static int conectar(const char *url, int verbose, int *kbps) {
  char host[256], port[8], path[1024];
  if (separar(url, host, sizeof host, port, path, sizeof path)) {
    fprintf(stderr, "URL no valida (solo http://): %s\n", url);
    return -1;
  }
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;  // the board has no IPv6 route
  hints.ai_socktype = SOCK_STREAM;
  int e = getaddrinfo(host, port, &hints, &res);
  if (e) {
    fprintf(stderr, "%s: %s\n", host, gai_strerror(e));
    return -1;
  }
  int s = socket(AF_INET, SOCK_STREAM, 0);
  int rcv = 64 * 1024;
  setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof rcv);
  fcntl(s, F_SETFL, O_NONBLOCK);
  int r = connect(s, res->ai_addr, res->ai_addrlen);
  freeaddrinfo(res);
  if (r < 0 && errno == EINPROGRESS) {
    struct pollfd p = {s, POLLOUT, 0};
    int err = 0;
    socklen_t el = sizeof err;
    if (poll(&p, 1, 3000) != 1 || getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &el) || err) {
      fprintf(stderr, "%s: no se pudo conectar (%s)\n", host, err ? strerror(err) : "3 s");
      close(s);
      return -1;
    }
  } else if (r < 0) {
    fprintf(stderr, "%s: %s\n", host, strerror(errno));
    close(s);
    return -1;
  }
  fcntl(s, F_SETFL, 0);  // blocking (3 s limit) for the request and headers
  struct timeval tv = {3, 0};
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
  char req[1400];
  int n = snprintf(req, sizeof req,
                   "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: radio-p4/1.1\r\n"
                   "Icy-MetaData: 0\r\nConnection: close\r\n\r\n", path, host);
  if (send(s, req, n, 0) != n) {
    close(s);
    return -1;
  }
  char linea[512];
  int l = 0, primera = 1, ok = 0;
  for (;;) {
    char c;
    if (recv(s, &c, 1, 0) != 1) {
      fprintf(stderr, "la radio no mando la cabecera\n");
      close(s);
      return -1;
    }
    if (c == '\n') {
      linea[l > 0 && linea[l - 1] == '\r' ? l - 1 : l] = 0;
      if (primera) {
        ok = strstr(linea, " 200") != NULL;
        if (!ok) fprintf(stderr, "respuesta: %s\n", linea);
        primera = 0;
      } else if (linea[0] == 0) {
        break;
      } else {
        if (!strncasecmp(linea, "icy-br:", 7) && kbps) *kbps = atoi(linea + 7);
        if (verbose && (!strncasecmp(linea, "icy-name:", 9) ||
                        !strncasecmp(linea, "ice-audio-info:", 15)))
          printf("  %s\n", linea);
      }
      l = 0;
    } else if (l < (int) sizeof linea - 1) {
      linea[l++] = c;
    }
  }
  if (!ok) {
    close(s);
    return -1;
  }
  fcntl(s, F_SETFL, O_NONBLOCK);
  return s;
}

// Read whatever the network has into the cache, without blocking.
// Returns 0, or -1 if the connection is gone.
static int bombear(int s) {
  for (;;) {
    size_t n;
    unsigned char *p = cache_hueco(&n);
    if (n == 0) return 0;  // cache full
    ssize_t r = recv(s, p, n, 0);
    if (r > 0) {
      len += (size_t) r;
      continue;
    }
    if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
    if (r < 0 && errno == EINTR) return 0;
    return -1;
  }
}

static int escribir(int fd, const int16_t *p, size_t bytes) {
  const char *c = (const char *) p;
  while (bytes) {
    ssize_t n = write(fd, c, bytes);
    if (n < 0) {
      if (errno == EINTR && !cortar) continue;
      return -1;
    }
    c += n;
    bytes -= n;
  }
  return 0;
}

int main(int argc, char **argv) {
  double vol = 1.0, limite = 0;
  int cache_s = 30, o;
  while ((o = getopt(argc, argv, "v:c:s:h")) != -1) {
    if (o == 'v') vol = atof(optarg) / 100.0;
    else if (o == 'c') cache_s = atoi(optarg);
    else if (o == 's') limite = atof(optarg);
    else {
      fprintf(stderr,
              "uso: %s [-v VOLUMEN] [-c SEGUNDOS] [-s SEGUNDOS] [URL]   (por defecto RFI Monde)\n",
              argv[0]);
      return o == 'h' ? 0 : 2;
    }
  }
  if (vol <= 0 || vol > 1 || cache_s < 3 || cache_s > 120) {
    fprintf(stderr, "-v de 1 a 100, -c de 3 a 120\n");
    return 2;
  }
  const char *url = optind < argc ? argv[optind] : RFI;
  struct sigaction sa;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = al_cortar;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  signal(SIGPIPE, SIG_IGN);

  printf("radio: %s, volumen %.0f %%, cache %d s  (Ctrl+C para salir)\n", url, vol * 100,
         cache_s);
  int kbps = 64;
  int s = -1;
  while (s < 0 && !cortar) {
    s = conectar(url, 1, &kbps);
    if (s < 0) sleep(3);
  }
  if (cortar) return 0;
  if (kbps < 8 || kbps > 320) kbps = 64;
  size_t objetivo = (size_t) kbps * 125 * cache_s;  // bytes of cache_s seconds
  cap = objetivo + 16 * 1024;
  cache = malloc(cap);
  if (!cache) {
    fprintf(stderr, "sin memoria para %zu KB de cache (probar -c menor)\n", cap / 1024);
    return 1;
  }
  printf("  cache de %zu KB (%d s a %d kbps)\n", cap / 1024, cache_s, kbps);

  int fd = open(DISPOSITIVO, O_WRONLY);
  if (fd < 0) {
    fprintf(stderr, "%s: %s%s\n", DISPOSITIVO, strerror(errno),
            errno == EBUSY ? " (otro programa esta reproduciendo)" : "");
    return 1;
  }

  static unsigned char win[VENTANA];
  size_t wlen = 0;
  static mp3dec_t dec;
  mp3dec_init(&dec);
  static int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
  static int16_t bloque[FRAMES * 2];
  int n_bloque = 0, hz = 0;
  int16_t ant[2] = {0, 0};
  double pos = 0, t0 = ahora(), ultimo = 0, ultimo_intento = 0;
  long frames_out = 0, vaciados = 0, reconexiones = 0;
  size_t espera = objetivo;  // fill the cache up to this before playing
  const size_t minimo = (size_t) kbps * 125 * MINIMO_S;

  while (!cortar && (!limite || ahora() - t0 < limite)) {
    // network: read all that is there; reconnect in the background
    if (s >= 0 && bombear(s) < 0) {
      fprintf(stderr, "\nconexion perdida; sigo con el cache (%.0f s) y reconecto\n",
              (double) (len + wlen) / (kbps * 125));
      close(s);
      s = -1;
    }
    if (s < 0 && ahora() - ultimo_intento > 3) {
      ultimo_intento = ahora();
      s = conectar(url, 0, NULL);
      if (s >= 0) reconexiones++;
    }

    // waiting for the cache to fill (at start, or after it ran dry)
    if (espera) {
      if (len + wlen >= espera || len == cap) {
        espera = 0;
        printf("\r  cache lleno (%.0f s), suena                \n",
               (double) (len + wlen) / (kbps * 125));
      } else {
        static double ult = 0;
        if (ahora() - ult >= 1) {
          printf("\r  llenando el cache: %.0f / %.0f s", (double) (len + wlen) / (kbps * 125),
                 (double) espera / (kbps * 125));
          fflush(stdout);
          ult = ahora();
        }
        struct pollfd p = {s, POLLIN, 0};
        poll(&p, s >= 0 ? 1 : 0, 200);
        continue;
      }
    }

    // keep the decoder's window topped up from the cache
    if (wlen < 4096) wlen += cache_sacar(win + wlen, sizeof win - wlen);
    if (wlen == 0) {
      vaciados++;
      espera = minimo;
      fprintf(stderr, "\nel cache se vacio: espero %d s de audio\n", MINIMO_S);
      continue;
    }
    mp3dec_frame_info_t info;
    int k = mp3dec_decode_frame(&dec, win, (int) wlen, pcm, &info);
    if (info.frame_bytes == 0) {  // not a whole frame in the window
      if (wlen == sizeof win) wlen = 0;  // garbage
      else if (len == 0) espera = minimo;
      else wlen += cache_sacar(win + wlen, sizeof win - wlen);
      continue;
    }
    memmove(win, win + info.frame_bytes, wlen - info.frame_bytes);
    wlen -= info.frame_bytes;
    if (k == 0) continue;
    if (!hz) {
      hz = info.hz;
      printf("  MP3 %d Hz, %s, %d kbps\n", hz, info.channels == 1 ? "mono" : "estereo",
             info.bitrate_kbps);
    }

    // linear resampling hz -> TASA, stereo out; pos 0 = previous frame's last sample
    double paso = (double) info.hz / TASA;
    int ch = info.channels;
    while (pos < k) {
      int i = (int) pos;
      double f = pos - i;
      for (int c = 0; c < 2; c++) {
        int cc = ch == 1 ? 0 : c;
        double a = i == 0 ? ant[c] : pcm[(i - 1) * ch + cc];
        double b = pcm[i * ch + cc];
        double v = (a * (1 - f) + b * f) * vol;
        bloque[2 * n_bloque + c] = (int16_t) (v > 32767 ? 32767 : v < -32768 ? -32768 : v);
      }
      pos += paso;
      if (++n_bloque == FRAMES) {
        if (escribir(fd, bloque, sizeof bloque)) {
          cortar = 1;
          break;
        }
        n_bloque = 0;
        frames_out += FRAMES;
        double t = ahora() - t0;
        if (t - ultimo >= 5) {
          printf("\r  %.0f s sonando, cache %.0f s%s      ", (double) frames_out / TASA,
                 (double) (len + wlen) / (kbps * 125), s < 0 ? " (sin conexion)" : "");
          fflush(stdout);
          ultimo = t;
        }
      }
    }
    pos -= k;
    ant[0] = pcm[(k - 1) * ch];
    ant[1] = pcm[(k - 1) * ch + (ch == 2)];
  }
  if (s >= 0) close(s);
  close(fd);  // the kernel plays what is queued and switches the amplifier off
  printf("\nradio: %.0f s reproducidos, %ld reconexion(es), cache vacio %ld vez/veces\n",
         (double) frames_out / TASA, reconexiones, vaciados);
  free(cache);
  return 0;
}
