# TensorFlow Lite for Microcontrollers for the Waveshare ESP32-P4 v2 board.
#
# From the Mac (the compiler lives in the build VM; make re-runs itself there):
#   sh ../terceros/descargar.sh tflite-micro stb   once: sources
#   make                 build libtflm.a, tflm-seno and tflm-mnist
#   make deploy          copy both programs, the .tflite models and the
#                        MNIST sample digits to the board
#   make clean
# Variables: PLACA=stamp-p4.local USUARIO=m5 DESTINO=/home/m5/bin VM=rv32dev
#
# The library is built once into build/libtflm.a (a few minutes); a program
# only links the operators it registers.

PLACA ?= stamp-p4.local
USUARIO ?= m5
DESTINO ?= /home/$(USUARIO)/bin
VM ?= rv32dev
SSH_OPTS ?= -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR

.PHONY: all clean deploy

ifeq ($(shell uname -s),Darwin)

all:
	orb -m $(VM) make -C "$(CURDIR)" -j4 all

clean:
	orb -m $(VM) make -C "$(CURDIR)" clean

else

CXX_TOOLCHAIN ?= $(HOME)/ws-out/cxx-toolchain
CROSS ?= $(CXX_TOOLCHAIN)/host/bin/riscv32-buildroot-linux-uclibc-
CXX := $(CROSS)g++
CC := $(CROSS)gcc
AR := $(CROSS)ar
FLTHDR := $(CROSS)flthdr

TREE ?= ../terceros/tflm-tree
MODELS ?= ../terceros/tflite-micro/tensorflow/lite/micro/examples/hello_world/models
MNIST ?= ../terceros/tflite-micro/tensorflow/lite/micro/examples/mnist_lstm
SPEECH ?= ../terceros/tflite-micro/tensorflow/lite/micro/examples/micro_speech
STB ?= ../terceros/stb
B := build

# Same flags TFLM uses for microcontrollers (no exceptions/RTTI, static
# memory), plus the board's -fPIC and -elf2flt=-r (docs/COMPILAR.md).
# uClibc-ng has no strtod_l: flatbuffers must not use the locale variants.
DEFS := -DTF_LITE_STATIC_MEMORY -DTF_LITE_USE_CTIME -DNDEBUG -DFLATBUFFERS_LOCALE_INDEPENDENT=0
INC := -I$(TREE) -I$(TREE)/third_party/flatbuffers/include \
	-I$(TREE)/third_party/gemmlowp -I$(TREE)/third_party/kissfft \
	-I$(TREE)/third_party/ruy -I$(B)
COMMON := -Os -fPIC -ffunction-sections -fdata-sections $(DEFS) $(INC)
CXXFLAGS := -std=c++17 -fno-rtti -fno-exceptions -fno-threadsafe-statics \
	-fno-unwind-tables $(COMMON)
CFLAGS := -std=c11 $(COMMON)
LDFLAGS := -Wl,--gc-sections -Wl,-elf2flt=-r

# tensorflow/ plus signal/ (FFT, filter bank, PCAN... for micro_speech's
# audio preprocessor; kissfft is #included by signal/src/kiss_fft_wrappers).
LIB_CC := $(shell test -d $(TREE) && find $(TREE)/tensorflow $(TREE)/signal -name '*.cc' \
	! -name '*_test.cc' ! -path '*/examples/*')
LIB_C := $(shell test -d $(TREE) && find $(TREE)/tensorflow -name '*.c')
LIB_OBJ := $(patsubst $(TREE)/%,$(B)/lib/%.o,$(LIB_CC) $(LIB_C))

all: tflm-seno tflm-mnist tflm-voz

$(B)/lib/%.cc.o: $(TREE)/%.cc
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -w -c $< -o $@
$(B)/lib/%.c.o: $(TREE)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -w -c $< -o $@

$(B)/libtflm.a: $(LIB_OBJ)
	@test -n "$(LIB_OBJ)" || { echo "no TFLM tree: run sh ../terceros/descargar.sh tflite-micro" >&2; exit 1; }
	rm -f $@
	$(AR) rcs $@ $^
	@echo "$@: $(words $^) objects"

# The two hello_world models as C arrays.
$(B)/modelos.h: $(MODELS)/hello_world_int8.tflite $(MODELS)/hello_world_float.tflite
	@mkdir -p $(B)
	{ echo "// generated from TFLM hello_world models (Apache-2.0)"; \
	  echo "alignas(16) const unsigned char g_seno_int8[] = {"; \
	  xxd -i < $(MODELS)/hello_world_int8.tflite; echo "};"; \
	  echo "alignas(16) const unsigned char g_seno_float[] = {"; \
	  xxd -i < $(MODELS)/hello_world_float.tflite; echo "};"; } > $@

