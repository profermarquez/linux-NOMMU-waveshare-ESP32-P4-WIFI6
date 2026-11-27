# grabar, reproducir and radio: microphone, speaker and internet radio (Waveshare ESP32-P4 v2).
# Needs a kernel with /dev/ws-audio (patch 0094, capture + playback) and the boot shim
# that sets up the ES8311 in full duplex (ws_audio.c).
#
# On Linux, WSL2 (Windows) or macOS (re-runs in the Linux VM): see ../host.mk
#   sh ../terceros/descargar.sh minimp3   once: the MP3 decoder for radio
#   make                 compile
#   make deploy          copy grabar, reproducir, radio and radio-rfi.sh to $(DESTINO)
#   make clean
# Variables: PLACA=stamp-p4.local USUARIO=m5 DESTINO=/home/m5/bin (more in ../host.mk)

include ../host.mk

PROGS := grabar reproducir radio

.PHONY: all clean deploy

ifeq ($(EN_VM),1)

all:
	$(call vm_rerun,all)

clean:
	$(call vm_rerun,clean)

else

all: $(PROGS)

grabar reproducir: %: %.c
	$(call need_cc,$(CC))
	$(CC) -Os -fPIC -ffunction-sections -fdata-sections -Wall -Wextra $< -o $@ \
	  -Wl,--gc-sections -Wl,-elf2flt=-r -lm
	$(FLTHDR) -s 32768 $@
	rm -f $@.gdb
	$(call show_ram,$@)

# MP3 radio: minimp3 (CC0) from ../terceros/minimp3 (descargar.sh minimp3).
# -O2: the decoder runs on software floating point and needs the speed.
radio: radio.c
	$(call need_cc,$(CC))
	@test -f ../terceros/minimp3/minimp3.h || { echo "missing minimp3.h: run sh ../terceros/descargar.sh minimp3" >&2; exit 1; }
	$(CC) -O2 -fPIC -ffunction-sections -fdata-sections -Wall -I../terceros/minimp3 $< -o $@ \
	  -Wl,--gc-sections -Wl,-elf2flt=-r -lm
	$(FLTHDR) -s 65536 $@
	rm -f $@.gdb
	$(call show_ram,$@)

clean:
	rm -f $(PROGS) $(addsuffix .gdb,$(PROGS))

endif

deploy: all
	ssh $(SSH_OPTS) $(USUARIO)@$(PLACA) 'mkdir -p $(DESTINO)'
	$(SCP) $(SSH_OPTS) $(PROGS) radio-rfi.sh $(USUARIO)@$(PLACA):$(DESTINO)/
	ssh $(SSH_OPTS) $(USUARIO)@$(PLACA) 'cd $(DESTINO) && chmod 755 $(PROGS) radio-rfi.sh; sync'
	@echo "copied $(PROGS) radio-rfi.sh to $(USUARIO)@$(PLACA):$(DESTINO)"
