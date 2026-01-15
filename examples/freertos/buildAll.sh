#!/bin/bash
set -e

# Set the target board
PICO_BOARD="pico_w"
PICO_FOLDER="pico"
echo "Entering $PICO_FOLDER"
    pushd $PICO_FOLDER

rm -rf build && mkdir build && cd build
cmake -DPICO_BOARD=pico_w -DPLATFORM=FREERTOS ..
cmake --build . -j
    popd
