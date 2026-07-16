#!/bin/bash

# Source shared build helpers
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
. "$SCRIPT_DIR/../build-helpers.sh"

LF_MAIN=S4NoCFedLF3
BIN_DIR=bin
CC=patmos-clang
N=3

# Parse command-line arguments
parse_build_args "$@"

# Optional environment knobs:
# - FORCE_REGEN_SRC_GEN=1 : remove all federate src-gen directories before generating
# - CLEAN_FEDERATES=1     : run `make clean` in each federate directory before rebuilding
if [ "${FORCE_REGEN_SRC_GEN:-0}" = "1" ]; then
    echo "FORCE_REGEN_SRC_GEN=1: removing all src-gen directories..."
    rm -rf "$LF_MAIN"/r*/src-gen
fi

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
    chmod +x ./run_lfc.sh
    ./run_lfc.sh
    make all || { echo "Error: failed to build federate for $LF_MAIN." >&2; exit 1; }
popd

# r2: build Repeater with proper federate uniqueness
pushd ./$LF_MAIN/r2
    REACTOR_PATH=$(pwd)/src-gen/$LF_MAIN/r2
    chmod +x ./run_lfc.sh
    ./run_lfc.sh
    sed -i "s/_lf_environment/_lf_environment_2/g; s/lf_exit/lf_exit_2/g; s/lf_start/lf_start_2/g" $REACTOR_PATH/lf_start.c
    sed -i "s/(Federate/(Federate2/g; s/FederateStartup/Federate2Startup/g; s/FederateShutdown/Federate2Shutdown/g; s/FederateClock/Federate2Clock/g; s/Reactor_${LF_MAIN}/Reactor_${LF_MAIN}_2/g; s/${LF_MAIN}_r1/${LF_MAIN}_r1_2/g; s/${LF_MAIN}_r3/${LF_MAIN}_r3_2/g" $REACTOR_PATH/lf_federate.h $REACTOR_PATH/lf_federate.c
    make all OBJECTS="$REACTOR_PATH/lf_federate.bc $REACTOR_PATH/$LF_MAIN/Repeater.bc $REACTOR_PATH/lf_start.bc" || { echo "Error: failed to build federate r2 for $LF_MAIN." >&2; exit 1; }
popd

# r3: build Receiver with proper federate uniqueness
pushd ./$LF_MAIN/r3
    REACTOR_PATH=$(pwd)/src-gen/$LF_MAIN/r3
    chmod +x ./run_lfc.sh
    ./run_lfc.sh
    sed -i "s/_lf_environment/_lf_environment_3/g; s/lf_exit/lf_exit_3/g; s/lf_start/lf_start_3/g" $REACTOR_PATH/lf_start.c
    sed -i "s/(Federate/(Federate3/g; s/FederateStartup/Federate3Startup/g; s/FederateShutdown/Federate3Shutdown/g; s/FederateClock/Federate3Clock/g; s/Reactor_${LF_MAIN}/Reactor_${LF_MAIN}_3/g; s/${LF_MAIN}_r1/${LF_MAIN}_r1_3/g; s/${LF_MAIN}_r2/${LF_MAIN}_r2_3/g" $REACTOR_PATH/lf_federate.h $REACTOR_PATH/lf_federate.c
    make all OBJECTS="$REACTOR_PATH/lf_federate.bc $REACTOR_PATH/$LF_MAIN/Receiver.bc $REACTOR_PATH/lf_start.bc" || { echo "Error: failed to build federate r3 for $LF_MAIN." >&2; exit 1; }
popd

# Collect archives and link main executable
mkdir -p $BIN_DIR
A_FILES=$(collect_federate_archives "$LF_MAIN" "$N")

chmod +x ./gen_main.sh
./gen_main.sh N=$N

link_main_executable "$CC" main.c "$A_FILES" "$BIN_DIR/$LF_MAIN"

# Cleanup nanopb and Unity intermediates
rm -f $REACTOR_UC_PATH/external/nanopb/pb_encode.bc $REACTOR_UC_PATH/external/nanopb/pb_decode.bc $REACTOR_UC_PATH/external/nanopb/pb_common.bc $REACTOR_UC_PATH/external/Unity/src/unity.bc

run_interactive_menu "$BIN_DIR" "$LF_MAIN"
