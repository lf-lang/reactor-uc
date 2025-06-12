#!/bin/bash
rm -rf build
make all
pasim ./build/hello.elf