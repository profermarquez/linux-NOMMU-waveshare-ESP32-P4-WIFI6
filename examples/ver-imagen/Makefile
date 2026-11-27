# ver-imagen: show images in the terminal (Waveshare ESP32-P4 v2 board).
#
# On Linux, WSL2 (Windows) or macOS (re-runs in the Linux VM): see ../host.mk
#   sh ../terceros/descargar.sh stb      once: stb_image.h
#   make                 compile
#   make deploy          copy ver-imagen to $(DESTINO) and the MNIST sample
#                        digits (if tflite-micro was downloaded) to
#                        /home/m5/imagenes
# Variables: PLACA=stamp-p4.local USUARIO=m5 DESTINO=/home/m5/bin (more in ../host.mk)

include ../host.mk

MUESTRAS := $(wildcard ../terceros/tflite-micro/tensorflow/lite/micro/examples/mnist_lstm/samples/*.png)
STB := ../terceros/stb

.PHONY: all clean deploy

ifeq ($(EN_VM),1)

all:
	$(call vm_rerun,all)

clean:
	$(call vm_rerun,clean)

else

all: ver-imagen

ver-imagen: ver-imagen.c
	$(call need_cc,$(CC))
	@test -f $(STB)/stb_image.h || { echo "missing stb_image.h: run sh ../terceros/descargar.sh stb" >&2; exit 1; }
	$(CC) -Os -fPIC -ffunction-sections -fdata-sections -Wall -I$(STB) $< -o $@ \
	  -Wl,--gc-sections -Wl,-elf2flt=-r -lm
	$(FLTHDR) -s 65536 $@
	rm -f $@.gdb
	$(call show_ram,$@)

clean:
	rm -f ver-imagen ver-imagen.gdb

endif

deploy: all
	ssh $(SSH_OPTS) $(USUARIO)@$(PLACA) 'mkdir -p $(DESTINO) /home/$(USUARIO)/imagenes'
	$(SCP) $(SSH_OPTS) ver-imagen $(USUARIO)@$(PLACA):$(DESTINO)/
	$(if $(MUESTRAS),$(SCP) $(SSH_OPTS) $(MUESTRAS) $(USUARIO)@$(PLACA):/home/$(USUARIO)/imagenes/)
	@echo "copied ver-imagen to $(USUARIO)@$(PLACA):$(DESTINO)"
