#!/bin/bash
LF_MAIN=CoapFederatedLF

$REACTOR_UC_PATH/lfc/bin/lfc-dev --gen-fed-templates src/$LF_MAIN.lf

PORT=tap0 make all -C ./CoapFederatedLF/r1
PORT=tap1 make all -C ./CoapFederatedLF/r2
