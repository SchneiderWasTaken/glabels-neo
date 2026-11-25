# gLabels Neo ribbon UI: implementation spec

Status: approved direction "A · Neo Ribbon". This file is the contract. The mockups in
[`mockups/`](mockups/) are the visual source of truth; this file is the behavioural and
dimensional source of truth. If the two disagree, the mockup wins on look, this file wins on
behaviour. Do not improvise: if something is not specified, keep today's behaviour and
write the question in `docs/ribbon-ui/QUESTIONS.md`.

Interactive canvas (private): https://claude.ai/artifact/QKdyGEDk4ZxqAdRKTUUe9D (page "A · Full spec").
Each mockup file below is named the same on the canvas (S01 to S20, D01 to D11).

---

## 0. Ground rules

1. **No behaviour change.** Every feature of today's app stays. This is a re-skin plus a
   re-organisation of where commands live. Model, undo/redo, file formats, printing, merge,
   Fill, Deploy, updates, kiosk: untouched.
2. **Keep every `QAction`.** The ribbon builds its buttons from the existing actions in
   `MainWindow` (`fileNewAction`, `editUndoAction`, `objectsCreateTextAction`, ...). Same
   shortcuts, same enabled/checked logic, same status tips. Do not duplicate logic.
3. **Qt 6 Widgets, C++17**, style per `docs/CODING-STYLE.md` (match the surrounding files:
   `///` doc comments, 8-space namespace indent, `tr()` on every user string).
4. **Brand and copy rules** (from the gLabels Neo design system):
   - Product name is always `gLabels Neo`. Sentence case for every label, menu and button
     ("Print labels", "Save as..."). No emoji, no exclamation marks.
   - The mark is `brand/mark.svg`. Never redraw it. Keep the attribution
     "A modernized fork of gLabels" on Welcome and About.
5. **No new dependencies** except icon artwork (section 4.4). No network access added.
6. **Do not push, do not open PRs.** Commit locally only, in small commits on your branch.
7. **No local C++ toolchain exists on this machine by default.** See section 13 for how builds
   are verified. Never claim "builds" or "tests pass" without a real build log.
8. **Translations:** every new string goes through `tr()`. Do not hand-edit `translations/*.ts`;
   a build regenerates `translations/*_C.ts`, which must be reverted, not committed.

## 1. Scope and phases

| Phase | Contents |
|---|---|
| **P0 (this branch)** | Theme + tokens, ribbon framework, File backstage, Welcome, all ribbon tabs, contextual tabs, Content panel, status bar, restyled pages (Label, Merge, Variables, Fill, Print, Deploy), light/dark/system theming, QSS restyle of every dialog, kiosk restyle. |
| P1 | Dialog re-layouts that differ from today's structure (Select product three-column, Template designer step list, Preferences single page). In P0 these dialogs keep today's layout and only take the new theme. |
| P2 | "Search commands" box, Alt key tips, ribbon customisation, custom (frameless) title bar. |

Everything in the mockups is P0 unless this file says otherwise. Items marked **(P1)** or
**(P2)** are drawn in the mockups for direction only.

## 2. Mockup index

