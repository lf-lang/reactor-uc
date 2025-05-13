#!/usr/bin/env bash
set -e

${REACTOR_UC_PATH}/lfc/bin/lfc-dev --gen-fed-templates src/MyFed.lf


pushd MyFed/src
./run_lfc.sh
cmake -Bbuild
cmake --build build
popd

if ! command -v west &> /dev/null; then
    echo "Error: 'west' is not installed."
else
    pushd MyFed/dest
    ./run_lfc.sh
    west build
    popd
fi
