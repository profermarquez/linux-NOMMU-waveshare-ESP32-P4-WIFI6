# board.mk - rules for C++ programs on the Waveshare ESP32-P4 v2 image
# (Linux NOMMU, static uClibc-ng + LinuxThreads, bFLT executables).
#
# Include it from a Makefile that runs where the C++ toolchain is (the build
# VM), then a program is just:   PROGS += foo   (built from foo.cpp)
#
# Why each rule exists (all found on the board, phase 6):
#   -fPIC            without it every C++ exception crashed (SIGSEGV in the
#                    unwinder reading an unrelocated pointer)
#   -Wl,-elf2flt=-r  the program is loaded whole into RAM (plain -elf2flt
#                    segfaults on this kernel)
#   string-inst.o    linked first: otherwise the linker takes std::string
#                    from sstream-inst.o and drags in all of <locale>
#                    (std::thread demo: 803 KB -> 274 KB)
#   nommu-cxx.o      makes std::thread work with LinuxThreads
#   flthdr -s        stack size (64 KB)
#   size check       the program, its data and stack must fit one 512 KB
#                    block at exec (496 KB usable).  <iostream> alone needs
#                    ~543 KB: it only starts when a 1 MB block is free.  Use
#                    <cstdio> (printf) instead.

# The C++ toolchain (examples/cpp/LEEME.md).  ../host.mk sets these too.
CXX_TOOLCHAIN ?= $(HOME)/ws-out/cxx-toolchain
CROSS ?= $(CXX_TOOLCHAIN)/host/bin/riscv32-buildroot-linux-uclibc-
CXX := $(CROSS)g++
CC := $(CROSS)gcc
AR := $(CROSS)ar
FLTHDR := $(CROSS)flthdr

CXXSTD ?= -std=c++17
OPT ?= -Os
WARN ?= -Wall -Wextra
STACK ?= 65536
# bss_end + stack must stay below this (one 512 KB block, kernel extras).
RAM_LIMIT := 507904
# STRICT=1: a program over the limit is an error instead of a warning.
STRICT ?= 0

BOARD_FLAGS := -fPIC -ffunction-sections -fdata-sections
BOARD_LDFLAGS := -Wl,--gc-sections -pthread -Wl,-elf2flt=-r

B := build
SUPPORT := $(B)/string-inst.o $(B)/nommu-cxx.o
BOARD_MK_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

$(B):
	mkdir -p $@

$(B)/string-inst.o: | $(B)
	@test -x "$(CXX)" || { echo "no C++ compiler at $(CXX): see examples/cpp/LEEME.md (sdk/build-cxx-toolchain.sh)" >&2; exit 1; }
	cd $(B) && $(AR) x "$$($(CXX) -print-file-name=libstdc++.a)" string-inst.o

$(B)/nommu-cxx.o: $(BOARD_MK_DIR)nommu-cxx.c | $(B)
	$(CC) $(OPT) -Wall -fPIC -c $< -o $@

# bFLT header: bss_end at byte 20, stack size at 24 (big endian).
define board_check
	@need=$$(od -An -tu4 --endian=big -j 20 -N 8 $(1) | awk '{print $$1 + $$2}'); \
	if [ $$need -lt $(RAM_LIMIT) ]; then \
		echo "$(1): $$((need / 1024)) KB of RAM at exec (limit 496 KB)"; \
	else \
		echo "WARNING: $(1) needs $$((need / 1024)) KB at exec (over 496 KB): it only starts when a 1 MB block is free" >&2; \
		[ "$(STRICT)" = 0 ] || { rm -f $(1); exit 1; }; \
	fi
endef

%: %.cpp $(SUPPORT)
	$(CXX) $(CXXSTD) $(OPT) $(WARN) $(BOARD_FLAGS) $(CXXFLAGS) -o $@ $< $(SUPPORT) $(BOARD_LDFLAGS) $(LDFLAGS) $(LDLIBS)
	$(FLTHDR) -s $(STACK) $@
	rm -f $@.gdb
	$(call board_check,$@)
