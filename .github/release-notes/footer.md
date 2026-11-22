## Which file do I need?

| System | Download | How to run |
|:--|:--|:--|
| Windows 10/11 (64-bit) | `glabels-neo-windows-x64.zip` | Unzip anywhere, then run `bin\glabels-qt.exe`. No install needed. |
| macOS (Apple Silicon) | `glabels-neo-macos-arm64.dmg` | Open the DMG and drag **glabels-qt** to Applications. The app is not yet notarized: on first launch, right-click it and choose **Open**. |
| Linux (x86_64) | `glabels-neo-linux-x86_64.tar.gz` | Extract, then run `./glabels-neo/glabels-neo`. |

Verify a download with `SHA256SUMS`. Build provenance attestations are published with this release (`gh attestation verify <file> --repo @REPO_URL@`).

## License and corresponding source

gLabels Neo is free software, licensed under the **GNU General Public License, version 3 or (at your option) any later version**. It is a modified version of [gLabels](https://github.com/j-evins/glabels-qt) by Jaye Evins and contributors; see `CREDITS.md` and `docs/CHANGES-FROM-UPSTREAM.md`. The bundled `glbarcode++` library is under the LGPL-3.0-or-later and the label template database under the MIT license. Every package contains `LICENSE`, `THIRD-PARTY-NOTICES.md` and a `licenses/` folder with the license texts of everything it bundles.

The complete corresponding source code for this release, including the build and packaging scripts, is available at no charge from the exact tag or commit it was built from:

- Source: @REPO_URL@/tree/@TAG@
- Source archive: @REPO_URL@/archive/@TAG@.tar.gz

Dynamically linked builds use Qt 6 under the LGPL-3.0; you may replace the bundled Qt shared libraries with a compatible modified version.
