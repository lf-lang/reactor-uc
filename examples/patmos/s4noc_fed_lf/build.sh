#!/bin/bash
LF_MAIN=S4NoCFedLF

# Generate configuration templates 
rm -rf $LF_MAIN
$REACTOR_UC_PATH/lfc/bin/lfc-dev --gen-fed-templates src/$LF_MAIN.lf


# Generate and build r1 sources
pushd ./$LF_MAIN/r1
    ./run_lfc.sh
    make clean
    make all 
popd

# Generate and build r2 sources
pushd ./$LF_MAIN/r2
    ./run_lfc.sh
    make clean
    make all 
popd

