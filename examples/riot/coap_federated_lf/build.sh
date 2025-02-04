#!/bin/bash
LF_MAIN=CoapFederatedLF

# Generate configuration templates if they don't exist already
$REACTOR_UC_PATH/lfc/bin/lfc-dev --gen-fed-templates src/$LF_MAIN.lf

# Generate and build r1 sources
pushd ./$LF_MAIN/r1
    ./generate.sh
    PORT=tap0 make all
popd

# Generate and build r2 sources
pushd ./$LF_MAIN/r2
    ./generate.sh
    PORT=tap1 make all
popd
