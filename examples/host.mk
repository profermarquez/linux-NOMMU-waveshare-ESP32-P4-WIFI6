# host.mk - what every example Makefile shares: where it compiles and where it copies.
#
# The board's cross-compiler (riscv32 uClibc-ng, bFLT) is a Linux program, built with
# the images (docs/COMPILAR.md).  So:
#   Linux                  make compiles here.
#   Windows                run make inside WSL2 (Ubuntu): it is Linux, same as above.
#   macOS                  make re-runs itself inside a Linux VM with $(VM_RUN).  The
#                          VM must see this folder at the same path, with write access.
#                          Default: OrbStack, machine $(VM).  Others, for example:
#                            make VM_RUN="limactl shell rv32dev"
#                          EN_VM=0 compiles on the Mac itself (only with a native toolchain).
#
# Variables (on the command line or in the environment):
#   PLACA=stamp-p4.local  board name or IP (WSL2 does not resolve .local: use the IP)
#   USUARIO=m5            user on the board
#   DESTINO=/home/m5/bin  where deploy copies the programs (microSD)
#   VM=rv32dev  VM_RUN=   only on macOS, see above
#   CROSS=                compiler prefix; by default the C++ toolchain if it exists,
#                         otherwise the Buildroot one (C only; ~/ws-out/buildroot)

PLACA ?= stamp-p4.local
USUARIO ?= m5
DESTINO ?= /home/$(USUARIO)/bin
# The board makes a new SSH host key on every boot: do not record it.
SSH_OPTS ?= -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR
# scp -O: the board has no sftp-server.  Old scp without -O (OpenSSH < 8.7): SCP=scp
SCP ?= scp -O

VM ?= rv32dev
VM_RUN ?= orb -m $(VM)
ifeq ($(shell uname -s),Darwin)
EN_VM ?= 1
else
EN_VM ?= 0
endif

CXX_TOOLCHAIN ?= $(HOME)/ws-out/cxx-toolchain
BUILDROOT_OUT ?= $(HOME)/ws-out/buildroot
TRIPLE := riscv32-buildroot-linux-uclibc-
# NEED_CXX=1 (set before the include): C++ programs, only the C++ toolchain has g++.
ifeq ($(NEED_CXX),1)
CROSS ?= $(CXX_TOOLCHAIN)/host/bin/$(TRIPLE)
endif
CROSS ?= $(firstword $(patsubst %gcc,%,$(wildcard $(CXX_TOOLCHAIN)/host/bin/$(TRIPLE)gcc \
	$(BUILDROOT_OUT)/host/bin/$(TRIPLE)gcc)) $(CXX_TOOLCHAIN)/host/bin/$(TRIPLE))
CC := $(CROSS)gcc
CXX := $(CROSS)g++
AR := $(CROSS)ar
FLTHDR := $(CROSS)flthdr

# $(call need_cc,COMPILER) in a recipe: a clear error instead of "gcc: not found".
define need_cc
	@test -x "$(1)" || { echo "no compiler at $(1): build it first (docs/COMPILAR.md;" \
	  "C++: examples/cpp/LEEME.md) or pass CROSS=/path/prefix-" >&2; exit 1; }
endef

# $(call show_ram,PROGRAM): RAM the bFLT asks at exec (bss_end + stack, big endian).
define show_ram
	@echo "$(1): $$(od -An -tu4 --endian=big -j 20 -N 8 $(1) | awk '{print int(($$1 + $$2) / 1024)}') KB of RAM at exec"
endef

# $(call vm_rerun,TARGETS): the macOS rule body that runs make inside the VM.
define vm_rerun
	$(VM_RUN) make -C "$(CURDIR)" EN_VM=0 $(1)
endef
