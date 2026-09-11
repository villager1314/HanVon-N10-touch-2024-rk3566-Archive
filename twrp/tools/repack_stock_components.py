#!/usr/bin/env python3
"""Replace only the ramdisk while retaining all extracted stock boot-v2 parts."""

import argparse
import hashlib
import struct
from pathlib import Path

MAGIC = b"ANDROID!"


def align(value, page):
    return (value + page - 1) // page * page


def ramdisk_from_image(path):
    image = path.read_bytes()
    if image[:8] != MAGIC:
        raise ValueError("compiled recovery is not an Android boot image")
    kernel_size = struct.unpack_from("<I", image, 8)[0]
    ramdisk_size = struct.unpack_from("<I", image, 16)[0]
    page = struct.unpack_from("<I", image, 36)[0]
    offset = page + align(kernel_size, page)
    ramdisk = image[offset : offset + ramdisk_size]
    if len(ramdisk) != ramdisk_size:
        raise ValueError("compiled recovery ramdisk is truncated")
    return ramdisk


parser = argparse.ArgumentParser()
parser.add_argument("compiled_recovery", type=Path)
parser.add_argument("stock_dir", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()

header = bytearray((args.stock_dir / "header-v2.bin").read_bytes())
kernel = (args.stock_dir / "kernel").read_bytes()
second = (args.stock_dir / "second").read_bytes()
recovery_dtbo = (args.stock_dir / "recovery_dtbo").read_bytes()
dtb = (args.stock_dir / "dtb").read_bytes()
ramdisk = ramdisk_from_image(args.compiled_recovery)

if header[:8] != MAGIC or len(header) != 2048:
    raise SystemExit("invalid stock header-v2.bin")
page = struct.unpack_from("<I", header, 36)[0]
version = struct.unpack_from("<I", header, 40)[0]
if page != 2048 or version != 2:
    raise SystemExit(f"unexpected header: page={page}, version={version}")

struct.pack_into("<I", header, 8, len(kernel))
struct.pack_into("<I", header, 16, len(ramdisk))
struct.pack_into("<I", header, 24, len(second))
struct.pack_into("<I", header, 1632, len(recovery_dtbo))
struct.pack_into("<I", header, 1648, len(dtb))
dtbo_offset = page + align(len(kernel), page) + align(len(ramdisk), page) + align(len(second), page)
struct.pack_into("<Q", header, 1636, dtbo_offset)

digest = hashlib.sha1()
for payload in (kernel, ramdisk, second, recovery_dtbo, dtb):
    digest.update(payload)
    digest.update(struct.pack("<I", len(payload)))
header[576:608] = digest.digest() + b"\0" * 12

parts = [bytes(header)]
for payload in (kernel, ramdisk, second, recovery_dtbo, dtb):
    parts.extend((payload, b"\0" * (align(len(payload), page) - len(payload))))
image = b"".join(parts)
partition_size = 100663296
if len(image) > partition_size:
    raise SystemExit(f"image too large: {len(image)} > {partition_size}")
image += b"\0" * (partition_size - len(image))
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_bytes(image)
print(f"ramdisk={len(ramdisk)} output={len(image)} sha256={hashlib.sha256(image).hexdigest()}")
