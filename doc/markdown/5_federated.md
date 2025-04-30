\page federated Federated Execution

reactor-uc supports federated (distributed) execution with decentralized, peer-to-peer coordination based on the PTIDES
principle. This opens up a vast design space within distributed real-time systems.

To make a program distributed, the main reactor is marked as `federated`. This will turn each of the top-level reactors
into a federate.

## Coordination
Consider the following, simple, federated program.

```
reactor Src {
  output out: int

  timer t(0, 100 msec)
  state cnt: int = 0

  reaction(t) -> out {=
    lf_set(out, self->cnt++);
  =}
}

reactor Sink {
  input in: int 
  reaction(in) {
    printf("Got %d\n", in->value);
  }
}

federated reactor {
  src = new Src()
  sink = new Sink()
}
```

By changing `main reactor` into `federated reactor`, we create a federated program. Each reactor in the top-level
is a federate. There can not be any reactions or triggers in the top-level, only reactors and connections. Currently,
the federates only support decentralized coordination using PTIDES. Each federate will handle its own local events at 
its own speed and exchange events with its neighbors. This creates the risk of out-of-order processing of events.

E.g. consider the following change to `Sink`

```
reactor Sink {
  input in: int

  timer t(0, 100 msec)
  reaction(in) {
    printf("Got %d\n", in->value);
  }
  reaction(t) {=
    printf("Timer!\n");
  =}
}
```

This reactor specifies that if `t` and `in` are present at the same tag, the reaction to
`in` should be handled first. With any modifications, this program will likely lead to
STP violations, because the reaction to `t` will likely be handled immediately, and then later
the event on `in` will arrive. 

There are two strategies to avoid STP violations. 

### LET-based coordination
First, we can use the LET principle and introduce a logical delay
on the connection between `src` and `sink`. This will modify the tag of the events sent between the federates and
thus the requirement to ordering. This is done with the `after` keywords as follows:

```
src.out -> sink.in after 100 msec
```

If the delay on the connection exceeds the total latency from the `t` trigger at `src`
to the `in` port of `sink`, then an STP violation cannot occur. However, it is important
to note that logical delays change the semantic of the program.

### maxwait-based coordination
The other strategy is to associate a `maxwait` with the reaction to the input port `in` as follows:

```
reaction(in) {
  printf("Got %d\n", in->value);
} maxwait(100 msec) {=
  printf("STP violation");
=}
```

Here the `maxwait` of a 100 msec specifies that the scheduler should wait up to a 100
msec before assuming that any associated federated input port is absent at a given tag.
If the state of an input port is known, because a message has arrived with the same or
later tag, the scheduler does not have to wait. This waiting happens before the
scheduler starts handling a particular tag.

It is important to note that the scheduler will wait for the `maxwait` duration
when handling any tag from any trigger, except the shutdown trigger. So in `Sink`,
every time a trigger from `t` is handled, the scheduler first waits for `maxwait`. 
This works well in our current program because `t` in `in` are always logically
simultaneous.

You can also set the `maxwait` to `forever`, in which case a tag will never be
handled until all input ports are known at the tag or later. The default value of
`maxwait` is 0, meaning that the federate does not wait at all.

You can pass an optional STP handler after specifying the `maxwait`. This will be
invoked if the reaction is triggered at the wrong tag, due to an STP violation. The
intended tag can be inspected by looking at `in->intended_tag`. If no STP handler is
provided, the reaction body will be executed.

### Federate cycles
reactor-uc does not yet support zero-delay cycles between federates. Such programs will
always yield STP violations. Once a tag has been committed to, any message later
received with this tag, will be handled later, and lead to an STP violation.

`maxwait`s should be used with care in cycles. Consider the following program which
will deadlock.

```
reactor Producer {
  output out: int
  input in: int

  timer t(0, 100 msec)
  state cnt: int = 0

  reaction(t) -> out {=
    lf_set(out, self->cnt++);
  =}
  reaction(in) {=
    printf("Got %d back\n", in->value);
  =} maxwait(forever)
}

reactor Passthrough{
  input in: int 
  output out: int
  reaction(in) -> out {
    lf_set(out, in->value);s
  }
}

federated reactor {
  prod = new Producer()
  pt = new Passthrough()
  
  prod.out -> pt.in
  pt.out -> prod.in
}
```

Here, we immediately get a deadlock because `Producer` can not handle the `t` trigger
until it has resolved its input port `in`. However, `in` will not be resolved until we
have handled `t` and produced an output. Introducing a delay on the connection does not
help. Using a physical connection would solve the problem as it means that you always
know the state of an input port at the current physical time.

