# Experimental TWRP build

This directory contains the reproducible build inputs for an experimental
Hanvon N10 Touch 2024 recovery. It reuses the stock recovery kernel, DTB,
second-stage payload and recovery DTBO. Only the TWRP ramdisk and userspace
Rockchip E-Ink backend are rebuilt.

The generated image is **not verified on hardware**. A successful CI build is
not evidence that display, touch, mounting or reboot behavior is safe. Keep a
Loader-mode recovery route and the stock recovery image available before any
flash test.

The stock Rockchip boot chain stores its bootloader control block at byte
offset `0x4000` in `misc`, instead of the AOSP offset `0`. The build applies a
device-specific `bootloader_message` patch so TWRP reads and clears the same
BCB location. This prevents a persistent `boot-recovery` command from sending
every subsequent reboot back to recovery. The device is A-only; Android A/B
wipe-package handling is outside the supported scope of this experimental
recovery.

The E-Ink backend targets the ABI observed in firmware 1.00.84:

- `/dev/ebc`
- mapping size `0x01400000`
- ioctls `0x7000`, `0x7001`, `0x7002`, `0x7010`
- 68-byte `ebc_buf_info`
- Y4/Y8 output and partial GC16 refresh

Stock binary components under `device/.../prebuilt/` are redistributed only
for device interoperability and recovery research. See the repository notice.
