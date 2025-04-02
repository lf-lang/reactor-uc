#!/bin/bash

set -e

# Check if 'west' is installed
if ! command -v west &> /dev/null; then
    echo "Error: 'west' is not installed. Please install it and the Zephyr SDK as described in https://github.com/lf-lang/lf-zephyr-uc-template/"
    exit 1
fi

# List of folders
FOLDERS=("blinky" "hello" "basic_federated/federated_sender" "basic_federated/federated_receiver1" "basic_federated/federated_receiver2" )

BOARD=frdm_k64f

# Command to execute in each folder
COMMAND="west build -b $BOARD -p always"

# Iterate over each folder and execute the command
for dir in "${FOLDERS[@]}"; do
    echo "Entering $dir"
		pushd $dir
		$COMMAND
		popd
done

# Build lf example
pushd hello_lf
run/build.sh
popd