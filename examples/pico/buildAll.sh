#!/bin/env bash
set -e

if [ -z "$PICO_SDK_PATH" ]; then
  echo "Error: PICO_SDK_PATH is not defined. Please set it before running this script."
else
  cmake -Bbuild
  cmake --build build
fi
