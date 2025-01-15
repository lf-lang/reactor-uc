#!/bin/bash

SENDER_IP=fe80::44e5:1bff:fee4:dac8
RECEIVER_IP=fe80::8cc3:33ff:febb:1b3

# Create tap interfaces
# TODO

# --------------------------- Print IP-Addresses ---------------------------

# Build sender
make ONLY_PRINT_IP=1 BOARD=native PORT=tap0 all -C ./sender

# Build receiver
make ONLY_PRINT_IP=1 BOARD=native PORT=tap1 all -C ./receiver

# Run sender and receiver
./sender/bin/native/*.elf tap0
./receiver/bin/native/*.elf tap1

# --------------------------- Build and run test --------------------------- 

# Build sender
make REMOTE_ADDRESS=$RECEIVER_IP BOARD=native PORT=tap0 all -C ./sender

# Build receiver
make REMOTE_ADDRESS=$SENDER_IP BOARD=native PORT=tap1 all -C ./receiver

SESSION_NAME="federated_coap_test"

# Create a new tmux session
tmux new-session -d -s $SESSION_NAME

# Run the sender
tmux send-keys -t $SESSION_NAME "./sender/bin/native/*.elf tap0" C-m

sleep 3

# Run the receiver
tmux split-window -h -t $SESSION_NAME
tmux send-keys -t $SESSION_NAME "./receiver/bin/native/*.elf tap1" C-m

# Attach to the tmux session
tmux attach-session -t $SESSION_NAME

# Evaluate test output
# if [ $? -eq 0 ]; then
#     echo "All tests passed."
# else
#     echo "$? tests failed." >&2
#     exit 1
# fi
