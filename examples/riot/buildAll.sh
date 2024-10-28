#!/bin/bash

set -e

# List of folders
FOLDERS=("blinky" "hello" )

BOARD=nucleo-f429zi

# Command to execute in each folder
COMMAND="make BOARD=$BOARD all"

# Iterate over each folder and execute the command
for dir in "${FOLDERS[@]}"; do
    echo "Entering $dir"
		pushd $dir
		$COMMAND
		popd
done