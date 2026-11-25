# Pictures and samples for the README and website

| Folder | What |
|---|---|
| `samples/` | Small, realistic projects used for the pictures and offered as examples: `shipping-label.glabels`, `shipping-label-merge.glabels` (with `${field}` placeholders) plus `shipping-orders.csv` to import on the Fill page, and a round `product-label.glabels`. They are plain gLabels files; open them in gLabels Neo. |
| `readme/` | Pictures used by `README.md`, each in a light and a dark version. Generated: do not edit by hand. |

## Regenerating the pictures

The pictures are real screenshots of the real windows, taken off-screen so they are reproducible
and the same on every machine. Run this after a visible UI change, then commit the result.

```bash
# 1. Capture raw screenshots (needs a build with the test targets)
cmake --build build --target TestMarketingShots
QT_QPA_PLATFORM=offscreen GLABELS_MARKETING_SHOTS=raw ./build/glabels/unit_tests/TestMarketingShots

# 2. Turn them into the README pictures (needs Python 3 and Pillow)
python3 scripts/make-marketing-assets.py raw
```

On Windows set `QT_QPA_FONTDIR=C:\Windows\Fonts` as well, otherwise the off-screen platform draws empty
boxes instead of text. The samples name the *Segoe UI* font, so the pictures are taken on Windows;
elsewhere Qt substitutes a similar font and the pictures will look slightly different.

`TestMarketingShots` does nothing (it reports "skipped") unless `GLABELS_MARKETING_SHOTS` is set, so
it is safe in the normal `ctest` run.
