#!/bin/bash
set -e

# --------------------- Networking setup ---------------------

# Cleanup network if it exists already
sudo ip netns del sender_ns || true
sudo ip netns del receiver_ns || true

# 1. Create network namespaces
echo "Setting up network namespaces..."
sudo ip netns add sender_ns
sudo ip netns add receiver_ns

# Create virtual ethernet pair
sudo ip link add veth_sender type veth peer name veth_receiver

# Move interfaces to namespaces
sudo ip link set veth_sender netns sender_ns
sudo ip link set veth_receiver netns receiver_ns

# Configure IP addresses
sudo ip netns exec sender_ns ip addr add 192.168.100.1/24 dev veth_sender
sudo ip netns exec receiver_ns ip addr add 192.168.100.2/24 dev veth_receiver

# Bring up interfaces
sudo ip netns exec sender_ns ip link set veth_sender up
sudo ip netns exec receiver_ns ip link set veth_receiver up
sudo ip netns exec sender_ns ip link set lo up
sudo ip netns exec receiver_ns ip link set lo up

echo "Network setup complete. Building applications..."

# Test connectivity
# sudo ip netns exec sender_ns ping 192.168.100.2
# sudo ip netns exec receiver_ns ping 192.168.100.1


# --------------------- BUILD AND RUN ---------------------

# Build sender
cmake -B sender/build -S sender
make -C sender/build

# Build receiver
cmake -B receiver/build -S receiver
make -C receiver/build

echo "Building complete. Starting applications..."

# Start the sender with a 60-second timeout in sender namespace
sudo ip netns exec sender_ns timeout 60 ./sender/build/app &
pid1=$!

# Wait for 3 seconds
sleep 3

# Start the receiver with a 60-second timeout in receiver namespace  
sudo ip netns exec receiver_ns timeout 60 ./receiver/build/app &
pid2=$!

# Wait for both binaries to complete or timeout
wait $pid1
exit_code_sender=$?

wait $pid2
exit_code_receiver=$?

echo "Test completed"
echo "Sender exit code: $exit_code_sender"
echo "Receiver exit code: $exit_code_receiver"

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
