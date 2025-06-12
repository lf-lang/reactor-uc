#!/bin/bash

# Make test
make clean
make all
patemu ./build/*.elf

