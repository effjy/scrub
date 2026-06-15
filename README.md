<div align="center">

<a href="https://github.com/effjy/scrub/"><img src="titles/scrub-title.svg" height="44" alt="Scrub"></a>

**Forensic-grade metadata scrubber for Linux.**

[![C](https://img.shields.io/badge/Language-C-teal?style=flat-square&labelColor=1a1a1a)](#)
[![Platform](https://img.shields.io/badge/Platform-Linux-8a2be2?style=flat-square&labelColor=1a1a1a)](#)
[![License](https://img.shields.io/badge/License-MIT-teal?style=flat-square&labelColor=1a1a1a)](LICENSE)
[![Dependencies](https://img.shields.io/badge/Dependencies-none-teal?style=flat-square&labelColor=1a1a1a)](#)
[![Formats](https://img.shields.io/badge/Formats-JPEG%20%C2%B7%20PNG-8a2be2?style=flat-square&labelColor=1a1a1a)](#supported-formats)
[![Standard](https://img.shields.io/badge/C-C11-teal?style=flat-square&labelColor=1a1a1a)](#)

<br>

`scrub` strips identifying metadata out of your files **before you share them** —
the defensive complement to a forensic extractor. It removes Exif/XMP/IPTC blocks
from images, drops textual and timestamp chunks, and can reset filesystem
timestamps. It parses the formats natively in C, so there are **no runtime
dependencies** — no libexif, no ImageMagick.

<br>

<img src="assets/demo.svg" width="700" alt="scrub stripping Exif from an image: GPS, camera Make and Software metadata are gone after a single command">

<sub><i>One command strips the camera make, editing software and GPS coordinates — the file still opens, but there's nothing left to leak.</i></sub>

</div>

---

## Supported formats

| Format | Stripped |
|:---|:---|
| **JPEG** | `APP1` (Exif/XMP), `APP2`–`APP15` (ICC/Photoshop/vendor), `APP13` (IPTC), `COM` comments |
| **PNG**  | `tEXt`, `zTXt`, `iTXt` (text), `tIME` (timestamp), `eXIf` (embedded Exif) |

Image pixels are never re-encoded — only metadata segments are removed, so the
image itself is byte-for-byte preserved.

## Build

```sh
make
sudo make install      # installs to /usr/local/bin
```

## Usage

```sh
scrub photo.jpg                  # scrub one file in place
scrub *.png                      # scrub many
scrub -t secret.jpg              # also reset atime/mtime to the epoch
```

```
✓ photo.jpg — removed 3 segments, now 184204 bytes
```

Options:

| Flag | Effect |
|:---|:---|
| `-t`, `--reset-times` | Reset file atime/mtime to the Unix epoch |
| `-h`, `--help` | Show help |
| `-V`, `--version` | Show version |

Scrubbing is done via a temp file and an atomic `rename(2)`, so an interrupted
run never corrupts the original. Original permissions are preserved.

## Roadmap

- [ ] PDF metadata (Info dictionary, XMP)
- [ ] Office / ODF document properties
- [ ] GTK3 drag-and-drop front-end
- [ ] Recursive directory mode

## License

MIT — see [LICENSE](LICENSE).
