# gpio-parpadeo: libgpiod 2 example (it is also in the image: /usr/bin/gpio-parpadeo).
#
# On Linux, WSL2 (Windows) or macOS (re-runs in the Linux VM): see ../host.mk
#   make                 compile ./gpio-parpadeo
#   make deploy          copy it to $(DESTINO) on the board
#   make clean
# libgpiod comes from the Buildroot build of the image ($(BUILDROOT_OUT)/staging),
# so this one needs ~/ws-out/buildroot, not only the C++ toolchain.
# Variables: PLACA=stamp-p4.local USUARIO=m5 DESTINO=/home/m5/bin (more in ../host.mk)

include ../host.mk

SYSROOT ?= $(BUILDROOT_OUT)/staging

.PHONY: all clean deploy

ifeq ($(EN_VM),1)

all:
	$(call vm_rerun,all)

clean:
	$(call vm_rerun,clean)

else

all: gpio-parpadeo

gpio-parpadeo: gpio-parpadeo.c
	$(call need_cc,$(CC))
	@test -f $(SYSROOT)/usr/include/gpiod.h || { echo "missing libgpiod: $(SYSROOT)/usr/include/gpiod.h (build the image, docs/COMPILAR.md)" >&2; exit 1; }
	$(CC) -Os -fPIC -Wall -I$(SYSROOT)/usr/include $< -o $@ \
	  -L$(SYSROOT)/usr/lib -Wl,-elf2flt=-r -lgpiod
	$(FLTHDR) -s 65536 $@
	rm -f $@.gdb
	$(call show_ram,$@)

clean:
	rm -f gpio-parpadeo gpio-parpadeo.gdb

endif

deploy: all
	ssh $(SSH_OPTS) $(USUARIO)@$(PLACA) 'mkdir -p $(DESTINO)'
	$(SCP) $(SSH_OPTS) gpio-parpadeo $(USUARIO)@$(PLACA):$(DESTINO)/
	@echo "copied gpio-parpadeo to $(USUARIO)@$(PLACA):$(DESTINO)"
