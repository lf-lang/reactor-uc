#!/bin/bash

set -e

if ! command -v pasim &> /dev/null; then
  echo "Error: pasim command not found. Please ensure it is installed and available in your PATH."
else
    # Iterate over each folder and execute the command
    for dir in ./*; do
        if [ -d $dir ]; then
            echo "Entering $dir"
            pushd $dir
            ./build.sh
            popd
        fi
    done
fi