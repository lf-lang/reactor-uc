#!/bin/bash

set -e

if ! command -v pasim &> /dev/null; then
  echo "Error: pasim command not found. Please ensure it is installed and available in your PATH."
else
    # Iterate over each folder and execute the command
    for dir in ./*; do
        if [ -d "$dir" ]; then
            # Skip directories that should not be built via this runner
            case "$dir" in
              "./s4noc_fed")
                echo "Skipping $dir (disabled for buildAll.sh)"
                continue
                ;;
            esac

            echo "Entering $dir"
            pushd "$dir"
            chmod +x build.sh
            ./build.sh
            popd
        fi
    done
fi