# gLabels Neo — Visual Identity & Brand Kit

> **A modernized fork of gLabels**  
> Design and print labels, barcodes and thermal labels — free and open source.

---

## Brand Concept

The **gLabels Neo** mark combines three functional visual cues into one simple, geometric, high-contrast symbol built on a `64 × 64` master grid:

1. **Die-cut adhesive label (`56 × 48`, `r = 10`)**: Landscape sticker proportions with a tactile **bottom-right peeling corner** (avoiding the top-right dog-ear associated with generic text files).
2. **Geometric barcode rhythm**: Four varied-width rounded bars (`thick–thin–medium–thin`) evoking thermal, product, and shipping barcodes.
3. **Four-point Neo spark**: A crisp precision star tucked in the upper-right quadrant symbolizing modernization ("Neo") and print alignment.

---

## Naming & Attribution Rule

- Always write the product name as **`gLabels Neo`** (lowercase `g`, capital `L`, space, capital `N`).
- Whenever the primary lockup appears on websites, READMEs, release notes, or splash screens, prefer the **with-tagline** variant carrying the attribution line: **`A modernized fork of gLabels`**.
- Tagline-free variants (`logo-horizontal.svg`, `logo-stacked.svg`, `logo-horizontal-onDark.svg`) are reserved for compact UI chrome (e.g. navbars or footers) where the attribution already appears nearby or space is tightly constrained.

---

## Color Palette & WCAG 2.1 Contrast Table

All contrast ratios below are computed per the WCAG 2.1 relative luminance formula against pure white (`#FFFFFF`) and near-black (`#090D1A`).
- **AA Normal Text**: requires **≥ 4.50:1**
- **AA Large Text / UI Components**: requires **≥ 3.00:1**

### Brand Gradient Stops & Tints

| Token | Hex | Role | vs `#FFFFFF` | AA Normal / Large (on White) | vs `#090D1A` | AA Normal / Large (on Near-Black) |
| :--- | :--- | :--- | ---: | :--- | ---: | :--- |
| `--gn-brand-violet` | `#7C3AED` | Primary gradient start (0%) | `5.70:1` | **PASS** / **PASS** | `3.40:1` | FAIL / **PASS** |
| `--gn-brand-indigo` | `#4F46E5` | Primary gradient midpoint (50%) | `6.29:1` | **PASS** / **PASS** | `3.08:1` | FAIL / **PASS** |
| `--gn-brand-blue` | `#2563EB` | Primary gradient end (100%) | `5.17:1` | **PASS** / **PASS** | `3.75:1` | FAIL / **PASS** |
| `--gn-brand-violet-light` | `#A78BFA` | Dark-mode accent gradient start (0%) | `2.72:1` | FAIL / FAIL | `7.12:1` | **PASS** / **PASS** |
| `--gn-brand-indigo-light` | `#818CF8` | Dark-mode accent gradient mid (50%) | `2.98:1` | FAIL / FAIL | `6.49:1` | **PASS** / **PASS** |
| `--gn-brand-blue-light` | `#60A5FA` | Dark-mode accent gradient end (100%) | `2.54:1` | FAIL / FAIL | `7.62:1` | **PASS** / **PASS** |
| `--gn-brand-tint-violet` | `#EDE9FE` | Peeled flap start / soft light surface | `1.19:1` | FAIL / FAIL | `16.32:1` | **PASS** / **PASS** |
| `--gn-brand-tint-blue` | `#DBEAFE` | Peeled flap end / soft light surface | `1.22:1` | FAIL / FAIL | `15.88:1` | **PASS** / **PASS** |

### Neutral Scale

| Token | Hex | Role | vs `#FFFFFF` | AA Normal / Large (on White) | vs `#090D1A` | AA Normal / Large (on Near-Black) |
| :--- | :--- | :--- | ---: | :--- | ---: | :--- |
| `--gn-neutral-0` | `#FFFFFF` | Light canvas / mark ink on brand | `1.00:1` | FAIL / FAIL | `19.37:1` | **PASS** / **PASS** |
| `--gn-neutral-50` | `#F8FAFC` | Subtle light surface / dark primary text | `1.05:1` | FAIL / FAIL | `18.51:1` | **PASS** / **PASS** |
| `--gn-neutral-100` | `#F1F5F9` | Muted light surface | `1.10:1` | FAIL / FAIL | `17.68:1` | **PASS** / **PASS** |
| `--gn-neutral-200` | `#E2E8F0` | Subtle light border | `1.23:1` | FAIL / FAIL | `15.71:1` | **PASS** / **PASS** |
| `--gn-neutral-300` | `#CBD5E1` | Default light border / dark secondary text | `1.48:1` | FAIL / FAIL | `13.05:1` | **PASS** / **PASS** |
| `--gn-neutral-400` | `#94A3B8` | Dark muted text & dark-mode tagline | `2.56:1` | FAIL / FAIL | `7.55:1` | **PASS** / **PASS** |
| `--gn-neutral-500` | `#64748B` | Light muted text | `4.76:1` | **PASS** / **PASS** | `4.07:1` | FAIL / **PASS** |
| `--gn-neutral-600` | `#475569` | Light secondary text & light-mode tagline | `7.58:1` | **PASS** / **PASS** | `2.56:1` | FAIL / FAIL |
| `--gn-neutral-700` | `#334155` | Default dark border | `10.35:1` | **PASS** / **PASS** | `1.87:1` | FAIL / FAIL |
| `--gn-neutral-800` | `#1E293B` | Muted dark surface / pill fill | `14.63:1` | **PASS** / **PASS** | `1.32:1` | FAIL / FAIL |
| `--gn-neutral-900` | `#0F172A` | Light primary text / mono-dark mark | `17.85:1` | **PASS** / **PASS** | `1.09:1` | FAIL / FAIL |
| `--gn-neutral-950` | `#090D1A` | Dark canvas (near-black) | `19.37:1` | **PASS** / **PASS** | `1.00:1` | FAIL / FAIL |