| File | What it specifies |
|---|---|
| `s01-welcome` | No project open: only File and Help tabs, ribbon hidden |
| `s02-file-backstage` | File backstage (replaces the menu bar) |
| `s03-home` | Home tab, nothing selected |
| `s04-text-tools` / `s18-text-tools-dark` | Contextual Text tab |
| `s05-barcode-tools` | Contextual Barcode tab |
| `s06-image-tools` | Contextual Image tab |
| `s07-shape-tools` | Contextual Shape tab (box and ellipse) |
| `s08-line-tools` | Contextual Line tab |
| `s09-label-tab` | Label tab + page (today's Properties) |
| `s10-data-merge` / `s11-data-variables` / `s12-data-fill` / `s19-data-fill-dark` | Data tab and its three pages |
| `s13-print` / `s20-print-dark` | Print tab + page |
| `s14-deploy` | Deploy tab + page |
| `s15-view-tab`, `s16-help-tab` | View and Help ribbons |
| `s17-home-dark` | Dark theme reference |
| `d01`..`d11` | Dialogs, popovers, menus, message boxes, kiosk |

## 3. Architecture

### 3.1 Today to new

| Today (`glabels/MainWindow.cpp`) | New |
|---|---|
| Menu bar (File, Edit, View, Objects, Help) | **Removed.** File menu becomes the **File backstage**; every other action lives on a ribbon tab. The canvas **context menu stays** (it is built from the same actions). |
| "Quick Access" toolbar (New, Open, Save) | **Quick Access bar** at the left of the tab strip: New, Open, Save, Undo, Redo. Hideable (View > Show). |
| Left page rail `mContents` (Welcome, Edit, Properties, Merge, Variables, Print, Fill, Deploy) | **Ribbon tabs** (see 3.2). `mPages` (`QStackedWidget`) stays as the page container. |
| Editor toolbar (`editorToolBar`) | **Removed.** Its actions are on Home. |
| `ObjectEditor` dock on the right | **Contextual tabs** (formatting, geometry, style) + **Content panel** (text, barcode data, image file, merge fields). |
| `statusBar()` | **Status bar** (section 7). |
| Native title bar | **Unchanged in P0** (native). The mockup's custom caption is just a drawing of the native one. |

### 3.2 Tabs and pages

| Ribbon tab | Page shown in `mPages` | Shortcut (unchanged) |
|---|---|---|
| (no document) | Welcome | |
| **Home** | Editor | Ctrl+1 |
| **Label** | Properties | Ctrl+2 |
| **Data** | Merge, Variables or Fill (a "Data page" switch inside the tab; remembers the last one) | Ctrl+3 Merge, Ctrl+4 Variables, Ctrl+5 Fill |
| **Print** | Print | Ctrl+P (`QKeySequence::Print`) |
| **Deploy** | Deploy | (none today) |
| **View** | stays on the current page | |
| **Help** | stays on the current page | F1 opens the manual |
| (contextual) **Text / Barcode / Image / Shape / Line** | Editor | |

The `fileShowEditorPageAction` ... `fileShowDeployPageAction` actions keep existing and now
select the matching ribbon tab (and page). Tab and page are always in sync.

**Rule that settles every "ribbon or page?" question:** the ribbon holds **commands and
compact toggles**; pages hold **forms, tables and previews**. A command appears once.

### 3.3 New code layout (suggested; keep file names unless there is a reason)

```
glabels/ribbon/            RibbonBar, RibbonTab, RibbonGroup, RibbonButton, RibbonDropButton,
                           RibbonField (labelled compact combo/spin/line edit), RibbonToggleGroup,
                           ContextualTabSet, QuickAccessBar, Backstage, ContentPanel, RibbonStatusBar
glabels/theme/             Theme (tokens), ThemeStyle (generated QSS), ThemeIcons (tinted SVG icons)
glabels/icons/neo/         monoline SVG icon set (section 4.4)
glabels/ui/                existing .ui files are edited, not duplicated
```

Add new sources to `glabels/CMakeLists.txt` and new resources to `glabels/icons.qrc` (or a new
`ribbon.qrc`). Existing unit-test layout: `glabels/unit_tests/`.

### 3.4 Moving object controls: the "widget transplant" (strongly preferred)

`ObjectEditor` already owns all the controls and all the binding logic (it loads values on
selection, writes changes through the undo checkpoints, handles the blocked/updating flags).
**Do not rewrite that logic.** Instead:

- Keep `ObjectEditor` as a non-visual controller (it stays created and wired to the model;
  it is simply never shown).
- Add `QWidget* ObjectEditor::control(ObjectEditor::ControlId)` that returns the existing
  child widgets (`textFontFamilyCombo`, `textFontSizeSpin`, `textFontBoldToggle`, `posXSpin`,
  `barcodeStyleButton`, `shadowOpacitySpin`, ...). The ribbon factory **re-parents** those
  widgets into its groups once, at startup. Because they are the same objects, every signal
  and slot keeps working.
- The content editors (`textEdit`, `barcodeDataEdit`, `imageFilenameLineEdit` + buttons) and
  `FieldButton`s are re-parented into the Content panel the same way.
- A group or tab is shown or hidden by selection state (section 6.2); individual controls
  keep today's enabled logic (for example `barcodeShowTextCheck` is disabled when
  `bcStyle.textOptional()` is false).

The same technique applies to `FillView`, `MergeView`, `VariablesView`, `PrintView`,
`PropertiesView`, `DeployView`: where a command button moves to the ribbon, expose it via an
accessor or an action and re-parent or bind it; do not fork the slot.

## 4. Design tokens

### 4.1 Colour

Implemented as a `Theme` with two palettes. The app already has `AppearanceModel` (Light, Dark,
System): extend it; do not add a second switch.

| Token | Light | Dark | Use |
|---|---|---|---|
| `bg` | `#F8FAFC` | `#090D1A` | Page background |
| `chrome` | `#F1F5F9` | `#0F172A` | Tab strip, status bar, dialog title/footer |
| `ribbon` | `#FFFFFF` | `#151E32` | Ribbon surface, active tab |
| `work` | `#E2E8F0` | `#090D1A` | Canvas backdrop, preview wells |
| `panel` | `#FFFFFF` | `#0F172A` | Cards, Content panel, dialogs |
| `line` | `#E2E8F0` | `#1E293B` | Hairlines, group separators |
| `line2` | `#CBD5E1` | `#334155` | Control borders |
| `ink` | `#0F172A` | `#F8FAFC` | Primary text |
| `ink2` | `#475569` | `#CBD5E1` | Secondary text, icons |
| `ink3` | `#64748B` | `#94A3B8` | Captions, group names |
| `accent` | `#4F46E5` | `#818CF8` | Active tab underline, checked toggles, focus, selection handles, links |
| `tint` | `#EDE9FE` | `#1E293B` | Field chips, icon chips |
| `hover` | `#F1F5F9` | `#1E293B` | Button hover |
| `pressed` | `#E0E7FF` | `#2B3556` | Pressed / checked button fill |
| `ctx` | `#EDE9FE` | `#2A2447` | Contextual tab background |
| `ctxLine` | `#7C3AED` | `#A78BFA` | Contextual tab top line |
| `ok` | `#047857` | `#34D399` | Success text (always with a word or check icon) |
| `okTint` | `#ECFDF5` | `#064E3B` | Success pill |
| `warn` | `#B45309` | `#FBBF24` | Warning text |
| `grad` | 135deg `#7C3AED` > `#4F46E5` > `#2563EB` | 135deg `#A78BFA` > `#818CF8` > `#60A5FA` | **Only** the File button and the primary ribbon button (Print). Never behind body text. |
| `onGrad` | `#FFFFFF` | `#090D1A` | Text on `grad` |

