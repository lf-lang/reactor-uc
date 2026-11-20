#!/bin/bash
# Federated test of the Zephyr CoAP network channel with two native_sim federates (sender and receiver).
#
# Each federate is attached to its own TAP interface (zeth0/zeth1), which are connected by a bridge.
# To not need root privileges or touch the host network, the test runs in a new network namespace
# (unprivileged user namespace via `unshare -rn`, or `sudo unshare -n` if those are not allowed).
#
# Requires west, the Zephyr SDK and ZEPHYR_BASE pointing to a Zephyr checkout.
set -e

cd "$(dirname "$0")"

if [ -z "$LF_IN_NETNS" ]; then
    BOARD=${BOARD:-native_sim}

    # Build sender
    west build -b $BOARD -p auto -d sender/build sender

    # Build receiver
    west build -b $BOARD -p auto -d receiver/build receiver

    # Run the test in a new network namespace
    export LF_IN_NETNS=1
    if unshare -rn true 2>/dev/null; then
        exec unshare -rn "$0"
    else
        exec sudo --preserve-env=LF_IN_NETNS unshare -n "$0"
    fi
fi

# Network setup: zeth0 <-> br0 <-> zeth1
ip link set lo up
ip link add br0 type bridge
for tap in zeth0 zeth1; do
    ip tuntap add $tap mode tap
    ip link set $tap master br0
    ip link set $tap up
done
ip link set br0 up

set +e

# Start the sender with a 60-second timeout
timeout 60 ./sender/build/zephyr/zephyr.exe --eth-if=zeth0 --mac-addr=00:00:5e:00:53:01 &
pid1=$!

# Wait for 3 seconds
sleep 3

# Start the receiver with a 60-second timeout
timeout 60 ./receiver/build/zephyr/zephyr.exe --eth-if=zeth1 --mac-addr=00:00:5e:00:53:02 &
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
