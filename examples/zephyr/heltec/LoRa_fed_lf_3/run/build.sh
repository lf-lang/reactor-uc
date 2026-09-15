#!/usr/bin/env bash
set -e

if [ -z "${REACTOR_UC_PATH}" ]; then
    echo "REACTOR_UC_PATH is not set. Please set it to the root of the reactor-uc repository."
    exit 1
fi

${REACTOR_UC_PATH}/ulf/bin/ulfc-dev src/LoRa_fed_lf_3.ulf

for federate in tx repeater rx; do
    echo "Building ${federate} federate..."
    west build -d "build-${federate}" \
        -b heltec_wifi_lora32_v2/esp32/procpu \
        -p always -- -DFEDERATE="${federate}"
done

west flash --esp-device /dev/ttyUSB0 -d build-tx
west flash --esp-device /dev/ttyUSB1 -d build-repeater
west flash --esp-device /dev/ttyUSB2 -d build-rx

echo "Firmware built and flashed. Monitor the boards separately:"
echo "  python3 -m serial.tools.miniterm /dev/ttyUSB0 115200"
echo "  python3 -m serial.tools.miniterm /dev/ttyUSB1 115200"
echo "  python3 -m serial.tools.miniterm /dev/ttyUSB2 115200"