The **label paper is always white** (it is paper) in both themes. Selection handles and the
dashed selection box use `accent`.

Focus: 2 px solid `accent`, 2 px offset, on every focusable control.
Disabled: 40% opacity (text and icon), no hover.
Status is never colour alone: pair `ok`/`warn` with a word or icon.

### 4.2 Type

Font stack: `Inter, "Segoe UI", system-ui, sans-serif` (bundle Inter only if its license
allows; otherwise fall back to the system font and keep the sizes). Mono (field chips,
code): `JetBrains Mono, Consolas, monospace`.

| Style | Size / weight | Use |
|---|---|---|
| ribbon label | 12 px / 400 | Large-button labels, small-button labels |
| group name | 12 px / 400, `ink3` | Under each group |
| tab | 13 px / 400 (active 600) | Ribbon tabs |
| field text | 12 px | Ribbon combos, spins, line edits |
| body | 13 px | Panels, tables, dialogs |
| page title | 20 px / 700 | Page headings (Label, Merge, ...) |
| card title | 13 px / 600 | Card headers |
| caption | 12 px, `ink3` | Helper text |

### 4.3 Space, shape, size

| | px |
|---|---|
| Native title bar | OS |
| Tab strip height | 42 (tab 34, active tab 36 with a 3 px `accent` underline inset 12 px) |
| Ribbon height | **118** (6 top padding, 90 group body, 22 group name) |
| Large button | 58 x 78, icon 30, label 12 px up to two lines, radius 8 |
| Small button | height 26, icon 16, label 12 px, radius 6 |
| Icon-only button | 28 x 28, icon 16, radius 6 |
| Ribbon field (combo/spin/line edit) | height 26, radius 6, 8 px horizontal padding, `line2` border |
| Group | 8 px horizontal padding, 1 px `line` separator on the right |
| File button | height 28, radius 8, padding 0 20, `grad`, 600 |
| Contextual tab | `ctx` fill, 2 px `ctxLine` top border, 14 px left margin from the last normal tab |
| Rulers (editor) | 22 px; ticks every 1 unit (5 px tall) and every 10 units (11 px tall) |
| Content panel | width 300; header 40 |
| Page padding | 20 top/bottom, 24 left/right; cards radius 10, card header 38 |
| Status bar | 28 |
| Dialog | radius 12, title 36, footer 56, content padding 18 x 20 |
| Radii | 4 chips, 6 fields and small buttons, 8 large buttons, 10 cards, 12 dialogs |
| Min window | 1180 x 720 |

### 4.4 Icons

New monoline set: 24 px grid, 1.75 px stroke, round caps and joins, `currentColor`, tinted at
runtime from the palette (`ink2` default, `accent` for large-button icons on the ribbon,
`onGrad` on gradient buttons). Use **Lucide** artwork (ISC license) for every icon; add the
license text to `THIRD-PARTY-NOTICES.md` and `third-party-licenses/`. File names
`glabels/icons/neo/<name>.svg`.

| Command | Lucide icon | Command | Lucide icon |
|---|---|---|---|
| New | `file-plus` | Select mode | `mouse-pointer-2` |
| Open | `folder-open` | Text | `type` |
| Save | `save` | Box | `square` |
| Save as | `save-all` | Line | `slash` |
| Undo / Redo | `undo-2` / `redo-2` | Ellipse | `circle` |
| Cut / Copy / Paste | `scissors` / `copy` / `clipboard-paste` | Image | `image` |
| Delete | `trash-2` | Barcode | `barcode` |
| Select all | `square-dashed-mouse-pointer` | Unselect all | `square-dashed` |
| Zoom in / out | `zoom-in` / `zoom-out` | Zoom 1 to 1 / fit | `scan-line` / `scan` |
| Bring to front / Send to back | `bring-to-front` / `send-to-back` | Grid / Markup | `grid-3x3` / `ruler` |
| Align left, center, right | `align-start-vertical` / `align-center-vertical` / `align-end-vertical` | Align top, middle, bottom | `align-start-horizontal` / `align-center-horizontal` / `align-end-horizontal` |
| Rotate left / right | `rotate-ccw` / `rotate-cw` | Flip horizontal / vertical | `flip-horizontal-2` / `flip-vertical-2` |
| Center H / V / both | `align-horizontal-justify-center` / `align-vertical-justify-center` / `crosshair` | Lock aspect | `lock` / `lock-open` |
| Text H align | `align-left` / `align-center` / `align-right` | Text V align | `align-start-horizontal` / `align-center-horizontal` / `align-end-horizontal` |
| Bold / Italic / Underline | `bold` / `italic` / `underline` | Insert field | `braces` |
| Change product | `tag` | Merge | `database` |
| Variables | `braces` | Fill | `table-2` |
| Print | `printer` | System print dialog | `settings-2` |
| Send to Zebra | `send` | Deploy / Build kiosk | `package` |
| Import / Export | `download` / `upload` | Add / Remove / Clear | `plus` / `minus` / `x` |
| Refresh (reload) | `refresh-cw` | Browse | `folder-open` |
| Manual | `book-open` | Report a problem | `bug` |
| Updates | `refresh-cw` | About | `info` |
| Preferences | `settings` | Shadow | `square-dashed-bottom-code` or a drawn 2-square "shadow" glyph |
| Welcome mark | `brand/mark.svg` (as is) | Window controls | OS |

