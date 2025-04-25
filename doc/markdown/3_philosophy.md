\page philosohy Philosophy

Learning a new framework for system-design and synthesis is an intimidating process.
This guide explains the core concepts of reactor-uc and is highly recommended for
beginners.

\note If you are not familiar with Lingua Franca (LF), please refer to the [manual](https://www.lf-lang.org/docs/).

## Building

The design of systems with reactor-uc and LF follows a top-down design
approach, where the programmer starts with his design written in LF. The LF
compiler (`lfc`) translates your design into C code. This generated code uses the
reactor-uc runtime functions to execute the program. When you create federated
(distributed) programs, `lfc` generates subfolders for each federate (node) in your
system. The goal of reactor-uc is to enable integration into existing toolchains. 
For example, for projects based on Zephyr, we provide `lfc`-integration through
a custom `west` command. When building an application the programmer interacts
with `west` as usual, and `west` calls `lfc` to generate C code from the LF source
files, finally `west` uses CMake to configure and build the final executable.
Another example is RIOT OS, which has a Make-based toolchain. For RIOT we
integrate `lfc` into the application Makefile such that calling `make all`
first invokes `lfc` on the LF sources before compiling the generated sources.

`lfc` produces importable CMake and make files that you can import into your
build-system. For common platforms like [Zephyr](https://zephyrproject.org/),
[RIOT](https://riot-os.org) or the
[pico-sdk](https://www.raspberrypi.com/documentation/pico-sdk/) we provide
build-templates. 

If the target platform is set to native using the following target property:

```
target uC {
  platform: Native
}
```

Then, lfc will invoke the CMake build tool on the host system to produce an executable
for the program. If the platform is speified to any other platform, or just left
unspecified, `lfc` will only do code-generation and leave the final compilation of the
program to the user. 