# ver-imagen: show images in the terminal (Waveshare ESP32-P4 v2 board).
#
# From the Mac (the compiler lives in the build VM; make re-runs itself there):
#   sh ../terceros/descargar.sh stb      once: stb_image.h
#   make                 compile
#   make deploy          copy ver-imagen to $(DESTINO) and the MNIST sample
#                        digits (if tflite-micro was downloaded) to
#                        /home/m5/imagenes
# Variables: PLACA=stamp-p4.local USUARIO=m5 DESTINO=/home/m5/bin VM=rv32dev

PLACA ?= stamp-p4.local
USUARIO ?= m5
DESTINO ?= /home/$(USUARIO)/bin
VM ?= rv32dev
SSH_OPTS ?= -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR
MUESTRAS := $(wildcard ../terceros/tflite-micro/tensorflow/lite/micro/examples/mnist_lstm/samples/*.png)

.PHONY: all clean deploy

ifeq ($(shell uname -s),Darwin)

all:
	orb -m $(VM) make -C "$(CURDIR)" all

clean:
	orb -m $(VM) make -C "$(CURDIR)" clean

else

CXX_TOOLCHAIN ?= $(HOME)/ws-out/cxx-toolchain
CROSS ?= $(CXX_TOOLCHAIN)/host/bin/riscv32-buildroot-linux-uclibc-
CC := $(CROSS)gcc
FLTHDR := $(CROSS)flthdr
STB := ../terceros/stb

all: ver-imagen

ver-imagen: ver-imagen.c
	@test -f $(STB)/stb_image.h || { echo "missing stb_image.h: run sh ../terceros/descargar.sh stb" >&2; exit 1; }
	$(CC) -Os -fPIC -ffunction-sections -fdata-sections -Wall -I$(STB) $< -o $@ \
	  -Wl,--gc-sections -Wl,-elf2flt=-r -lm
	$(FLTHDR) -s 65536 $@
	rm -f $@.gdb
	@echo "$@: $$(od -An -tu4 --endian=big -j 20 -N 8 $@ | awk '{print int(($$1 + $$2) / 1024)}') KB of RAM at exec"

clean:
	rm -f ver-imagen ver-imagen.gdb

endif

deploy: all
	ssh $(SSH_OPTS) $(USUARIO)@$(PLACA) 'mkdir -p $(DESTINO) /home/$(USUARIO)/imagenes'
	scp -O $(SSH_OPTS) ver-imagen $(USUARIO)@$(PLACA):$(DESTINO)/
	$(if $(MUESTRAS),scp -O $(SSH_OPTS) $(MUESTRAS) $(USUARIO)@$(PLACA):/home/$(USUARIO)/imagenes/)
	@echo "copied ver-imagen to $(DESTINO)"
