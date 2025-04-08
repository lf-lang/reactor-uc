#!/usr/bin/env bash

${REACTOR_UC_PATH}/lfuc/bin/lfuc-dev src/HelloLF.lf
west build -b qemu_cortex_m3 -p always