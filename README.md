# reactor-uc
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
- Avoid malloc as much as possible

## References
`reactor-uc` draws inspiration from the following existing open-source projects:
- reactor-cpp
- reactor-c
- qpc
- ssm-runtime

## TODO:
- [x] Basic support for timers
- [ ] Basic support for ports
- [ ] Basic connections
- [ ] Implement Event Queue and Reaction Queue
- [ ] Level assignment algorithm
- [ ] Casuality cycle algorithm
- [ ] Physical actions