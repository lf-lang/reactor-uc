#!/usr/bin/env bash

${REACTOR_UC_PATH}/ulf/bin/ulfc-dev src/HelloLF.ulf
west build -b qemu_cortex_m3 -p always