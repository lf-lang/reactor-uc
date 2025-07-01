#!/bin/bash
LF_MAIN=S4NoCFedLF
BIN_DIR=bin
# Generate configuration templates
rm -rf $LF_MAIN
$REACTOR_UC_PATH/lfc/bin/lfc-dev --gen-fed-templates src/$LF_MAIN.lf


# Generate and build r1 sources
pushd ./$LF_MAIN/r1
    ./run_lfc.sh
    make all 
popd

# Generate and build r2 sources
pushd ./$LF_MAIN/r2
    REACTOR_PATH=$(pwd)/src-gen/S4NoCFedLF/r2
    ./run_lfc.sh
    sed -i 's/_lf_environment/_lf_environment_2/g' $REACTOR_PATH/lf_start.c
    # make all FILTER_OUT="%unity.bc %action.bc %builtin_triggers.bc %clock_synchronization.bc %connection.bc %environment.bc %event.bc %federated.bc %logging.bc %network_channel.bc %physical_clock.bc %platform.bc %port.bc %queues.bc %reaction.bc %reactor.bc %scheduler.bc %serialization.bc %startup_coordinator.bc %tag.bc %timer.bc %trigger.bc %util.bc %pb_common.bc %pb_decode.bc %pb_encode.bc %message.pb.bc"
    make all OBJECTS="$REACTOR_PATH/lf_federate.bc $REACTOR_PATH/$LF_MAIN/Dst.bc $REACTOR_PATH/lf_start.bc"
popd

mkdir -p $BIN_DIR

patmos-clang main.c ./$LF_MAIN/r1/bin/$LF_MAIN.a ./$LF_MAIN/r2/bin/$LF_MAIN.a \
    -o $BIN_DIR/$LF_MAIN
patemu $BIN_DIR/$LF_MAIN

