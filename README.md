Reactor Micro-C 
====================

`reactor-uc` is a task scheduling runtime implementing the reactor
model-of-computation target at embedded and resource-constrained systems.

## Getting started

First you start by choosing a platform for which you want to write your application, second 
you instiantiate your repository from the corresponding template repository. Inside the templates
or the following sections is explained what build-tools and compilers the particular platform needs.

## Supported Platforms

`reactor-uc` can run on top of zephyr, riot, the rp2040 and operating systems which are posix compliant.

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
- [ssm-runtime](https://github.com/QuantumLeaps/qpc)

