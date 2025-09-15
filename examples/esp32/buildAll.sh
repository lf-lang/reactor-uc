#!/bin/bash
set -e

# Set the target platform
IDF_TARGET="esp32c6"
# List of folders
FOLDERS=("hello" "blink")

# setup IDF
source ${IDF_PATH}/export.sh

# Iterate over each folder and execute the command
for dir in "${FOLDERS[@]}"; do
    echo "Entering $dir"
		pushd $dir

    rm -rf build && mkdir build && cd build
    cmake .. -DCMAKE_TOOLCHAIN_FILE=$IDF_PATH/tools/cmake/toolchain-${IDF_TARGET}.cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DPLATFORM=ESP32 -DIDF_TARGET=${IDF_TARGET} -GNinja
    cmake --build .
		popd
done