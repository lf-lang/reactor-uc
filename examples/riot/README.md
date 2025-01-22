# RIOT Examples

This doc explains how to compile and run the various RIOT OS examples.

## Setup RIOT Environment

Make sure that the environment variable `RIOTBASE` points to a `RIOT` codebase.

## Build and Run

### Blinky

```shell
cd blinky
make BOARD=native all term
```

### Hello

```shell
cd hello
make BOARD=native all term
```

### CoAP Federated

The federated example using CoAP channels needs to be run using 2 terminals.
Make sure to set the `PORT` environment variable to the correct `tap` interface such as `tap0` or `tap1` as can be seen in the code below.

#### Preparation

First you need to create the `tap` interfaces so that the `sender` and `receiver` application can communicate through the (linux) host.

```shell
sudo $RIOTBASE/dist/tools/tapsetup/tapsetup
```

#### Start the Sender

Start the sender with `PORT=tap0`:

```shell
cd sender
make BOARD=native PORT=tap0 all term
```

#### Start the Receiver

Start the receiver with `PORT=tap1`:

```shell
cd receiver
make BOARD=native PORT=tap1 all term
```
