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

## From a Lingua Franca program

`FederatedZenoh.ulf` is the same kind of federation written in Lingua Franca. `@interface_zenoh` on a federate selects the channel; the code generator fills in the names of the two federates of every connection, so there is nothing to say per connection and no `@link` is needed:

```
@interface_zenoh(session_mode="peer", listen="tcp/127.0.0.1:17447")
r1 = new Src(id=42)

@interface_zenoh(session_mode="peer", connect="tcp/127.0.0.1:17447")
r2 = new Dst()

r1.out -> r2.in
```

`session_mode`, `connect` and `listen` say how the federate opens its Zenoh session (`mode` is a keyword of the language, hence `session_mode`). For clients of a router it is `@interface_zenoh(session_mode="client", connect="tcp/<host>:7447")` on both. A federate whose network is not up yet, or whose peer does not listen yet, keeps trying.

`connect` may name several locators, separated by commas. zenoh-pico peers do not forward for each other, so in a federation of more than two peers every federate needs a link to each federate it has a connection with: one side of each pair listens and the other names it in `connect`. `connect_timeout` is how long zenoh-pico keeps trying the locators that do not answer, `forever` for no limit; it needs zenoh-pico built with `Z_FEATURE_UNSTABLE_API`.

```
@interface_zenoh(session_mode="peer", listen="tcp/10.0.0.1:7447")
a = new A()

@interface_zenoh(session_mode="peer", listen="tcp/10.0.0.2:7447", connect="tcp/10.0.0.1:7447", connect_timeout=forever)
b = new B()

@interface_zenoh(session_mode="peer", connect="tcp/10.0.0.1:7447, tcp/10.0.0.2:7447", connect_timeout=forever)
c = new C()
```

Note that zenoh-pico up to 1.10.2 can mix up the subscribers of two peers (eclipse-zenoh/zenoh-pico#1368); a peer with more than one link may then reach only one of them. With a router between the federates this does not happen.

Without any of the three, the session is the application's: it opens or borrows `ZenohChannel_default_session()` before the runtime starts, for example to share the session it already uses for its own data.

```
ulfc FederatedZenoh.ulf -n -o .
cmake -S src-gen/FederatedZenoh/r1 -B build_r1 && make -C build_r1
cmake -S src-gen/FederatedZenoh/r2 -B build_r2 && make -C build_r2
build_r1/FederatedZenoh_r1 &
build_r2/FederatedZenoh_r2
```
