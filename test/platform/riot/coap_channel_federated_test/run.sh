#!/bin/bash

SENDER_IP=${SENDER_IP:-fe80::44e5:1bff:fee4:dac8}
RECEIVER_IP=${RECEIVER_IP:-fe80::8cc3:33ff:febb:1b3}

# # Create tap interfaces
# sudo $RIOTBASE/dist/tools/tapsetup/tapsetup

# # --------------------------- Print IP-Addresses ---------------------------

# # Build sender
# make ONLY_PRINT_IP=1 BOARD=native PORT=tap0 all -C ./sender

# # Build receiver
# make ONLY_PRINT_IP=1 BOARD=native PORT=tap1 all -C ./receiver

# # Run sender and receiver
# ./sender/bin/native/*.elf tap0
# ./receiver/bin/native/*.elf tap1

# # --------------------------- Build and run test --------------------------- 

# Build sender
make REMOTE_ADDRESS=$RECEIVER_IP BOARD=native PORT=tap0 all -C ./sender

# Build receiver
make REMOTE_ADDRESS=$SENDER_IP BOARD=native PORT=tap1 all -C ./receiver

SESSION_NAME="federated_coap_test"

# Run the sender
tmux new-session -d -s "$SESSION_NAME" "bash -c './sender/bin/native/*.elf tap0; echo \$? > /tmp/${SESSION_NAME}_sender_state'"

sleep 3

# Run the receiver
tmux split-window -h -t $SESSION_NAME "bash -c './receiver/bin/native/*.elf tap1; echo \$? > /tmp/${SESSION_NAME}_receiver_state'"


# Attach to the tmux session
# Attach to the session for 5 seconds
{
    sleep 10 && tmux detach-client -s $SESSION_NAME
} &
tmux attach-session -t $SESSION_NAME

# Check if the tmux session is still running
if tmux has-session -t $SESSION_NAME; then
    tmux kill-session -t $SESSION_NAME
    echo "coap_channel_federated_test did not terminate correctly"
    exit 1
else
    echo "Test completed: tmux session has terminated."
    cat /tmp/${SESSION_NAME}_sender_state
    cat /tmp/${SESSION_NAME}_receiver_state
fi

# Evaluate test output
# if [ $? -eq 0 ]; then
#     echo "All tests passed."
# else
#     echo "$? tests failed." >&2
#     exit 1
# fi
