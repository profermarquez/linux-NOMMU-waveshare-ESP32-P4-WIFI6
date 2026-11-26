// reproducir - play WAV files or a test tone on the Waveshare ESP32-P4-WIFI6
// speaker (SPK header, ES8311 + on-board amplifier).
//
//   reproducir [-v VOLUMEN] ARCHIVO.wav...   play recordings, one after another
//   reproducir -t HZ [-s SEGUNDOS] [-v VOLUMEN]
//                                           a sine tone (default 440 Hz, 10 s)
//     -v  volume in percent, 1..100 (default 70)
//     -q  quiet: no progress line
//
// Writes /dev/ws-audio (kernel patch 0094, capture + playback): 16 kHz,
// 16-bit, stereo.  WAV files must be PCM 16-bit, mono or stereo.  Other
// sample rates are converted by simple linear interpolation (a 44.1 kHz
// music file plays at the right speed, with reduced quality: the board
// output is 16 kHz).  The file is read and played in 63.75 ms blocks, so
// long recordings need no extra memory.  Ctrl+C stops it (the kernel lets
// the queued half second finish and then switches the amplifier off).
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef DISPOSITIVO
#define DISPOSITIVO "/dev/ws-audio"
#endif
#define TASA 16000
#define FRAMES 1020  // one 4080-byte block of stereo S16

static volatile sig_atomic_t cortar = 0;
static void al_cortar(int s) {
  (void) s;
  cortar = 1;
}

static int escribir_todo(int fd, const int16_t *p, size_t bytes) {
  const char *c = (const char *) p;
  while (bytes > 0) {
    ssize_t n = write(fd, c, bytes);
    if (n < 0) {
      if (errno == EINTR) {
        if (cortar) return -1;
        continue;
      }
      fprintf(stderr, "escritura: %s\n", strerror(errno));
      return -1;
    }
    c += n;
    bytes -= (size_t) n;
  }
  return 0;
}

static double ahora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

static int16_t escalar(double v, double vol) {
  v *= vol;
  if (v > 32767) v = 32767;
  if (v < -32768) v = -32768;
  return (int16_t) v;
}

static int tono(int fd, double hz, double segundos, double vol, int callado) {
  static int16_t bloque[FRAMES * 2];
  long total = (long) (segundos * TASA), k = 0;
  const long fade = TASA / 50;  // 20 ms
  double t0 = ahora();
  if (!callado) printf("tono de %.0f Hz durante %.1f s, volumen %.0f %%\n", hz, segundos, vol * 100);
  while (k < total && !cortar) {
    int n = 0;
    for (; n < FRAMES && k < total; n++, k++) {
      double g = 1.0;
      if (k < fade) g = (double) k / fade;
      else if (total - k < fade) g = (double) (total - k) / fade;
      int16_t v = escalar(20000.0 * g * sin(2 * M_PI * hz * k / TASA), vol);
      bloque[2 * n] = bloque[2 * n + 1] = v;
    }
    for (; n < FRAMES; n++) bloque[2 * n] = bloque[2 * n + 1] = 0;
    if (escribir_todo(fd, bloque, sizeof(bloque))) return 1;
    if (!callado && k % TASA < FRAMES) {
      printf("\r  %4.0f s", ahora() - t0);
      fflush(stdout);
    }
  }
  if (!callado) printf("\n");
  return 0;
}

// WAV header: returns the data size and fills channels/rate/bits, or -1.
static long cabecera(FILE *f, int *canales, int *tasa, int *bits) {
  unsigned char h[12];
  if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return -1;
  *canales = *tasa = *bits = 0;
  for (;;) {
    unsigned char c[8];
    if (fread(c, 1, 8, f) != 8) return -1;
    long len = c[4] | c[5] << 8 | c[6] << 16 | (long) c[7] << 24;
    if (!memcmp(c, "fmt ", 4)) {
      unsigned char fm[16];
      if (len < 16 || fread(fm, 1, 16, f) != 16) return -1;
      if ((fm[0] | fm[1] << 8) != 1) return -1;  // PCM only
      *canales = fm[2] | fm[3] << 8;
      *tasa = fm[4] | fm[5] << 8 | fm[6] << 16 | fm[7] << 24;
      *bits = fm[14] | fm[15] << 8;
      fseek(f, len - 16 + (len & 1), SEEK_CUR);
    } else if (!memcmp(c, "data", 4)) {
      return len;
    } else {
      fseek(f, len + (len & 1), SEEK_CUR);
    }
  }
}

