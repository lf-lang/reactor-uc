#!/bin/bash

# Create tap interfaces
# TODO

# ---------------------------------------------------------------

# # Print IP-Addresses

## Build sender
# pushd sender
# make ONLY_PRINT_IP=1 BOARD=native PORT=tap0 all
# popd

# ## Build receiver
# pushd receiver
# make ONLY_PRINT_IP=1 BOARD=native PORT=tap1 all
# popd

# ## Run sender and receiver
# ./sender/bin/native/*.elf tap0 &
# ./receiver/bin/native/*.elf tap1 &

# ---------------------------------------------------------------

# # Build sender
# pushd sender
# make REMOTE_ADDRESS=fe80::8cc3:33ff:febb:1b3 BOARD=native PORT=tap0 all
# popd

# # Build receiver
# pushd receiver
# make REMOTE_ADDRESS=fe80::44e5:1bff:fee4:dac8 BOARD=native PORT=tap1 all
# popd

# Run sender and receiver
( ./sender/bin/native/*.elf tap0 ) &
( ./receiver/bin/native/*.elf tap1 ) &

# Wait for both tests to finish
wait

# Evaluate test output
if [ $? -eq 0 ]; then
    echo "All tests passed."
else
    echo "$? tests failed." >&2
    exit 1
fi
