\page philosohy Philosophy

Learning a new framework for system-design and synthesis is an intimidating process.
This guide explains the core concepts of reactor-uc and is highly recommended for
beginners.

\note If you are not familiar with Lingua Franca (LF), please refer to the [manual](https://www.lf-lang.org/docs/).

## Building

The design of systems with reactor-uc and LF follows a top-down design
approach, where the programmer starts with his design written in LF. The LF
compiler (`lfuc`) translates your design into C code. This generated code uses the
reactor-uc runtime functions to execute the program. When you create federated
(distributed) programs, `lfuc` generates subfolders for each federate (node) in your
system. The goal of reactor-uc is to enable integration into existing toolchains. 
For example, for projects based on Zephyr, we provide `lfuc`-integration through
a custom `west` command. When building an application the programmer interacts
with `west` as usual, and `west` calls `lfuc` to generate C code from the LF source
files, finally `west` uses CMake to configure and build the final executable.
Another example is RIOT OS, which has a Make-based toolchain. For RIOT we
integrate `lfuc` into the application Makefile such that calling `make all`
first invokes `lfuc` on the LF sources before compiling the generated sources.

`lfuc` produces importable CMake and make files that you can import into your
build-system. For common platforms like [Zephyr](https://zephyrproject.org/),
[RIOT](https://riot-os.org) or the
[pico-sdk](https://www.raspberrypi.com/documentation/pico-sdk/) we provide
build-templates. 


## Federation Design
We call the network abstraction in reactor-uc `NetworkChannels`, which are bidirectional
message pipes between two federates. Depending on which platform you are working on,
reactor-uc supports different NetworkChannels, such as: `TcpIpChannel`, `UARTChannel` or
`CoapUdpChannel`. You can find a complete table with all network channels that are
supported on the individual platforms [here](TODO).

```lf
federated reactor {
  @interface_tcp(name="if1", address="127.0.0.1")
  src = new Src()

  @interface_tcp(name="if1", address="127.0.0.1")
  dst = new Dst()

  @link(left="if1", right="if1", server_side="right", server_port=1042)
  src.out -> dst.in
}
```


