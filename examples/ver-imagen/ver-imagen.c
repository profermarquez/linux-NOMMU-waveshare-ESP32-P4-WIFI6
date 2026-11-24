// ver-imagen - show an image file in the terminal with characters.
//
//   ver-imagen [-m MODO] [-w COLUMNAS] [-i] [-n] IMAGEN...
//     -m  ascii     " .:-=+*#%@"              any terminal
//         bloques   " ░▒▓█"                   UTF-8 terminal
//         gris      half blocks, 24 grays     256-color terminal
//         256       half blocks, 256 colors   (default) Terminal.app, iTerm
//         color     half blocks, 24-bit color iTerm, most modern terminals
//     -w  width in columns (default: fits 80 columns, or $COLUMNS)
//     -i  invert brightness (ascii/bloques)
//     -n  only print the size and type of each image
//
// Reads PNG, JPEG, BMP, GIF (first frame), TGA and PGM/PPM with stb_image.
// The whole image is decoded into RAM (width x height x 3 bytes), so big
// photos are refused: see MAX_PIXELES.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// 1 Mpixel = 3 MB as RGB, more than most of the free RAM: stop before that.
#define MAX_PIXELES (700L * 1000L)

enum modo { ASCII, BLOQUES, GRIS, C256, COLOR };

static const char *RAMPA = " .:-=+*#%@";
static const char *BLOQ[] = {" ", "\xe2\x96\x91", "\xe2\x96\x92", "\xe2\x96\x93", "\xe2\x96\x88"};
static const char *MEDIO = "\xe2\x96\x80";  // ▀ upper half block

struct img {
  int w, h, c;  // c: channels in px (1 gray, 3 rgb)
  unsigned char *px;
};

// Average of the source box that maps to output pixel (x, y) in a
// w2 x h2 output.  Gives smooth downscaling and plain repeat upscaling.
static void muestra(const struct img *im, int w2, int h2, int x, int y,
                    int out[3]) {
  long x0 = (long) x * im->w / w2, x1 = (long) (x + 1) * im->w / w2;
  long y0 = (long) y * im->h / h2, y1 = (long) (y + 1) * im->h / h2;
  if (x1 <= x0) x1 = x0 + 1;
  if (y1 <= y0) y1 = y0 + 1;
  long s[3] = {0, 0, 0}, n = 0;
  for (long j = y0; j < y1 && j < im->h; j++)
    for (long i = x0; i < x1 && i < im->w; i++) {
      const unsigned char *p = im->px + (j * im->w + i) * im->c;
      for (int k = 0; k < 3; k++) s[k] += p[im->c == 1 ? 0 : k];
      n++;
    }
  for (int k = 0; k < 3; k++) out[k] = n ? (int) (s[k] / n) : 0;
}

static int luz(const int p[3]) {  // perceived brightness 0..255
  return (p[0] * 299 + p[1] * 587 + p[2] * 114) / 1000;
}

// Nearest entry of the xterm 256-color palette (6x6x6 cube + 24 grays).
static int color256(const int p[3]) {
  static const int nivel[6] = {0, 95, 135, 175, 215, 255};
  int q[3], d_cubo = 0;
  for (int k = 0; k < 3; k++) {
    int v = p[k] < 48 ? 0 : p[k] < 115 ? 1 : (p[k] - 35) / 40;
    q[k] = v;
    int e = p[k] - nivel[v];
    d_cubo += e * e;
  }
  int g = (p[0] + p[1] + p[2]) / 3;
  int gi = g > 238 ? 23 : g < 8 ? 0 : (g - 8) / 10;
  int gv = 8 + gi * 10, d_gris = 0;
  for (int k = 0; k < 3; k++) d_gris += (p[k] - gv) * (p[k] - gv);
  return d_gris < d_cubo ? 232 + gi : 16 + 36 * q[0] + 6 * q[1] + q[2];
}

static int gris256(const int p[3]) {
  int g = luz(p);
  return g > 238 ? 231 : g < 8 ? 16 : 232 + (g - 8) / 10;
}

// Color escape, skipped when it repeats the previous one (*ultimo): keeps
// the output small over SSH.  *ultimo = -1 forces it.
static void color_ansi(int fondo, const int p[3], enum modo m, long *ultimo) {
  long clave = m == COLOR ? ((long) p[0] << 16 | p[1] << 8 | p[2])
                          : (m == GRIS ? gris256(p) : color256(p));
  if (clave == *ultimo) return;
  *ultimo = clave;
  if (m == COLOR)
    printf("\x1b[%d;2;%d;%d;%dm", fondo ? 48 : 38, p[0], p[1], p[2]);
  else
    printf("\x1b[%d;5;%ldm", fondo ? 48 : 38, clave);
}

