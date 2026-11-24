// tflm-voz - recognize the words "yes" and "no" with TensorFlow Lite Micro
// (TFLM's micro_speech example) on the ESP32-P4 board.
//
//   tflm-voz ARCHIVO.wav...     classify 1-second WAV clips (16 kHz, 16 bits)
//   tflm-voz -m [-s SEGUNDOS]   listen to the board's microphone
//                               (/dev/ws-audio) and print every word heard
//   options:  -u UMBRAL   score needed to report a word (default 0.80)
//             -v          microphone mode: also print each inference
//
// Two models run back to back, both built into the program:
//   1. audio_preprocessor_int8: every 20 ms, a 30 ms window of audio -> 40
//      features (FFT, mel filter bank, noise reduction, log)
//   2. micro_speech_quantized: the last 49 feature rows (~1 s) -> scores
//      for silence / unknown / yes / no
// The model only knows "yes" and "no" (English), as in TFLM's example.
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/schema/schema_generated.h"

#include "modelos-voz.h"  // g_preproc[], g_speech[] (made by make)

namespace {

constexpr int kRate = 16000;
constexpr int kFeatureSize = 40;
constexpr int kFeatureCount = 49;
constexpr int kWindow = 30 * kRate / 1000;  // 480 samples
constexpr int kStride = 20 * kRate / 1000;  // 320 samples
constexpr int kCategories = 4;
const char *kLabels[kCategories] = {"silencio", "desconocido", "yes", "no"};
constexpr size_t kArenaPre = 16 * 1024;
constexpr size_t kArenaSpeech = 32 * 1024;

using PreResolver = tflite::MicroMutableOpResolver<18>;
using SpeechResolver = tflite::MicroMutableOpResolver<4>;

double ahora() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

bool registrar(PreResolver &r) {
  return r.AddReshape() == kTfLiteOk && r.AddCast() == kTfLiteOk &&
         r.AddStridedSlice() == kTfLiteOk && r.AddConcatenation() == kTfLiteOk &&
         r.AddMul() == kTfLiteOk && r.AddAdd() == kTfLiteOk &&
         r.AddDiv() == kTfLiteOk && r.AddMinimum() == kTfLiteOk &&
         r.AddMaximum() == kTfLiteOk && r.AddWindow() == kTfLiteOk &&
         r.AddFftAutoScale() == kTfLiteOk && r.AddRfft() == kTfLiteOk &&
         r.AddEnergy() == kTfLiteOk && r.AddFilterBank() == kTfLiteOk &&
         r.AddFilterBankSquareRoot() == kTfLiteOk &&
         r.AddFilterBankSpectralSubtraction() == kTfLiteOk &&
         r.AddPCAN() == kTfLiteOk && r.AddFilterBankLog() == kTfLiteOk;
}

bool registrar(SpeechResolver &r) {
  return r.AddReshape() == kTfLiteOk && r.AddFullyConnected() == kTfLiteOk &&
         r.AddDepthwiseConv2D() == kTfLiteOk && r.AddSoftmax() == kTfLiteOk;
}

// The two interpreters, created once.
struct Motor {
  PreResolver pre_ops;
  SpeechResolver speech_ops;
  alignas(16) uint8_t arena_pre[kArenaPre];
  alignas(16) uint8_t arena_speech[kArenaSpeech];
  tflite::MicroInterpreter *pre = nullptr;
  tflite::MicroInterpreter *speech = nullptr;
  int8_t features[kFeatureCount][kFeatureSize];

  bool iniciar() {
    if (!registrar(pre_ops) || !registrar(speech_ops)) {
      fprintf(stderr, "no se pudieron registrar los operadores\n");
      return false;
    }
    static tflite::MicroInterpreter p(tflite::GetModel(g_preproc), pre_ops,
                                      arena_pre, kArenaPre);
    static tflite::MicroInterpreter s(tflite::GetModel(g_speech), speech_ops,
                                      arena_speech, kArenaSpeech);
    pre = &p;
    speech = &s;
    if (pre->AllocateTensors() != kTfLiteOk ||
        speech->AllocateTensors() != kTfLiteOk) {
      fprintf(stderr, "AllocateTensors fallo\n");
      return false;
    }
    memset(features, 0, sizeof(features));
    return true;
  }

