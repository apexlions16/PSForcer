# PSForcer katalog bildirimi

PSForcer tek bir JSON belgesi okur. Paket içindeki `assets/catalog.json` çevrimdışı yedektir. Güncel katalog GitHub’daki `catalog/katalog.json` dosyasında tutulur; Hugging Face yalnızca büyük PKG dosyalarını barındırır.

## Üst düzey alanlar

| Alan | Tür | Amaç |
|---|---|---|
| `schemaVersion` | tam sayı | Değer `1` olmalıdır |
| `catalogTitle` | metin | Görünen katalog adı |
| `updatedAt` | metin | Okunabilir tarih veya ISO-8601 zaman damgası |
| `items` | dizi | Mağazada gösterilen oyunlar ve ürünler |

## İçerik alanları

- `id`: değişmeyen ASCII tanımlayıcısı
- `title`: oyun adı
- `developer`, `genre`, `description`: geliştirici, tür ve açıklama
- `releaseYear`: çıkış yılı
- `accent`: altı basamaklı RGB onaltılık renk
- `media.cover`: yerel `/app0/...` yolu veya uzak HTTPS bağlantısı
- `media.hero`: yerel `/app0/...` yolu veya uzak HTTPS bağlantısı
- `media.trailer`: isteğe bağlı tanıtım videosu bağlantısı
- `media.screenshots`: görsel bağlantıları dizisi
- `packages`: indirilebilir paket kayıtları dizisi

## Paket alanları

| Alan | Zorunlu | Açıklama |
|---|---|---|
| `id` | evet | İçerik içinde benzersiz olmalıdır |
| `kind` | evet | `oyun`, `guncelleme`, `ek-paket` veya `ekstra` |
| `label` | evet | Kullanıcıya gösterilen ad |
| `version` | evet | Gösterilen sürüm |
| `size` | evet | Bayt cinsinden tam sayı |
| `url` | indirme için | Hugging Face dosya bağlantısı dâhil HTTPS adresi |
| `sha256` | kesinlikle önerilir | Küçük harfli SHA-256 özeti |
| `minFirmware` | isteğe bağlı | Bilgilendirme amaçlı sistem yazılımı değeri |
| `deleteAfterInstall` | isteğe bağlı | Varsayılan `false`; yalnızca kurucu başarı bildirdikten sonra silinir |

## Örnek

```json
{
  "schemaVersion": 1,
  "catalogTitle": "PSForcer İçerik Kütüphanesi",
  "updatedAt": "2026-07-12T00:00:00Z",
  "items": [
    {
      "id": "ornek-oyun",
      "title": "Örnek Oyun",
      "developer": "Örnek Stüdyo",
      "genre": "Aksiyon",
      "description": "Katalog açıklaması.",
      "releaseYear": 2026,
      "accent": "6C63FF",
      "media": {
        "cover": "https://raw.githubusercontent.com/KULLANICI/KATALOG/main/medya/ornek-kapak.jpg",
        "hero": "https://raw.githubusercontent.com/KULLANICI/KATALOG/main/medya/ornek-genis.jpg",
        "trailer": "https://github.com/KULLANICI/KATALOG/releases/download/medya/ornek-tanitim.mp4",
        "screenshots": []
      },
      "packages": [
        {
          "id": "ornek-ana-100",
          "kind": "oyun",
          "label": "Ana Oyun",
          "version": "1.00",
          "size": 123456789,
          "url": "https://huggingface.co/datasets/KULLANICI/DEPO/resolve/main/paketler/ornek.pkg",
          "sha256": "",
          "minFirmware": "9.00",
          "deleteAfterInstall": false
        }
      ]
    }
  ]
}
```

## Depolama ayrımı

```text
GitHub
  catalog/katalog.json
  medya bağlantıları ve küçük yönetim dosyaları

Hugging Face
  oyun-kimligi-ana-1.00.pkg
  oyun-kimligi-guncelleme-1.01.pkg
  oyun-kimligi-ek-paket-adi.pkg
```

Yayınlanan PKG dosyaları için değişmeyen dosya adları kullanın. Bir dosya değişirse SHA-256 değerini de mutlaka güncelleyin.
