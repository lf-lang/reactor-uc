#!/bin/bash
set -e

# List of folders
FOLDERS=("hello" "federated")

# Iterate over each folder and execute the command
for dir in "${FOLDERS[@]}"; do
    echo "Entering $dir"
		pushd $dir
    cmake -B../build
    make -C ../build
		popd
done