  // One feature row from kWindow samples.
  bool fila(const int16_t *audio, int8_t out[kFeatureSize]) {
    memcpy(pre->input(0)->data.i16, audio, kWindow * sizeof(int16_t));
    if (pre->Invoke() != kTfLiteOk) return false;
    memcpy(out, pre->output(0)->data.int8, kFeatureSize);
    return true;
  }

  // Scores (0..1) for the current kFeatureCount rows.
  bool clasificar(float p[kCategories]) {
    TfLiteTensor *in = speech->input(0), *out = speech->output(0);
    memcpy(in->data.int8, features, sizeof(features));
    if (speech->Invoke() != kTfLiteOk) return false;
    for (int k = 0; k < kCategories; k++)
      p[k] = (out->data.int8[k] - out->params.zero_point) * out->params.scale;
    return true;
  }

  void desplazar(const int8_t nueva[kFeatureSize]) {
    memmove(features[0], features[1], (kFeatureCount - 1) * kFeatureSize);
    memcpy(features[kFeatureCount - 1], nueva, kFeatureSize);
  }
};

int mejor(const float p[kCategories]) {
  int m = 0;
  for (int k = 1; k < kCategories; k++)
    if (p[k] > p[m]) m = k;
  return m;
}

// Minimal WAV reader: PCM 16-bit, mono or stereo (left channel used).
int16_t *leer_wav(const char *ruta, int *n, int *tasa) {
  FILE *f = fopen(ruta, "rb");
  if (!f) return NULL;
  unsigned char h[12];
  int canales = 0, bits = 0;
  int16_t *datos = NULL;
  *n = 0;
  if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4))
    goto fin;
  for (;;) {
    unsigned char c[8];
    if (fread(c, 1, 8, f) != 8) goto fin;
    uint32_t len = c[4] | c[5] << 8 | c[6] << 16 | (uint32_t) c[7] << 24;
    if (!memcmp(c, "fmt ", 4)) {
      unsigned char fm[16];
      if (len < 16 || fread(fm, 1, 16, f) != 16) goto fin;
      canales = fm[2] | fm[3] << 8;
      *tasa = fm[4] | fm[5] << 8 | fm[6] << 16 | fm[7] << 24;
      bits = fm[14] | fm[15] << 8;
      fseek(f, len - 16, SEEK_CUR);
    } else if (!memcmp(c, "data", 4)) {
      if (bits != 16 || canales < 1 || canales > 2) goto fin;
      int frames = len / (2 * canales);
      datos = (int16_t *) malloc(frames * sizeof(int16_t));
      if (!datos) goto fin;
      for (int i = 0; i < frames; i++) {
        int16_t s[2];
        if (fread(s, 2, canales, f) != (size_t) canales) break;
        datos[(*n)++] = s[0];
      }
      goto fin;
    } else {
      fseek(f, len + (len & 1), SEEK_CUR);
    }
  }
fin:
  fclose(f);
  return datos;
}

int modo_archivos(Motor &m, int argc, char **argv) {
  int rc = 0;
  for (int a = 0; a < argc; a++) {
    int n = 0, tasa = 0;
    int16_t *audio = leer_wav(argv[a], &n, &tasa);
    if (!audio) {
      fprintf(stderr, "%s: no es un WAV PCM de 16 bits\n", argv[a]);
      rc = 1;
      continue;
    }
    if (tasa != kRate) {
      fprintf(stderr, "%s: %d Hz, hace falta 16000 Hz\n", argv[a], tasa);
      free(audio);
      rc = 1;
      continue;
    }
    // Fresh preprocessor state per clip, as TFLM's test does.
    m.pre->Reset();
    memset(m.features, 0, sizeof(m.features));
    double t0 = ahora();
    int filas = 0;
    for (int pos = 0; pos + kWindow <= n && filas < kFeatureCount; pos += kStride)
      if (m.fila(audio + pos, m.features[filas])) filas++;
    double t1 = ahora();
    float p[kCategories];
    if (!m.clasificar(p)) {
      fprintf(stderr, "%s: Invoke fallo\n", argv[a]);
      free(audio);
      rc = 1;
      continue;
    }
    double t2 = ahora();
    int k = mejor(p);
    printf("%s: %s (%.0f %%)   [silencio %.2f  desconocido %.2f  yes %.2f  no %.2f]"
           "  %d filas, %.0f ms + %.0f ms\n",
           argv[a], kLabels[k], p[k] * 100, p[0], p[1], p[2], p[3], filas,
           (t1 - t0) * 1000, (t2 - t1) * 1000);
    free(audio);
  }
  return rc;
}

