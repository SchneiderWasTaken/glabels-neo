# Colour schemes (themes)

gLabels Neo ships six colour schemes of the **same** UI. Layout, metrics, fonts, icons and
behaviour are identical; a scheme only changes the colour tokens of SPEC.md 4.1 and whether the
File button and the primary buttons (Print, Build kiosk) use a gradient. Light, Dark and System
still choose the variant on top of the scheme.

| Id | Name | Description (as shown in the picker) | Gradient |
|---|---|---|---|
| `neo` | Neo (default) | Violet-blue gradient and an indigo accent; the original look | yes |
| `graphite` | Graphite | Neutral greys and a steel-blue accent, like a professional tool | no |
| `paper` | Paper | Warm stone surfaces, ink-black text and a forest-green accent | no |
| `harbor` | Harbor | Cool navy surfaces and a teal accent | no |
| `signal` | Signal | High contrast with a burnt-orange accent | no |
| `system` | System | Neutral surfaces and the Windows 11 blue accent, like a native app | no |

## Design rationale

The owner found the default look "a bit too much of a vibe-coded feel". The five alternatives
keep the exact UI and follow one brief:

- **Restraint**: a single accent per scheme, used for the active tab underline, checked
  toggles, focus rings, links and selection handles. Nothing else is coloured.
- **Solid File button**: the violet-to-blue gradient is replaced by the accent itself
  (`gradA = gradB = gradC`), so the File and Print buttons read as plain primary buttons.
- **Neutral tints**: `tint`, `hover`, `pressed` and `ctx` are pale versions of the scheme's own
  accent or plain greys, not lavender, so chips, selections and the contextual tab are calm.
- **Each scheme has a personality without decoration**: Graphite is a neutral pro tool,
  Paper is warm and print-like, Harbor is cool and nautical, Signal is high contrast with a
  family resemblance to the LabelOS web app, System imitates Windows 11 Fluent.

Neo is unchanged and stays the default.

## How it works

- `glabels/theme/ThemeScheme.{hpp,cpp}`: the registry (`ThemeSchemes::all()`, `find()`
  with fallback to Neo for unknown ids, `fillCombo()` for pickers with swatch icons).
- `Theme::setScheme( id )` / `Theme::scheme()`: `palette()` and `paletteFor()` return the
  current scheme's tokens; `applyToApplication()` re-applies palette and stylesheet.
- Stored in the settings next to the colour scheme: group `Appearance`, key `themeScheme`
  (`model::Settings::themeScheme()` / `setThemeScheme()`), default `neo`.
- `AppearanceModel` applies it live, through the same path as Light / Dark / System: a
  settings change re-applies the theme, no restart.
- Pickers, both bound to the same setting and kept in sync through it:
  - **Preferences > Appearance > Theme**: a standard `QComboBox` with a swatch icon per scheme
    (background, panel and accent of the current mode) and the one-line description below;
    choosing a scheme applies it immediately (live preview).
  - **View tab > Theme**: a compact drop-down with the same list.

## Contrast

Rules (asserted per scheme and mode by `TestTheme::schemeContrast`): ink >= 7:1 on bg, chrome,
ribbon, panel (>= 4.5:1 accepted on `work`); ink2 >= 4.5:1 on bg, chrome, ribbon, panel; ink3
>= 4.5:1 on bg, ribbon, panel; onGrad >= 4.5:1 on the accent and the gradient; accent >= 4.5:1
on ribbon and panel.

**No palette value was adjusted**: every scheme passes every rule with the normative hex
values. The table also reports pairs that are not rules (right-hand columns):

| Scheme | Mode | ink / surfaces (min) | ink / work | ink2 (min of bg, chrome, ribbon, panel) | ink3 (min of bg, ribbon, panel) | onGrad / accent | accent / panel | line2 / panel | ink3 / chrome | accent / pressed | ok / okTint |
|---|---|---|---|---|---|---|---|---|---|---|---|
| neo | light | 16.30 | 14.48 | 6.92 | 4.55 | 6.29 | 6.29 | 1.48 | 4.34 | 5.10 | 5.21 |
| neo | dark | 15.88 | 18.51 | 11.19 | 6.48 | 6.49 | 5.57 | 1.72 | 6.96 | 4.03 | 5.06 |
| graphite | light | 14.75 | 12.95 | 8.69 | 6.11 | 5.28 | 5.28 | 1.97 | 5.59 | 4.19 | 4.47 |
| graphite | dark | 14.77 | 17.78 | 10.56 | 5.82 | 7.36 | 6.11 | 1.98 | 6.22 | 4.34 | 8.01 |
| paper | light | 14.96 | 13.30 | 8.79 | 5.44 | 6.29 | 6.19 | 1.98 | 4.93 | 5.06 | 6.20 |
| paper | dark | 14.34 | 17.39 | 10.45 | 5.88 | 9.29 | 7.67 | 1.93 | 6.37 | 5.30 | 9.26 |
| harbor | light | 14.43 | 12.51 | 7.94 | 5.25 | 5.47 | 5.47 | 1.96 | 4.87 | 4.29 | 4.40 |
| harbor | dark | 13.68 | 17.24 | 10.09 | 6.11 | 9.90 | 8.31 | 2.12 | 6.75 | 6.11 | 8.43 |
| signal | light | 16.58 | 14.58 | 10.14 | 6.60 | 5.18 | 5.18 | 2.46 | 6.05 | 3.98 | 4.42 |
| signal | dark | 15.81 | 19.18 | 11.15 | 6.17 | 8.64 | 7.29 | 2.40 | 6.71 | 5.58 | 8.60 |
| system | light | 14.60 | 13.18 | 9.11 | 5.75 | 5.38 | 5.20 | 2.10 | 5.36 | 4.23 | 4.67 |
| system | dark | 13.39 | 17.76 | 9.21 | 5.50 | 8.72 | 7.44 | 2.23 | 5.82 | 5.24 | 6.20 |

