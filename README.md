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
- [ ] Basic code-generation 

## More advanced topics
- [ ] More platform abstractions (Riot, Zephyr and FlexPRET/InterPRET)
- [ ] Runtime errors
- [ ] Logging
- [ ] Multiports and banks
- [ ] Modal reactors
- [ ] Federated
- [ ] Delayed connections