# sysinfo for the Waveshare ESP32-P4-WIFI6 Linux (NOMMU, bFLT).
#
# On Linux, WSL2 (Windows) or macOS (re-runs in the Linux VM): see ../host.mk
#   make                 compile ./sysinfo
#   make deploy          copy it to $(DESTINO) on the board
#   make clean
# Variables: PLACA=stamp-p4.local USUARIO=m5 DESTINO=/home/m5/bin (more in ../host.mk)
#
# -Wl,-elf2flt alone builds, but the binary segfaults: the static uClibc keeps
# absolute pointers in .data that only the bFLT relocation table (-r) fixes.
# flthdr -s raises the 4 KiB default stack (printf needs more).

include ../host.mk

STACK ?= 65536

.PHONY: all clean deploy

ifeq ($(EN_VM),1)

all:
	$(call vm_rerun,all)

clean:
	$(call vm_rerun,clean)

else

all: sysinfo

sysinfo: sysinfo.c
	$(call need_cc,$(CC))
	$(CC) -Os -fPIC -Wall $< -o $@ -Wl,-elf2flt=-r
	$(FLTHDR) -s $(STACK) $@
	rm -f $@.gdb
	$(call show_ram,$@)

clean:
	rm -f sysinfo sysinfo.gdb

endif

deploy: all
	ssh $(SSH_OPTS) $(USUARIO)@$(PLACA) 'mkdir -p $(DESTINO)'
	$(SCP) $(SSH_OPTS) sysinfo $(USUARIO)@$(PLACA):$(DESTINO)/
	@echo "copied sysinfo to $(USUARIO)@$(PLACA):$(DESTINO)"
