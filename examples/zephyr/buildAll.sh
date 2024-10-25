#!/bin/bash

set -e

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