### Semantic Status Colors

| Token | Hex | Role | vs `#FFFFFF` | AA Normal / Large (on White) | vs `#090D1A` | AA Normal / Large (on Near-Black) |
| :--- | :--- | :--- | ---: | :--- | ---: | :--- |
| `--gn-success` | `#059669` | Success icon / badge fill | `3.77:1` | FAIL / **PASS** | `5.14:1` | **PASS** / **PASS** |
| `--gn-success-text-light` | `#047857` | Success text on light surfaces | `5.48:1` | **PASS** / **PASS** | `3.53:1` | FAIL / **PASS** |
| `--gn-success-text-dark` | `#34D399` | Success text on dark surfaces | `1.92:1` | FAIL / FAIL | `10.08:1` | **PASS** / **PASS** |
| `--gn-warning` | `#D97706` | Warning icon / badge fill | `3.19:1` | FAIL / **PASS** | `6.08:1` | **PASS** / **PASS** |
| `--gn-warning-text-light` | `#B45309` | Warning text on light surfaces | `5.02:1` | **PASS** / **PASS** | `3.86:1` | FAIL / **PASS** |
| `--gn-warning-text-dark` | `#FBBF24` | Warning text on dark surfaces | `1.67:1` | FAIL / FAIL | `11.60:1` | **PASS** / **PASS** |
| `--gn-danger` | `#DC2626` | Danger icon / badge fill | `4.83:1` | **PASS** / **PASS** | `4.01:1` | FAIL / **PASS** |
| `--gn-danger-text-light` | `#B91C1C` | Danger text on light surfaces | `6.47:1` | **PASS** / **PASS** | `2.99:1` | FAIL / FAIL |
| `--gn-danger-text-dark` | `#F87171` | Danger text on dark surfaces | `2.77:1` | FAIL / FAIL | `7.00:1` | **PASS** / **PASS** |

> **Accessibility takeaway**: All three core gradient stops (`#7C3AED`, `#4F46E5`, `#2563EB`) pass WCAG AA normal text against `#FFFFFF` (5.17–6.29:1), so white graphics on the brand mark and gradient `"Neo"` text on light backgrounds both meet AA. On dark surfaces (`#090D1A`), `"Neo"` text switches to the luminous gradient (`#A78BFA` → `#818CF8` → `#60A5FA`, 6.49–7.62:1) to maintain AA/AAA readability.

---

## Clear-Space & Minimum-Size Rules

### Clear Space
Define **`x`** as **half the height of the label body** (on the `64 × 64` mark grid, the label body height is `48`, so `x = 24`; equivalently, `0.5 × mark height`).
- Maintain at least **`0.5x`** (`12` units on the 64-unit grid) of unobstructed clear space on all four sides around standalone marks and lockups.
- In `app-icon.svg` (`1024 × 1024`), the rounded-square plate occupies `832 × 832` (`96px` outer margin on every side), preserving OS safe-area for macOS `.icns`, Windows `.ico`, and Linux desktop icon themes.

### Minimum Sizes

| Asset | Digital Minimum | Print Minimum | Notes |
| :--- | :--- | :--- | :--- |
| `mark.svg` / `mark-mono-*.svg` | `20px` height | `6 mm` | Below `20px`, switch to `app-icon-small.svg` or `favicon.svg` |
| `app-icon-small.svg` / `favicon.svg` | `16px × 16px` | — | Simplified 3-bar geometry engineered for 16px & 32px pixel grids |
| `logo-horizontal.svg` | `24px` height (`~110px` wide) | `12 mm` height | Tagline-free compact header use |
| `logo-horizontal-tagline.svg` | `36px` height (`~150px` wide) | `16 mm` height | Keeps tagline legible at ≥ `~6px` cap-height |
| `logo-stacked.svg` | `64px` height | `20 mm` height | Square badges, about dialogs, cards |
| `logo-stacked-tagline.svg` | `84px` height | `26 mm` height | Hero sections, documentation headers, splash screens |