Where a name does not exist in the pinned Lucide release, pick the closest and list the
substitution in `docs/ribbon-ui/QUESTIONS.md`.

## 5. Ribbon framework behaviour

### 5.1 Widgets

- **RibbonBar**: tab strip (42) + stacked tab bodies (118). Owns the Quick Access bar, the
  File button, the normal tabs and the contextual tab(s). Emits `tabChanged`.
- **RibbonTab body**: horizontal row of **RibbonGroup**s, left aligned, 8 px padding.
- **RibbonGroup**: body (up to 3 rows of small controls, or 1 large + stacked smalls) plus a
  centred name. A group is either always visible or visible per selection (6.2).
- **RibbonButton** has three sizes (large, small, icon-only), built **from a QAction**
  (text, icon, shortcut, status tip, checkable, enabled all come from the action). Checked =
  `pressed` fill and `accent` icon. The primary variant (Print, Print on the Fill page,
  Build kiosk) uses `grad`.
- **RibbonDropButton**: button + chevron that opens a menu or popover (Undo history is not
  needed; Barcode type menu, Color popover, Shadow popover, Insert field menu).
- **RibbonField**: label + compact control (`QComboBox`, `QDoubleSpinBox`, `QLineEdit`).
  Use the **existing widgets** from the transplant (3.4), only restyled.
- **RibbonToggleGroup**: segmented icon buttons (text alignment, Color scheme, Icon style).

### 5.2 Interaction

- Click a tab: switch tab and page (3.2). Double-click an active tab or Ctrl+F1: collapse
  the ribbon to the tab strip (bodies open as a popup on click; Esc closes it). The state is
  remembered in `QSettings`.
- Hover: `hover` fill. Pressed/checked: `pressed` fill. Focus ring per 4.1.
- Tooltip (see D10): bold title, shortcut in parentheses, status tip underneath. Titles use
  the action's text without `&`. Undo/Redo tooltips and labels keep today's dynamic text
  ("Undo Create Barcode").
- Keyboard: Tab order follows visual order; Space/Enter activates; arrow keys move between
  buttons in a group; existing shortcuts untouched. Alt key tips are **(P2)**.
