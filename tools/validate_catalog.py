#!/usr/bin/env python3
import hashlib
import json
import pathlib
import sys

GECERLI_TURLER = {
    "game", "base", "update", "patch", "dlc", "extra", "bonus",
    "oyun", "ana-oyun", "guncelleme", "ek-paket", "ekstra"
}


def hata(ileti: str) -> None:
    raise SystemExit(f"Katalog doğrulaması başarısız: {ileti}")


def ana() -> None:
    if len(sys.argv) != 2:
        hata("Kullanım: validate_catalog.py DOSYA_YOLU")
    yol = pathlib.Path(sys.argv[1])
    veri = json.loads(yol.read_text(encoding="utf-8"))
    if veri.get("schemaVersion") != 1:
        hata("schemaVersion değeri 1 olmalıdır")
    icerikler = veri.get("items")
    if not isinstance(icerikler, list):
        hata("items alanı bir dizi olmalıdır")
    gorulen_icerikler: set[str] = set()
    for sira, icerik in enumerate(icerikler):
        icerik_kimligi = icerik.get("id")
        if not isinstance(icerik_kimligi, str) or not icerik_kimligi:
            hata(f"items[{sira}].id alanı zorunludur")
        if icerik_kimligi in gorulen_icerikler:
            hata(f"Yinelenen içerik kimliği: {icerik_kimligi}")
        gorulen_icerikler.add(icerik_kimligi)
        if not isinstance(icerik.get("title"), str) or not icerik["title"]:
            hata(f"{icerik_kimligi}: title alanı zorunludur")
        paketler = icerik.get("packages")
        if not isinstance(paketler, list):
            hata(f"{icerik_kimligi}: packages alanı bir dizi olmalıdır")
        gorulen_paketler: set[str] = set()
        for paket in paketler:
            paket_kimligi = paket.get("id")
            if not isinstance(paket_kimligi, str) or not paket_kimligi:
                hata(f"{icerik_kimligi}: paket kimliği zorunludur")
            if paket_kimligi in gorulen_paketler:
                hata(f"{icerik_kimligi}: yinelenen paket kimliği {paket_kimligi}")
            gorulen_paketler.add(paket_kimligi)
            if paket.get("kind") not in GECERLI_TURLER:
                hata(f"{icerik_kimligi}/{paket_kimligi}: paket türü geçersiz")
            ozet = paket.get("sha256", "")
            if ozet and (len(ozet) != 64 or any(c not in "0123456789abcdefABCDEF" for c in ozet)):
                hata(f"{icerik_kimligi}/{paket_kimligi}: sha256 değeri 64 onaltılık karakter olmalıdır")
    ozet = hashlib.sha256(yol.read_bytes()).hexdigest()
    print(f"Geçerli katalog: {len(icerikler)} içerik, sha256={ozet}")


if __name__ == "__main__":
    ana()