static void dibujar(const struct img *im, enum modo m, int cols, int invertir) {
  int w2 = cols, h2;
  if (m == ASCII || m == BLOQUES) {
    // a character is about twice as tall as wide
    h2 = (int) ((long) im->h * w2 / im->w / 2);
    if (h2 < 1) h2 = 1;
    int n = m == ASCII ? (int) strlen(RAMPA) : 5;
    for (int y = 0; y < h2; y++) {
      for (int x = 0; x < w2; x++) {
        int p[3];
        muestra(im, w2, h2, x, y, p);
        int l = luz(p);
        if (invertir) l = 255 - l;
        int i = l * n / 256;
        if (m == ASCII) putchar(RAMPA[i]);
        else fputs(BLOQ[i], stdout);
      }
      putchar('\n');
    }
  } else {
    // ▀: foreground = upper pixel, background = lower pixel
    h2 = (int) ((long) im->h * w2 / im->w);
    if (h2 < 2) h2 = 2;
    h2 += h2 & 1;
    for (int y = 0; y < h2; y += 2) {
      long uf = -1, ub = -1;
      for (int x = 0; x < w2; x++) {
        int a[3], b[3];
        muestra(im, w2, h2, x, y, a);
        muestra(im, w2, h2, x, y + 1, b);
        color_ansi(0, a, m, &uf);
        color_ansi(1, b, m, &ub);
        fputs(MEDIO, stdout);
      }
      fputs("\x1b[0m\n", stdout);
    }
  }
}

int main(int argc, char **argv) {
  enum modo m = C256;
  int cols = 0, invertir = 0, solo_info = 0, o, rc = 0;
  while ((o = getopt(argc, argv, "m:w:inh")) != -1) {
    if (o == 'm') {
      if (!strcmp(optarg, "ascii")) m = ASCII;
      else if (!strcmp(optarg, "bloques")) m = BLOQUES;
      else if (!strcmp(optarg, "gris")) m = GRIS;
      else if (!strcmp(optarg, "256")) m = C256;
      else if (!strcmp(optarg, "color")) m = COLOR;
      else {
        fprintf(stderr, "modo desconocido: %s (ascii, bloques, gris, 256, color)\n", optarg);
        return 2;
      }
    } else if (o == 'w') cols = atoi(optarg);
    else if (o == 'i') invertir = 1;
    else if (o == 'n') solo_info = 1;
    else {
      fprintf(stderr,
              "uso: %s [-m ascii|bloques|gris|256|color] [-w COLUMNAS] [-i] [-n] IMAGEN...\n",
              argv[0]);
      return o == 'h' ? 0 : 2;
    }
  }
  if (optind >= argc) {
    fprintf(stderr, "falta la imagen (ver-imagen -h)\n");
    return 2;
  }
  int max_cols = 80;
  const char *ce = getenv("COLUMNS");
  if (ce && atoi(ce) > 10) max_cols = atoi(ce);

  for (int a = optind; a < argc; a++) {
    const char *ruta = argv[a];
    int w, h, comp;
    if (!stbi_info(ruta, &w, &h, &comp)) {
      fprintf(stderr, "%s: no se puede leer (%s)\n", ruta, stbi_failure_reason());
      rc = 1;
      continue;
    }
    if (solo_info || argc - optind > 1)
      printf("%s: %d x %d, %d canal%s\n", ruta, w, h, comp, comp == 1 ? "" : "es");
    if (solo_info) continue;
    if ((long) w * h > MAX_PIXELES) {
      fprintf(stderr,
              "%s: %d x %d es demasiado grande para la RAM de la placa (max %ld pixeles).\n"
              "  Achicala antes, por ejemplo a 640 x 480.\n",
              ruta, w, h, MAX_PIXELES);
      rc = 1;
      continue;
    }
    struct img im;
    // gray modes decode to 1 channel: a third of the memory
    int canales = (m == ASCII || m == BLOQUES || m == GRIS) ? 1 : 3;
    im.px = stbi_load(ruta, &im.w, &im.h, &im.c, canales);
    if (im.px == NULL) {
      fprintf(stderr, "%s: %s\n", ruta, stbi_failure_reason());
      rc = 1;
      continue;
    }
    im.c = canales;
    int c = cols;
    if (c <= 0) {
      // small images (like 28x28 MNIST) are enlarged, big ones fit the width
      c = (m == ASCII || m == BLOQUES) ? im.w * 2 : im.w;
      if (c < 40) c = c * ((40 + c - 1) / c);
      if (c > max_cols) c = max_cols;
    }
    dibujar(&im, m, c, invertir);
    stbi_image_free(im.px);
  }
  return rc;
}