- Disabled actions stay visible and dimmed.
- **Narrow windows** (below the width where a tab's groups fit): collapse groups into a
  single drop-down group button in this order, last first: Zoom, Arrange (Rotate/Flip, Center,
  Align), Edit, Clipboard. Never hide a group silently; never wrap to a second row.
- The mouse wheel over the tab strip switches tabs.

### 5.3 Selection-driven ribbon state

- `Home` is the default tab for an open project.
- When the selection becomes **one object** (`mModel->isSelectionAtomic()` is true), the
  contextual tab for that object type appears at the end of the strip. It does **not** steal
  focus when you simply click an object. It **is activated automatically** right after you
  create an object with a tool, and when you double-click an object.
- When the selection becomes empty or multiple, the contextual tab disappears; if it was
  active, fall back to Home.
- Selecting a different object of the same type keeps the contextual tab active; a different
  type switches the tab to the new type.
- On Label, Data, Print, Deploy tabs no contextual tab is shown (the selection is kept).

## 6. Tab contents

Group name in **bold**; items in order left to right. "(row)" groups stack small controls.

### 6.1 Normal tabs

**File button** opens the backstage (6.3).

**Home** (mockup `s03`)
1. **Clipboard**: Paste (large, `editPasteAction`), Cut, Copy, Delete (small).
2. **Edit**: Undo, Redo (column), Select all, Unselect all (column).
3. **Insert object**: Select (large, checked by default, `objectsArrowModeAction`), Text,
   Box, Line, Ellipse, Image, Barcode (large). The creation tools stay mutually exclusive
   with Select exactly as the editor toolbar behaves today.
4. **Arrange**: Bring to Front, Send to Back (column); Align left, center, right / top,
   middle, bottom (2 x 3 icon grid); Rotate left, Rotate right, Flip horizontally, Flip
   vertically (2 x 2); Center horizontally, Center vertically, Center both (icon row).
5. **Zoom**: Zoom in, Zoom out, Zoom 1 to 1, Zoom to fit (2 x 2).

**Label** (`s09`): **Product**: Change product (large, `changeProductButton`'s action);
**Current**: product name, size and layout (read only text); **Parameters**: Label length
(only when the template has an adjustable length, as today); **Orientation**: Horizontal or
Vertical combo (`orientationCombo`). The page (6.4) holds the full product information and
similar products.

**Data** (`s10` to `s12`): **Data page** (three large toggle buttons: Merge, Variables, Fill;
exclusive) then groups per page:
- Merge: **Source**: Browse... (opens the file chooser for the location), Reload;
  **Records**: Select all, Unselect all.
- Variables: **Variables**: Add variable, Edit, Delete (large).
- Fill: **Rows**: Add row, Remove, Clear; **Import and export**: Import CSV, Import Excel,
  Export CSV, Export Excel; **Output**: Print (primary), Send to Zebra, System dialog...

**Print** (`s13`): **Output**: Print (primary, large), System dialog... (large);
**Printer**: Destination combo + status text (the printer name and "Ready" / state, as
`PrinterMonitor` reports). Everything else is on the page (6.4).

**Deploy** (`s14`): **Kiosk**: Build kiosk... (primary, large); **About**: three lines of
read-only help text.

**View** (`s15`): **Show**: checkable rows Grid, Markup, Quick Access toolbar, Content panel
(`viewGridAction`, `viewMarkupAction`, `viewFileToolBarAction`, plus a new checkable action
for the Content panel; the old "Editor" toolbar action is removed); **Zoom**: four large
buttons; **Appearance**: Light / Dark / System segmented (same setting as Preferences);
**Icons**: Flat / Vector segmented (same setting as Preferences; see open question Q1);
**Ribbon**: Collapse ribbon.

**Help** (`s16`): **Help (F1)**: User Manual (`helpContentsAction`), Report a Problem
(`helpReportBugAction`); **gLabels Neo**: Check for Updates (the action `UpdateUi` provides),
About (`helpAboutAction`).

### 6.2 Contextual tabs

Single selected object. Each control is the transplanted existing widget (3.4). "Maps to" is
the existing `ObjectEditor` member.

**Text** (`s04`): *Font*: family `textFontFamilyCombo`, size `textFontSizeSpin` (pt), Bold
`textFontBoldToggle`, Italic `textFontItalicToggle`, Underline `textFontUnderlineToggle`,
color `textColorButton`. *Alignment*: horizontal `textHAlignLeft/Center/RightToggle`,
vertical `textVAlignTop/Middle/BottomToggle`. *Paragraph*: line spacing `textLineSpacingSpin`,
wrap mode `textWrapModeCombo` (Word, Anywhere, None), "Shrink text to fit when printing"
`textAutoShrinkCheck`. *Content*: Insert field `textInsertFieldButton`. *Position and size*:
X `posXSpin`, Y `posYSpin`, W `sizeWSpin`, H `sizeHSpin`, Lock aspect `sizeAspectCheck`.
*Effects*: Shadow (popover, below). *Arrange*: Bring to Front, Send to Back, Delete, Rotate
left/right, Flip horizontally/vertically.

**Barcode** (`s05`): *Type*: Barcode style `barcodeStyleButton` (opens the existing
`BarcodeMenu`), Show text `barcodeShowTextCheck`, Checksum `barcodeChecksumCheck` (each
enabled exactly as today). *Color*: `barcodeColorButton`. *Data*: Insert field
`barcodeInsertFieldButton`. *Position and size*: as Text. **No Effects group**: barcodes have
no shadow today and must not get one. *Arrange*: as Text.

**Image** (`s06`): *Source*: file `imageFilenameLineEdit`, Browse `imageBrowseButton`, Use
merge field `imageFieldButton`. *Position and size*: X, Y, W, H, Lock aspect, Original size
`sizeOriginalSizeLabel` + Reset `sizeResetImageButton`. *Effects*: Shadow. *Arrange*.

**Shape** (box and ellipse, `s07`): *Line*: width `lineWidthSpin`, color `lineColorButton`.
*Fill*: color `fillColorButton`. *Position and size*: as Text (no original size). *Effects*:
Shadow. *Arrange*.

**Line** (`s08`): *Line*: width, color. *Position*: X, Y. *Size*: Length `sizeLineLengthSpin`,
Angle `sizeLineAngleSpin`. *Effects*: Shadow. *Arrange*.

**Shadow popover** (D10, from the Effects button): Enable shadow (`shadowEnableCheck`), X
offset `shadowXSpin`, Y offset `shadowYSpin`, Color `shadowColorButton`, Opacity
`shadowOpacitySpin` (percent).

**Color popover** (D10): replaces `ColorPaletteDialog` visually but keeps its model:
Standard colors grid, Recent colors (`ColorHistory`), "Custom color...", and, where the
button supports it, "Use substitution field" (`mFieldButton` in `ColorPaletteDialog`).
**Insert field menu**: sections "Merge fields" and "Variables" exactly as `FieldButton`
builds them, items rendered as mono chips.

Today's tab titles "text/barcode/..." in `ObjectEditor` disappear; the contextual tab label
is the object type: Text, Barcode, Image, Shape, Line.

### 6.3 File backstage (`s02`) and Welcome (`s01`)

Full-window overlay under the native title bar. Left nav (240): Back, New..., Open... (Ctrl+O),
Open Recent, Save (Ctrl+S), Save As..., separator, Product Template Designer...,
Preferences..., separator, Close (Ctrl+W), Exit (Ctrl+Q). Right pane for Open: **Browse...**
command link and the **Recent** list (file name left, folder right; uses the existing recent
files list and `fileRecentActionList`). New... starts the existing new-project flow (Select
product dialog). Preferences opens the Preferences dialog. Esc or Back returns.

**Welcome** (no project open) shows the same command links on the page: New... ("Create a new
blank gLabels project"), Browse... ("Open an existing gLabels project"), Recent ("Open a
recent gLabels project") plus the mark, "Welcome to gLabels Neo", and "A modernized fork of
gLabels". Tabs: only File and Help. Ribbon hidden.

### 6.4 Pages

Common: page title (20/700) top left, then cards. Cards use the same widgets as today
(`QGroupBox` becomes a card via QSS or a thin card wrapper).

- **Label** (`PropertiesView`, `s09`): two columns. Left (460): cards Product (vendor, part
  #, description, page size, label size, layout + Change product), Adjustable parameters
  (label length), Orientation, Similar products (the existing `QTextBrowser`). Right: Preview
  card with the template sheet (`SimplePreview`) on the `work` backdrop.
- **Merge** (`MergeView`, `s10`): card Source (Format combo with the nine formats, Location
  line edit + Browse...); card Records (table with a leading checkbox column, "N records, M
  selected" in the header). Select all, Unselect all, Reload are on the ribbon.
- **Variables** (`VariablesView`, `s11`): intro caption, table Name, Type (String, Integer,
  Floating Point, Color), Value (color shows a swatch), Increment (Never, Per item, Per
  copy, Per page), Step Size. Add/Edit/Delete are on the ribbon; double-click a row edits.
- **Fill** (`FillView`, `s12`): header row (title, product summary, Preview combo: Selected
  row / Checked rows (what will print) / All rows); table (Qty, Print checkbox, one column
  per field); Sheet preview card with page spinner; bottom row Destination combo, Start at
  position spin, hint text. Add row/Remove/Clear/Import/Export/Print/Send to Zebra/System
  dialog are on the ribbon.
- **Print** (`PrintView`, `s13`): left column cards Destination, Print range (Pages, Positions
  from to), Merge control (Copies, collate combo, group combo, start groups at position),
  Options (outlines, crop marks, reverse); right Preview card with page spinner. Print and
  System dialog are on the ribbon.
- **Deploy** (`DeployView`, `s14`): card Kiosk settings (Application name, Embedded label
  name, Base kiosk executable + Browse, Data source + Browse, Printer, Admin PIN), card Locked
  fields (check list), status line (success pill "Ready to build" / the result text).
- **Editor** (`s03`): rulers + canvas (`LabelEditor`, unchanged behaviour: grid, markup,
  zoom, rubber-band, all handles) + Content panel.

**FillView hosting modes.** `FillView` is also the whole UI of the kiosk app. Add a property
so it can run **ribbon-hosted** (its toolbar buttons are hidden because the ribbon exposes
them) or **standalone** (kiosk: toolbar shown inline, as in D11).

### 6.5 Content panel (right, 300 px, toggled by View > Content panel)

Header "Content" + the selected type on the right. Body by selection:
- none: "Select an object to edit its content. Several objects at once: use the Arrange
  commands on the Home tab."
- Text: label "Text", the existing text editor (`textEdit`), Insert field button, caption.
- Barcode: label "Barcode data", `barcodeDataEdit`, Insert field.
- Image: label "Image file", file line edit, Browse..., Use field.
- Shape / Line: one caption saying there is no content and which tab to use.
Below, always: **Fields** (merge keys of the current source, with the first record's value,
grip icon, mono chip; double-click or drag inserts into the selected text/barcode) and
**Variables** (name chip + value and increment). Hide the Fields list when there are none.

### 6.6 Status bar (`RibbonStatusBar`)

Left: hovered action's status tip, else "<Label name> · W x H unit · N per sheet". Right:
selection ("Nothing selected" / "1 object selected" / "N objects selected"), cursor position
in the preferred unit, zoom out button, slider, zoom in button, percent.

## 7. Dialogs (P0: restyle; P1: re-layout)

All dialogs take the theme through the app-wide stylesheet (section 8). In P0 they keep
today's structure unless the line says otherwise. The mockups show the **target** look.

| Dialog | Mockup | P0 | P1 |
|---|---|---|---|
| Select product (`SelectProductDialog`) | d01 | restyle | three-column layout, tile picker with part #, size line |
| Preferences (`PreferencesDialog`) | d02 | restyle tabs: Locale, Grid, Appearance, Startup, **Updates** (exists) | single page, five cards |
| Product Template Designer | d03, d04 | restyle the wizard | left step list |
| Edit variable | d05 | restyle | |
| About | d06 | restyle; keep License, Website, Close; add attribution sentence | |
| Report a problem | d07 | restyle | |
| Update consent / Update available | d08 | restyle (`UpdateUi`) | |
| Message boxes | d09 | restyle | |
| Popovers and menus | d10 | color, shadow, insert-field, barcode, context menu, tooltips (P0) | |
| Kiosk app + admin | d11 | restyle `glabels-fill` | |

Buttons: default (affirmative) button uses the filled `accent` style; others outlined.
Dialog footers sit on `chrome`.

## 8. Theming implementation

- `Theme` holds the two palettes (section 4.1) as `QColor`s plus metrics.
- `ThemeStyle` generates **one** application stylesheet from the active theme and applies it
  with `qApp->setStyleSheet()`. It must cover: ribbon widgets, tab bar, buttons, tool buttons,
  combos, spins, line edits, text edits, check boxes, radio buttons, group boxes (as cards),
  tables and headers, tabs, scroll bars, menus (context menu, popups), tooltips, dialogs,
  message boxes, the status bar, sliders. Use `QProperty`-driven selectors
  (`[role="ribbonLarge"]`) rather than object-name coupling.
- Keep `AppearanceModel`'s Light / Dark / System semantics (system follows `QStyleHints::colorScheme`).
  Re-apply on change without restart.
- Fusion style stays the base style (`QStyleFactory`), as today.
- `ThemeIcons` loads `neo/*.svg`, recolours `currentColor` per state (normal, hover/checked,
  disabled, on-gradient) and caches by (name, color, size). HiDPI aware.
- The canvas (`LabelEditor`, `Preview`) keeps its own painting; only the backdrop colour
  (`work`), grid colour and selection colour (`accent`) come from the theme.

## 9. Command map (every action)

`Where` is the ribbon location. Shortcut and status tip stay as in `MainWindow::createActions`.

| Action | Where |
|---|---|
| `fileNewAction` | Quick Access, Backstage New |
| `fileOpenAction` | Quick Access, Backstage Open |
| `fileRecentMenu` / recent list | Backstage Open Recent, Welcome Recent |
| `fileSaveAction` | Quick Access, Backstage Save |
| `fileSaveAsAction` | Backstage Save As |
| `fileShowEditorPageAction` (Ctrl+1) | Home tab |
| `fileShowPropertiesPageAction` (Ctrl+2) | Label tab |
| `fileShowMergePageAction` (Ctrl+3) | Data tab, Merge |
| `fileShowVariablesPageAction` (Ctrl+4) | Data tab, Variables |
| `fileShowFillPageAction` (Ctrl+5) | Data tab, Fill |
| `fileShowPrintPageAction` (Ctrl+P) | Print tab |
| `fileShowDeployPageAction` | Deploy tab |
| `fileTemplateDesignerAction` | Backstage |
| `fileCloseAction`, `fileExitAction` | Backstage |
| `editUndoAction`, `editRedoAction` | Quick Access, Home > Edit |
| `editCutAction`, `editCopyAction`, `editPasteAction`, `editDeleteAction` | Home > Clipboard; Delete also in contextual Arrange |
| `editSelectAllAction`, `editUnSelectAllAction` | Home > Edit |
| `editPreferencesAction` (Ctrl+,) | Backstage |
| `viewFileToolBarAction` | View > Show: Quick Access toolbar |
| `viewEditorToolBarAction` | **Removed** (the editor toolbar no longer exists) |
| `viewGridAction`, `viewMarkupAction` | View > Show |
| `viewZoomIn/Out/1To1/ToFitAction` | Home > Zoom, View > Zoom, status bar (in/out) |
| `objectsArrowModeAction` | Home > Insert object |
| `objectsCreateText/Box/Line/Ellipse/Image/BarcodeAction` | Home > Insert object |
| `objectsOrderRaise/LowerAction` | Home > Arrange, contextual Arrange |
| `objectsXformRotateLeft/Right/FlipHoriz/FlipVertAction` | Home > Arrange, contextual Arrange |
| `objectsAlignLeft/HCenter/Right/Top/VCenter/BottomAction` | Home > Arrange |
| `objectsCenterHoriz/Vert/CenterAction` | Home > Arrange |
| `helpContentsAction` (F1), `helpReportBugAction`, `helpAboutAction` | Help tab |
| check-for-updates action (`UpdateUi`) | Help tab |
| `contextCut/Copy/Paste/DeleteAction` + context submenus | Canvas context menu (unchanged) |

## 10. Feature parity checklist

Every row must be true before the branch is called done. Tick in `docs/ribbon-ui/PARITY.md`.

**Project and files**: new, open, recent (and the recent list updates), save, save as,
close with the unsaved-changes prompt ("Save changes to project ... before closing?"), exit,
window title shows "(modified)", Welcome page with New/Browse/Recent, multiple windows still
work, Product Template Designer (all pages: intro, copy/edit, new, new thermal/roll, name,
page size, shape, rect/ellipse/round/CD sizes, one/two layouts, path, continuous, apply),
Preferences (Units: points, centimeters, millimeters, inches, picas; Grid: origin, spacing,
reset; Appearance: Light/Dark/System, Flat/Vector; Startup directory: parent, home, documents,
other + browse; Updates).

**Editing**: undo/redo with descriptive labels, cut/copy/paste/delete, select all/unselect
all, all six object tools, select mode, order, rotate, flip, align (6), center (3), context
menu (with submenus, no-selection variant), keyboard shortcuts, grid and markup toggles, zoom
in/out/1:1/fit, rulers, drag-to-create, rubber-band, handles, move/resize/rotate.

**Object properties (all)**: text content with fields, font family, size, bold, italic,
underline, color, horizontal and vertical alignment, line spacing, wrap mode, auto-shrink;
barcode style (full barcode menu), show text, checksum, color, data with fields; image file,
browse, field, original size, reset; line width and color; fill color; position X/Y; size
W/H, lock aspect; line length and angle; shadow enable, X/Y offset, color, opacity. Barcode
has **no** shadow and no line/fill. Controls disabled when the object type or style does
not support them.

**Label**: product information, Change product (Select product dialog: search, recent,
paper-size filters, category filters, picker, preview, Custom..., Edit..., Cancel, Select),
adjustable parameter, orientation, similar products, sheet preview.

**Data**: merge format (all nine), location, browse, reload, records table, select
all/unselect all; variables (add, edit, delete, four types, four increment modes, step size,
color variable); Fill (add row, remove, clear, import/export CSV and Excel, preview combo,
destination, start position, print, send to Zebra, system print dialog, page navigation).

**Print**: destination, print range (pages, positions), copies, collate, merge groups, start
position, outlines, crop marks, reverse, preview with page spinner, system print dialog,
printer monitor states.

**Deploy**: every field of `DeployView`, locked fields, build, result messages and errors.

**App**: Check for Updates, consent dialog, update available/progress/ready, Report a
Problem (all options, preview of what is sent, GitHub fallback), About with License and
Website, user manual (F1), light/dark/system and Flat/Vector without restart, translations
(strings through `tr()`), kiosk app (`glabels-fill`) and its admin dialogs, `glabels-batch`
unaffected.

## 11. Accessibility

Contrast 4.5:1 for text, 3:1 for control borders and icons in both themes (flag any token
that fails; do not silently change a token). Every icon-only button has `accessibleName`
and a tooltip. Tab order matches visual order. The ribbon is operable by keyboard only.
No information by colour alone.

## 12. Performance and robustness

Ribbon construction happens once. Re-parenting (3.4) happens once. Selection changes must
not rebuild widgets; they show/hide groups and tabs. Theme switch re-applies the stylesheet
and invalidates the icon cache, nothing else. No per-frame allocations in `LabelEditor`.

## 13. Build and verification

- CI (`.github/workflows/ci.yml`) builds Linux, Windows and macOS. You cannot push, so use a
  local build to verify. If there is no toolchain, bootstrap a portable one **outside the
  repo** (embeddable Python, `pip install aqtinstall`, Qt 6.8.3 `win64_mingw` plus
  `tools_mingw1310`, `tools_cmake`, `tools_ninja`) and keep scripts in the scratchpad, not in
  the repository. A build regenerates `translations/*_C.ts`; revert them.
- Configure/build with the project's CMake. Run the existing tests (`ctest`).
- Add unit tests for the new logic that can be tested without a display (Theme palettes and
  stylesheet generation, tab/page mapping, contextual-tab visibility rules, command map
  coverage: every `QAction` created in `MainWindow` appears in the ribbon or backstage).
- Visual check: run the app, open `test-data` labels, and compare each screen to its mockup
  at 100% scale in light and dark. Save screenshots under `docs/ribbon-ui/screenshots/`.

## 14. Work split

| Package | Owner | Notes |
|---|---|---|
| WP1 Theme, tokens, ThemeStyle (QSS), ThemeIcons, `neo` icon set, `AppearanceModel` integration, notices | Opus | No dependency on the ribbon; deliver first |
| WP2 Ribbon framework widgets (`glabels/ribbon/*`), RibbonBar, groups, buttons, fields, toggle group, Quick Access, collapse/overflow | Gemini 4 Neon | Depends on WP1 API only |
| WP3 `MainWindow` integration: remove menu bar and rail, build tabs from actions, tab and page sync, backstage, Welcome, status bar, contextual-tab visibility | Gemini 4 Neon | |
| WP4 ObjectEditor transplant (3.4), contextual tabs, Content panel, popovers (color, shadow, field, barcode) | Gemini 4 Neon, reviewed by Opus | Highest regression risk |
| WP5 Page restyle and relayout: Label, Merge, Variables, Fill (+ kiosk hosting mode), Print, Deploy | Gemini 4 Neon | |
| WP6 Dialog QSS pass, kiosk restyle, About attribution | Opus | |
| WP7 Build bootstrap, tests, parity checklist, screenshots, review of WP2 to WP5 | Opus | |

Files each package owns are listed in `docs/ribbon-ui/WORKPLAN.md`; do not edit another
package's files without writing it in `docs/ribbon-ui/QUESTIONS.md` first.

## 15. Open questions (the owner decides; implement the default meanwhile)

| # | Question | Default |
|---|---|---|
| Q1 | The Icon style (Flat/Vector) setting: the ribbon uses the new Neo set. | Keep the setting and keep it working for legacy places (kiosk, dialogs); the ribbon always uses Neo. |
| Q2 | Native vs custom title bar. | Native in P0. |
| Q3 | Auto-activate the contextual tab on creation and double-click only. | As written in 5.3. |
| Q4 | Search commands box. | P2, not built. |
| Q5 | One-page Preferences and the three-column Select product dialog. | P1. |
