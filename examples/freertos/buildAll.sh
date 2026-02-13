#!/bin/bash
set -e
if [ -z "$PICO_SDK_PATH" ]; then
  echo "Error: PICO_SDK_PATH is not defined. Please set it before running this script."
else
    # Set the target board
    PICO_BOARD="pico_w"
    PICO_FOLDER="pico"
    echo "Entering $PICO_FOLDER"
        pushd $PICO_FOLDER

    rm -rf build && mkdir build && cd build
    cmake -DPICO_BOARD=pico_w -DPLATFORM=FREERTOS ..
    cmake --build . -j
        popd
fi