tflm-seno: tflm-seno.cc $(B)/modelos.h $(B)/libtflm.a
	$(CXX) $(CXXFLAGS) -Wall -o $@ $< $(B)/libtflm.a $(LDFLAGS) -lm
	$(FLTHDR) -s 65536 $@
	rm -f $@.gdb
	@echo "$@: $$(od -An -tu4 --endian=big -j 20 -N 8 $@ | awk '{print int(($$1 + $$2) / 1024)}') KB of RAM at exec"

# The mnist_lstm models (LSTM, 28x28 digit -> 10 classes) as C arrays.
$(B)/modelos-mnist.h: $(MNIST)/trained_lstm_int8.tflite $(MNIST)/trained_lstm.tflite
	@mkdir -p $(B)
	{ echo "// generated from TFLM mnist_lstm models (Apache-2.0)"; \
	  echo "alignas(16) const unsigned char g_mnist_int8[] = {"; \
	  xxd -i < $(MNIST)/trained_lstm_int8.tflite; echo "};"; \
	  echo "alignas(16) const unsigned char g_mnist_float[] = {"; \
	  xxd -i < $(MNIST)/trained_lstm.tflite; echo "};"; } > $@

tflm-mnist: tflm-mnist.cc $(B)/modelos-mnist.h $(B)/libtflm.a
	@test -f $(STB)/stb_image.h || { echo "missing stb_image.h: run sh ../terceros/descargar.sh stb" >&2; exit 1; }
	$(CXX) $(CXXFLAGS) -Wall -I$(STB) -o $@ $< $(B)/libtflm.a $(LDFLAGS) -lm
	$(FLTHDR) -s 65536 $@
	rm -f $@.gdb
	@echo "$@: $$(od -An -tu4 --endian=big -j 20 -N 8 $@ | awk '{print int(($$1 + $$2) / 1024)}') KB of RAM at exec"

# micro_speech: audio preprocessor + yes/no classifier as C arrays.
$(B)/modelos-voz.h: $(SPEECH)/models/audio_preprocessor_int8.tflite $(SPEECH)/models/micro_speech_quantized.tflite
	@mkdir -p $(B)
	{ echo "// generated from TFLM micro_speech models (Apache-2.0)"; \
	  echo "alignas(16) const unsigned char g_preproc[] = {"; \
	  xxd -i < $(SPEECH)/models/audio_preprocessor_int8.tflite; echo "};"; \
	  echo "alignas(16) const unsigned char g_speech[] = {"; \
	  xxd -i < $(SPEECH)/models/micro_speech_quantized.tflite; echo "};"; } > $@

tflm-voz: tflm-voz.cc $(B)/modelos-voz.h $(B)/libtflm.a
	$(CXX) $(CXXFLAGS) -Wall -o $@ $< $(B)/libtflm.a $(LDFLAGS) -lm
	$(FLTHDR) -s 65536 $@
	rm -f $@.gdb
	@echo "$@: $$(od -An -tu4 --endian=big -j 20 -N 8 $@ | awk '{print int(($$1 + $$2) / 1024)}') KB of RAM at exec"

clean:
	rm -rf $(B) tflm-seno tflm-seno.gdb tflm-mnist tflm-mnist.gdb tflm-voz tflm-voz.gdb

endif

deploy: all
	ssh $(SSH_OPTS) $(USUARIO)@$(PLACA) 'mkdir -p $(DESTINO) /home/$(USUARIO)/modelos /home/$(USUARIO)/imagenes'
	scp -O $(SSH_OPTS) tflm-seno tflm-mnist tflm-voz $(USUARIO)@$(PLACA):$(DESTINO)/
	ssh $(SSH_OPTS) $(USUARIO)@$(PLACA) 'mkdir -p /home/$(USUARIO)/audio'
	scp -O $(SSH_OPTS) ../terceros/tflite-micro/tensorflow/lite/micro/examples/micro_speech/testdata/*_1000ms.wav $(USUARIO)@$(PLACA):/home/$(USUARIO)/audio/
	scp -O $(SSH_OPTS) ../terceros/tflite-micro/tensorflow/lite/micro/examples/hello_world/models/*.tflite $(USUARIO)@$(PLACA):/home/$(USUARIO)/modelos/
	scp -O $(SSH_OPTS) ../terceros/tflite-micro/tensorflow/lite/micro/examples/mnist_lstm/samples/*.png $(USUARIO)@$(PLACA):/home/$(USUARIO)/imagenes/
	@echo "copied tflm-seno and tflm-mnist to $(DESTINO), the models to /home/$(USUARIO)/modelos, the digits to /home/$(USUARIO)/imagenes"
