# Changes from upstream gLabels

This file is the prominent modification notice required by section 5(a) of the
GNU General Public License v3 for a modified version of a covered work.

**gLabels Neo is a modified version of gLabels-qt**
(<https://github.com/j-evins/glabels-qt>, (c) Jaye Evins and contributors,
GPL-3.0-or-later).

| | |
|---|---|
| Fork base | upstream `master` at commit `37901c1` (2026-09-29) |
| Fork rebased onto it | 2026-11-22 |
| Modifications since | 2026-07-08 (first fork commit) |

The upstream project history is preserved in this repository; `git log
upstream/master..HEAD` lists exactly what this fork adds
(`git remote add upstream https://github.com/j-evins/glabels-qt.git`).

## Added

| Area | Files |
|---|---|
| Fill page (batch-fill table, live preview, CSV/Excel I/O) | `glabels/FillView.*`, `glabels/FillJobModel.*`, `glabels/FillMerge.*`, `glabels/FillConfig.*`, `glabels/ui/FillView.ui` |
| Deploy tab (standalone kiosk builder) | `glabels/DeployView.*` |
| Kiosk application and self-extracting single-file build | `glabels-fill/` |
| ZPL thermal printing | `glabels/ZplRenderer.*` |
| Thermal/roll template designer page | `glabels/TemplateDesigner.*`, `glabels/ui/TemplateDesignerThermalPage.ui` |
| Ribbon interface (tabs, File backstage, contextual tools, Content panel) | `glabels/ribbon/`, `glabels/MainWindow.*`, `glabels/ObjectEditor.*` |
| Theme layer: six colour schemes in light and dark, theme picker, monoline icon set | `glabels/theme/`, `glabels/icons/neo/`, `glabels/icons/neo-art/`, `glabels/AppearanceModel.*` |
| Snapping and guides in the label editor | `glabels/SnapEngine.*`, `glabels/LabelEditor.*` |
| Template Designer step sidebar | `glabels/TemplateDesignerSteps.*`, `glabels/TemplateDesignerSidebar.*` |
| Brand kit, README and website pictures, sample projects | `brand/`, `docs/assets/`, `scripts/make-marketing-assets.py` |
| Build, CI and packaging | `.github/workflows/`, `scripts/package-*`, `cmake/MacDeployFixup.cmake`, `run.ps1` |
| Documentation | `docs/FILL-AND-DEPLOY.md`, `docs/CHANGES-FROM-UPSTREAM.md` |

## Modified

`CMakeLists.txt`, `glabels/CMakeLists.txt`, `glabels/MainWindow.*`,
`glabels/PrintView.cpp`, `glabels/SelectProductDialog.*`,
`glabels/TemplatePicker*.cpp`, `model/PageRenderer.cpp`, `model/FileUtil.cpp`,
`README.md`, `CREDITS.md`, `.gitignore`, `translations/glabels_C.ts`.

## Unchanged by policy

To keep merging upstream cheap and to remain a good citizen of the ecosystem,
the following are intentionally **not** renamed: the `.glabels` file format and
MIME type, binary and CMake target names, namespaces, and translation catalog
names.  Display branding ("gLabels Neo") is applied only at user-visible
points, always together with attribution to gLabels.
