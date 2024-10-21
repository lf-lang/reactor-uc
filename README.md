# reactor-uc
`reactor-uc` is a task scheduling runtime implementing the reactor
model-of-computation target at embedded and resource-constrained 32 bit systems.

## Getting started

NB: reactor-uc is still work-in-progress and many (most?) features are not supported
yet. This only documents how to run a few example programs.

Setup the environment
```
source env.bash
```

Initialize submodules
```
git submodule update --init
```

Compile and run unit tests:
```
make test
```

Compile and run a simple timer test on Posix
```
cd examples/posix
cmake -Bbuild
cmake --build build
build/timer_ex
```

Compile and run a simple test on Zephyr. This requires a correctly configured
Zehyr environment, with West installed in a Python virtual environment which is
activated:

```
cd examples/zephyr/hello
west build -b qemu_cortex_m3 -p always -t run
```

```
cd examples/zephyr/blinky
west build -b frdm_k64f -p always
west flash
```

For more information on running LF programs using the reactor-uc runtime on 
Zephyr take a look at this template: https://github.com/lf-lang/lf-west-template/tree/reactor-uc

## Lingua Franca
We have copied a very limited version of the Lingua Franca Compiler (lfc) into
`~/lfc` of this repo. In the future, the `reactor-uc` specific code-generation
will be merged back upstream. By sourcing `env.bash` or `env.fish` the Lingua
Franca Compiler will be aliased by `lfcg` for Lingua Franca Generator since this
limited version mainly oes does code-generat

```
cd examples/lf
lfcg src/HelloUc.lf
```

Since a target platform is not specified, we will target POSIX in which case
`lfc` will generate a main function and invoke CMake directly on the generated
sources. Run the program with:

```
bin/HelloUc
```

## Goals
- Incorporate unit testing and test-driven development from the start
- Optimized for single-core 32-bit systems
- Backend for LF
- Standalone library
- Use clang-format and clang-tidy to write safe code from the start.
- CMake-based
- Optimized for single-threaded runtime, but support for federated execution
which enable distributed embedded systems.
- Avoid malloc as much as possible (or entirely?)

## References
`reactor-uc` draws inspiration from the following existing open-source projects:
- [reactor-cpp](https://github.com/lf-lang/reactor-cpp)
- [reactor-c](https://github.com/lf-lang/reactor-c)
- [qpc](https://github.com/QuantumLeaps/qpc)
- [ssm-runtime](https://github.com/QuantumLeaps/qpc)

## TODO for the MVP:
- [x] Timers
- [x] Input/Output Ports
- [x] Logical Actions
- [x] Basic connections
- [x] Implement Event Queue and Reaction Queue
- [x] Level assignment algorithm (Includes the more elaborate connection setup)
- [x] Startup
- [x] Shutdown
- [x] Physical actions
- [x] Posix Platform abstractions
- [x] Basic code-generation


## More advanced topics
- [x] More platform abstractions (Riot, Zephyr and FlexPRET/InterPRET)
- [x] Reconsider where to buffer data (outputs vs inputs)
- [x] Consider if we should have FIFOs of pending events, not just a single for a trigger.
- [x] Runtime errors
- [x] Logging
- [x] Delayed connections
- [x] Basic decentralized federations
- [ ] Multiports and banks
- [ ] Modal reactors


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

Please source `env.bash` or `env.fish`. 

