# ESP-Hosted 3.3.7 RX allocation + realloc fix

These RISC-V objects are rebuilt from the exact ESP-Hosted release the
ESP32-P4 Arduino core 3.3.7 libraries ship (`espressif__esp_hosted 2.11.6`,
esp-hosted-mcu commit `74f5e7d64ae97a4957a1c5c88bcd23c9447ccdfa`) with
the relevant changes from Espressif's upstream fix commits applied:

- `a914274d10e8f0e754b5902c7867ff96cea4a8b9` ("fix(sdio): gracefully
  drop packets on RX mempool exhaustion")
- `971cf0d859073c4839d3b7b87bdae4c68240883c` ("fix(sdio): throttle RX
  mempool drop log to burst start/recovery")
- `d7ebe00a22e618e47a2b89f7569d5ed37163fa1e` ("fix: buffer allocation
  improvements from the audit")

- `sdio_drv.c`: `sdio_get_len_from_slave()` drops all-ones PKT_LEN reads
  (floating-bus signature that turned into a ~960 KB RX allocation),
  `sdio_rx_get_buffer()` returns NULL instead of `assert(*buf)` on alloc
  failure, and `sdio_read_task()` drops the read instead of `assert(rxbuff)`.
  Field crash fixed: `assert failed: sdio_rx_get_buffer sdio_drv.c:896 (*buf)`
  (waveshare_touch_lcd_8, v0.4.14, task `sdio_read`).
  Streaming RX now skips only the packet whose `pkt_rxbuff` allocation failed
  instead of asserting, and logs one OOM-start/OOM-end pair per exhaustion
  burst. Field crash fixed: `assert failed: sdio_push_data_to_queue
  sdio_drv.c:964 (pkt_rxbuff)` (waveshare_touch_lcd_8, v0.5.6, tasks
  `sdio_rx_buf` and `loopTask`). The shared SDIO TX mempool allocation also
  fails gracefully instead of asserting.
- `port_esp_hosted_host_os.c`: `hosted_realloc()` uses libc `realloc` instead
  of malloc + memcpy(newsize), which over-read the old block by the growth
  delta (heap over-read; the serial RX reassembly buffer grows through it).

Build verification: an unpatched rebuild of `sdio_drv.c` with the same
command matches the original archive object opcode-for-opcode (remaining
diffs are path-string offsets only) and has an identical symbol table.
The patched objects remove four fatal allocation asserts from the affected
SDIO RX/TX paths and add `realloc` as an undefined symbol (resolved by
newlib/ESP heap).

Toolchain: crosstool-NG esp-14.2.0_20251107 (riscv32-esp-elf-gcc 14.2.0),
compiled with each variant's shipped `flags/` + `qio_qspi/include/sdkconfig.h`
plus `-Os`. The firmware workflow injects the objects into each precompiled
core archive and verifies the result before compiling.

- `esp32p4-libs/sdio_drv.c.obj`
  - SHA-256: `0c5df1741b87b1b24ca27f7293fdd844e65bd776c0dc3a8d6331da8c3a2a393d`
- `esp32p4-libs/port_esp_hosted_host_os.c.obj`
  - SHA-256: `1daeab5c942c161887a3f8aae5f9d221b317864b5235ada56a51ba3f641a5f88`
- `esp32p4_es-libs/sdio_drv.c.obj`
  - SHA-256: `34c2316d151778b459d849b1af6bd1e2d98376722ffa3aa3618d68b9070de637`
- `esp32p4_es-libs/port_esp_hosted_host_os.c.obj`
  - SHA-256: `b5f842c6a6bc04f0b598f6d3c2f4aca4ed6e7cb33285eaf031d1896fc6adccbe`
