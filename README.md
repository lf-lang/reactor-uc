# reactor-uc
`reactor-uc` is a task scheduling runtime implementing the reactor
model-of-computation target at embedded and resource-constrained 32 bit systems.

## Getting started

NB: reactor-uc is still work-in-progress and many (most?) features are not supported
yet. This only documents how to run a few example programs.


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
cd examples/zephyr
west build -b qemu_cortex_m3 -p always -t run
```

## Lingua Franca
To use reactor-uc as a target for Lingua Franca perform the following steps:

Clone and check out the reactor-uc branch of `lingua-franca` somewhere.
We will assume that an environment variable LF_PATH points to it;

```
git clone --recursive -b reactor-uc https://github.com/lf-lang/lingua-franca/ lf
```

Build the simple LF program and run it. By default it targets POSIX.
```
cd examples/lf
$LF_PATH/bin/lfc-dev src/HelloUc.lf -c
build/HelloUc
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
- reactor-cpp
- reactor-c
- qpc
- ssm-runtime

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
- [ ] Runtime errors
- [ ] Logging
- [ ] Multiports and banks
- [ ] Modal reactors
- [ ] Federated
- [ ] Delayed connections