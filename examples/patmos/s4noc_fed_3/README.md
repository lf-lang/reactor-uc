# S4NOC Federated 3-Core Example

This example demonstrates federated communication across 3 Patmos cores using the S4NOC network-on-chip.

## Architecture

```
Core 0 (Sender)  -->  Core 1 (Repeater)  -->  Core 2 (Receiver)
```

### Federates

1. **Sender (Core 0)**
   - Periodically generates numbered messages
   - Sends messages via S4NOC to the repeater
   - Shuts down after sending MAX_MESSAGES

2. **Repeater (Core 1)**
   - Receives messages from the sender
   - Forwards messages unchanged to the receiver
   - Acts as a message relay/pass-through

3. **Receiver (Core 2)**
   - Receives messages from the repeater
   - Displays each message with sequence number
   - Waits for all MAX_MESSAGES to arrive

## Compilation

```bash
./build.sh
```

This will:
1. Build sender federate as `sender/build/sender.a`
2. Build repeater federate as `repeater/build/repeater.a`
3. Build receiver federate as `receiver/build/receiver.a`
4. Link all together into `bin/s4noc_fed_3.elf`

## Configuration

Edit `common_config.h` to adjust:

- `MAX_MESSAGES`: Number of messages to send (default: 10)
- `DO_CLOCK_SYNC`: Enable/disable clock synchronization (default: false)
- `NUM_NEIGHBORS`: Number of neighboring federates (should be 2 for repeater, 2 for others)
- `NUM_BUNDLES`: Number of network connections (should be 2 for repeater)

Edit individual federate `.c` files for timing parameters:

### Sender
- `TIMER_START_SEC`: Delay before first message (default: 1 second)
- `TIMER_PERIOD_SEC`: Interval between messages (default: 2 seconds)
- `SHUTDOWN_TIMEOUT_SEC`: When to stop (default: 5 seconds after last message)

### Repeater & Receiver
- `TIMEOUT`: Scheduler timeout (default: 60 seconds)
- `KEEP_ALIVE`: Keep waiting for messages (default: true)

## Message Format

Messages use the format: `"Hello X"` where X is the message sequence number in hexadecimal.

Example messages sent:
- Message 0: "Hello 0"
- Message 1: "Hello 1"
- ...
- Message 9: "Hello 9"
- Message 10: "Hello a"

This format keeps all payloads at exactly 7 bytes, matching S4NOC's optimal packet size (11 bytes = 4-byte header + 7-byte payload).

## Output Example

```
Starting S4NOC Federated 3-Core Example (Sender -> Repeater -> Receiver)
All federate threads started.
Sender: Sending message 1/10
=== REPEATER RECEIVED MESSAGE 1/10 (seq #0) ===
Repeater: Content: Hello 0
Repeater: Forwarding to receiver...
=== RECEIVED MESSAGE 1/10 (seq #0) ===
Receiver: Content: Hello 0
...
```

## Cleanup

```bash
make clean
```

This removes all build artifacts and executables.
