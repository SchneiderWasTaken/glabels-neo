# gLabels Neo

**A modernized fork of [gLabels](https://github.com/j-evins/glabels-qt) by Jaye Evins and contributors.**

[![Latest release](https://img.shields.io/github/v/release/SchneiderWasTaken/glabels-neo?label=download)](https://github.com/SchneiderWasTaken/glabels-neo/releases/latest)
[![License: GPL v3+](https://img.shields.io/badge/license-GPL--3.0--or--later-blue)](LICENSE)

![gLabels Neo](glabels/images/glabels-label-designer.png)

gLabels Neo is a Qt6 label designer for creating and printing address labels,
product labels, shipping labels, business cards and barcodes - with support for
thermal printers, batch filling from CSV/Excel, and ready-to-run builds for
Windows, macOS and Linux.

> gLabels Neo is an independent fork. It is **not** the official gLabels project
> and is not endorsed by its author.  All credit for the original application
> goes to [Jaye Evins and the gLabels contributors](CREDITS.md).  If you want
> the upstream project, see [j-evins/glabels-qt](https://github.com/j-evins/glabels-qt).


## Download

Get the latest build for your system from the
**[Releases page](https://github.com/SchneiderWasTaken/glabels-neo/releases/latest)**.

| Platform | Package |
|:---------|:--------|
| Windows (x64) | `glabels-neo-windows-x64.zip` - unzip and run, no install |
| macOS (Apple Silicon) | `glabels-neo-macos-arm64.dmg` |
| Linux (x86_64) | `glabels-neo-linux-x86_64.tar.gz` |

The newest build of `main` is published continuously as the
[edge prerelease](https://github.com/SchneiderWasTaken/glabels-neo/releases/tag/edge).
On Windows, the [auto-updating Start menu launcher](tools/windows-launcher/README.md) keeps you on it
automatically: it checks GitHub each time you open gLabels Neo and updates first when needed.

From the next release on, the program also updates itself in the background (in any folder you can write to), and
swaps the new version in when you close it, so the next start is already current
(Help > Check for Updates, or Preferences > Updates; see [Automatic updates](docs/AUTO-UPDATE.md)).

> Installers, AppImage/Flatpak, package-manager support (winget, Homebrew, Scoop)
> and automatic release builds on every merge are in progress - see the
> [roadmap](docs/ROADMAP.md).


## What gLabels Neo adds

Everything in gLabels is preserved.  Neo adds:

- **Fill page** (Ctrl+5) - a batch table where each row is one print job with a
  quantity, a Print checkbox and one column per `${FieldName}` in your label,
  with a live sheet preview and CSV/Excel import/export.
- **Deploy tab** - build a standalone, single-file kiosk executable from the
  designer: the label layout is embedded, selected fields are locked, and the
  result runs on any Windows machine with no install.
- **Kiosk mode** (`glabels-fill`) - a stripped-down app with only the Fill
  table, preview and print.  An admin PIN allows reconfiguration.
- **ZPL thermal printing** - a built-in ZPL renderer for Zebra/ZDesigner printers
  that sends raw ZPL over USB or network, bypassing the OS driver.
- **Custom and thermal/roll label templates** via the Template Designer wizard.
- **Per-column locking** - pre-fill locked columns from a data source while
  operators enter only the per-day values.

See [docs/FILL-AND-DEPLOY.md](docs/FILL-AND-DEPLOY.md) for details, and
[docs/CHANGES-FROM-UPSTREAM.md](docs/CHANGES-FROM-UPSTREAM.md) for exactly what
differs from upstream gLabels.


## Building from source

Requirements: a C++20 compiler, CMake, Ninja and **Qt 6.7 or newer** (6.8+
recommended - QXlsx needs `Qt6::GuiPrivate`).  Optional: libzint, libqrencode,
GNU barcode, QXlsx (fetched automatically when not installed).

```bash
git clone https://github.com/SchneiderWasTaken/glabels-neo.git
cd glabels-neo
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Platform guides inherited from upstream:
[Linux](docs/BUILD-INSTRUCTIONS-LINUX.md),
[macOS](docs/BUILD-INSTRUCTIONS-MACOS.md),
[Windows](docs/BUILD-INSTRUCTIONS-WINDOWS.md).
The single-file kiosk (SFX) build is described in
[docs/FILL-AND-DEPLOY.md](docs/FILL-AND-DEPLOY.md).


## License

gLabels Neo is free software licensed under the **GNU General Public License,
version 3 or (at your option) any later version**, inherited from gLabels.
You may use, modify and redistribute it provided derivative works remain under
the same license and source is made available.  See [LICENSE](LICENSE).

- Original work (c) Jaye Evins and gLabels contributors - see [CREDITS.md](CREDITS.md).
- Bundled and linked third-party components are listed in
  [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) (in preparation).
- Product names such as Avery, Dymo, Brother, Herma and Zweckform appear in label
  templates only to describe compatible label stock; no affiliation or endorsement
  is implied.


## Contributing

Pull requests are welcome - see [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) and
[docs/CODING-STYLE.md](docs/CODING-STYLE.md).  Improvements that are generally
useful to gLabels are also offered upstream where practical.
