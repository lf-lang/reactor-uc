# reactor-uc

`reactor-uc` is a task scheduling runtime implementing the reactor
model-of-computation target at embedded and resource-constrained systems.

## Getting started

NB: reactor-uc is still work-in-progress and sevearal reactor features are not supported
yet. Moreover, the runtime has not been thoroughly tested so if you find any bugs or issues, please report them.


Initialize submodules

```shell
git submodule update --init
```

Setup the environment

```shell
source env.bash # or env.zsh or env.fish 
```

Compile and run unit tests:

```shell
make test
```

## Examples

### Linux/POSIX

```shell
cd examples/posix
./buildAll.sh
hello/build/app
```

### Zephyr
Compile and run a simple test on Zephyr. This requires a correctly configured
Zehyr environment, with West installed in a Python virtual environment which is
activated. Inspect `.github/actions/zephyr/action.yml` for an example of how to setup your Zephyr workspace. 

First a simple HelloWorld on the `native_posix` target:
```shell
cd examples/zephyr/hello
west build -b native_posix -p always -t run
```

Then a simple blinky targeting NXP FRDM K64F. This will run with most boards supporting Zephyr that has a user LED.
```shell
cd examples/zephyr/blinky
west build -b frdm_k64f -p always
west flash
```

### RIOT
Compile and run a simple blinky example on RIOT.
This requires a correctly configured RIOT environment.
Make sure that the environment variable `RIOTBASE` points to a `RIOT` codebase.

```shell
cd examples/riot/blinky
make BOARD=native all term
```

### Pico
Download `pico-sdk` and define PICO_SDK_PATH as an environmental variable.

```shell
cd examples/pico
cmake -Bbuild
cd build
make
```

### Lingua Franca
Reactor-uc includes a limited version of the Lingua Franca Compiler (lfc) found in `~/lfc`. In the future, the
`reactor-uc` specific code-generation will be merged back upstream. By sourcing `env.bash`, `env.fish` or `env.zsh` the
Lingua Franca Compiler will be aliased by `lfcg`.

```shell
lfcg test/lf/src/HelloUc.lf
```

Since the target platform is set to `Native`,`lfc` will generate a main function and invoke CMake directly on the
generated sources. Run the program with:

```shell
test/lf/bin/HelloUc
```

## References

`reactor-uc` draws inspiration from the following existing open-source projects:

- [reactor-cpp](https://github.com/lf-lang/reactor-cpp)
- [reactor-c](https://github.com/lf-lang/reactor-c)
- [qpc](https://github.com/QuantumLeaps/qpc)
- [ssm-runtime](https://github.com/QuantumLeaps/qpc)

## Troubleshooting

### Formatting

We are using `clang-format` version 18.1.3 which is default with Ubuntu 24.04 for formatting in CI.

### LFC

If you get the following CMake error when calling `lfc/bin/lfc-dev`

```
CMake Error at CMakeLists.txt:7 (message):
  Environment variable REACTOR_UC_PATH is not set.  Please source the
  env.bash in reactor-uc.
```

Please source `env.bash`, `env.fish` or `env.zsh` to get the REACTOR_UC_PATH environment variable defined.
