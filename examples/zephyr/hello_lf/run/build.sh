#!/usr/bin/env bash

${REACTOR_UC_PATH}/lfc/bin/lfc-dev -c --runtime-symlink src/HelloLF.lf
west build -b qemu_cortex_m3 -p always