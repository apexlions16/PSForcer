#!/usr/bin/env python3
"""PSForcer paket simgesini ek Python paketi olmadan üretir."""
import pathlib
import struct
import zlib

GENISLIK = YUKSEKLIK = 512


def parca(tur: bytes, veri: bytes) -> bytes:
    return struct.pack(">I", len(veri)) + tur + veri + struct.pack(">I", zlib.crc32(tur + veri) & 0xFFFFFFFF)


def ana() -> None:
    hedef = pathlib.Path(__file__).resolve().parents[1] / "sce_sys" / "icon0.png"
    hedef.parent.mkdir(parents=True, exist_ok=True)
    satirlar = []
    for y in range(YUKSEKLIK):
        satir = bytearray([0])
        for x in range(GENISLIK):
            oran = (x + y) / (GENISLIK + YUKSEKLIK - 2)
            r = int(18 + 92 * oran)
            g = int(20 + 75 * oran)
            b = int(42 + 190 * oran)
            if 54 < x < 458 and 54 < y < 458:
                r, g, b = int(r * 0.28), int(g * 0.28), int(b * 0.28)
            if (135 < x < 165 and 145 < y < 365) or (165 < x < 270 and 145 < y < 175) or (165 < x < 270 and 235 < y < 265) or (240 < x < 270 and 145 < y < 265):
                r, g, b = 248, 249, 255
            if (310 < x < 340 and 145 < y < 365) or (340 < x < 425 and 145 < y < 175) or (340 < x < 405 and 235 < y < 265):
                r, g, b = 190, 196, 255
            satir.extend((r, g, b, 255))
        satirlar.append(bytes(satir))
    ham = b"".join(satirlar)
    png = b"\x89PNG\r\n\x1a\n"
    png += parca(b"IHDR", struct.pack(">IIBBBBB", GENISLIK, YUKSEKLIK, 8, 6, 0, 0, 0))
    png += parca(b"IDAT", zlib.compress(ham, 9))
    png += parca(b"IEND", b"")
    hedef.write_bytes(png)
    print(f"Üretildi: {hedef}")


if __name__ == "__main__":
    ana()
