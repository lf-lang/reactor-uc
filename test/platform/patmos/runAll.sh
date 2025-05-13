#!/bin/bash

# Make test
rm -r build
make all
pasim ./build/*.elf