Reported, not changed (the hex values are normative):

- `line2` as a control border is 1.9 to 2.5:1 on panel in every scheme (Neo 1.5 / 1.7), below
  the 3:1 SPEC.md 11 asks for non-text. Reaching 3:1 needs borders roughly 25 % darker
  (light) / lighter (dark), which visibly changes the look; left to the owner.
- `ink3` on chrome is >= 4.87:1 in every new scheme (Neo light 4.34), so captions on the tab
  strip and status bar pass in the new schemes.
- accent on `pressed` (accent text on a checked button) is 3.98 to 4.34:1 in Graphite light and
  dark, Harbor light, Signal light and System light, and 4.03:1 in Neo dark: fine for icons
  (3:1), short of 4.5:1 for text such as the checked "Light" segment label.
- `ok` on `okTint` (success pill text) is 4.40 to 4.47:1 in Graphite, Harbor and Signal light,
  just under 4.5:1.

## Screenshots

`screenshots/themes/<scheme>-<mode>-<screen>.png` for screens home, text-tools, data-fill,
print and preferences (Appearance tab with the picker), every scheme in light and dark, and
`screenshots/themes/overview.png`: the Text tools screen of all six schemes, light on top,
dark below, labelled. Produced by `TestThemeGallery::themeSchemes` (offscreen, 1440 x 900,
Windows fonts), which also asserts that each scheme applies live with no stylesheet warning and
that the Preferences picker changes the setting and the running theme.

## Token values

Order of the 21 values in the code: bg, chrome, ribbon, work, panel, line, line2, ink, ink2,
ink3, accent, tint, hover, pressed, ctx, ctxLine, ok, okTint, warn, grad (solid), onGrad.
Neo is listed in SPEC.md 4.1.

### Graphite

| Token | Light | Dark |
|---|---|---|
| `bg` | `#F4F4F5` | `#121214` |
| `chrome` | `#EAEAEC` | `#1A1A1D` |
| `ribbon` | `#FFFFFF` | `#202024` |
| `work` | `#DCDCDF` | `#0C0C0E` |
| `panel` | `#FFFFFF` | `#18181B` |
| `line` | `#E2E2E5` | `#2C2C31` |
| `line2` | `#B8B8BE` | `#494950` |
| `ink` | `#18181B` | `#F4F4F5` |
| `ink2` | `#3F3F46` | `#D0D0D5` |
| `ink3` | `#5B5B64` | `#9A9AA3` |
| `accent` | `#2F6DB5` | `#6EA2E0` |
| `tint` | `#E4ECF6` | `#26303D` |
| `hover` | `#EDEDEF` | `#2A2A2F` |
| `pressed` | `#DCE6F3` | `#2E3A4B` |
| `ctx` | `#E4ECF6` | `#26303D` |
| `ctxLine` | `#2F6DB5` | `#6EA2E0` |
| `ok` | `#15803D` | `#4ADE80` |
| `okTint` | `#E8F5EC` | `#123221` |
| `warn` | `#A15C07` | `#E0A93B` |
| `grad` | `#2F6DB5` | `#5B93D6` |
| `onGrad` | `#FFFFFF` | `#0C0C0E` |

### Paper

