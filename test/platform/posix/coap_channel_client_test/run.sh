#!/bin/bash

# Make test
cmake -Bbuild
make -C build

# Evaluate make output
if [ $? -eq 0 ]; then
    echo "All tests passed."
else
    echo "$? tests failed." >&2
    exit 1
fi

# Run test
./build/app

# Evaluate test output
if [ $? -eq 0 ]; then
    echo "All tests passed."
else
    echo "$? tests failed." >&2
    exit 1
fi