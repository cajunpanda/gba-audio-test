#!/usr/bin/env python3
"""Patch a GBA ROM's header complement check byte (offset 0xBD).

The GBA BIOS refuses to boot a cartridge whose header checksum is wrong, so
this must run after linking the raw binary. Equivalent to devkitPro `gbafix`
for the one field we care about. Usage: gbafix.py rom.gba
"""
import sys


def main(path):
    with open(path, "rb") as f:
        data = bytearray(f.read())
    if len(data) < 0xC0:
        sys.exit("ROM too small to contain a header")
    # complement = -(0x19 + sum(header[0xA0..0xBC])) & 0xFF
    chk = 0
    for b in data[0xA0:0xBD]:
        chk += b
    chk = (-(0x19 + chk)) & 0xFF
    data[0xBD] = chk
    with open(path, "wb") as f:
        f.write(data)
    print("gbafix: header complement check = 0x%02X" % chk)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: gbafix.py <rom.gba>")
    main(sys.argv[1])
