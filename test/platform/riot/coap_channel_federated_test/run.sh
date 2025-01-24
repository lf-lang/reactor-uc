#!/bin/bash

# # Create tap interfaces
# sudo $RIOTBASE/dist/tools/tapsetup/tapsetup

# Build sender
make BOARD=native PORT=tap0 all -C ./sender

# Build receiver
make BOARD=native PORT=tap1 all -C ./receiver

# Start the sender with a 13-second timeout
timeout 13 ./sender/bin/native/*.elf tap0 &
pid1=$!

# Wait for 3 seconds
sleep 3

# Start the receiver with a 10-second timeout
timeout 10 ./receiver/bin/native/*.elf tap1 &
pid2=$!

# Wait for both binaries to complete or timeout
wait $pid1
exit_code_sender=$?

wait $pid2
exit_code_receiver=$?

echo "Test completed"

# Check if timeout occurred
if [ $exit_code_sender -eq 124 ]; then
  echo "Error sender timed out"
  echo "Test failed"
  exit 1
else
  echo "Exit code of sender: $exit_code_sender"
fi

if [ $exit_code_receiver -eq 124 ]; then
  echo "Error receiver timed out"
  echo "Test failed"
  exit 1
else
  echo "Exit code of receiver: $exit_code_receiver"
fi

# Check exit code
if [[ $exit_code_receiver -ne 0 ]]; then
    echo "Error: Receiver received wrong message text"
    exit 1
fi

echo "All tests passed."
