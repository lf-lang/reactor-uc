# !/bin/bash

LF_MAIN=s4noc_fed
BIN_DIR=bin
CC=patmos-clang

make clean -C ./sender
make clean -C ./receiver
rm -f $REACTOR_UC_PATH/src/scheduler.bc $REACTOR_UC_PATH/src/platform.bc $REACTOR_UC_PATH/src/network_channel.bc

make all -C ./sender
make all -C ./receiver

echo "Linking sender and receiver to create executable"
make clean 
make all

read -n 1 -t 10 -p "Choose action: [e]mulate or [f]pga? (default: e) " action
action=${action:-e}
if [[ "$action" == "e" ]]; then
    patemu $BIN_DIR/$LF_MAIN.elf
elif [[ "$action" == "f" ]]; then
    if jtagconfig | grep -q "USB-Blaster"; then
        mv $BIN_DIR/$LF_MAIN.elf ~/t-crest/patmos/tmp/$LF_MAIN.elf
        make -C ~/t-crest/patmos APP=$LF_MAIN config download
    else
        echo "JTAG not connected. Please connect USB-Blaster."
    fi 
else
    echo "Invalid option. Please choose 'e' for emulate or 'f' for fpga."
fi

