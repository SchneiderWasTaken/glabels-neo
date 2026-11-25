# QSS hooks (`role` dynamic properties)

Every dynamic property set by `feat/ribbon-ui-core` (WP2–WP5) for matching in `ThemeStyle` QSS selectors (`[role="..."]`):

| `role` value | Widget class | Where used |
|---|---|---|
| `ribbonBar` | `QWidget` (`RibbonBar`) | Top-level ribbon container |
| `ribbonTabStrip` | `QWidget` | 42 px tab strip |
| `ribbonQuickAccess` | `QWidget` (`QuickAccessBar`) | Quick Access bar at left of tab strip |
| `ribbonFileButton` | `QAbstractButton` | Gradient File button |
| `ribbonTab` | `QAbstractButton` | Normal ribbon tab header |
| `ribbonContextTab` | `QAbstractButton` | Contextual ribbon tab header |
| `ribbonBody` | `QWidget` (`RibbonTab`) | 118 px tab body |
| `ribbonGroup` | `QWidget` (`RibbonGroup`) | Group column inside a tab |
| `ribbonGroupName` | `QLabel` | Centered 22 px group caption |
| `ribbonLarge` | `RibbonButton` | 58x78 large button |
| `ribbonSmall` | `RibbonButton` | 26 px small button |
| `ribbonIcon` | `RibbonButton` | 28x28 icon-only button |
| `ribbonPrimary` | `RibbonButton` | Gradient primary ribbon button (Print, Build kiosk) |
| `ribbonField` | `QComboBox`, `QDoubleSpinBox`, `QSpinBox`, `QLineEdit`, `ColorButton`, `BarcodeMenuButton` | 26 px compact ribbon field |
| `ribbonCaption` | `QLabel` | 12 px `ink3` caption inside ribbon / popovers |
| `ribbonToggleGroup` | `RibbonToggleGroup` | Segmented toggle control |
| `ribbonPopover` | `QWidget` | Shadow popover / group overflow popup |
| `ribbonStatusBar` | `RibbonStatusBar` | 28 px bottom status bar |
| `statusText` | `QLabel` | Status bar labels |
| `statusZoomSlider` | `QSlider` | Status bar zoom slider |
| `backstage` | `Backstage` | Full-window File backstage |
| `backstageNav` | `QWidget` | 240 px left nav rail in backstage |
| `backstageNavItem` | `QAbstractButton` | Nav item in backstage |
| `backstageSeparator` | `QFrame` | Hairline divider in backstage nav |
| `commandLink` | `QAbstractButton` | 560 px command-link card (Backstage Browse, Welcome New/Browse/Recent) |
| `contentPanel` | `ContentPanel` | 300 px right-hand Content panel |
| `contentPanelHeader` | `QWidget` | 40 px / 32 px section header in Content panel |
| `pageTitle` | `QLabel` | 20 px bold page heading |
| `card` | `QWidget` / `QGroupBox` | Surface card with `panel` bg, `line` border, 10 px radius |
| `cardTitle` | `QLabel` | 13 px 600 card / section heading |
| `fieldLabel` | `QLabel` | 12 px `ink2` field label |
| `caption` | `QLabel` | 12 px `ink3` helper caption |
| `statusPillOk` | `QLabel` | `okTint` background, `ok` text, 11 px radius pill (Deploy "Ready to build") |
| `welcomePage` | `StartupView` | Welcome page surface |
| `previewWell` | `QWidget` / `QGraphicsView` | `work` backdrop preview container |
