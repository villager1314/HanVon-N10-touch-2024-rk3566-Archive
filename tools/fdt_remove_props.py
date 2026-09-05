"""Remove selected properties from a node in one or more concatenated FDT blobs.

This script edits the input file in place. Work on a copy and verify the result.
It was written for offline boot-image research and is not a flashing tool.
"""

import argparse
import struct
from pathlib import Path

FDT_BEGIN_NODE = 1
FDT_END_NODE = 2
FDT_PROP = 3
FDT_NOP = 4
FDT_END = 9


def align4(value):
    return (value + 3) & ~3


def cstring(blob, offset):
    end = blob.index(0, offset)
    return bytes(blob[offset:end]).decode("ascii", "replace")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dtb")
    parser.add_argument("node")
    parser.add_argument("properties", nargs="+")
    args = parser.parse_args()

    path = Path(args.dtb)
    data = bytearray(path.read_bytes())
    removed = []
    wanted = set(args.properties)
    magic = struct.pack(">I", 0xD00DFEED)
    bases = [i for i in range(len(data) - 39) if data[i:i + 4] == magic]
    valid = 0

    for base in bases:
        total_size = struct.unpack_from(">I", data, base + 4)[0]
        if total_size < 40 or base + total_size > len(data):
            continue
        off_struct = base + struct.unpack_from(">I", data, base + 8)[0]
        off_strings = base + struct.unpack_from(">I", data, base + 12)[0]
        size_strings = struct.unpack_from(">I", data, base + 32)[0]
        size_struct = struct.unpack_from(">I", data, base + 36)[0]
        if off_struct + size_struct > base + total_size:
            continue
        strings = data[off_strings:off_strings + size_strings]
        valid += 1

        pos = off_struct
        end_struct = off_struct + size_struct
        nodes = []
        while pos < end_struct:
            token_pos = pos
            token = struct.unpack_from(">I", data, pos)[0]
            pos += 4
            if token == FDT_BEGIN_NODE:
                name = cstring(data, pos)
                nodes.append(name)
                pos = align4(pos + len(name.encode()) + 1)
            elif token == FDT_END_NODE:
                nodes.pop()
            elif token == FDT_PROP:
                length, nameoff = struct.unpack_from(">II", data, pos)
                pos += 8
                name = cstring(strings, nameoff)
                prop_end = align4(pos + length)
                current = "/" + "/".join(n for n in nodes if n)
                if current == args.node and name in wanted:
                    for word in range(token_pos, prop_end, 4):
                        struct.pack_into(">I", data, word, FDT_NOP)
                    removed.append(f"{name}@0x{base:x}")
                pos = prop_end
            elif token == FDT_NOP:
                continue
            elif token == FDT_END:
                break
            else:
                raise SystemExit(f"unknown token {token} at 0x{token_pos:x}")

    if not valid:
        raise SystemExit("no valid flattened device tree found")

    path.write_bytes(data)
    print("removed:", ", ".join(removed) if removed else "none")
    removed_names = {item.split("@", 1)[0] for item in removed}
    missing = wanted.difference(removed_names)
    if missing:
        print("not present:", ", ".join(sorted(missing)))


if __name__ == "__main__":
    main()
