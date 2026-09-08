#!/usr/bin/env bash
set -e

if [ -z "${REACTOR_UC_PATH}" ]; then
    echo "REACTOR_UC_PATH is not set. Please set it to the root of the reactor-uc repository."
    exit 1
fi

${REACTOR_UC_PATH}/ulf/bin/ulfc-dev src/hello_heltec.ulf
west build -b heltec_wifi_lora32_v2/esp32/procpu -p always
west flash
python3 -m serial.tools.miniterm /dev/ttyUSB0 115200 #Press Ctrl+] to exit