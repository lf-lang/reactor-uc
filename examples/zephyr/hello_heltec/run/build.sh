#!/usr/bin/env bash

${REACTOR_UC_PATH}/ulf/bin/ulfc-dev src/hello_heltec.ulf
west build -b heltec_wifi_lora32_v2/esp32/procpu -p always