---

## Do / Don't Guidelines

### Do
- Use the canonical 135° gradient (`#7C3AED` → `#4F46E5` → `#2563EB`) via `tokens.css` / `tokens.json` or the named `<linearGradient id="glabels-brand-gradient">` definitions.
- Use `logo-horizontal-onDark.svg` / `logo-horizontal-tagline-onDark.svg` on dark backgrounds (`#090D1A`, `#0F172A`, `#1E293B`).
- Use `mark-mono-dark.svg` or `mark-mono-light.svg` for single-color printing, thermal test labels, embroidery, or low-ink stamps.
- Use `app-icon-small.svg` (or `favicon.svg`) when rendering at `16px–32px`.
- Include the attribution tagline (`A modernized fork of gLabels`) in primary lockups and social/docs banners.

### Don't
- Do **not** stretch, skew, rotate, or alter the aspect ratio of the mark or wordmark.
- Do **not** place the light-mode lockup (`#0F172A` text) on dark backgrounds or the `-onDark` lockup (`#F8FAFC` text) on light backgrounds.
- Do **not** recolor the gradient to unapproved hues or invert the peeled corner to the top-right (which makes it read as a generic document icon).
- Do **not** add heavy outer glows, beveled 3D effects, or drop shadows to the flat horizontal/stacked lockups.
- Do **not** alter the capitalization of `gLabels Neo` (never `Glabels Neo`, `GLABELS NEO`, or `glabels-neo` in display lockups).

---

## File Guide & Usage

| File | Dimensions / `viewBox` | Intended Use |
| :--- | :--- | :--- |
| `mark.svg` | `0 0 64 64` | Primary full-color gradient symbol for light & dark surfaces |
| `mark-mono-dark.svg` | `0 0 64 64` | Single-color dark (`#0F172A`) symbol with true `evenodd` cutouts for light surfaces & print |
| `mark-mono-light.svg` | `0 0 64 64` | Single-color light (`#F8FAFC`) symbol with true `evenodd` cutouts for dark surfaces |
| `logo-horizontal.svg` | `0 0 364 80` | Compact horizontal lockup for light backgrounds (navbars, toolbars) |
| `logo-horizontal-tagline.svg` | `0 0 364 92` | **Primary** horizontal lockup with attribution tagline for light backgrounds |
| `logo-horizontal-onDark.svg` | `0 0 364 80` | Compact horizontal lockup for dark backgrounds |
| `logo-horizontal-tagline-onDark.svg` | `0 0 364 92` | **Primary** horizontal lockup with attribution tagline for dark backgrounds |
| `logo-stacked.svg` | `0 0 280 148` | Compact vertical/stacked lockup for light backgrounds (about dialogs, avatars) |
| `logo-stacked-tagline.svg` | `0 0 280 174` | **Primary** vertical/stacked lockup with attribution tagline for light backgrounds |
| `app-icon.svg` | `0 0 1024 1024` | Master square app icon (`832 × 832` rounded-square plate, `96px` safe area) for `.icns`/`.ico`/PNG export |
| `app-icon-small.svg` | `0 0 64 64` | Simplified 3-bar icon engineered specifically for `16px` and `32px` exports (`.ico`, tray, taskbar) |
| `favicon.svg` | `0 0 32 32` | Web SVG favicon with `@media (prefers-color-scheme: dark)` luminance boost & rim |
| `social-card.svg` | `0 0 1200 630` | Open Graph / GitHub social preview card (`og:image`, 1200×630) |
| `tokens.css` | CSS Custom Properties | Drop-in stylesheet for web/GTK/Qt-adjacent webviews supporting light & dark themes |
| `tokens.json` | JSON | Machine-readable design token export mirroring `tokens.css` |
| `README.md` | Markdown | Brand guidelines, contrast table, and asset reference |

### Note on Live `<text>` vs. Outlined Wordmarks
All lockups (`logo-*.svg`) and `social-card.svg` keep typography as live `<text>` elements using the stack `'Inter', 'Segoe UI', system-ui, sans-serif`. Outlined-path duplicates were intentionally **skipped** because converting fonts to outlines without bundling a specific licensed/local `Inter` binary produces platform-dependent metrics or bulky, non-editable SVG path data. If static path outlines are later required for print or headless PNG rasterization without `Inter` installed, convert the live `<text>` lockups in Inkscape (`Path → Object to Path`) or `resvg` with the `Inter` font family installed.

---

## Provenance Statement

All SVG artwork, geometric paths, compositions, and design tokens in this brand kit are **100% original** hand-authored vector creations designed specifically for **gLabels Neo**. No paths, silhouettes, or visual elements were traced, copied, or derived from the original gLabels logo or any third-party brand identity.
