#!/usr/bin/env python3
# make-marketing-assets.py
#
# Copyright (C) 2026  gLabels Neo contributors
#
# This file is part of gLabels Neo.
#
# gLabels Neo is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# gLabels Neo is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with gLabels Neo.  If not, see <http://www.gnu.org/licenses/>.

"""Turn the raw captures of TestMarketingShots into the README and website pictures.

    GLABELS_MARKETING_SHOTS=raw ctest -R MarketingShots      (or run TestMarketingShots directly)
    python3 scripts/make-marketing-assets.py raw

Needs Pillow.  Writes
  docs/assets/readme/*.png   optimised PNGs for README.md
  site/assets/img/*.webp     WebP for the website (when the site folder exists)
"""

import os
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
README_DIR = os.path.join(ROOT, "docs", "assets", "readme")
SITE_DIR = os.path.join(ROOT, "site", "assets", "img")

SCHEMES = ["graphite", "neo", "paper", "harbor", "signal", "system"]

# README name -> raw name (without -light/-dark)
SINGLES = {
    "hero": "editor-text",
    "editor": "editor-home",
    "round-label": "editor-round",
    "fill": "fill",
    "templates": "templates",
    "designer": "designer",
    "welcome": "welcome",
}


def load(raw, name):
    return Image.open(os.path.join(raw, name + ".png")).convert("RGB")


def save_png(image, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    image.save(path, optimize=True)
    print("  %-48s %6.0f KB" % (os.path.relpath(path, ROOT), os.path.getsize(path) / 1024))


def save_webp(image, path, quality=86):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    image.save(path, "WEBP", quality=quality, method=6)
    print("  %-48s %6.0f KB" % (os.path.relpath(path, ROOT), os.path.getsize(path) / 1024))


def montage(raw, mode, cell_w=640):
    """The six colour schemes of one mode in a 3 x 2 grid."""
    first = load(raw, "theme-graphite-%s" % mode)
    cell_h = round(first.height * cell_w / first.width)
    gap = 12
    cols, rows = 3, 2
    bg = (242, 244, 247) if mode == "light" else (13, 15, 20)
    sheet = Image.new("RGB", (cols * cell_w + (cols + 1) * gap, rows * cell_h + (rows + 1) * gap), bg)
    for i, scheme in enumerate(SCHEMES):
        shot = load(raw, "theme-%s-%s" % (scheme, mode)).resize((cell_w, cell_h), Image.LANCZOS)
        x = gap + (i % cols) * (cell_w + gap)
        y = gap + (i // cols) * (cell_h + gap)
        sheet.paste(shot, (x, y))
    return sheet


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    raw = sys.argv[1]

    print("README pictures")
    for out, src in SINGLES.items():
        for mode in ("light", "dark"):
            save_png(load(raw, "%s-%s" % (src, mode)), os.path.join(README_DIR, "%s-%s.png" % (out, mode)))
    for mode in ("light", "dark"):
        save_png(montage(raw, mode), os.path.join(README_DIR, "themes-%s.png" % mode))

    if os.path.isdir(os.path.join(ROOT, "site")):
        print("Website pictures")
        for out, src in SINGLES.items():
            for mode in ("light", "dark"):
                save_webp(load(raw, "%s-%s" % (src, mode)), os.path.join(SITE_DIR, "%s-%s.webp" % (out, mode)))
        for scheme in SCHEMES:
            for mode in ("light", "dark"):
                shot = load(raw, "theme-%s-%s" % (scheme, mode))
                save_webp(shot.resize((960, round(shot.height * 960 / shot.width)), Image.LANCZOS),
                          os.path.join(SITE_DIR, "theme-%s-%s.webp" % (scheme, mode)), 82)


if __name__ == "__main__":
    main()
