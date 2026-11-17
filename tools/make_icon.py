#!/usr/bin/env python3
"""Writes res/crowman.ico: a dark disc with a white lens ring and pupil."""
import math, os, struct

SIZES = (16, 24, 32, 48, 64, 128, 256)


def pixels(size):
    cx = cy = (size - 1) / 2.0
    outer, ring_outer, ring_inner, pupil = size*0.46, size*0.30, size*0.20, size*0.11
    rows = []
    for y in range(size):
        row = bytearray()
        for x in range(size):
            d = math.hypot(x - cx, y - cy)
            if d > outer:
                row += b"\x00\x00\x00\x00"
            elif d > ring_outer or d < pupil or d > ring_inner:
                row += b"\xff\xff\xff\xff"
            else:
                row += b"\xff\x5a\x1e\x14"
        rows.append(bytes(row))
    return rows


def entry(size):
    rows = pixels(size)
    xor = b"".join(reversed(rows))
    stride = ((size + 31) // 32) * 4
    mask = b"".join(b"\x00" * stride for _ in range(size))
    header = struct.pack("<IiiHHIIiiII", 40, size, size*2, 1, 32, 0,
                         len(xor) + len(mask), 0, 0, 0, 0)
    return header + xor + mask


def main():
    out = os.path.join(os.path.dirname(__file__), "..", "res", "crowman.ico")
    blobs = [(s, entry(s)) for s in SIZES]
    with open(out, "wb") as f:
        f.write(struct.pack("<HHH", 0, 1, len(blobs)))
        offset = 6 + 16 * len(blobs)
        for size, blob in blobs:
            f.write(struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32,
                                len(blob), offset))
            offset += len(blob)
        for _, blob in blobs:
            f.write(blob)
    print("wrote", os.path.normpath(out))


if __name__ == "__main__":
    main()
