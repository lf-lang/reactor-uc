#!/bin/bash
LF_MAIN=CoapFederatedLF

# Generate configuration templates if they don't exist already
$REACTOR_UC_PATH/ulf/bin/ulfc-dev --gen-fed-templates src/$LF_MAIN.ulf

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
