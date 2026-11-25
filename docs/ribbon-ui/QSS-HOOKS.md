# QSS hooks (dynamic properties matched by the generated stylesheet)

`glabels/theme/ThemeStyle.cpp` generates the one application stylesheet. Plain Qt widgets get
their look from it through the dynamic property **`role`** (and a few modifier properties).
Set the property **before** the widget is first shown; if you change it later, call
`w->style()->unpolish( w ); w->style()->polish( w );`.

```cpp
button->setProperty( "role", QStringLiteral( "ribbonLarge" ) );
```

Custom-painted widgets read `theme::Theme::instance().palette()` instead and repaint on
`Theme::changed()`; a role on them still gets the font, padding and focus ring below.

Both agents append here. Add a row for every new property **and** tell the QSS owner (Opus)
in `QUESTIONS.md` so the selector gets written; an unmatched role is simply unstyled.

## Ribbon (SPEC 4.3, 5.1)

| `role` value | Widget type | Look |
|---|---|---|
| `ribbonBar` | `QWidget` (whole bar) | `chrome` background |
| `ribbonTabStrip` | `QWidget` | 42 px strip on `chrome`, 1 px `line` bottom border |
| `ribbonTab` | `QAbstractButton`, checkable (checked = active tab) | 34 px, 13 px text `ink2`; checked: `ribbon` fill, `ink` 600, 3 px `accent` underline |
| `ribbonContextTab` | `QAbstractButton`, checkable | `ctx` fill, 2 px `ctxLine` top border, 14 px left margin |
| `fileButton` | `QAbstractButton` | 28 px high, radius 8, padding 0 20, `grad`, `onGrad` 600 text |
| `quickAccess` | `QWidget` | transparent container |
| `quickAccessButton` | `QToolButton` | 28 x 28 icon-only, radius 6 |
| `ribbonBody` | `QWidget` / `QStackedWidget` | `ribbon` surface, 118 px, 1 px `line` bottom border |
| `ribbonGroup` | `QWidget` / `QFrame` | 8 px horizontal padding, 1 px `line` right border |
| `ribbonGroupName` | `QLabel` | 12 px `ink3`, centred, 22 px |
| `ribbonGroupOverflow` | `QToolButton` | drop-down that holds a collapsed group (narrow windows) |
| `ribbonLarge` | `QToolButton` | 58 x 78, icon 30, radius 8; hover `hover`; checked/pressed `pressed` |
| `ribbonSmall` | `QToolButton` | 26 high, icon 16, radius 6 |
| `ribbonIcon` | `QToolButton` | 28 x 28, icon 16, radius 6 |
| `ribbonPrimary` | `QToolButton` | large button on `grad`, `onGrad` text (Print, Build kiosk) |
| `ribbonDrop` | `QToolButton` (menu button) | as `ribbonSmall`, chevron on the right |
| `ribbonField` | `QComboBox`, `QAbstractSpinBox`, `QLineEdit` | 26 high, radius 6, 0 8 px padding, `line2` border, 12 px |
| `ribbonCaption` | `QLabel` | 12 px `ink2` field label |
| `ribbonToggleGroup` | `QWidget` | segmented container: `line2` border, radius 6 |
| `ribbonToggle` | `QToolButton`, checkable | segment, checked = `pressed` fill and `accent` text |
| `ribbonInfo` | `QLabel` | 12 px `ink2` read-only help text inside a group |

Modifier: `primary="true"` on a `ribbonLarge` or `ribbonSmall` button gives the `grad` look
(same as `role="ribbonPrimary"`).

## Backstage, Welcome, Content panel, status bar

| `role` value | Widget type | Look |
|---|---|---|
| `backstage` | `QWidget` | `bg` background |
| `backstageNav` | `QWidget` | 240 px, `chrome`, 1 px `line` right border |
| `backstageNavItem` | `QAbstractButton`, checkable | 36 px row, radius 6, checked = `pressed` fill + `accent` text |
| `backstageSeparator` | `QFrame` | 1 px `line` |
| `backstagePane` | `QWidget` | `bg`, padding 24 |
| `commandLink` | `QAbstractButton` | large command row: title 13/600 + description, hover `hover`, radius 8 |
| `recentList` | `QListWidget` / `QTreeWidget` | borderless list, 32 px rows |
| `welcome` | `QWidget` | `bg` background |
| `contentPanel` | `QWidget` | 300 px, `panel`, 1 px `line` left border |
| `contentPanelHeader` | `QWidget` | 40 px, 1 px `line` bottom border |
| `panelTitle` | `QLabel` | 13 px 600 `ink` |
| `fieldChip` | `QLabel` / `QToolButton` | mono 12 px, `tint` fill, `accent` text, radius 4, padding 1 6 |
| `ribbonStatusBar` | `QStatusBar` / `QWidget` | 28 px, `chrome`, 1 px `line` top border, 12 px `ink2` |
| `statusText` | `QLabel` | 12 px `ink2` |
| `zoomSlider` | `QSlider` | thin groove `line2`, filled `accent`, round handle |

## Pages and cards (SPEC 6.4)

| `role` value | Widget type | Look |
|---|---|---|
| `page` | `QWidget` / `QScrollArea` viewport | `bg` background |
| `pageTitle` | `QLabel` | 20 px 700 `ink` |
| `card` | `QFrame` or `QGroupBox` | `panel`, 1 px `line` border, radius 10 (every `QGroupBox` is already a card) |
| `cardHeader` | `QWidget` / `QLabel` | 38 px, 13 px 600, 1 px `line` bottom border, `chrome` fill |
| `caption` | `QLabel` | 12 px `ink3` |
| `workWell` | `QWidget` / `QGraphicsView` | `work` backdrop (previews, canvas) |
| `pill` | `QLabel` | radius 10, padding 2 10, 12 px; modifier `tone="ok"` (`okTint`/`ok`) or `tone="warn"` (`warn`) |
| `popover` | `QFrame` (popup) | `panel`, 1 px `line2` border, radius 10 |
| `popoverSection` | `QLabel` | 11 px 600 uppercase-style caption `ink3` |
| `swatch` | `QToolButton` | 20 x 20 colour square, radius 4, `line2` border |

## Dialogs (owned by the theme layer)

| `role` value | Widget type | Look |
|---|---|---|
| `dialogHeading` | `QLabel` | 15 px 600 `ink` (message box text, update dialogs) |
| `dialogBody` | `QLabel` | 13 px `ink2` |
| `primary` | `QPushButton` | filled `accent` (the default button already gets this through `:default`) |
| `mono` | `QPlainTextEdit` / `QTextEdit` / `QLabel` | mono font 12 px, `bg` fill |

The dialog footer band (`chrome` with a `line` top border behind the bottom button row) is
painted by `theme::DialogDecorator`; dialogs need no property for it.