int modo_microfono(Motor &m, int segundos, float umbral, bool verbose) {
  int fd = open("/dev/ws-audio", O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "/dev/ws-audio: %s\n", strerror(errno));
    return 1;
  }
  printf("Escuchando%s... decí \"yes\" o \"no\" (Ctrl+C para salir)\n",
         segundos ? "" : " sin limite");
  fflush(stdout);
  static int16_t ventana[kWindow];
  int llenas = 0;                 // feature rows since start
  int en_ventana = 0;             // samples in ventana
  int desde_clasif = 0;
  float media[kCategories] = {0, 0, 0, 0};
  double t0 = ahora(), ultimo_aviso = -10, ms_pre = 0, ms_cls = 0;
  long n_pre = 0, n_cls = 0;
  static int16_t bloque[2040];    // 4080 bytes = 1020 stereo frames

  while (!segundos || ahora() - t0 < segundos) {
    ssize_t r = read(fd, bloque, sizeof(bloque));
    if (r < 0) {
      if (errno == EINTR) continue;
      fprintf(stderr, "lectura: %s\n", strerror(errno));
      break;
    }
    for (int i = 0; i < (int) (r / 4); i++) {
      ventana[en_ventana++] = bloque[2 * i];  // left = microphone
      if (en_ventana < kWindow) continue;
      int8_t f[kFeatureSize];
      double a = ahora();
      bool ok = m.fila(ventana, f);
      ms_pre += (ahora() - a) * 1000;
      n_pre++;
      // keep the last 10 ms: windows overlap by kWindow - kStride
      memmove(ventana, ventana + kStride, (kWindow - kStride) * sizeof(int16_t));
      en_ventana = kWindow - kStride;
      if (!ok) continue;
      m.desplazar(f);
      llenas++;
      // classify every 10 rows (200 ms) once a full second is buffered:
      // one classification takes ~62 ms on the board (one core)
      if (llenas < kFeatureCount || ++desde_clasif < 10) continue;
      desde_clasif = 0;
      float p[kCategories];
      a = ahora();
      if (!m.clasificar(p)) continue;
      ms_cls += (ahora() - a) * 1000;
      n_cls++;
      for (int k = 0; k < kCategories; k++) media[k] = 0.6f * media[k] + 0.4f * p[k];
      int k = mejor(media);
      double t = ahora() - t0;
      if (verbose)
        printf("%6.1f s  silencio %.2f  desconocido %.2f  yes %.2f  no %.2f\n", t,
               media[0], media[1], media[2], media[3]);
      if (k >= 2 && media[k] >= umbral && t - ultimo_aviso > 1.0) {
        printf("%6.1f s  >>> %s (%.0f %%)\n", t, kLabels[k], media[k] * 100);
        ultimo_aviso = t;
      }
      fflush(stdout);
    }
  }
  close(fd);
  if (n_pre && n_cls)
    printf("promedio: preprocesado %.2f ms cada 20 ms, clasificacion %.1f ms cada 200 ms\n",
           ms_pre / n_pre, ms_cls / n_cls);
  return 0;
}

}  // namespace

int main(int argc, char **argv) {
  bool microfono = false, verbose = false;
  int segundos = 0, o;
  float umbral = 0.80f;
  while ((o = getopt(argc, argv, "ms:u:vh")) != -1) {
    if (o == 'm') microfono = true;
    else if (o == 's') segundos = atoi(optarg);
    else if (o == 'u') umbral = strtof(optarg, NULL);
    else if (o == 'v') verbose = true;
    else {
      fprintf(stderr, "uso: %s ARCHIVO.wav...  |  %s -m [-s SEGUNDOS] [-u UMBRAL] [-v]\n",
              argv[0], argv[0]);
      return o == 'h' ? 0 : 2;
    }
  }
  static Motor m;
  if (!m.iniciar()) return 1;
  if (microfono) return modo_microfono(m, segundos, umbral, verbose);
  if (optind >= argc) {
    fprintf(stderr, "falta un .wav, o -m para el microfono (tflm-voz -h)\n");
    return 2;
  }
  return modo_archivos(m, argc - optind, argv + optind);
}
