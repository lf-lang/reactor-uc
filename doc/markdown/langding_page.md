Reactor Micro-C 
====================

@subpage platform/riot.md
@subpage platform/zephyr.md
@subpage platform/pico.md

`reactor-uc` is a task scheduling runtime implementing the reactor
model-of-computation target at embedded and resource-constrained systems.

## Getting started

First you start by choosing a platform for which you want to write your application, second 
you instiantiate your repository from the corresponding template repository. Inside the templates
or the following sections is explained what build-tools and compilers the particular platform needs.

## Supported Platforms

`reactor-uc` can run on top of zephyr, riot, the rp2040 and operating systems which are posix compliant.

## Contributing

We are using `clang-format` version 18.1.3 which is default with Ubuntu 24.04 for formatting in CI. For kotlin we use `ktfmt` from facebook, both formatters can be called via `make format`.

## References & Sponsors

`reactor-uc` draws inspiration from the following existing open-source projects:

- [reactor-cpp](https://github.com/lf-lang/reactor-cpp)
- [reactor-c](https://github.com/lf-lang/reactor-c)
- [qpc](https://github.com/QuantumLeaps/qpc)
- [ssm-runtime](https://github.com/QuantumLeaps/qpc)

