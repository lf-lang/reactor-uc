#!/bin/bash

# Requires west, the Zephyr SDK and ZEPHYR_BASE pointing to a Zephyr checkout.
BOARD=${BOARD:-native_sim}

# Make test
west build -b $BOARD -p auto -d build . || exit 1

# Run test (native_sim exits with the number of failed tests)
timeout 60 ./build/zephyr/zephyr.exe

# Evaluate test output
ret=$?
if [ $ret -eq 0 ]; then
    echo "All tests passed."
else
    echo "Tests failed (exit code $ret)." >&2
    exit 1
fi
