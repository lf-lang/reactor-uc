#!/bin/bash
set -e

BOARD=frdm_k64f
FED_PATHS=( "federated_sender" "federated_receiver1" "federated_receiver2" )

# Get the IDs of all NXP boards attached
FED_PROBE_IDS=($(LinkServer probes | grep -oP '(?<=MBED CMSIS-DAP\s{2})\S+'))
num_boards_attached=${#FED_PROBE_IDS[@]}
if [ "$num_boards_attached" -lt 2 ]; then
    echo "Error: At least 2 NXP boards must be attached. Found $num_boards_attached."
    exit 1
fi

# Build each board in parallel
build() {
for FED_PATH in "${FED_PATHS[@]}"; do
    pushd $FED_PATH
    west build -b $BOARD -p always &
    popd
done
wait
}

# Flash each board sequentially
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