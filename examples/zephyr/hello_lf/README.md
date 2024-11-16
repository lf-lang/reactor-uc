# Simple example of using the reactor-uc with LF and Zephyr

```
lfc src/HelloLF.lf -c
west build -b qemu_cortex_m3
west build -t run
```