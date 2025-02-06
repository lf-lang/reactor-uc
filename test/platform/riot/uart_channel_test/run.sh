#!/bin/bash

# Make test
make BOARD=feather-nrf52840-sense all

# TODO: Only build for now since native doesn't want to build with uart_mode()

# Run test
# ./bin/feather-nrf52840-sense/*.elf

# Evaluate test output
# if [ $? -eq 0 ]; then
#     echo "All tests passed."
# else
#     echo "$? tests failed." >&2
#     exit 1
# fi
