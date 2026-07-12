# PSForcer

PSForcer is an OpenOrbis-based PlayStation 4 catalog and download client. It presents games in a console-friendly storefront, separates base-game, update, DLC and extra packages, downloads content from manifest-provided URLs, verifies SHA-256 hashes, and hands completed files to an installer adapter.

> The repository does not contain an exploit, jailbreak, Sony SDK material, proprietary runtime modules, or code that bypasses platform security. The included installer adapter marks a verified package as ready for manual/authorized installation. Automatic installation can only be connected to an installer API that is legitimately available in the target environment.

## Current milestone

- 1920x1080 SDL2 storefront UI with browse and detail screens
- Base game / update / DLC / extra package grouping
- Local cover and hero artwork, plus remote media URL fields
- JSON catalog loaded from the package or refreshed from a remote manifest
- Background HTTP(S) downloads with resume support
- SHA-256 verification before install handoff
- Delete-after-install policy, triggered only after an installer returns `Installed`
- Hugging Face-ready manifest schema

## Controls

| Control | Action |
|---|---|
| D-pad | Navigate |
| L1 / R1 | Change catalog filter |
| Cross | Open game details |
| Circle | Go back |
| Square | Download selected package |
| Triangle | Refresh remote catalog |
| Options | Exit |

## Build with OpenOrbis

1. Install the OpenOrbis PS4 Toolchain and set `OO_PS4_TOOLCHAIN`.
2. Run `make bootstrap` once. This copies the package runtime modules and `right.sprx` from the local OpenOrbis SDL2 sample. These binaries are intentionally not committed.
3. Run `make`. The package icon is generated from source automatically.
4. Install the generated PKG using the authorized installation method available on your test environment.

```sh
export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis-PS4-Toolchain
make bootstrap
make
```

The generated package is named `IV0000-PSFC00001_00-PSFORCERCLIENT00.pkg`.

## Configure a remote catalog

Create this text file on the console:

```text
/data/psforcer/manifest_url.txt
```

Put a single HTTPS URL in it. A bundled empty template is also available at `assets/manifest_url.txt`. Press Triangle in PSForcer to download the manifest to `/data/psforcer/catalog.json` and reload the catalog.

Hugging Face links will be added later. Prefer a stable `resolve/main/catalog.json` URL. Package objects can independently point to base game, update, DLC and extra PKGs.

See [docs/MANIFEST.md](docs/MANIFEST.md) for the schema.

## Host-side validation

The catalog parser and SHA-256 implementation are platform-independent:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
python3 tools/validate_catalog.py assets/catalog.json
```

## Project status

This is the first functional storefront milestone. Remote cover caching and video playback are represented in the data model but are not yet connected to a media player. The package downloader and verification path are implemented; installation remains behind the explicit `Installer` interface.