| Token | Light | Dark |
|---|---|---|
| `bg` | `#FAF8F4` | `#141311` |
| `chrome` | `#F0EDE6` | `#1C1A17` |
| `ribbon` | `#FEFDFB` | `#24211D` |
| `work` | `#E5E0D6` | `#0E0D0B` |
| `panel` | `#FEFDFB` | `#1A1815` |
| `line` | `#E7E2D8` | `#2F2B25` |
| `line2` | `#BDB6A8` | `#4D473E` |
| `ink` | `#1C1917` | `#F5F2EC` |
| `ink2` | `#44403C` | `#D6D0C5` |
| `ink3` | `#6B655C` | `#A39C8F` |
| `accent` | `#2F6B4F` | `#7CC29B` |
| `tint` | `#E3EEE7` | `#243229` |
| `hover` | `#F3F0E9` | `#2B2823` |
| `pressed` | `#DCEAE1` | `#2D4034` |
| `ctx` | `#E3EEE7` | `#243229` |
| `ctxLine` | `#2F6B4F` | `#7CC29B` |
| `ok` | `#166534` | `#6EE7A0` |
| `okTint` | `#E6F2EA` | `#12301F` |
| `warn` | `#9A5B0B` | `#E3AE4F` |
| `grad` | `#2F6B4F` | `#7CC29B` |
| `onGrad` | `#FFFFFF` | `#0E0D0B` |

### Harbor

| Token | Light | Dark |
|---|---|---|
| `bg` | `#F3F6F8` | `#0B141A` |
| `chrome` | `#E8EEF2` | `#111D25` |
| `ribbon` | `#FFFFFF` | `#17262F` |
| `work` | `#D6DFE6` | `#070D11` |
| `panel` | `#FFFFFF` | `#0F1A21` |
| `line` | `#DCE4EA` | `#1F313C` |
| `line2` | `#AEBBC6` | `#3A5160` |
| `ink` | `#0B1F2A` | `#EAF2F6` |
| `ink2` | `#334A58` | `#C3D3DC` |
| `ink3` | `#55697A` | `#8FA6B4` |
| `accent` | `#0F766E` | `#4FD1C5` |
| `tint` | `#DDF0EE` | `#12302F` |
| `hover` | `#EDF2F5` | `#1C2E39` |
| `pressed` | `#CFE9E6` | `#17403F` |
| `ctx` | `#DDF0EE` | `#12302F` |
| `ctxLine` | `#0F766E` | `#4FD1C5` |
| `ok` | `#15803D` | `#4ADE80` |
| `okTint` | `#E4F4EA` | `#0F2E1D` |
| `warn` | `#A15C07` | `#E5B04A` |
| `grad` | `#0F766E` | `#4FD1C5` |
| `onGrad` | `#FFFFFF` | `#06161A` |

### Signal

| Token | Light | Dark |
|---|---|---|
| `bg` | `#FAFAF9` | `#0E0E0D` |
| `chrome` | `#F1F0EE` | `#171716` |
| `ribbon` | `#FFFFFF` | `#1F1F1D` |
| `work` | `#E3E2DF` | `#080807` |
| `panel` | `#FFFFFF` | `#141413` |
| `line` | `#E5E3DF` | `#2B2A28` |
| `line2` | `#A8A59E` | `#55534E` |
| `ink` | `#111111` | `#FAFAF9` |
| `ink2` | `#3A3936` | `#D6D4CF` |
| `ink3` | `#5C5A55` | `#A19E97` |
| `accent` | `#C2410C` | `#FB923C` |
| `tint` | `#FDEBDD` | `#3A2412` |
| `hover` | `#F4F3F0` | `#262523` |
| `pressed` | `#FBDCC4` | `#4A2C14` |
| `ctx` | `#FDEBDD` | `#3A2412` |
| `ctxLine` | `#C2410C` | `#FB923C` |
| `ok` | `#15803D` | `#4ADE80` |
| `okTint` | `#E6F4EA` | `#122C1B` |
| `warn` | `#92400E` | `#FBBF24` |
| `grad` | `#C2410C` | `#FB923C` |
| `onGrad` | `#FFFFFF` | `#140A02` |

### System

| Token | Light | Dark |
|---|---|---|
| `bg` | `#F3F3F3` | `#202020` |
| `chrome` | `#EBEBEB` | `#2B2B2B` |
| `ribbon` | `#FBFBFB` | `#2F2F2F` |
| `work` | `#E0E0E0` | `#181818` |
| `panel` | `#FFFFFF` | `#272727` |
| `line` | `#E0E0E0` | `#3A3A3A` |
| `line2` | `#B3B3B3` | `#5C5C5C` |
| `ink` | `#1A1A1A` | `#FFFFFF` |
| `ink2` | `#3D3D3D` | `#D6D6D6` |
| `ink3` | `#5F5F5F` | `#A6A6A6` |
| `accent` | `#0F6CBD` | `#60CDFF` |
| `tint` | `#E1EDF8` | `#1F3A4D` |
| `hover` | `#F0F0F0` | `#353535` |
| `pressed` | `#D6E6F5` | `#1F4A66` |
| `ctx` | `#E1EDF8` | `#1F3A4D` |
| `ctxLine` | `#0F6CBD` | `#60CDFF` |
| `ok` | `#107C10` | `#6CCB5F` |
| `okTint` | `#E5F3E5` | `#1B3A1B` |
| `warn` | `#8A5A00` | `#E0B03A` |
| `grad` | `#0F6CBD` | `#60CDFF` |
| `onGrad` | `#FFFFFF` | `#00263A` |
