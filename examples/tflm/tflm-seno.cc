// tflm-seno - TensorFlow Lite for Microcontrollers on the ESP32-P4 board.
//
//   tflm-seno                    run the built-in sine model (int8 and float)
//   tflm-seno MODELO.tflite X... run your own model with 1 float input and
//                                1 float output for each value X
//   options:  -a KB   tensor arena size for a model file (default 64)
//
// The built-in models are TFLM's "hello_world": a tiny network trained to
// approximate sin(x) for x in [0, 2*pi].  The output compares the network's
// prediction with the real sin(x) and times each inference.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/schema/schema_generated.h"

#include "modelos.h"  // g_seno_int8[], g_seno_float[] (made from .tflite by make)

namespace {

// Operators available to a model file.  Each one adds code; this set covers
// small dense, convolutional and recurrent-free classifiers.
using Resolver = tflite::MicroMutableOpResolver<16>;

bool registrar(Resolver &r) {
  return r.AddFullyConnected() == kTfLiteOk && r.AddConv2D() == kTfLiteOk &&
         r.AddDepthwiseConv2D() == kTfLiteOk && r.AddMaxPool2D() == kTfLiteOk &&
         r.AddAveragePool2D() == kTfLiteOk && r.AddReshape() == kTfLiteOk &&
         r.AddSoftmax() == kTfLiteOk && r.AddRelu() == kTfLiteOk &&
         r.AddRelu6() == kTfLiteOk && r.AddLogistic() == kTfLiteOk &&
         r.AddTanh() == kTfLiteOk && r.AddAdd() == kTfLiteOk &&
         r.AddMul() == kTfLiteOk && r.AddQuantize() == kTfLiteOk &&
         r.AddDequantize() == kTfLiteOk && r.AddMean() == kTfLiteOk;
}

double ahora_us() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

// Set the input from a float, quantizing it if the tensor is int8.
bool poner(TfLiteTensor *in, float x) {
  if (in->type == kTfLiteFloat32) {
    in->data.f[0] = x;
  } else if (in->type == kTfLiteInt8) {
    long q = lroundf(x / in->params.scale) + in->params.zero_point;
    in->data.int8[0] = (int8_t) (q < -128 ? -128 : q > 127 ? 127 : q);
  } else {
    return false;
  }
  return true;
}

float leer(const TfLiteTensor *out) {
  if (out->type == kTfLiteInt8)
    return (out->data.int8[0] - out->params.zero_point) * out->params.scale;
  return out->data.f[0];
}

// Run a model on each x.  sin_ref: print sin(x) and the error too.
int correr(const char *nombre, const unsigned char *datos, size_t arena_kb,
           const float *xs, int n, bool sin_ref) {
  const tflite::Model *modelo = tflite::GetModel(datos);
  if (modelo->version() != TFLITE_SCHEMA_VERSION) {
    fprintf(stderr, "%s: version de esquema %u, se esperaba %d\n", nombre,
            (unsigned) modelo->version(), TFLITE_SCHEMA_VERSION);
    return 1;
  }
  static Resolver resolver;
  static bool listo = false;
  if (!listo && !(listo = registrar(resolver))) {
    fprintf(stderr, "no se pudieron registrar los operadores\n");
    return 1;
  }
  size_t tam = arena_kb * 1024;
  uint8_t *arena = (uint8_t *) aligned_alloc(16, tam);
  if (arena == NULL) {
    fprintf(stderr, "sin memoria para un arena de %zu KB\n", arena_kb);
    return 1;
  }
  tflite::MicroInterpreter interp(modelo, resolver, arena, tam);
  if (interp.AllocateTensors() != kTfLiteOk) {
    fprintf(stderr, "%s: AllocateTensors fallo (arena chico? probar -a)\n",
            nombre);
    free(arena);
    return 1;
  }
  TfLiteTensor *in = interp.input(0), *out = interp.output(0);
  printf("\n%s: entrada %s, salida %s, arena usado %zu de %zu bytes\n", nombre,
         TfLiteTypeGetName(in->type), TfLiteTypeGetName(out->type),
         interp.arena_used_bytes(), tam);
  if (sin_ref) printf("     x   sin(x)   modelo   error   tiempo\n");
  else printf("     x   salida   tiempo\n");

  double peor = 0, total_us = 0;
  for (int i = 0; i < n; i++) {
    if (!poner(in, xs[i])) {
      fprintf(stderr, "tipo de entrada no soportado\n");
      free(arena);
      return 1;
    }
    double t0 = ahora_us();
    if (interp.Invoke() != kTfLiteOk) {
      fprintf(stderr, "Invoke fallo\n");
      free(arena);
      return 1;
    }
    double dt = ahora_us() - t0;
    total_us += dt;
    float y = leer(out);
    if (sin_ref) {
      double e = fabs(y - sin(xs[i]));
      if (e > peor) peor = e;
      printf("%6.2f  %7.3f  %7.3f  %6.3f  %5.0f us\n", xs[i], sin(xs[i]), y,
             e, dt);
    } else {
      printf("%6.2f  %7.4f  %5.0f us\n", xs[i], y, dt);
    }
  }
  printf("promedio %.0f us por inferencia", total_us / n);
  if (sin_ref) printf(", error maximo %.3f", peor);
  printf("\n");
  free(arena);
  return 0;
}

unsigned char *leer_archivo(const char *ruta) {
  FILE *f = fopen(ruta, "rb");
  if (f == NULL) return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  // flatbuffers want the buffer 16-byte aligned
  unsigned char *b = (unsigned char *) aligned_alloc(16, (n + 15) & ~15L);
  if (b != NULL && fread(b, 1, n, f) != (size_t) n) {
    free(b);
    b = NULL;
  }
  fclose(f);
  return b;
}

}  // namespace

int main(int argc, char **argv) {
  size_t arena_kb = 64;
  int o;
  while ((o = getopt(argc, argv, "a:h")) != -1) {
    if (o == 'a') arena_kb = strtoul(optarg, NULL, 10);
    else {
      fprintf(stderr, "uso: %s [-a KB] [MODELO.tflite X...]\n", argv[0]);
      return o == 'h' ? 0 : 2;
    }
  }
  if (optind < argc) {
    const char *ruta = argv[optind++];
    int n = argc - optind;
    if (n == 0) {
      fprintf(stderr, "faltan valores de entrada\n");
      return 2;
    }
    float *xs = (float *) malloc(n * sizeof(float));
    for (int i = 0; i < n; i++) xs[i] = strtof(argv[optind + i], NULL);
    unsigned char *m = leer_archivo(ruta);
    if (m == NULL) {
      perror(ruta);
      return 1;
    }
    int rc = correr(ruta, m, arena_kb, xs, n, false);
    free(m);
    free(xs);
    return rc;
  }

  float xs[9];
  for (int i = 0; i < 9; i++) xs[i] = (float) (i * 2 * M_PI / 8);
  int rc = correr("seno int8", g_seno_int8, 8, xs, 9, true);
  if (rc == 0) rc = correr("seno float", g_seno_float, 8, xs, 9, true);
  return rc;
}
