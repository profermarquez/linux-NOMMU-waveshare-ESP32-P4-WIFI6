# C++ examples for the Waveshare ESP32-P4 v2 board.
#
# From the Mac (the compiler lives in the build VM; make re-runs itself there):
#   make                 compile
#   make deploy          copy to the board (scp -O) into $(DESTINO)
#   make run             deploy and run stl-demo and threads-demo
#   make clean
# Variables: PLACA=stamp-p4.local USUARIO=m5 DESTINO=/home/m5/bin VM=rv32dev
#
# A new program: add its name to PROGS and write NAME.cpp next to this file.
# Rules for the board (flags, size limit): board.mk.

PROGS := stl-demo sockets-demo threads-demo

PLACA ?= stamp-p4.local
USUARIO ?= m5
DESTINO ?= /home/$(USUARIO)/bin
VM ?= rv32dev
# The board makes a new SSH host key on every boot: do not record it.
SSH_OPTS ?= -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR

.PHONY: all clean deploy run

ifeq ($(shell uname -s),Darwin)

all:
	orb -m $(VM) make -C "$(CURDIR)" all

clean:
	orb -m $(VM) make -C "$(CURDIR)" clean

else

include board.mk

all: $(PROGS)

clean:
	rm -rf $(B) $(PROGS) $(addsuffix .gdb,$(PROGS))

endif

deploy: all
	ssh $(SSH_OPTS) $(USUARIO)@$(PLACA) 'mkdir -p $(DESTINO)'
	scp -O $(SSH_OPTS) $(PROGS) $(USUARIO)@$(PLACA):$(DESTINO)/
	@echo "copied to $(USUARIO)@$(PLACA):$(DESTINO): $(PROGS)"

run: deploy
	ssh $(SSH_OPTS) $(USUARIO)@$(PLACA) 'cd $(DESTINO) && ./stl-demo && ./threads-demo'
