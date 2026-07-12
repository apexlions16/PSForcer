# PSForcer catalog manifest

PSForcer reads one JSON document. The bundled example is `assets/catalog.json`; the same shape can be hosted on Hugging Face.

## Top-level fields

| Field | Type | Purpose |
|---|---|---|
| `schemaVersion` | integer | Must be `1` |
| `catalogTitle` | string | Display name |
| `updatedAt` | string | Human-readable or ISO-8601 timestamp |
| `items` | array | Games/products shown in the storefront |

## Item fields

- `id`: stable ASCII identifier
- `title`: game title
- `developer`, `genre`, `description`
- `releaseYear`: integer
- `accent`: six-digit RGB hex color
- `media.cover`: local `/app0/...` path or remote HTTPS URL
- `media.hero`: local `/app0/...` path or remote HTTPS URL
- `media.trailer`: optional trailer URL
- `media.screenshots`: array of image URLs
- `packages`: array of downloadable package records

## Package fields

| Field | Required | Notes |
|---|---|---|
| `id` | yes | Unique within the item |
| `kind` | yes | `game`, `update`, `dlc`, or `extra` |
| `label` | yes | User-facing label |
| `version` | yes | Displayed version |
| `size` | yes | Byte size as an integer |
| `url` | for downloads | HTTPS URL, including a Hugging Face file URL |
| `sha256` | strongly recommended | Lowercase SHA-256 digest |
| `minFirmware` | optional | Informational firmware value |
| `deleteAfterInstall` | optional | Defaults to `false`; deletion occurs only after installer success |

## Example

```json
{
  "schemaVersion": 1,
  "catalogTitle": "PSForcer",
  "updatedAt": "2026-07-12T00:00:00Z",
  "items": [
    {
      "id": "sample-game",
      "title": "Sample Game",
      "developer": "Studio",
      "genre": "Action",
      "description": "Catalog description.",
      "releaseYear": 2026,
      "accent": "6C63FF",
      "media": {
        "cover": "https://huggingface.co/datasets/USER/REPO/resolve/main/media/sample-cover.png",
        "hero": "https://huggingface.co/datasets/USER/REPO/resolve/main/media/sample-hero.jpg",
        "trailer": "https://huggingface.co/datasets/USER/REPO/resolve/main/media/sample-trailer.mp4",
        "screenshots": []
      },
      "packages": [
        {
          "id": "sample-base-100",
          "kind": "game",
          "label": "Base Game",
          "version": "1.00",
          "size": 123456789,
          "url": "https://huggingface.co/datasets/USER/REPO/resolve/main/pkg/sample.pkg",
          "sha256": "",
          "minFirmware": "9.00",
          "deleteAfterInstall": false
        }
      ]
    }
  ]
}
```

## Hugging Face layout suggestion

```text
catalog.json
media/
  game-id-cover.png
  game-id-hero.jpg
  game-id-trailer.mp4
pkg/
  game-id-base-1.00.pkg
  game-id-update-1.01.pkg
  game-id-dlc-name.pkg
```

Use immutable filenames for published PKGs or update the SHA-256 whenever a file changes.
