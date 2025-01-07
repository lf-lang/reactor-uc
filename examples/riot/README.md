# RIOT Examples

This doc explains how to compile and run the various RIOT OS examples.

## Setup RIOT Environment

Make sure that the environment variable `RIOTBASE` points to a `RIOT` codebase.

## Build and Run

### Blinky

```shell
cd examples/riot/blinky
make BOARD=native all term
```

### Hello

```shell
cd examples/riot/hello
make BOARD=native all term
```

### CoAP Federated

The federated example using CoAP channels needs to be run using 2 terminals.
Make sure to set the `PORT` environment variable to the correct `tap` interface such as `tap0` or `tap1` as can be seen in the code below.

#### Preparation

```shell
# Setup tap interfaces for communication on the (linux) host
sudo $RIOTBASE/dist/tools/tapsetup/tapsetup
```

#### Terminal 1

Start the `sender` federated program to sends out a message to the `receiver` once the connection is established.

```shell
cd examples/riot/coap_federated/sender
make BOARD=native PORT=tap0 all term
```

#### Terminal 2

Start the `receiver` federated program to receive the message from the `sender`.

```shell
cd examples/riot/coap_federated/receiver
make BOARD=native PORT=tap1 all term
```
