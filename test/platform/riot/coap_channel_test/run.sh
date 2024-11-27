#!/bin/bash

# Make test
make BOARD=native all

# Run test
./bin/native/*.elf

# Evaluate test output
if [ $? -eq 0 ]; then
    echo "All tests passed."
else
    echo "$? tests failed." >&2
    exit 1
fi