\page intro Introduction

\note These docs assumes familiarity with the Lingua Franca coordination langauge. Refer to
the [Lingua Franca Handbook](https://www.lf-lang.org/docs/) for an in-depth guide to
reactor-oriented programming.

## Getting started

To get started clone, the reactor-uc repository and compile and run a simple Lingua Franca program
targeting the Native posix platform.
\code{console}
  git clone git@github.com:lf-lang/reactor-uc.git --recursive
  cat > HelloWorld.lf << EOF
  target uC {
    platform: Native
  }

  main reactor() {
    reaction(startup) {=
      printf("Hello World!");
    =}
  }
  EOF
  reactor-uc/lfc/bin/lfc-dev HelloWorld.lf
  bin/HelloWorld
\endcode

