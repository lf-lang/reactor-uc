# Reactor-UC Examples

This directory contains example applications for the Reactor-UC runtime across various platforms. These examples demonstrate the core functionality of the framework and can serve as a starting point for exploring Reactor-UC.

## Directory Structure

```
examples/
├── common/          # Shared header files used across examples
├── esp-idf/         # Espressif ESP-IDF examples
├── freertos/        # FreeRTOS examples
├── posix/           # POSIX-compliant systems (Linux, macOS)
├── pico/            # Raspberry Pi Pico examples
├── zephyr/          # Zephyr RTOS examples
├── riot/            # RIOT OS examples
├── flexpret/        # FlexPRET platform examples
└── fed-template/    # Federated application templates
```

## Building All Examples

Each platform directory contains a `buildAll.sh` script to build all examples for that platform:

```sh
cd examples/<platform>
./buildAll.sh
```

To build all examples across all platforms (requires all platform dependencies installed):

```sh
cd examples
./runAll.sh
```

## WSL (Windows Subsystem for Linux) Considerations

If you're developing on Windows using WSL, there are important considerations when working with embedded devices.

### Serial Port Access

WSL2 doesn't natively support USB device passthrough. To access serial ports from your embedded devices, you can follow the [Microsoft WSL USB guide](https://learn.microsoft.com/en-us/windows/wsl/connect-usb).

### Configuring Runtime Duration for Serial Monitoring

When using WSL with external serial ports, it's convenient to configure your application to run indefinitely. Otherwise, the program may terminate before you can connect your serial monitor.

Edit the timeout in the `LF_ENTRY_POINT` macro in the `examples/common/timer_source.h` file:

```c
// Run for 1 second (default)
LF_ENTRY_POINT(TimerSource, 32, 32, SEC(1), false, false);

// Run indefinitely (recommended for WSL with external serial monitors)
LF_ENTRY_POINT(TimerSource, 32, 32, FOREVER, false, false);

// Run for other durations
LF_ENTRY_POINT(TimerSource, 32, 32, MINS(5), false, false);  // 5 minutes
LF_ENTRY_POINT(TimerSource, 32, 32, HOUR(1), false, false);  // 1 hour
```

This ensures your device continues outputting serial data, giving you enough time to connect your serial monitor on the Windows side.

## Troubleshooting

### Build Failures

If you encounter build issues:

1. Clean the build directory:
   ```sh
   rm -rf build && mkdir build
   ```

2. Verify all prerequisites are installed for your platform

3. Check that platform-specific environment variables are set correctly

## Contributing

When adding new examples:

1. Place shared code in `common/`
2. Follow the existing directory structure for platform-specific examples
3. Add a `buildAll.sh` script for batch building
4. Test on the target platform before submitting a PR