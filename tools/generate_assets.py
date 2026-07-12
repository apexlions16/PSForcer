#!/usr/bin/env python3
"""Generate the PSForcer package icon without external Python packages."""
import pathlib
import struct
import zlib

WIDTH = HEIGHT = 512


def chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


def main() -> None:
    target = pathlib.Path(__file__).resolve().parents[1] / "sce_sys" / "icon0.png"
    target.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    for y in range(HEIGHT):
        row = bytearray([0])
        for x in range(WIDTH):
            t = (x + y) / (WIDTH + HEIGHT - 2)
            r = int(18 + 92 * t)
            g = int(20 + 75 * t)
            b = int(42 + 190 * t)
            if 54 < x < 458 and 54 < y < 458:
                r, g, b = int(r * 0.28), int(g * 0.28), int(b * 0.28)
            if (135 < x < 165 and 145 < y < 365) or (165 < x < 270 and 145 < y < 175) or (165 < x < 270 and 235 < y < 265) or (240 < x < 270 and 145 < y < 265):
                r, g, b = 248, 249, 255
            if (310 < x < 340 and 145 < y < 365) or (340 < x < 425 and 145 < y < 175) or (340 < x < 405 and 235 < y < 265):
                r, g, b = 190, 196, 255
            row.extend((r, g, b, 255))
        rows.append(bytes(row))
    raw = b"".join(rows)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    target.write_bytes(png)
    print(f"generated {target}")


if __name__ == "__main__":
    main()
