#!/bin/bash
set -e

BOARD=frdm_k64f
FED_PATHS=( "federated_sender" "federated_receiver")
FED_PROBE_IDS=("0240020119CE5E0CE430A3B4" "0240020119CD5E08E433A3B0")

# Build each board in parallel
build() {
for FED_PATH in "${FED_PATHS[@]}"; do
    pushd $FED_PATH
    west build -b $BOARD -p always &
    popd
done
wait
}

# Flash each board in parallel
flash() {
for i in "${!FED_PATHS[@]}"; do
    FED_PATH=${FED_PATHS[$i]}
    FED_PROBE_ID=${FED_PROBE_IDS[$i]}
    pushd $FED_PATH
    west flash --probe $FED_PROBE_ID
    popd
done
}

build
flash