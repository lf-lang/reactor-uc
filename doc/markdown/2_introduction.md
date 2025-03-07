\page intro Introduction

\note These docs assumes familiarity with the Lingua Franca coordination langauge. Refer to
the [Lingua Franca Handbook](https://www.lf-lang.org/docs/) for an in-depth guide to
reactor-oriented programming.

## Requirements
- CMake
- Make
- A C compiler like GCC or clang
- Linux or macOS development environment
- Java 17
- Additional requirements depend on the target platform

## Getting started

To get started clone, the reactor-uc repository and compile and run a simple Lingua Franca program
targeting the Native platform.

\code{console}
  git clone git@github.com:lf-lang/reactor-uc.git --recursive
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
  reactor-uc/lfc/bin/lfc-dev HelloWorld.lf
  bin/HelloWorld
\endcode

When executed the program should print `Hello World!` and terminate.