static int archivo(int fd, const char *ruta, double vol, int callado) {
  FILE *f = fopen(ruta, "rb");
  if (!f) {
    perror(ruta);
    return 1;
  }
  int canales, tasa, bits;
  long datos = cabecera(f, &canales, &tasa, &bits);
  if (datos < 0 || bits != 16 || canales < 1 || canales > 2 || tasa < 4000 || tasa > 48000) {
    fprintf(stderr, "%s: hace falta un WAV PCM de 16 bits, mono o estereo, de 4 a 48 kHz\n", ruta);
    fclose(f);
    return 1;
  }
  long frames_in = datos / (2 * canales);
  double dur = (double) frames_in / tasa;
  if (!callado)
    printf("%s: %.1f s, %d Hz, %s, volumen %.0f %%%s\n", ruta, dur, tasa,
           canales == 1 ? "mono" : "estereo", vol * 100,
           tasa != TASA ? " (convertido a 16 kHz)" : "");

  // Linear resampling from tasa to TASA, reading the file in small chunks.
  static int16_t entrada[4096 * 2];
  static int16_t bloque[FRAMES * 2];
  long leidos = 0, en_buf = 0, base = 0;  // base = file frame index of entrada[0]
  double pos = 0, paso = (double) tasa / TASA, t0 = ahora();
  int n = 0, rc = 0;
  double ultimo_seg = -1;
  for (;;) {
    if (cortar) break;
    long i = (long) pos;
    // keep frames i and i+1 in the buffer
    if (i + 1 >= base + en_buf) {
      if (leidos >= frames_in) break;
      // keep the last frame, load more
      long keep = en_buf > 0 ? 1 : 0;
      if (keep) {
        entrada[0] = entrada[2 * (en_buf - 1)];
        entrada[1] = entrada[2 * (en_buf - 1) + 1];
        base += en_buf - 1;
      }
      long quiero = 4096 - keep;
      if (quiero > frames_in - leidos) quiero = frames_in - leidos;
      static int16_t tmp[4096 * 2];
      size_t got = fread(tmp, 2 * canales, quiero, f);
      if (got == 0) break;
      for (size_t j = 0; j < got; j++) {
        entrada[2 * (keep + j)] = tmp[canales * j];
        entrada[2 * (keep + j) + 1] = tmp[canales * j + (canales - 1)];
      }
      leidos += got;
      en_buf = keep + got;
      continue;
    }
    double fr = pos - i;
    long a = i - base;
    long b = (a + 1 < en_buf) ? a + 1 : a;
    for (int ch = 0; ch < 2; ch++) {
      double v = entrada[2 * a + ch] * (1 - fr) + entrada[2 * b + ch] * fr;
      bloque[2 * n + ch] = escalar(v, vol);
    }
    pos += paso;
    if (++n == FRAMES) {
      if (escribir_todo(fd, bloque, sizeof(bloque))) {
        rc = 1;
        break;
      }
      n = 0;
      double t = ahora() - t0;
      if (!callado && t - ultimo_seg >= 1.0) {
        printf("\r  %4.0f / %.0f s", pos / tasa, dur);
        fflush(stdout);
        ultimo_seg = t;
      }
    }
  }
  if (n > 0 && rc == 0) {
    memset(bloque + 2 * n, 0, (FRAMES - n) * 4);
    rc = escribir_todo(fd, bloque, sizeof(bloque)) ? 1 : 0;
  }
  if (!callado) printf("\r  %4.1f / %.1f s%s\n", pos / tasa, dur, cortar ? "  (cortado)" : "");
  fclose(f);
  return rc;
}

int main(int argc, char **argv) {
  double hz = 0, segundos = 10, vol = 0.70;
  int callado = 0, o;
  while ((o = getopt(argc, argv, "t:s:v:qh")) != -1) {
    if (o == 't') hz = atof(optarg);
    else if (o == 's') segundos = atof(optarg);
    else if (o == 'v') vol = atof(optarg) / 100.0;
    else if (o == 'q') callado = 1;
    else {
      fprintf(stderr,
              "uso: %s [-v VOLUMEN] ARCHIVO.wav...\n"
              "     %s -t HZ [-s SEGUNDOS] [-v VOLUMEN]\n", argv[0], argv[0]);
      return o == 'h' ? 0 : 2;
    }
  }
  if (vol <= 0 || vol > 1.0) {
    fprintf(stderr, "-v tiene que ser de 1 a 100\n");
    return 2;
  }
  if (hz == 0 && optind >= argc) {
    fprintf(stderr, "falta un .wav, o -t HZ para un tono (reproducir -h)\n");
    return 2;
  }
  if (hz != 0 && (hz < 20 || hz > 7900 || segundos <= 0 || segundos > 3600)) {
    fprintf(stderr, "-t de 20 a 7900 Hz, -s de 0 a 3600 s\n");
    return 2;
  }
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = al_cortar;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  int fd = open(DISPOSITIVO, O_WRONLY);
  if (fd < 0) {
    fprintf(stderr, "%s: %s%s\n", DISPOSITIVO, strerror(errno),
            errno == EBUSY ? " (otro programa esta reproduciendo)" : "");
    return 1;
  }
  int rc = 0;
  if (hz != 0) rc = tono(fd, hz, segundos, vol, callado);
  else
    for (int a = optind; a < argc && !cortar; a++) rc |= archivo(fd, argv[a], vol, callado);
  close(fd);  // the kernel plays what is queued, then switches the amplifier off
  return rc;
}
