#!/usr/bin/env python3
"""Hugging Face doğrudan bağlantısından dosya bilgisi okur."""

from __future__ import annotations

import json
import pathlib
import sys
import urllib.parse
import urllib.request


def hata(ileti: str) -> None:
    raise SystemExit(f"Hugging Face dosya bilgisi okunamadı: {ileti}")


def baglantiyi_coz(url: str) -> tuple[str, str, str]:
    parcalar = urllib.parse.urlsplit(url)
    if parcalar.scheme != "https" or parcalar.netloc != "huggingface.co":
        hata("yalnızca huggingface.co HTTPS bağlantıları desteklenir")

    yol = [urllib.parse.unquote(p) for p in parcalar.path.split("/") if p]
    if len(yol) < 7 or yol[0] not in {"datasets", "models", "spaces"}:
        hata("bağlantı desteklenen Hugging Face resolve yapısında değil")
    if yol[3] != "resolve":
        hata("bağlantıda resolve bölümü bulunamadı")

    depo_turu = yol[0]
    depo_kimligi = f"{yol[1]}/{yol[2]}"
    surum = yol[4]
    dosya_yolu = "/".join(yol[5:])
    api_turu = {"datasets": "datasets", "models": "models", "spaces": "spaces"}[depo_turu]
    return api_turu + "/" + depo_kimligi, surum, dosya_yolu


def json_oku(url: str) -> object:
    istek = urllib.request.Request(url, headers={"User-Agent": "PSForcer-Katalog-Araci/1.0"})
    with urllib.request.urlopen(istek, timeout=45) as yanit:
        return json.loads(yanit.read().decode("utf-8"))


def dosya_bilgisi(url: str) -> dict[str, object]:
    depo, surum, dosya_yolu = baglantiyi_coz(url)
    ust_klasor = str(pathlib.PurePosixPath(dosya_yolu).parent)
    if ust_klasor == ".":
        ust_klasor = ""

    api = f"https://huggingface.co/api/{depo}/tree/{urllib.parse.quote(surum, safe='')}"
    if ust_klasor:
        api += "/" + urllib.parse.quote(ust_klasor, safe="/")
    api += "?recursive=false&expand=true"

    liste = json_oku(api)
    if not isinstance(liste, list):
        hata("Hugging Face API yanıtı beklenen dizi biçiminde değil")

    kayit = next((oge for oge in liste if isinstance(oge, dict) and oge.get("path") == dosya_yolu), None)
    if not kayit:
        hata(f"dosya bulunamadı: {dosya_yolu}")

    boyut = kayit.get("size")
    if not isinstance(boyut, int) or boyut <= 0:
        hata("dosya boyutu alınamadı")

    lfs = kayit.get("lfs") if isinstance(kayit.get("lfs"), dict) else {}
    ozet = lfs.get("oid") or kayit.get("oid") or ""
    return {
        "url": url,
        "path": dosya_yolu,
        "size": boyut,
        "mib": round(boyut / 1024 / 1024, 2),
        "gib": round(boyut / 1024 / 1024 / 1024, 3),
        "sha256": ozet if isinstance(ozet, str) and len(ozet) == 64 else "",
    }


def ana() -> None:
    if len(sys.argv) != 2:
        hata("kullanım: huggingface_dosya_bilgisi.py DOĞRUDAN_BAĞLANTI")
    print(json.dumps(dosya_bilgisi(sys.argv[1]), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    ana()
