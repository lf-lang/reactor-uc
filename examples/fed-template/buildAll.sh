#!/usr/bin/env bash
set -e

${REACTOR_UC_PATH}/lfuc/bin/lfuc-dev --gen-fed-templates src/MyFed.lf


pushd MyFed/src
./run_lfuc.sh
cmake -Bbuild
cmake --build build
popd

pushd MyFed/dest
./run_lfuc.sh
west build
popd

