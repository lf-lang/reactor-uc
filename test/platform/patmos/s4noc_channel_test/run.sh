#!/bin/bash

LF_MAIN=main
BIN_DIR=./build

# Make test
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

