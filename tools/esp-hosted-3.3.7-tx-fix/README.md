# ESP-Hosted 3.3.7 TX allocation fix

These two RISC-V objects are the ESP32-P4 Arduino core 3.3.7 ESP-Hosted
libraries rebuilt with Espressif's upstream fix from commit
`7fd49206ad116d1a69d64e69ef0636754a681308`.

The fix replaces the fatal `assert(copy_buff)` in `transport_drv_sta_tx()`
with a graceful error return when a temporary TX buffer allocation fails.
The firmware workflow injects the matching object into each precompiled core
archive and verifies the result before compiling.

- `esp32p4-libs/transport_drv.c.obj`
  - SHA-256: `bab61f7be9c5fda54b33c447d9f3109a392523a4957ed36a4a209a42354d7918`
- `esp32p4_es-libs/transport_drv.c.obj`
  - SHA-256: `1129808dce03ca4f6f28528f98773e8b800509775a61d8be63398b9380a64441`
