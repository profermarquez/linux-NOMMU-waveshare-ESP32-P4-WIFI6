# C programs with SQLite and Mongoose for the Waveshare ESP32-P4 v2 board.
#
# From the Mac (the compiler lives in the build VM; make re-runs itself there):
#   sh ../terceros/descargar.sh sqlite mongoose     once: the sources
#   make                 compile sqlite3, servidor-web and api-sqlite
#   make deploy          copy them to the board (scp -O) into $(DESTINO)
#   make clean
# Variables: PLACA=stamp-p4.local USUARIO=m5 DESTINO=/home/m5/bin VM=rv32dev
#
# These programs are bigger than the 496 KB the C++ examples aim for:
# sqlite3 asks ~950 KB at exec, api-sqlite ~1.1 MB.  They start when the
# NOMMU pool has a free block that big (largest_free in
# /sys/kernel/board/nommu_pool; usually ~5 MB).

PROGS := sqlite3 servidor-web api-sqlite

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

T := ../terceros
SQLITE := $(T)/sqlite/src
MONGOOSE := $(T)/mongoose

# -fPIC and -elf2flt=-r: required on this kernel (see docs/COMPILAR.md).
CFLAGS := -Os -fPIC -ffunction-sections -fdata-sections -Wall
LDFLAGS := -Wl,--gc-sections -Wl,-elf2flt=-r

# SQLite for a NOMMU board on a slow microSD: no threads, no mmap and no
# WAL (both need shared mmap), no extensions.  The rest is standard SQLite,
# JSON functions included.
SQLITE_OPTS := -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION \
	-DSQLITE_OMIT_WAL -DSQLITE_DEFAULT_MMAP_SIZE=0 -DSQLITE_MAX_MMAP_SIZE=0 \
	-DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_OMIT_DEPRECATED -DSQLITE_DQS=0 \
	-DSQLITE_OMIT_SHARED_CACHE -DSQLITE_DEFAULT_CACHE_SIZE=-256 \
	-DHAVE_USLEEP=1
MONGOOSE_OPTS := -DMG_ENABLE_LINES=0 -DMG_ENABLE_LOG=1

all: $(PROGS)

sources:
	@test -f $(SQLITE)/sqlite3.c -a -f $(MONGOOSE)/mongoose.c || { \
	  echo "missing sources: run  sh ../terceros/descargar.sh sqlite mongoose" >&2; exit 1; }

# bFLT header: data_start 12, bss_end 20, stack 24 (big endian).
define show
	@echo "$(1): $$(od -An -tu4 --endian=big -j 20 -N 8 $(1) | awk '{print int(($$1 + $$2) / 1024)}') KB of RAM at exec"
endef

sqlite3: sources
	$(CC) $(CFLAGS) $(SQLITE_OPTS) -w -I$(SQLITE) $(SQLITE)/shell.c $(SQLITE)/sqlite3.c -o $@ $(LDFLAGS) -lm
	$(FLTHDR) -s 131072 $@
	rm -f $@.gdb
	$(call show,$@)

servidor-web: servidor-web.c sources
	$(CC) $(CFLAGS) $(MONGOOSE_OPTS) -I$(MONGOOSE) $< $(MONGOOSE)/mongoose.c -o $@ $(LDFLAGS)
	$(FLTHDR) -s 65536 $@
	rm -f $@.gdb
	$(call show,$@)

api-sqlite: api-sqlite.c sources
	$(CC) $(CFLAGS) $(MONGOOSE_OPTS) $(SQLITE_OPTS) -I$(MONGOOSE) -I$(SQLITE) $< \
	  $(MONGOOSE)/mongoose.c -x c -w $(SQLITE)/sqlite3.c -o $@ $(LDFLAGS) -lm
	$(FLTHDR) -s 65536 $@
	rm -f $@.gdb
	$(call show,$@)

clean:
	rm -f $(PROGS) $(addsuffix .gdb,$(PROGS))

endif

deploy: all
	ssh $(SSH_OPTS) $(USUARIO)@$(PLACA) 'mkdir -p $(DESTINO)'
	scp -O $(SSH_OPTS) $(PROGS) $(USUARIO)@$(PLACA):$(DESTINO)/
	@echo "copied to $(USUARIO)@$(PLACA):$(DESTINO): $(PROGS)"
