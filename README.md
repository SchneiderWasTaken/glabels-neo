>[!IMPORTANT]
>This is a fork of [glabels-qt](https://github.com/j-evins/glabels-qt) by Jaye Evins, extending it with an interactive Fill page, a Deploy tab for building standalone kiosk deployments, and CSV/Excel data-source support.

![gLabels-neo](glabels/images/glabels-label-designer.png)

*******************************************************************************

## What is gLabels-neo?

gLabels-neo is a fork of gLabels-qt — a Qt6-based label designer that lets you
design and print labels, business cards, and barcodes.  It adds three major
features on top of the original:

- **Fill page** — an interactive batch-fill table with live preview, CSV/Excel
  import/export, and a "Print/No Print" checkbox column for staging rows.
- **Deploy tab** — build a standalone, single-file kiosk executable from the
  designer: the current label layout is embedded, selected fields are locked,
  and the result is a self-extracting `.exe` that runs on any Windows machine
  with no install.
- **Data-source model** — pre-fill locked columns from an Excel/CSV file while
  the employee enters only the per-day values (e.g. names from a spreadsheet,
  price typed in at the terminal).


## What's new in gLabels-neo (vs. glabels-qt)

- **Ready-to-run builds for macOS, Windows and Linux** — see
  [Releases](https://github.com/SchneiderWasTaken/glabels-neo/releases):
  a signed-free `.dmg` for Apple Silicon, a portable `.zip` for Windows
  (no install, no DLL hunting), and a self-contained `.tar.gz` for Linux.
- **Fill page** (Ctrl+5): a batch table where each row is one print job with
  a Qty, a Print checkbox, and one column per `${FieldName}` in your label.
  Live sheet preview reflects the checked rows.  CSV/Excel round-trip.
- **Deploy tab**: configure branding, admin PIN, locked printer, data source,
  and locked fields, then click "Build kiosk…" to produce a single `.exe`.
- **Kiosk mode** (`glabels-fill`): a stripped-down app with only the Fill
  table + preview + print — no editor, no template designer.  The label is
  embedded in the executable; an admin PIN (Ctrl+Shift+A) allows reconfiguration.
- **Single-file SFX**: the kiosk ships as one ~44 MB `.exe` (static Qt,
  self-extracting to `%TEMP%`).  No install, no DLLs, no external files.
- **Thermal printer support**: built-in ZPL renderer for Zebra/ZDesigner
  printers (GK420, ZD4, ZT series, etc.).  Auto-detects the printer, renders
  at native DPI, sends raw ZPL via USB or network TCP.  Bypasses the Windows
  driver entirely for reliable label output.
- **Custom label templates**: create custom-sized labels for thermal printers
  via the Template Designer wizard ("New Thermal / Roll Label" path).  Edit
  and tag custom templates in the product picker.
- **CSV/Excel import/export** in the Fill table.
- **Per-column locking**: mark fields as read-only (pre-filled from source)
  vs. employee-entered.
- **Portable build**: the full designer also ships as a no-install portable
  folder.

All features from the original glabels-qt are preserved.


## Download

Pre-built Windows binaries are available on the [Releases](../../releases) page:

| Package | Description |
|:--------|:------------|
| `glabels-neo-portable.zip` | Full designer, no-install portable folder (~35 MB) |
| `glabels-neo-kiosk.exe` | A sample kiosk build (configure via the Deploy tab) |

To build a kiosk for your own label, download the designer, open your label,
go to the Deploy tab, and click "Build kiosk…".

## Build Instructions

### Windows (MSYS2 / MinGW)

Prerequisites: [MSYS2](https://www.msys2.org/)

From an MSYS2 UCRT64 shell:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain cmake ninja \
  mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-svg \
  mingw-w64-ucrt-x86_64-qt6-tools mingw-w64-ucrt-x86_64-qt6-declarative \
  mingw-w64-ucrt-x86_64-qrencode mingw-w64-ucrt-x86_64-zint \
  mingw-w64-ucrt-x86_64-qxlsx
```

```bash
git clone https://github.com/<your-org>/glabels-neo.git
cd glabels-neo
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The designer is at `build/glabels/glabels-qt.exe`; the kiosk at
`build/glabels-fill/glabels-fill.exe`.

### Building the single-file kiosk (SFX)

Requires the static Qt package:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-qt6-static \
  mingw-w64-ucrt-x86_64-libwebp mingw-w64-ucrt-x86_64-libtiff
```

```bash
cmake -B build-static -G Ninja \
  -DCMAKE_PREFIX_PATH="C:/msys64/ucrt64/qt6-static" \
  -DQt6_DIR="C:/msys64/ucrt64/qt6-static/lib/cmake/Qt6" \
  -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
  -DGLABELS_FILL_STATIC=ON -DGLABELS_USE_XLSX=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-static --target glabels-fill -j
```

Then assemble the SFX using the PowerShell script `scripts/build-sfx.ps1`.

See [docs/FILL-AND-DEPLOY.md](docs/FILL-AND-DEPLOY.md) for full details.


## License

gLabels-neo is free software licensed under the **GNU General Public License
v3** (inherited from glabels-qt).  You are free to use, modify, and
redistribute it, provided derivative works are also GPLv3 and source is
available.

This fork is based on [glabels-qt](https://github.com/j-evins/glabels-qt) by
Jaye Evins.  See [CREDITS.md](CREDITS.md) for the full list of contributors.

Bundled third-party components:

| Component | License |
|:----------|:--------|
| Qt6 | LGPL v3 (open-source build) |
| glbarcode (bundled) | LGPL v3 |
| QXlsx (Excel I/O) | MIT |
| libzint (barcodes) | BSD-3-Clause |
| libqrencode | LGPL v2.1 |
| zlib | zlib license |
| Template database | MIT/X |


## Contributing

Pull requests welcome.  See [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) and
[docs/CODING-STYLE.md](docs/CODING-STYLE.md).
