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
    sed -i 's/_lf_environment/_lf_environment_2/g; s/lf_exit/lf_exit_2/g; s/lf_start/lf_start_2/g' $REACTOR_PATH/lf_start.c
    sed -i 's/(Federate/(Federate2/g; s/FederateStartupCoordinator/Federate2StartupCoordinator/g; s/FederateClockSynchronization/Federate2ClockSynchronization/g; s/Reactor_S4NoCFedLF/Reactor_S4NoCFedLF_2/g; s/S4NoCFedLF_r1/S4NoCFedLF_r1_2/g' $REACTOR_PATH/lf_federate.h $REACTOR_PATH/lf_federate.c
    make all OBJECTS="$REACTOR_PATH/lf_federate.bc $REACTOR_PATH/$LF_MAIN/Dst.bc $REACTOR_PATH/lf_start.bc"
popd

mkdir -p $BIN_DIR

patmos-clang -O2 -Wall -Wextra main.c ./$LF_MAIN/r1/bin/$LF_MAIN.a ./$LF_MAIN/r2/bin/$LF_MAIN.a -o $BIN_DIR/$LF_MAIN
read -t 10 -p "Choose action: [e]mulate or [f]pga? (default: e) " action
action=${action:-e}
if [[ "$action" == "e" ]]; then
    patemu $BIN_DIR/$LF_MAIN
elif [[ "$action" == "f" ]]; then
    if jtagconfig | grep -q "USB-Blaster"; then
        mv $BIN_DIR/$LF_MAIN ~/t-crest/patmos/tmp/$LF_MAIN.elf
        make -C ~/t-crest/patmos APP=$LF_MAIN config download
    else
        echo "JTAG not connected. Please connect USB-Blaster."
    fi 
else
    echo "Invalid option. Please choose 'e' for emulate or 'f' for fpga."
fi

