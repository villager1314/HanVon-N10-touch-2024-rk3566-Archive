# Experimental TWRP build

This directory contains the reproducible build inputs for an experimental
Hanvon N10 Touch 2024 recovery. It reuses the stock recovery kernel, DTB,
second-stage payload and recovery DTBO. Only the TWRP ramdisk and userspace
Rockchip E-Ink backend are rebuilt.

An earlier build reached the TWRP UI on hardware, but ADB, touch coordinates
and reboot-to-system were not yet correct. Keep a Loader-mode recovery route
and the stock recovery image available before every flash test.

The device-specific init file only supplies the Rockchip DWC3 controller and
the gadget parameters copied from stock recovery. The common TWRP `init.rc`
owns adbd, FunctionFS and the configfs USB state machine; duplicating its
`none -> adb` transition in the board file can race FunctionFS attachment.

The build also moves TWRP's existing one-shot BCB clear before graphics
initialization. This does not change the misc offsets or introduce a second
BCB implementation; it ensures a failed first E-Ink update cannot leave the
device permanently requesting recovery. E-Ink mode 7 is retained because it
is the mode of the build that reached the UI; mode 13 caused a boot-logo hang.

Offline disassembly of the stock firmware 1.00.84
`libbootloader_message.so` confirms the standard Android 11 misc layout: the
2-KiB BCB is read and written at byte offset `0`, wipe-package access uses
`0x4000`, and the library's generic Virtual A/B message access uses `0x8000`.
The device itself is A-only with dynamic partitions, so the presence of the
generic Virtual A/B API does not make it a Virtual A/B device. The build keeps
the upstream TWRP/AOSP offsets unchanged to match stock recovery behavior.

The E-Ink backend targets the ABI observed in firmware 1.00.84:

- `/dev/ebc`
- mapping size `0x01400000`
- ioctls `0x7000`, `0x7001`, `0x7002`, `0x7010`
- 68-byte `ebc_buf_info`
- Y4/Y8 output and partial GC16 refresh

Stock binary components under `device/.../prebuilt/` are redistributed only
for device interoperability and recovery research. See the repository notice.
