# Federated example on Zenoh

The `federated` example with a `ZenohChannel` instead of a `TcpIpChannel`: the two federates exchange their messages over Zenoh publish/subscribe ([zenoh-pico](https://github.com/eclipse-zenoh/zenoh-pico)).

Build, with zenoh-pico installed where CMake finds it (or `-Dzenohpico_DIR=<prefix>/lib/cmake/zenohpico`):

```
cmake -Bbuild
make -C build
```

Run the two federates as peers that connect to each other directly. zenoh-pico needs `Z_FEATURE_UNICAST_PEER` for this:

```
build/sender &
build/receiver
```

Or as clients of a Zenoh router, which relays the messages. The federates then need not reach each other:

```
build/sender tcp/127.0.0.1:7447 &
build/receiver tcp/127.0.0.1:7447
```
