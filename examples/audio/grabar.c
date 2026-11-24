// grabar - record the Waveshare ESP32-P4-WIFI6 microphone to a WAV file.
//
//   grabar [-s SEGUNDOS] [-e] [-n] ARCHIVO.wav
//     -s  length in seconds (default 5, max 600)
//     -e  keep both channels (stereo WAV); default: mono, the microphone
//         (left channel)
//     -n  only print the level once per second, do not write a file
//
// Reads /dev/ws-audio (kernel patch 0094): 16 kHz, S16_LE, 2 channels, at
// most 4080 bytes (63.75 ms) per read.  The kernel captures continuously
// from open() to close() (DMA ring, patch 0094) and replaces the stale
// first FIFO frame, so the WAV has no gaps or clicks.  The audio is kept
// in RAM and written at the end (writing to the microSD while recording
// made the reader fall behind).  Prints RMS and peak of the microphone
// channel once per second.
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DISPOSITIVO "/dev/ws-audio"
#define TASA 16000
#define BLOQUE 4080

static void le16(unsigned char *p, unsigned v) {
  p[0] = v & 0xff;
  p[1] = (v >> 8) & 0xff;
}

static void le32(unsigned char *p, unsigned long v) {
  for (int i = 0; i < 4; i++) p[i] = (v >> (8 * i)) & 0xff;
}

static void cabecera_wav(unsigned char h[44], int canales, unsigned long datos) {
  memcpy(h, "RIFF", 4);
  le32(h + 4, 36 + datos);
  memcpy(h + 8, "WAVEfmt ", 8);
  le32(h + 16, 16);
  le16(h + 20, 1);  // PCM
  le16(h + 22, canales);
  le32(h + 24, TASA);
  le32(h + 28, TASA * canales * 2);
  le16(h + 32, canales * 2);
  le16(h + 34, 16);
  memcpy(h + 36, "data", 4);
  le32(h + 40, datos);
}

// Recorded audio kept in RAM until the end, in 64 KB pieces (a NOMMU
// system has few big contiguous blocks, and one malloc per 2 KB read was
// slow enough to cause gaps).
#define TROZO 65536
struct trozo {
  struct trozo *sig;
  size_t n;
  unsigned char datos[];
};

static unsigned long volcar(struct trozo **primero, struct trozo **ultimo, FILE *out) {
  unsigned long total = 0;
  for (struct trozo *t = *primero, *s; t != NULL; t = s) {
    s = t->sig;
    fwrite(t->datos, 1, t->n, out);
    total += t->n;
    free(t);
  }
  *primero = *ultimo = NULL;
  return total;
}

static FILE *abrir(const char *ruta, unsigned char h[44], int canales) {
  FILE *out = fopen(ruta, "wb");
  if (out == NULL) {
    perror(ruta);
    return NULL;
  }
  cabecera_wav(h, canales, 0);  // rewritten at the end
  fwrite(h, 1, 44, out);
  return out;
}

