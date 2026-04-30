#!/bin/bash
LF_MAIN=CoapFederatedLF

# CURRENTLY THERE ARE PROBLEM WITH FED-GEN RETURNING EARLY
exit 0

# Generate configuration templates if they don't exist already
$REACTOR_UC_PATH/lfc/bin/lfc-dev --gen-fed-templates src/$LF_MAIN.lf

# Generate and build r1 sources
pushd ./$LF_MAIN/r1
    ./run_lfc.sh
    PORT=tap0 make all
popd

# Generate and build r2 sources
pushd ./$LF_MAIN/r2
    ./run_lfc.sh
    PORT=tap1 make all
popd
