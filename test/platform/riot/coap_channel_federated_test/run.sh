#!/bin/bash

# Create tap interfaces
# TODO

# Make sender
pushd sender
make BOARD=native PORT=tap0 all
popd

# Make receiver
pushd receiver
make BOARD=native PORT=tap1 all
popd

# Run sender and receiver
./sender/bin/native/*.elf tap0 &
./receiver/bin/native/*.elf tap1 &

# Wait for both tests to finish
wait

# Evaluate test output
if [ $? -eq 0 ]; then
    echo "All tests passed."
else
    echo "$? tests failed." >&2
    exit 1
fi