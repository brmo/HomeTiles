# ESP-Hosted 3.3.7 RX allocation + realloc fix

These RISC-V objects are rebuilt from the exact ESP-Hosted release the
ESP32-P4 Arduino core 3.3.7 libraries ship (`espressif__esp_hosted 2.11.6`,
esp-hosted-mcu commit `74f5e7d64ae97a4957a1c5c88bcd23c9447ccdfa`) with
Espressif's upstream fix commit `d7ebe00a22` ("fix: buffer allocation
improvements from the audit") applied:

- `sdio_drv.c`: `sdio_get_len_from_slave()` drops all-ones PKT_LEN reads
  (floating-bus signature that turned into a ~960 KB RX allocation),
  `sdio_rx_get_buffer()` returns NULL instead of `assert(*buf)` on alloc
  failure, and `sdio_read_task()` drops the read instead of `assert(rxbuff)`.
  Field crash fixed: `assert failed: sdio_rx_get_buffer sdio_drv.c:896 (*buf)`
  (waveshare_touch_lcd_8, v0.4.14, task `sdio_read`).
- `port_esp_hosted_host_os.c`: `hosted_realloc()` uses libc `realloc` instead
  of malloc + memcpy(newsize), which over-read the old block by the growth
  delta (heap over-read; the serial RX reassembly buffer grows through it).

Build verification: an unpatched rebuild of `sdio_drv.c` with the same
command matches the original archive object opcode-for-opcode (remaining
diffs are path-string offsets only) and has an identical symbol table.
The patched objects change local `__func__` numbering (two asserts removed)
and add `realloc` as an undefined symbol (resolved by newlib/ESP heap).

Toolchain: crosstool-NG esp-14.2.0_20251107 (riscv32-esp-elf-gcc 14.2.0),
compiled with each variant's shipped `flags/` + `qio_qspi/include/sdkconfig.h`
plus `-Os`. The firmware workflow injects the objects into each precompiled
core archive and verifies the result before compiling.

- `esp32p4-libs/sdio_drv.c.obj`
  - SHA-256: `3f75a936f9581585d954c1a22afd2724de76832f1245a1f4f13a36501c8d9cd7`
- `esp32p4-libs/port_esp_hosted_host_os.c.obj`
  - SHA-256: `1daeab5c942c161887a3f8aae5f9d221b317864b5235ada56a51ba3f641a5f88`
- `esp32p4_es-libs/sdio_drv.c.obj`
  - SHA-256: `5c44563aedcf633470de5ba1bdbee0caa1c7e6b54655a26a794b728562469daf`
- `esp32p4_es-libs/port_esp_hosted_host_os.c.obj`
  - SHA-256: `b5f842c6a6bc04f0b598f6d3c2f4aca4ed6e7cb33285eaf031d1896fc6adccbe`
