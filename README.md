Reactor Micro-C 
====================

`reactor-uc` is a task scheduling runtime for Lingua Franca, targeted at embedded and resource-constrained systems. Please refer to the
[Lingua Franca Handbook](https://www.lf-lang.org/docs/) for more information on reactor-oriented programming using Lingua Franca. For more
information on reactor-uc see our [docs](https://www.lf-lang.org/reactor-uc/)


## Getting started

### Requirements
- CMake
- Make
- A C compiler like GCC or clang
- Linux or macOS development environment
- Java 17
- Additional requirements depend on the target platform

## Supported Platforms
`reactor-uc` can run on top of Zephyr, RIOT, Raspberry Pi Pico and POSIX-compliant OSes.

### Native (macOS and Linux)
`reactor-uc` can also run natively on a host system based on Linux or macOS. This is very useful for developing and testing applications
without the target hardware. By setting the platform target property in your LF program to `Native` the compiler will automatically generate 
a CMake project and compile it natively. E.g.

```sh
cat > HelloWorld.lf << EOF
target uC {
  platform: Native
}
 
main reactor {
  reaction(startup) {=
    printf("Hello World!\n");
  =}
}
EOF
./lfc/bin/lfc-dev HelloWorld.lf
bin/HelloWorld
```


### Zephyr

**Template Repository:** [https://github.com/lf-lang/lf-zephyr-uc-template/](https://github.com/lf-lang/lf-zephyr-uc-template/)

Compile and run a simple test on Zephyr. This requires a correctly configured
Zehyr environment, with West installed in a Python virtual environment, is
activated. Inspect `.github/actions/zephyr/action.yml` for an example of setting up your Zephyr workspace. 

First a simple HelloWorld on the `native_posix` target:
```shell
cd examples/zephyr/hello
west build -b native_posix -p always -t run
```

Then a simple blinky targeting NXP FRDM K64F. This will run with most boards supporting Zephyr that have a user LED.
```shell
cd examples/zephyr/blinky
west build -b frdm_k64f -p always
west flash
```
### RIOT 

**Template Repository:** [https://github.com/lf-lang/lf-riot-uc-template/](https://github.com/lf-lang/lf-riot-uc-template/)

Compile and run a simple blinky example on RIOT.
This requires a correctly configured RIOT environment.
Make sure that the environment variable `RIOTBASE` points to a `RIOT` codebase.

```shell
cd examples/riot/blinky
make BOARD=native all term
```

### RP2040

**Template Repository:** [https://github.com/lf-lang/lf-pico-uc-template/](https://github.com/lf-lang/lf-pico-uc-template/)

Download `pico-sdk` and define PICO_SDK_PATH as an environmental variable.

```shell
cd examples/pico
cmake -Bbuild
cd build
make
```

## Documentation

## Contributing

We are using `clang-format` version 18.1.3 which is default with Ubuntu 24.04 for formatting in CI.

## References & Sponsors

`reactor-uc` draws inspiration from the following existing open-source projects:

- [reactor-cpp](https://github.com/lf-lang/reactor-cpp)
- [reactor-c](https://github.com/lf-lang/reactor-c)
- [qpc](https://github.com/QuantumLeaps/qpc)
- [ssm-runtime](https://github.com/ssm-lang/ssm-runtime)

