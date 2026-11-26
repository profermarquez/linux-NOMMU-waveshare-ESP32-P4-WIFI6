# grabar and reproducir: record the microphone and play on the speaker (Waveshare ESP32-P4 v2).
# Needs a kernel with /dev/ws-audio (patch 0094, capture + playback) and the boot shim
# that sets up the ES8311 in full duplex (ws_audio.c).
#
# From the Mac (the compiler lives in the build VM; make re-runs itself there):
#   make                 compile
#   make deploy          copy grabar and reproducir to $(DESTINO)
# Variables: PLACA=stamp-p4.local USUARIO=m5 DESTINO=/home/m5/bin VM=rv32dev

PLACA ?= stamp-p4.local
USUARIO ?= m5
DESTINO ?= /home/$(USUARIO)/bin
VM ?= rv32dev
SSH_OPTS ?= -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR

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

all: grabar reproducir

grabar reproducir: %: %.c
	$(CC) -Os -fPIC -ffunction-sections -fdata-sections -Wall -Wextra $< -o $@ \
	  -Wl,--gc-sections -Wl,-elf2flt=-r -lm
	$(FLTHDR) -s 32768 $@
	rm -f $@.gdb
	@echo "$@: $$(od -An -tu4 --endian=big -j 20 -N 8 $@ | awk '{print int(($$1 + $$2) / 1024)}') KB of RAM at exec"

clean:
	rm -f grabar grabar.gdb reproducir reproducir.gdb

endif

deploy: all
	ssh $(SSH_OPTS) $(USUARIO)@$(PLACA) 'mkdir -p $(DESTINO)'
	scp -O $(SSH_OPTS) grabar reproducir $(USUARIO)@$(PLACA):$(DESTINO)/
	@echo "copied grabar and reproducir to $(DESTINO)"
