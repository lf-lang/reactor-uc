# !/bin/bash

LF_MAIN=s4noc_fed
BIN_DIR=bin
CC=patmos-clang
rm -rf $BIN_DIR

make clean -C ./sender
make clean -C ./receiver

make all -C ./sender
make all -C ./receiver

echo "Linking sender and receiver to create executable"
make clean 
make all
patemu $BIN_DIR/$LF_MAIN.elf
