# Popup audit

Owner report (real app, dark): the Increment combo of Edit variable lists Never, Per item, Per
copy, Per page, but the popup is one row too short, so it scrolls ("Per page" cut off, scroll
arrow visible), and a sharp black corner shows behind the popup's rounded corner.

Every popup kind was opened offscreen and measured by `glabels/unit_tests/TestThemePopups`
(test `ThemePopups` in ctest). Rules it asserts, in light and dark:

- a combo list of **N <= 15** items shows all N rows fully, with **no** scrolling (no scroll bar
  range, no menu scroller arrow); a longer list shows 15 rows and scrolls;
- **no unpainted pixel** in the corners of any popup window. The popup is rendered with its
  children but *without* the window background onto a magenta sentinel; whatever stays
  magenta is painted by nobody, and on Windows that area of a frameless popup window shows
  black. (The offscreen backing store pre-fills windows, so a plain screen grab cannot show
  the defect; the sentinel rendering does.)

Before/after renderings (magenta = unpainted = black on Windows):
`screenshots/popups/before/` and `screenshots/popups/after/`, same file names.

## Root causes

1. **Menu-style combo popup sized from the wrong row height.** Under an application
   stylesheet a `QComboBox` uses the "menu-like" popup (`SH_ComboBox_Popup`), drawn by Qt's
   private `QComboMenuDelegate` (a `QItemDelegate`). That delegate ignores the stylesheet's
   item rules, while `QComboBox::showPopup()` adds up row heights and margins that did not
   include the view padding (4 px) set by the stylesheet. Result: every non-editable combo
   was **one row short** (N items, N-1 fully visible) and showed the menu scroller arrow,
   exactly the owner's "Per page" case. Measured before: 1 of 1, 1 of 2, 2 of 3, 3 of 4 ...
   14 of 15 rows visible.
2. **Rounded list inside a rectangular popup window.** The global stylesheet rule
   `QAbstractItemView { border-radius: 8px }` (meant for tables in cards) also matched the
   list inside combo popups and completer popups. The list clipped its background to the
   rounded rectangle; the popup window around it is a plain rectangle, so the four corner
   areas were painted by nobody: **black corners** on Windows. Same for editable combos,
   the font list and the completer.
3. **Custom popovers painting a rounded panel on an opaque frameless window.**
   `ribbon::ShadowPopover` and the ribbon group overflow popup paint `drawRoundedRect(...,
   10, 10)` on a `Qt::Popup | Qt::FramelessWindowHint` window that does not fill its
   background: unpainted corners, black on Windows.
4. **Ten rows maximum.** `QComboBox::maxVisibleItems` defaults to 10, so lists of 11..15
   items scrolled (the 54-entry font list showed 10).

## Fixes (class-wide, in the theme layer, applied on every polish)

| Where | Change |
|---|---|
| `ThemeStyle` | `QComboBox { combobox-popup: 0; }`: the standard Qt list drop-down instead of the menu-like one (owner: standard Qt dropdowns preferred). Popup lists (`QComboBox QAbstractItemView` and any list in a popup window, property `popupList`) get **square corners, no padding**, 24 px rows with 0 8 px padding, `pressed` highlight with `ink` text. `*[role="popover"]` is square. |
| `Decorator::polishCombo` | Qt's own combo delegates (`QComboBoxDelegate`, `QComboMenuDelegate`) are replaced by a `QStyledItemDelegate`, so rows are sized and painted from the same stylesheet rule and `showPopup()` measures the real height. Custom delegates (the font list) are kept. `maxVisibleItems` 10 (Qt's default) becomes 15; a combo that set its own limit keeps it. |
| `Decorator::polishPopupView` | any item view inside a `Qt::Popup` window (combo, completer) gets the `popupList` property, so the square popup rule applies. |
| `Decorator::polishPopupWindow` | every non-translucent `Qt::Popup` window that is not a `QMenu` fills its background before painting, so a popover that paints a rounded panel can never leave black corners. |
| `ribbon/ShadowPopover.cpp`, `ribbon/RibbonGroup.cpp` (Gemini's files, logged in QUESTIONS.md) | the panel is drawn as a rectangle (flat popup with a 1 px `line2` border). |

No per-dialog code: every combo, completer and popup in the app is fixed by polish.

## Every popup kind

| Popup | Before | After | Test |
|---|---|---|---|
| Combo box, 1..15 items (light, dark) | one row short, scroller arrow, unpainted corners | all rows, no scrolling, square | `comboShowsEveryItem` 1, 2, 3, 4, 5, 8, 10, 11, 12, 15 |
| Combo box, 16 and 30 items | scrolls (correct), 10 rows | scrolls, 15 rows | `comboShowsEveryItem` 16, 30 |
| Editable combo box | rows fine, unpainted corners | square | `comboShowsEveryItem` editable-4 |
| Edit variable: Type, Increment | one row short (owner's report) | all rows | `comboInEveryDialogAndPage` |
| Preferences, Label (Orientation), Merge (Format), Fill (Preview, Destination), Print (Destination), Deploy, View, Text tools (Wrap, Font family) | one row short; font list 10 rows and corners | all rows; font list 15 rows, square | `comboInEveryDialogAndPage` (every visible enabled combo of each, light and dark) |
| Completer popup | rows fine, unpainted corners | square | `toolTipAndCompleter` |
| Context menu with separator and submenu | fine (no radius; every action inside) | unchanged | `menusAndSubmenus` |
| Barcode type menu (sections, submenus) | fine | unchanged | `menusAndSubmenus` |
| Insert field menu (`FieldButton`) | a `QMenu`: same rules as the context menu | unchanged | covered by the `QMenu` cases, not opened separately |
| Tooltip | fine | unchanged | `toolTipAndCompleter` |
| Colour palette popup | fine (rectangular dialog, no footer band for popups) | unchanged | `customPopups` |
| Shadow popover | unpainted corners (rounded paint) | square, background filled | `customPopups` |
| Ribbon group overflow popup, collapsed-ribbon popup | rounded paint / no fill | square, background filled by the guard | same code path as the Shadow popover; the guard is asserted with a generic rounded popup in `customPopups` |
| Spin box arrows, scroll bars | not popups; checked in the gallery captures, no defect found | unchanged | `TestThemeGallery` |
| File backstage | not a popup (a page in the main window) | n/a | |
