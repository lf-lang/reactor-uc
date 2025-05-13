# set -e
# echo "Building Patmos example"
# ${REACTOR_UC_PATH}/lfc/bin/lfc-dev hello_lf/src/HelloLF.lf
# cmake -Bbuild -DPLATFORM=PATMOS
# make -C build

# echo "Running Patmos example"
# pasim ./bin/hello_lf.elf


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