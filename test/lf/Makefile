# Very simple Makefile script to build and compile all the LF tests.
SRCS = $(wildcard src/*.lf)
BINS = $(patsubst src/%.lf, bin/%, $(SRCS))
LFC_PATH=../../lfc
LFC = ${LFC_PATH}/build/install/lf-cli/bin/lfc

.PHONY: all clean
all: build_lfc ${BINS}

build_lfc:
	cd ${LFC_PATH} && ./gradle assemble

bin/%: src/%.lf
	${LFC} $^ -c
	./$@

 
clean:
	rm -rf build bin src-gen