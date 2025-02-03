#!/bin/bash
make all

PORT=tap0 make all -C ./CoapFederatedLF/r1
PORT=tap1 make all -C ./CoapFederatedLF/r2