## Network channels
reactor-uc supports inter-federate communication over various protocols, known as network channels. Federates can be annotated with a set of network interfaces that they have, and 

Which network
channel to use is configured using attributes. By default, a TCP connection is used and all federates are assumed to
run on the same host. Through annotations, we can add several network channel interfaces to each federate. Consider
the following program where both federates has a TCP and a COAP interface,
but we use the TCP interface for the connection.

```
federated reactor {
  @interface_tcp(name="if1", address="127.0.0.1")
  @interface_coap(name="if2", address="127.0.0.1")
  src = new Src()

  @interface_tcp(name="if1", address="127.0.0.1")
  @interface_coap(name="if2", address="127.0.0.1")
  sink = new Sink()

  @link(left="if1", right="if1", server_side="right", server_port=1042)
  r1.out -> r2.in 
}

```

It is also possible to provide your own, custom network channel implementation as follows:

```
federated reactor {
  @interface_custom(name="MyInterface", include="my_interface.h", args="1")
  source = new Src()

  @interface_custom(name="MyInterface", include="my_interface.h", args="2")
  sink = new Sink()

  r1.out -> r2.in 
}
```

Here you must provide a file `my_interface.h` which is on the include path for the
compiler, also you must add sources files that defines a `MyInterface` struct which
should inherit from `NetworkChannel` and also has a function `void

MyInterface_ctor(MyInterface *self, int arg)`. The additional arguments must match what
is passed as a string to the `args` annotation. You must provide a complete
implementation of the `NetworkChannel` interface defined in `network_channel.h`.


## Platform and Network Channel Support Matrix

| **Platform**        | **TCP** | **UART** | **CoAP** | **Custom** |
|---------------------|---------|---------|----------|----------|
| **Native (POSIX)**  | ✅      | ❌      | ❌       | ✅      |
| **Zephyr**          | ✅      | ❌      | ❌       | ✅      |
| **RIOT**            | ✅      | ❌      | ✅       | ✅      |
| **PICO**            | ❌      | ✅      | ❌       | ✅      |



## Clock synchronization
reactor-uc includes clock-synchronization that is enabled by default. Unless specified otherwise, the first federate assumes the role as the clock-sync grandmaster.
We can disable clock-sync by setting the target property 
```
target uC {
  clock-sync: off
}
```

Clock-sync can be configured using federate annotations as follows:

```
federated reactor {
  @clock_sync(grandmaster=true, disabled=false)  
  src = new Src()
  @clock_sync(grandmaster=false, disabled=false, period=1000000000, max_adj=512000, kp=0.5, ki=0.1)  
  sink = new Sink()
}
```
All the arguments are optional, and `disabled=false` is redundant but shown for completeness. 
The `period`, `max_adj`, `kp`, and `ki` arguments are only used by clock sync slaves and denote
the period between clock sync rounds, the maximum parts-per-billion adjustment applied to the clock frequency, as well as the PI-controller constant. The clock-sync algorithm is inspired by PTP.

## Coordination of the startup tag
The first thing that happens when a federate is started is that it establishes a connection to its neighboring federates. Once it has connected to all its federates, the startup tag negotiation starts. Only clock-sync grandmasters are allowed to propose a start tag, and this start tag is distributed using gossip. If there are multiple grandmasters, the maximum of their proposed start tag is used.

In order for this to work in all scenarios, we require all network channels to be
bidirectional, meaning that messages can be sent in either direction. Further we require
that the federation is fully-connected, which means that there exists a path between any
two federates by following the bidirectional connections.

## Heterogeneous Federations
reactor-uc works Heterogeneous Federations which is federations consisting of different platforms. E.g. one federate running Linux and another running Zephyr. This is achieved by annotating the target
platform for each federate.

```
federated reactor {
  @platform_native
  src = new Src()

  @platform_zephyr
  dest = new Sink()

  src.out -> dest.in
}
```

To compile the federates we compile this program as follows: `lfc --gen-fed-templates src/MyFederation.lf`. Assuming the program is called `MyFederation.lf` and is located in a `src` directory relative the current working directory. This will produce a template project for each of the federates under `MyFederation/src` and `MyFederation/sink`. A `run_lfc.sh` script is produced into each template. This script runs code-generation using `lfc` and puts the sources into `src-gen`. The generated sources can be compiled using `west build` for Zephyr and `cmake -BBuild . && cmake --build build` for the native execution.

