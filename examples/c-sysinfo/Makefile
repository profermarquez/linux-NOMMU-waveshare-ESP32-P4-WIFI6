# sysinfo example for the Waveshare ESP32-P4-WIFI6 Linux (NOMMU, bFLT).
# Run on the Linux build machine:  make            -> ./sysinfo
# Copy:  scp -O sysinfo root@<board-ip>:/boot/programs/   (the board has no sftp: -O)
#
# -Wl,-elf2flt alone builds, but the binary segfaults: the static uClibc keeps
# absolute pointers in .data that only the bFLT relocation table (-r) fixes.
# flthdr -s raises the 4 KiB default stack (printf needs more).
TOOLBIN ?= $(HOME)/ws-out/buildroot/host/bin   # la toolchain de Buildroot (docs/COMPILAR.md)
CC      := $(TOOLBIN)/riscv32-linux-gcc
FLTHDR  := $(TOOLBIN)/riscv32-linux-flthdr
CFLAGS  ?= -Os -fPIC
LDFLAGS ?= -Wl,-elf2flt=-r
STACK   ?= 65536

sysinfo: sysinfo.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
	$(FLTHDR) -s $(STACK) $@ >/dev/null

clean:
	rm -f sysinfo *.gdb

.PHONY: clean
