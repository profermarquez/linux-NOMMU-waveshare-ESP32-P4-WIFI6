# C++ examples for the Waveshare ESP32-P4 v2 board.
#
# On Linux, WSL2 (Windows) or macOS (re-runs in the Linux VM): see ../host.mk
#   make                 compile
#   make deploy          copy to the board (scp -O) into $(DESTINO)
#   make run             deploy and run stl-demo and threads-demo
#   make clean
# Variables: PLACA=stamp-p4.local USUARIO=m5 DESTINO=/home/m5/bin (more in ../host.mk)
#
# A new program: add its name to PROGS and write NAME.cpp next to this file.
# Rules for the board (flags, size limit): board.mk.

PROGS := stl-demo sockets-demo threads-demo

NEED_CXX := 1
include ../host.mk

.PHONY: all clean deploy run

ifeq ($(EN_VM),1)

all:
	$(call vm_rerun,all)

clean:
	$(call vm_rerun,clean)

else

include board.mk

all: $(PROGS)

clean:
	rm -rf $(B) $(PROGS) $(addsuffix .gdb,$(PROGS))

endif

deploy: all
	ssh $(SSH_OPTS) $(USUARIO)@$(PLACA) 'mkdir -p $(DESTINO)'
	$(SCP) $(SSH_OPTS) $(PROGS) $(USUARIO)@$(PLACA):$(DESTINO)/
	@echo "copied to $(USUARIO)@$(PLACA):$(DESTINO): $(PROGS)"

run: deploy
	ssh $(SSH_OPTS) $(USUARIO)@$(PLACA) 'cd $(DESTINO) && ./stl-demo && ./threads-demo'
