# Very simple Makefile script to build and compile all the LF tests.
SRCS = $(wildcard src/*.lf)
BINS = $(patsubst src/%.lf, bin/%, $(SRCS))
TIMEOUT_S = 30

SRCS_ONLY_BUILD = $(wildcard src/only_build/*.lf)
BINS_ONLY_BUILD = $(patsubst src/only_build/%.lf, bin/%, $(SRCS_ONLY_BUILD))

SRCS_LEGACY = $(wildcard src/legacy/*.lf)
BINS_LEGACY = $(patsubst src/legacy/%.lf, bin/%, $(SRCS_LEGACY))

LFC_PATH=../../lfc
LFC = ${LFC_PATH}/build/install/lf-cli/bin/lfc -c --runtime-symlink

.PHONY: all clean legacy
all: build_lfc ${BINS} legacy

build_lfc:
	${LFC_PATH}/bin/lfc-dev --version

bin/%: src/%.lf
	${LFC} $^
	timeout ${TIMEOUT_S}s ./$@
 
bin/%: src/only_build/%.lf
	${LFC} $^

bin/%: src/legacy/%.lf
	${LFC} $^
	timeout ${TIMEOUT_S}s ./$@

legacy: ${BINS_LEGACY}

clean:
	rm -rf build bin src-gen