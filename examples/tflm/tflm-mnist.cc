// tflm-mnist - recognize a handwritten digit (0-9) with TensorFlow Lite
// Micro on the ESP32-P4 board.
//
//   tflm-mnist [-f] [-i|-I] [-q] IMAGEN...
//     -f  use the float model (default: int8, smaller and faster)
//     -i  invert the image (use for dark digit on light paper)
//     -I  never invert (by default a mostly light image is inverted)
//     -q  quiet: no drawing, no probabilities, just "file: digit"
//
// The image (PNG, JPEG, BMP, GIF, PGM... read with stb_image) is converted
// to gray, shrunk or enlarged to 28x28, scaled to 0..1 and fed to the LSTM
// model of TFLM's mnist_lstm example, which reads the image as 28 rows of
// 28 pixels.  MNIST digits are white on black: a photo of ink on paper has
// to be inverted, which -i does (and the default does on its own when the
// image is mostly light).
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/schema/schema_generated.h"

#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "modelos-mnist.h"  // g_mnist_int8[], g_mnist_float[] (made by make)

namespace {

constexpr int N = 28;
constexpr size_t ARENA = 64 * 1024;
constexpr long MAX_PIXELES = 700L * 1000L;
const char *RAMPA = " .:-=+*#%@";

double ahora_us() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

// Gray image of any size -> 28x28 floats in 0..1 (box average).
void a_28(const unsigned char *px, int w, int h, float out[N * N]) {
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      long x0 = (long) x * w / N, x1 = (long) (x + 1) * w / N;
      long y0 = (long) y * h / N, y1 = (long) (y + 1) * h / N;
      if (x1 <= x0) x1 = x0 + 1;
      if (y1 <= y0) y1 = y0 + 1;
      long s = 0, n = 0;
      for (long j = y0; j < y1 && j < h; j++)
        for (long i = x0; i < x1 && i < w; i++) {
          s += px[j * w + i];
          n++;
        }
      out[y * N + x] = n ? s / (255.0f * n) : 0.0f;
    }
}

void dibujar(const float img[N * N]) {
  int n = (int) strlen(RAMPA);
  for (int y = 0; y < N; y++) {
    for (int x = 0; x < N; x++) {
      int i = (int) (img[y * N + x] * n);
      if (i >= n) i = n - 1;
      putchar(RAMPA[i]);
      putchar(RAMPA[i]);
    }
    putchar('\n');
  }
}

}  // namespace

int main(int argc, char **argv) {
  int usar_float = 0, invertir = -1, callado = 0, o;
  while ((o = getopt(argc, argv, "fiIqh")) != -1) {
    if (o == 'f') usar_float = 1;
    else if (o == 'i') invertir = 1;
    else if (o == 'I') invertir = 0;
    else if (o == 'q') callado = 1;
    else {
      fprintf(stderr, "uso: %s [-f] [-i|-I] [-q] IMAGEN...\n", argv[0]);
      return o == 'h' ? 0 : 2;
    }
  }
  if (optind >= argc) {
    fprintf(stderr, "falta la imagen (tflm-mnist -h)\n");
    return 2;
  }

  const tflite::Model *modelo =
      tflite::GetModel(usar_float ? g_mnist_float : g_mnist_int8);
  if (modelo->version() != TFLITE_SCHEMA_VERSION) {
    fprintf(stderr, "version de esquema %u, se esperaba %d\n",
            (unsigned) modelo->version(), TFLITE_SCHEMA_VERSION);
    return 1;
  }
  static tflite::MicroMutableOpResolver<4> resolver;
  if (resolver.AddUnidirectionalSequenceLSTM() != kTfLiteOk ||
      resolver.AddReshape() != kTfLiteOk ||
      resolver.AddFullyConnected() != kTfLiteOk ||
      resolver.AddSoftmax() != kTfLiteOk) {
    fprintf(stderr, "no se pudieron registrar los operadores\n");
    return 1;
  }
  uint8_t *arena = (uint8_t *) aligned_alloc(16, ARENA);
  if (arena == NULL) {
    fprintf(stderr, "sin memoria para el arena\n");
    return 1;
  }
  tflite::MicroInterpreter interp(modelo, resolver, arena, ARENA);
  if (interp.AllocateTensors() != kTfLiteOk) {
    fprintf(stderr, "AllocateTensors fallo\n");
    return 1;
  }
  TfLiteTensor *in = interp.input(0), *out = interp.output(0);
  if (!callado)
    printf("modelo %s (LSTM de mnist_lstm), arena usado %zu bytes\n",
           usar_float ? "float" : "int8", interp.arena_used_bytes());

  int rc = 0;
  for (int a = optind; a < argc; a++) {
    const char *ruta = argv[a];
    int w, h, c;
    if (!stbi_info(ruta, &w, &h, &c)) {
      fprintf(stderr, "%s: no se puede leer (%s)\n", ruta, stbi_failure_reason());
      rc = 1;
      continue;
    }
    if ((long) w * h > MAX_PIXELES) {
      fprintf(stderr, "%s: %d x %d es demasiado grande (max %ld pixeles)\n",
              ruta, w, h, MAX_PIXELES);
      rc = 1;
      continue;
    }
    unsigned char *px = stbi_load(ruta, &w, &h, &c, 1);
    if (px == NULL) {
      fprintf(stderr, "%s: %s\n", ruta, stbi_failure_reason());
      rc = 1;
      continue;
    }
    float img[N * N];
    a_28(px, w, h, img);
    stbi_image_free(px);

    float media = 0;
    for (int i = 0; i < N * N; i++) media += img[i];
    media /= N * N;
    int inv = invertir < 0 ? media > 0.5f : invertir;
    if (inv)
      for (int i = 0; i < N * N; i++) img[i] = 1.0f - img[i];

    for (int i = 0; i < N * N; i++) {
      if (in->type == kTfLiteInt8) {
        long q = lroundf(img[i] / in->params.scale) + in->params.zero_point;
        in->data.int8[i] = (int8_t) (q < -128 ? -128 : q > 127 ? 127 : q);
      } else {
        in->data.f[i] = img[i];
      }
    }
    // The LSTM keeps state between invocations: start each image clean, or
    // the previous image changes the answer (evaluate.py does the same).
    if (interp.Reset() != kTfLiteOk) {
      fprintf(stderr, "%s: Reset fallo\n", ruta);
      rc = 1;
      continue;
    }
    double t0 = ahora_us();
    if (interp.Invoke() != kTfLiteOk) {
      fprintf(stderr, "%s: Invoke fallo\n", ruta);
      rc = 1;
      continue;
    }
    double dt = ahora_us() - t0;

    float p[10];
    int mejor = 0;
    for (int k = 0; k < 10; k++) {
      p[k] = out->type == kTfLiteInt8
                 ? (out->data.int8[k] - out->params.zero_point) * out->params.scale
                 : out->data.f[k];
      if (p[k] > p[mejor]) mejor = k;
    }

    if (callado) {
      printf("%s: %d\n", ruta, mejor);
      continue;
    }
    printf("\n%s (%d x %d%s):\n", ruta, w, h, inv ? ", invertida" : "");
    dibujar(img);
    printf("digito: %d   (%.1f %%, %.1f ms)\n", mejor, p[mejor] * 100, dt / 1000);
    for (int k = 0; k < 10; k++) printf("%d:%5.1f%%%s", k, p[k] * 100, k == 4 ? "\n" : "  ");
    printf("\n");
  }
  free(arena);
  return rc;
}
