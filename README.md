# reactor-uc

NB: reactor-uc is still work-in-progress

`reactor-uc` is a task scheduling runtime implementing the reactor
model-of-computation target at embedded and resource-constrained 32 bit systems.

## Goals
- Incorporate unit testing and test-driven development from the start
- Optimized for single-core 32-bit systems
- Backend for LF
- Standalone library
- Use clang-format and clang-tidy to write safe code from the start.
- CMake-based
- Optimized for single-threaded runtime, but support for enclaves which enable 
real-time scheduling.
- Avoid malloc as much as possible (or entirely?)

## References
`reactor-uc` draws inspiration from the following existing open-source projects:
- reactor-cpp
- reactor-c
- qpc
- ssm-runtime

## TODO for the MVP:
- [x] Basic support for timers
- [x] Basic support for ports
- [x] Basic connections
- [x] Implement Event Queue and Reaction Queue
- [ ] Hardware Abstractions: 
  - [x] RIOT-os
  - [x] Posix
  - [ ] Zephyr
- [ ] Level assignment algorithm (Includes the more elaborate connection setup)
- [ ] Casuality cycle algorithm
- [ ] Physical actions
