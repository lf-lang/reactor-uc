#!/bin/bash

# Source shared build helpers
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
. "$SCRIPT_DIR/../build-helpers.sh"

LF_MAIN=S4NoCFedLF
BIN_DIR=bin
CC=patmos-clang
N=2

# Parse command-line arguments
parse_build_args "$@"

# Optional environment knobs:
# - FORCE_REGEN_SRC_GEN=1 : remove all federate src-gen directories before generating
# - CLEAN_FEDERATES=1     : run `make clean` in each federate directory before rebuilding
if [ "${FORCE_REGEN_SRC_GEN:-0}" = "1" ]; then
    echo "FORCE_REGEN_SRC_GEN=1: removing all src-gen directories..."
    rm -rf "$LF_MAIN"/r*/src-gen
fi


usage() {
  echo "Usage: $0 [-e] [-f] [-h]"
  echo "  -e    Set default action to emulate"
  echo "  -f    Set default action to FPGA"
  echo "  -h    Show this help message"
  echo "  -d    Delete all .bc files in REACTOR_UC_PATH/src"
}

while getopts ":fedh" opt; do 
  case $opt in
    f) DEF_TOOL=f;;
    e) DEF_TOOL=e;;
    h) usage; exit 0;;
    d) rm -f $REACTOR_UC_PATH/src/*.bc;;
    :) echo "Option -$OPTARG requires an argument." >&2; exit 1;;
    \?) echo "Invalid option -$OPTARG" >&2; exit 1;;
  esac
done

$REACTOR_UC_PATH/ulf/bin/ulfc-dev --gen-fed-templates src/$LF_MAIN.ulf
if [ "${CLEAN_FEDERATES:-0}" = "1" ]; then
    echo "CLEAN_FEDERATES=1: running 'make clean' in each federate directory..."
    for fed_dir in "$LF_MAIN"/r*; do
        if [ -d "$fed_dir" ]; then
            make clean -C "$fed_dir" 2>/dev/null || true
        fi
    done
fi

# Main build sequence
rm -rf $BIN_DIR
cleanup_intermediates

generate_federate_scaffold "$LF_MAIN"
cleanup_generated_federate_outputs "$LF_MAIN"

# Build all federates
# r1: straightforward build
pushd ./$LF_MAIN/r1
    ./run_lfc.sh
    make all || exit 1 || { echo "Error: failed to build federate for $LF_MAIN." >&2; exit 1; }
popd

# r2+: build with federate-specific transformations
for i in $(seq 2 $N); do
    pushd ./$LF_MAIN/r$i
        REACTOR_PATH=$(pwd)/src-gen/$LF_MAIN/r$i
        ./run_lfc.sh
        sed -i "s/_lf_environment/_lf_environment_$i/g; s/lf_exit/lf_exit_$i/g; s/lf_start/lf_start_$i/g" $REACTOR_PATH/lf_start.c
        sed -i "s/(Federate/(Federate$i/g; s/FederateStartup/Federate${i}Startup/g; s/FederateShutdown/Federate${i}Shutdown/g; s/FederateClock/Federate${i}Clock/g; s/Reactor_${LF_MAIN}/Reactor_${LF_MAIN}_$i/g; s/${LF_MAIN}_r1/${LF_MAIN}_r1_$i/g" $REACTOR_PATH/lf_federate.h $REACTOR_PATH/lf_federate.c
        make all OBJECTS="$REACTOR_PATH/lf_federate.bc $REACTOR_PATH/$LF_MAIN/Dst.bc $REACTOR_PATH/lf_start.bc" || exit 1 || { echo "Error: failed to build federate for $LF_MAIN." >&2; exit 1; }
    popd
done

# Collect archives and link main executable
mkdir -p $BIN_DIR
A_FILES=$(collect_federate_archives "$LF_MAIN" "$N")

chmod +x ./gen_main.sh
./gen_main.sh N=$N

link_main_executable "$CC" main.c "$A_FILES" "$BIN_DIR/$LF_MAIN || exit 1"

# Cleanup nanopb and Unity intermediates
rm -f $REACTOR_UC_PATH/external/nanopb/pb_encode.bc $REACTOR_UC_PATH/external/nanopb/pb_decode.bc $REACTOR_UC_PATH/external/nanopb/pb_common.bc $REACTOR_UC_PATH/external/Unity/src/unity.bc

run_interactive_menu "$BIN_DIR" "$LF_MAIN"


