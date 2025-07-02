#!/bin/bash
LF_MAIN=S4NoCFedLF
BIN_DIR=bin
# Generate configuration templates
rm -rf $LF_MAIN $BIN_DIR
$REACTOR_UC_PATH/lfc/bin/lfc-dev --gen-fed-templates src/$LF_MAIN.lf


# Generate and build r1 sources
pushd ./$LF_MAIN/r1
    ./run_lfc.sh
    make all 
popd

# Generate and build r2 sources
pushd ./$LF_MAIN/r2
    REACTOR_PATH=$(pwd)/src-gen/$LF_MAIN/r2
    ./run_lfc.sh
        
    sed -i 's/_lf_environment/_lf_environment_2/g' $REACTOR_PATH/lf_start.c
    sed -i 's/lf_exit/lf_exit_2/g' $REACTOR_PATH/lf_start.c
    sed -i 's/lf_start/lf_start_2/g' $REACTOR_PATH/lf_start.c
    sed -i 's/(Federate/(Federate2/g' $REACTOR_PATH/lf_federate.h $REACTOR_PATH/lf_federate.c
    sed -i 's/FederateStartupCoordinator/Federate2StartupCoordinator/g' $REACTOR_PATH/lf_federate.h
    sed -i 's/FederateClockSynchronization/Federate2ClockSynchronization/g' $REACTOR_PATH/lf_federate.h
    sed -i 's/Reactor_S4NoCFedLF/Reactor_S4NoCFedLF_2/g' $REACTOR_PATH/lf_federate.h $REACTOR_PATH/lf_federate.c
    sed -i 's/S4NoCFedLF_r1/S4NoCFedLF_r1_2/g' $REACTOR_PATH/lf_federate.h $REACTOR_PATH/lf_federate.c

    make all OBJECTS="$REACTOR_PATH/lf_federate.bc $REACTOR_PATH/$LF_MAIN/Dst.bc $REACTOR_PATH/lf_start.bc"
popd

mkdir -p $BIN_DIR

patmos-clang -v -O1 main.c ./$LF_MAIN/r1/bin/$LF_MAIN.a ./$LF_MAIN/r2/bin/$LF_MAIN.a \
    -o $BIN_DIR/$LF_MAIN
patemu $BIN_DIR/$LF_MAIN

