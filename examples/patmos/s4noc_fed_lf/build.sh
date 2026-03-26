#!/bin/bash

# Source shared build helpers
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
. "$SCRIPT_DIR/../build-helpers.sh"

LF_MAIN=S4NoCFedLF
BIN_DIR=bin
CC=patmos-clang

# Parse command-line arguments
parse_build_args "$@"

# Generate configuration templates
rm -rf $LF_MAIN $BIN_DIR
rm -f $REACTOR_UC_PATH/src/scheduler.bc $REACTOR_UC_PATH/src/platform.bc $REACTOR_UC_PATH/src/network_channel.bc $REACTOR_UC_PATH/src/environment.bc


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

# Generate and build r1 sources
pushd ./$LF_MAIN/r1
    ./run_lfc.sh
    make all || exit 1
popd

N=2
# Generate and build other sources
for i in $(seq 2 $N); do
    pushd ./$LF_MAIN/r$i
        REACTOR_PATH=$(pwd)/src-gen/$LF_MAIN/r$i
        ./run_lfc.sh
        sed -i "s/_lf_environment/_lf_environment_$i/g; s/lf_exit/lf_exit_$i/g; s/lf_start/lf_start_$i/g" $REACTOR_PATH/lf_start.c
        sed -i "s/(Federate/(Federate$i/g; s/FederateStartup/Federate${i}Startup/g; s/FederateShutdown/Federate${i}Shutdown/g; s/FederateClock/Federate${i}Clock/g; s/Reactor_${LF_MAIN}/Reactor_${LF_MAIN}_$i/g; s/${LF_MAIN}_r1/${LF_MAIN}_r1_$i/g" $REACTOR_PATH/lf_federate.h $REACTOR_PATH/lf_federate.c
        make all OBJECTS="$REACTOR_PATH/lf_federate.bc $REACTOR_PATH/$LF_MAIN/Dst.bc $REACTOR_PATH/lf_start.bc" || exit 1
    popd
done

mkdir -p $BIN_DIR

A_FILES=""
for i in $(seq 1 $N); do
    A_FILES="$A_FILES ./$LF_MAIN/r$i/bin/$LF_MAIN.a"
done

chmod +x ./gen_main.sh
./gen_main.sh N=$N

$CC -O2 -Wall -Wextra main.c $A_FILES -o $BIN_DIR/$LF_MAIN || exit 1

rm $REACTOR_UC_PATH/external/nanopb/pb_encode.bc $REACTOR_UC_PATH/external/nanopb/pb_decode.bc $REACTOR_UC_PATH/external/nanopb/pb_common.bc $REACTOR_UC_PATH/external/Unity/src/unity.bc

run_interactive_menu "$BIN_DIR" "$LF_MAIN"


