#!/bin/bash

set -e

# List of folders
FOLDERS=("blinky" "hello")

# List of boards
BOARDS=("native" "nucleo-f429zi")

# Iterate over each board
for board in "${BOARDS[@]}"; do
	# Command to execute in each folder
	COMMAND="make BOARD=$board all"

	# Iterate over each folder and execute the command
	for dir in "${FOLDERS[@]}"; do
		echo "Entering $dir"
			pushd $dir
			$COMMAND
			popd
	done
done
