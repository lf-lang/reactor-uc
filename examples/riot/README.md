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

#### Get IPv6 address of receiver

Enter the directory of the `sender` application:

```shell
cd coap_federated/sender
```

Get the IP address of the `receiver` by specifying the `PORT=tap1` and `ONLY_PRINT_IP=1` environment variables:

*If the program returns more than one IP-Address then select the one that starts with `fe80`*.

```shell
make ONLY_PRINT_IP=1 BOARD=native PORT=tap1 all term
```

The resulting program will print out the IPv6 address of `tap1` and terminate. 
This address must be used when starting the sender below.


#### Get IPv6 address of sender

Enter the directory of the `receiver` application:

```shell
cd coap_federated/receiver
```

Get the IP address of the `sender` by specifying the `PORT=tap0` and `ONLY_PRINT_IP=1` environment variables:

*If the program returns more than one IP-Address then select the one that starts with `fe80`*.

```shell
make ONLY_PRINT_IP=1 BOARD=native PORT=tap0 all term
```

The resulting program will print out the IPv6 address of `tap0` and terminate. 
This address must be used when starting the receiver below.

#### Start the applications

##### Sender
Start the sender with `PORT=tap0`, make sure to replace `REMOTE_ADDRESS` with 
the address of `tap1` that you found above.  

```shell
cd sender
make REMOTE_ADDRESS=fe80::8cc3:33ff:febb:1b3 BOARD=native PORT=tap0 all term
```

##### Receiver

Start the receiver with `PORT=tap1`, make sure to replace `REMOTE_ADDRESS` with 
the address of `tap0` that you found above.  

```shell
cd receiver
make REMOTE_ADDRESS=fe80::44e5:1bff:fee4:dac8 BOARD=native PORT=tap1 all term
```
