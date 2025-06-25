#!/bin/bash
LF_MAIN=S4NoCFedLF
BIN_DIR=bin
# Generate configuration templates
rm -rf $LF_MAIN
$REACTOR_UC_PATH/lfc/bin/lfc-dev --gen-fed-templates src/$LF_MAIN.lf


# Generate and build r1 sources
pushd ./$LF_MAIN/r1
    ./run_lfc.sh
    make all 
popd

# Generate and build r2 sources
pushd ./$LF_MAIN/r2
    ./run_lfc.sh
    make all 
popd

mkdir -p $BIN_DIR
patmos-clang main.c ./$LF_MAIN/r1/bin/$LF_MAIN.a ./$LF_MAIN/r2/bin/$LF_MAIN.a \
    -o $BIN_DIR/$LF_MAIN
patemu $BIN_DIR/$LF_MAIN
