# Basic federated Zephyr example
This is an example of a basic federation using the TcpIpChannel and the
Zephyr target.


## Prerequisits
- Tested on Ubuntu 20.04
- A Zephyr workspace activated
- NXP LinkServer installed for flashing the boards.
- 2x NXP FRDM K64F boards connected by an ethernet cable.


## The program
The program consists of a `Sender` federate which has a physical action hooked
up to a button. The reaction triggered by the physical action toggles an LED
and sets an output port going to `Receiver`. `Receiver` also toggles his LED 
upon receiving an event.

## Run the example
```
./buildAndFlash.sh
```

This will compile both federates and flash them using `west` and `LinkServer`.
After sucessfully flashing the boards their green LED should light up.
Inspect the debug output from each board with minicom:
```
sudo minicom -D /dev/ttyACM0
sudo minicom -D /dev/ttyACM1
```


To get the clocks of the boards synchronized I suggest to do a power cycle after
the flashing.

When pressing SW2 on the Sender, the LED on both boards should toggle
simultanously.