static double ahora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
  int segundos = 5, estereo = 0, solo_nivel = 0, o;
  while ((o = getopt(argc, argv, "s:enh")) != -1) {
    if (o == 's') segundos = atoi(optarg);
    else if (o == 'e') estereo = 1;
    else if (o == 'n') solo_nivel = 1;
    else {
      fprintf(stderr, "uso: %s [-s SEGUNDOS] [-e] [-n] ARCHIVO.wav\n", argv[0]);
      return o == 'h' ? 0 : 2;
    }
  }
  if (segundos < 1 || segundos > 600) {
    fprintf(stderr, "-s tiene que ser de 1 a 600\n");
    return 2;
  }
  if (!solo_nivel && optind >= argc) {
    fprintf(stderr, "falta el archivo .wav (o -n para ver solo el nivel)\n");
    return 2;
  }

  // Flush pending microSD writes first, and create the file only after the
  // recording: an ext4 journal commit over the software SPI holds the only
  // core for seconds, and the 0.5 s capture ring overflows meanwhile (seen
  // on the board: 10 s of audio took 14 s, about 4 s lost).
  sync();
  int fd = open(DISPOSITIVO, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "%s: %s (el kernel tiene el driver de audio?)\n", DISPOSITIVO,
            strerror(errno));
    return 1;
  }
  FILE *out = NULL;
  int canales = estereo ? 2 : 1;
  unsigned char h[44];
  int guardar = !solo_nivel;
  const char *ruta = guardar ? argv[optind] : NULL;

  static int16_t bloque[BLOQUE / 2];
  static int16_t mono[BLOQUE / 4];
  long frames_meta = (long) segundos * TASA, frames = 0;
  unsigned long bytes = 0;
  double sq = 0, t0 = ahora();
  long n_seg = 0, pico = 0, frames_seg = 0;
  int errores = 0, aviso_memoria = 0;
  struct trozo *primero = NULL, *ultimo = NULL;

  while (frames < frames_meta) {
    ssize_t n = read(fd, bloque, BLOQUE);
    if (n < 0) {
      if (errno == EINTR) continue;
      fprintf(stderr, "lectura: %s\n", strerror(errno));
      if (++errores >= 3) break;
      continue;
    }
    int f = (int) (n / 4);
    if (f < 2) continue;
    if (frames + f > frames_meta) f = (int) (frames_meta - frames);
    for (int i = 0; i < f; i++) {
      int v = bloque[2 * i];  // left = microphone
      mono[i] = (int16_t) v;
      sq += (double) v * v;
      if (abs(v) > pico) pico = abs(v);
    }
    frames += f;
    frames_seg += f;
    if (guardar) {
      // Keep the audio in RAM while recording: writing to the microSD
      // (software SPI) between reads made 5 s of audio take 8.5 s, with
      // gaps.  If memory runs out, write what there is and go on.
      size_t b = estereo ? (size_t) f * 4 : (size_t) f * 2;
      const void *src = estereo ? (void *) bloque : (void *) mono;
      if (ultimo == NULL || ultimo->n + b > TROZO) {
        struct trozo *t = malloc(sizeof(*t) + TROZO);
        if (t == NULL) {
          if (!aviso_memoria++)
            fprintf(stderr, "sin memoria: escribo a la microSD mientras grabo (habra cortes)\n");
          if (out == NULL && (out = abrir(ruta, h, canales)) == NULL) return 1;
          bytes += volcar(&primero, &ultimo, out);
          fwrite(src, 1, b, out);
          bytes += b;
          b = 0;
        } else {
          t->n = 0;
          t->sig = NULL;
          if (ultimo) ultimo->sig = t;
          else primero = t;
          ultimo = t;
        }
      }
      if (b) {
        memcpy(ultimo->datos + ultimo->n, src, b);
        ultimo->n += b;
      }
    }
    if (frames_seg >= TASA || frames >= frames_meta) {
      n_seg++;
      int rms = (int) sqrt(sq / frames_seg);
      int barras = rms * 40 / 4000;
      if (barras > 40) barras = 40;
      printf("%3lds  rms %5d  pico %5ld  |%.*s%*s|\n", n_seg, rms, pico, barras,
             "########################################", 40 - barras, "");
      fflush(stdout);
      sq = 0;
      pico = 0;
      frames_seg = 0;
    }
  }
  double dt = ahora() - t0;
  close(fd);
  double audio_s = (double) frames / TASA;
  if (frames > 0 && dt > audio_s + 0.5)
    fprintf(stderr, "aviso: %.1f s de audio en %.1f s: el sistema estuvo ocupado y se perdio "
            "audio (dmesg: overrun)\n", audio_s, dt);
  if (guardar) {
    if (out == NULL && (out = abrir(ruta, h, canales)) == NULL) return 1;
    bytes += volcar(&primero, &ultimo, out);
    cabecera_wav(h, canales, bytes);
    fseek(out, 0, SEEK_SET);
    fwrite(h, 1, 44, out);
    fclose(out);
    printf("%s: %.1f s de audio en %.1f s, %s, 16 kHz, 16 bits\n", ruta, audio_s, dt,
           estereo ? "estereo" : "mono");
  }
  return frames > 0 ? 0 : 1;
}
