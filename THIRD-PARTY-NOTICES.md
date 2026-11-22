# Third-Party Software Notices and Licenses

**gLabels Neo** is a fork of **gLabels-qt** (originally by Jaye Evins,
<https://github.com/j-evins/glabels-qt>) and is licensed under the **GNU General
Public License v3.0 or later (`GPL-3.0-or-later`)**. See [`LICENSE`](LICENSE)
for the full text of the GPLv3.

This file lists the third-party software components, libraries, and data assets
included in the gLabels Neo source repository and/or bundled into its binary
releases (Windows, macOS, Linux, and the standalone `glabels-fill-sfx` kiosk).
Full license texts are provided in the accompanying `licenses/` directory.

> **Maintainer note:** This file is being finalized together with the release pipeline (MVP 1). Placeholders formatted as `<VERSION>` are replaced
> automatically by the release build pipeline from the build manifest (e.g., Qt
> patch version, vcpkg/Homebrew/MSYS2/apt package versions).

---

## 1. Bundled In-Repository Components

### gLabels-qt (Upstream Application)
- **Version:** gLabels Neo is based on `j-evins/glabels-qt` `master` at commit `37901c1` (2026-09-29); see `docs/CHANGES-FROM-UPSTREAM.md`
- **SPDX License:** `GPL-3.0-or-later`
- **Project URL:** <https://github.com/j-evins/glabels-qt>
- **Copyright:** Copyright (C) 2013–2026 Jaye Evins and contributors (see `CREDITS.md`)
- **License Text:** `LICENSE` (and `licenses/GPL-3.0.txt`)

### glbarcode++ (Bundled Barcode Library)
- **Version:** Bundled (`glbarcode/`)
- **SPDX License:** `LGPL-3.0-or-later`
- **Project URL:** <https://github.com/j-evins/glbarcode>
- **Copyright:** Copyright (C) 2013–2026 Jaye Evins
- **Notice:** This software uses the `glbarcode++` library, which is licensed
  under the GNU Lesser General Public License v3.0 or later.
- **License Text:** `glbarcode/LICENSE` (and `licenses/LGPL-3.0.txt`, `licenses/GPL-3.0.txt`)

### gLabels Product Template Database
- **Version:** Bundled (`templates/`)
- **SPDX License:** `MIT`
- **Project URL:** <https://github.com/j-evins/glabels-qt>
- **Copyright:** Copyright (C) 2001–2026 Jaye Evins (and template contributors listed in `CREDITS.md`)
- **License Text:** `templates/LICENSE` (and `licenses/templates-MIT.txt`)

### AppStream Metadata
- **Version:** Bundled (`data/org.glabels.glabels-qt.metainfo.xml`)
- **SPDX License:** `CC0-1.0`
- **Project URL:** <https://github.com/j-evins/glabels-qt>
- **License Text:** `licenses/CC0-1.0.txt`

---

## 2. Core Application & Runtime Dependencies

### Qt 6 (QtBase, QtSvg, QtWaylandClient, QtTranslations)
- **Version:** `<QT_VERSION>` (e.g., `6.8.x`)
- **SPDX License:** `LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only`
  *(Dynamic releases use Qt under `LGPL-3.0-only`; static `glabels-fill-sfx`
  kiosk releases use Qt under `GPL-3.0-only` / `LGPL-3.0-only` with full
  Corresponding Source provided.)*
- **Project URL:** <https://www.qt.io/> / <https://code.qt.io/>
- **Copyright:** Copyright (C) The Qt Company Ltd. and other contributors.
- **Notice:** This application uses the Qt framework. Users of dynamically
  linked builds may replace the bundled Qt shared libraries with a compatible
  modified version in accordance with LGPLv3 §4(d)(1). Qt itself incorporates
  third-party open-source components whose notices and SPDX attributions are
  included under `licenses/qt/`.
- **License Text:** `licenses/LGPL-3.0.txt`, `licenses/GPL-3.0.txt`, and `licenses/qt/`

### QXlsx
- **Version:** `v1.5.1.1` *(or `<QXLSX_VERSION>` if built against system package; omitted in `glabels-fill-sfx`)*
- **SPDX License:** `MIT`
- **Project URL:** <https://github.com/QtExcel/QXlsx>
- **Copyright:** Copyright (c) 2013–2015 Debao Zhang; Copyright (c) 2017–2025 QXlsx contributors
- **License Text:** `licenses/QXlsx-MIT.txt`

### Zint (`libzint` Barcode Encoding Backend)
- **Version:** `<ZINT_VERSION>` (e.g., `2.15.0`)
- **SPDX License:** `BSD-3-Clause` *(applies to the `libzint` backend library linked by gLabels Neo)*
- **Project URL:** <https://www.zint.org.uk/> / <https://github.com/zint/zint>
- **Copyright:** Copyright (C) 2006–2025 Robin Stuart and Zint contributors
- **License Text:** `licenses/libzint-BSD-3-Clause.txt`

### libqrencode
- **Version:** `<QRENCODE_VERSION>` (e.g., `4.1.1`)
- **SPDX License:** `LGPL-2.1-or-later`
- **Project URL:** <https://fukuchi.org/works/qrencode/> / <https://github.com/fukuchi/libqrencode>
- **Copyright:** Copyright (C) 2006–2020 Kentaro Fukuchi
- **Notice:** This application uses `libqrencode`, licensed under the GNU Lesser
  General Public License v2.1 or later.
- **License Text:** `licenses/LGPL-2.1.txt`

### GNU Barcode *(Optional — included only in builds where `HAVE_GNU_BARCODE=1`)*
- **Version:** `<GNUBARCODE_VERSION>` *(omitted in default GitHub release builds)*
- **SPDX License:** `GPL-3.0-or-later` (v0.99+) / `GPL-2.0-or-later` (v0.98)
- **Project URL:** <https://www.gnu.org/software/barcode/>
- **Copyright:** Copyright (C) Free Software Foundation, Inc. / Alessandro Rubini and contributors
- **License Text:** `licenses/GPL-3.0.txt`

### zlib
- **Version:** `<ZLIB_VERSION>`
- **SPDX License:** `Zlib`
- **Project URL:** <https://zlib.net/>
- **Copyright:** Copyright (C) 1995–2024 Jean-loup Gailly and Mark Adler
- **License Text:** `licenses/zlib.txt`

### ICU — International Components for Unicode *(Bundled on platforms where linked by Qt)*
- **Version:** `<ICU_VERSION>`
- **SPDX License:** `Unicode-3.0` *(verify exact version if older than ICU 72)*
- **Project URL:** <https://icu.unicode.org/> / <https://github.com/unicode-org/icu>
- **Copyright:** Copyright (c) 1991–2025 Unicode, Inc. All rights reserved.
- **License Text:** `licenses/ICU-Unicode-3.0.txt`

### OpenSSL *(Included ONLY if bundled with TLS plugin; must be OpenSSL 3.0+)*
- **Version:** `<OPENSSL_VERSION>` *(omitted when using OS native TLS backends)*
- **SPDX License:** `Apache-2.0` *(OpenSSL 3.x only — do NOT bundle OpenSSL 1.1.x)*
- **Project URL:** <https://www.openssl.org/>
- **Copyright:** Copyright (c) The OpenSSL Project Authors
- **License Text:** `licenses/Apache-2.0.txt`

---

## 3. Compiler Runtimes & Static-Build (`glabels-fill-sfx`) Dependencies

The following libraries are bundled in MinGW-w64 builds and/or statically linked
into the standalone Windows kiosk executable (`glabels-fill-sfx.exe`), as well
as transitively used by Qt image/font/text codecs:

| Component | Version | SPDX License | Project URL | License File |
| :--- | :--- | :--- | :--- | :--- |
| **GCC Runtime (`libgcc`, `libstdc++`)** | `<GCC_VERSION>` | `GPL-3.0-or-later WITH GCC-exception-3.1` | <https://gcc.gnu.org/> | `licenses/GPL-3.0.txt`, `licenses/GCC-exception-3.1.txt` |
| **MinGW-w64 `winpthreads`** | `<MINGW_VERSION>` | `MIT AND BSD-3-Clause` | <https://www.mingw-w64.org/> | `licenses/winpthreads.txt` |
| **FreeType** | `<FREETYPE_VERSION>` | `FTL OR GPL-2.0-or-later` | <https://freetype.org/> | `licenses/FreeType-FTL.txt` *(Portions of this software are copyright © The FreeType Project (www.freetype.org). All rights reserved.)* |
| **HarfBuzz** | `<HARFBUZZ_VERSION>` | `MIT` | <https://harfbuzz.github.io/> | `licenses/HarfBuzz-MIT.txt` |
| **SIL Graphite2** | `<GRAPHITE2_VERSION>` | `MIT OR MPL-2.0 OR LGPL-2.1-or-later OR GPL-2.0-or-later` | <https://github.com/silnrsi/graphite> | `licenses/Graphite2-MIT.txt` |
| **libpng** | `<LIBPNG_VERSION>` | `libpng-2.0` | <http://www.libpng.org/pub/png/libpng.html> | `licenses/libpng-2.0.txt` |
| **libjpeg-turbo** | `<LIBJPEG_TURBO_VERSION>` | `IJG AND BSD-3-Clause AND Zlib` | <https://libjpeg-turbo.org/> | `licenses/libjpeg-turbo.txt` *(This software is based in part on the work of the Independent JPEG Group.)* |
| **libtiff** | `<LIBTIFF_VERSION>` | `libtiff` | <https://libtiff.gitlab.io/libtiff/> | `licenses/libtiff.txt` |
| **libwebp** | `<LIBWEBP_VERSION>` | `BSD-3-Clause` | <https://chromium.googlesource.com/webm/libwebp> | `licenses/libwebp-BSD-3-Clause.txt` |
| **Zstandard (`zstd`)** | `<ZSTD_VERSION>` | `BSD-3-Clause OR GPL-2.0-only` | <https://github.com/facebook/zstd> | `licenses/zstd-BSD-3-Clause.txt` |
| **bzip2** | `<BZIP2_VERSION>` | `bzip2-1.0.6` | <https://sourceware.org/bzip2/> | `licenses/bzip2-1.0.6.txt` |
| **Brotli** | `<BROTLI_VERSION>` | `MIT` | <https://github.com/google/brotli> | `licenses/Brotli-MIT.txt` |
| **PCRE2** | `<PCRE2_VERSION>` | `BSD-3-Clause WITH PCRE2-exception` | <https://github.com/PCRE2Project/pcre2> | `licenses/PCRE2-BSD-3-Clause.txt` |
| **MD4C** | `<MD4C_VERSION>` | `MIT` | <https://github.com/mity/md4c> | `licenses/MD4C-MIT.txt` |
| **double-conversion** | `<DOUBLE_CONVERSION_VERSION>` | `BSD-3-Clause` | <https://github.com/google/double-conversion> | `licenses/double-conversion-BSD-3-Clause.txt` |
| **libb2** | `<LIBB2_VERSION>` | `CC0-1.0` | <https://github.com/BLAKE2/libb2> | `licenses/CC0-1.0.txt` |

*(Windows MSVC builds also include the Microsoft Visual C++ runtime DLLs, redistributed under Microsoft's redistribution terms for Visual Studio "Redistributable Code".)*

### Packaging Wrappers *(Applicable per Artifact Format)*
- **NSIS Installer Stub** *(Windows installer, if enabled)* — `Zlib` (<https://nsis.sourceforge.io/>), `licenses/NSIS-Zlib.txt`
- **AppImage Runtime** *(Linux AppImage, if enabled)* — `MIT` (<https://github.com/AppImage/type2-runtime>), `licenses/AppImage-MIT.txt`

---

## 4. How the Build Should Collect License Texts (`licenses/` Folder)

Each release packaging script (and CMake install target) should stage a
self-contained `licenses/` directory populated from the actual build inputs so
that bundled notices always match the shipped binaries:

1. **Repository license files (always copied):**
   - `LICENSE` → `licenses/GPL-3.0.txt` (and top-level `LICENSE`)
   - `glbarcode/LICENSE` → `licenses/LGPL-3.0.txt`
   - `templates/LICENSE` → `licenses/templates-MIT.txt`
   - `THIRD-PARTY-NOTICES.md` → top-level `THIRD-PARTY-NOTICES.md`
   - `CREDITS.md` → top-level `CREDITS.md`
2. **Fetched / packaged library licenses (collected by platform):**
   - **QXlsx:** Copy `${CMAKE_BINARY_DIR}/_deps/qxlsx-src/LICENSE` (or MSYS2 `/ucrt64/share/licenses/qxlsx/LICENSE`) → `licenses/QXlsx-MIT.txt`.
   - **Qt 6:** Copy `${QT_INSTALL_PREFIX}/LICENSES/*` and Qt 6.8's SPDX/attribution files (`qt_attribution.json` / `sbom/*.spdx`) → `licenses/qt/`.
   - **Windows (`vcpkg`):** Copy `${VCPKG_INSTALLED_DIR}/x64-windows/share/<pkg>/copyright` for every installed package (`zlib`, `libqrencode`, `zint`, `libpng`, etc.) → `licenses/<pkg>.txt`.
   - **Windows (`MSYS2` / static `glabels-fill-sfx`):** Copy `/ucrt64/share/licenses/<pkg>/*` for every statically linked package and SFX payload DLL → `licenses/<pkg>/`, **and** embed `LICENSE`, `THIRD-PARTY-NOTICES.md`, `templates/LICENSE`, and `licenses/*` into a Qt resource (`:/licenses/`) compiled into `glabels-fill.exe` (reachable from its About/Legal dialog).
   - **macOS (`Homebrew`):** Copy `$(brew --prefix <pkg>)/LICENSE*` or `COPYING*` for every dylib copied into `Contents/Frameworks/` → `glabels-qt.app/Contents/Resources/licenses/` and `glabels-fill.app/Contents/Resources/licenses/` (plus top-level `LICENSE` & `THIRD-PARTY-NOTICES.md` in the DMG root).
   - **Linux (`apt` + source `zint`):** Copy `/usr/share/doc/<pkg>/copyright` for any copied distro `.so` (e.g., `libqrencode4`), `zint-2.15.0-src/backend/LICENSE` → `licenses/libzint-BSD-3-Clause.txt`, and place everything under `share/doc/glabels-neo/licenses/` (with top-level `LICENSE` and `THIRD-PARTY-NOTICES.md` in the tarball/AppDir root).
3. **Artifact install paths:**
   - **Windows `.zip` / NSIS:** `<root>/LICENSE`, `<root>/THIRD-PARTY-NOTICES.md`, `<root>/licenses/...`
   - **macOS `.dmg`:** `<DMG root>/LICENSE`, `<DMG root>/THIRD-PARTY-NOTICES.md`, and inside both `glabels-qt.app/Contents/Resources/licenses/` and `glabels-fill.app/Contents/Resources/licenses/`
   - **Linux `.tar.gz` / AppImage:** `<root>/LICENSE`, `<root>/THIRD-PARTY-NOTICES.md`, `<root>/share/doc/glabels-neo/licenses/...`
   - **Windows `glabels-fill-sfx.exe`:** Embedded Qt resource `:/licenses/...` viewable in-app + extracted alongside runtime or documented in release notes.

---

## 5. Release Notes Source-Availability Statement (GPLv3 §6 / LGPLv3 §4)

This is the template that the release workflow fills in for every GitHub Release description (replacing `<TAG>`, `<REPO_URL>`, `<QT_VERSION>`, and `<SOURCE_ARCHIVE_URL>`):

```markdown
### License & Corresponding Source Code

gLabels Neo is free software licensed under the **GNU General Public License v3.0
or later (GPL-3.0-or-later)** (with bundled `glbarcode++` under
**LGPL-3.0-or-later** and the label template database under the **MIT License**).
Full license texts and third-party notices for all bundled libraries (including
Qt, QXlsx, libzint, libqrencode, zlib, and ICU) are included in each release
archive under `LICENSE`, `THIRD-PARTY-NOTICES.md`, and `licenses/`.

In accordance with **Section 6(d) of the GNU General Public License v3** (and
**Section 4(d) of the GNU Lesser General Public License v3**), the complete
machine-readable **Corresponding Source** for this release — including all
gLabels Neo source code, build scripts, and packaging scripts — is available at
no charge from the same release page and git tag:

- **gLabels Neo source tag:** <REPO_URL>/tree/<TAG>
- **Source archive:** <REPO_URL>/archive/refs/tags/<TAG>.tar.gz
- **Exact third-party dependency sources & build manifest:** Included in the
  attached `glabels-neo-<TAG>-corresponding-source-manifest.json` (and bundled
  source archives for statically linked / LGPL dependencies, including Qt
  `<QT_VERSION>`, QXlsx `v1.5.1.1`, Zint `<ZINT_VERSION>`, and libqrencode
  `<QRENCODE_VERSION>`).

Dynamically linked desktop releases use Qt 6 under the LGPL-3.0-only license;
users may replace the bundled Qt shared libraries with interface-compatible
modified versions. Standalone static kiosk builds (`glabels-fill-sfx.exe`) are
conveyed under GPL-3.0-only / GPL-3.0-or-later with complete Corresponding
Source and build scripts available at the links above.
```
