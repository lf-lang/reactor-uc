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

#### Sender

Enter the directory of the `sender` application:

```shell
cd coap_federated/sender
```

Get the IP address of the `receiver` by specifying the `PORT=tap1` and `ONLY_PRINT_IP=1` environment variables:

*If the program returns more than one IP-Address then select the one that starts with `fe80`*.

```shell
make ONLY_PRINT_IP=1 BOARD=native PORT=tap1 all term
```

Start the `sender` federated program to sends out a message to the `receiver` once the connection is established.
Make sure to replace `REMOTE_ADDRESS` with the correct address of the `receiver` and set `PORT=tap0` for the `sender`.

```shell
make REMOTE_ADDRESS=fe80::8cc3:33ff:febb:1b3 BOARD=native PORT=tap0 all term
```

#### Receiver

Enter the directory of the `receiver` application:

```shell
cd coap_federated/receiver
```

Get the IP address of the `sender` by specifying the `PORT=tap0` and `ONLY_PRINT_IP=1` environment variables:

*If the program returns more than one IP-Address then select the one that starts with `fe80`*.

```shell
make ONLY_PRINT_IP=1 BOARD=native PORT=tap0 all term
```

Start the `receiver` federated program to receive the message from the `sender`.
Make sure to replace `REMOTE_ADDRESS` with the correct address of the `sender` and set `PORT=tap1` for the `receiver`.

```shell
make REMOTE_ADDRESS=fe80::44e5:1bff:fee4:dac8 BOARD=native PORT=tap1 all term
```
