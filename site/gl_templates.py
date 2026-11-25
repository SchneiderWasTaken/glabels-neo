# gl_templates.py
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

"""Reads the label template database (templates/*.xml) into plain Python objects.

Only what the website needs: who makes it, the part number, the paper, the shape and size of one label
and where the labels sit on the sheet.  Lengths are kept in points (1/72 inch), like the application does.
"""

import glob
import os
import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field

PT_PER_IN = 72.0
PT_PER_MM = 72.0 / 25.4

_UNITS = {"pt": 1.0, "in": PT_PER_IN, "mm": PT_PER_MM, "cm": PT_PER_MM * 10.0, "pc": 12.0}


def length(text, default=0.0):
    """'2.5in' / '66.7mm' / '19' (points) -> points."""
    if text is None or text.strip() == "":
        return default
    m = re.fullmatch(r"\s*(-?(?:[0-9]+\.?[0-9]*|\.[0-9]+))\s*([a-z]*)\s*", text)
    if not m:
        return default
    return float(m.group(1)) * _UNITS.get(m.group(2) or "pt", 1.0)


def slug(text):
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")


@dataclass
class Paper:
    id: str
    name: str
    width: float
    height: float


@dataclass
class Layout:
    nx: int
    ny: int
    x0: float
    y0: float
    dx: float
    dy: float


@dataclass
class Template:
    brand: str
    part: str
    description: str            # "Address labels"
    paper: Paper                # sheet, or the roll
    shape: str                  # rectangle | round | ellipse | cd | continuous | path
    width: float                # one label, points
    height: float
    corner: float = 0.0         # rounded corner radius of a rectangle
    hole: float = 0.0           # CD hole radius
    margin: float = 0.0
    layouts: list = field(default_factory=list)
    categories: list = field(default_factory=list)
    is_roll: bool = False
    base_part: str = ""         # the part this one is the same as (== part when it is the original)
    element: ET.Element = None  # the label element of the original, for writing a starter file

    @property
    def per_sheet(self):
        return sum(l.nx * l.ny for l in self.layouts) or 1

    @property
    def key(self):
        return (self.brand, self.part)

    @property
    def group(self):
        return (self.brand, self.base_part)

    @property
    def slug(self):
        return getattr(self, "slug_override", None) or slug(self.part)

    @property
    def brand_slug(self):
        return slug(self.brand)


def _label_size(el, tag):
    """Width and height of one label, from its element."""
    if tag == "Label-rectangle" or tag == "Label-ellipse":
        return length(el.get("width")), length(el.get("height"))
    if tag == "Label-round":
        r = length(el.get("radius"))
        return 2 * r, 2 * r
    if tag == "Label-cd":
        r = length(el.get("radius"))
        w = length(el.get("width"), 2 * r)
        h = length(el.get("height"), 2 * r)
        return w, h
    if tag == "Label-continuous":
        return length(el.get("width")), length(el.get("default_height"))
    return 0.0, 0.0


def _read_papers(root):
    papers = {}
    for p in ET.parse(os.path.join(root, "paper-sizes.xml")).getroot().iter("Paper-size"):
        papers[p.get("id")] = Paper(p.get("id"), p.get("name") or p.get("id"),
                                    length(p.get("width")), length(p.get("height")))
    return papers


def _read_categories(root):
    out = {}
    for c in ET.parse(os.path.join(root, "categories.xml")).getroot().iter("Category"):
        out[c.get("id")] = c.get("_name") or c.get("name") or c.get("id")
    return out


def read_templates(root):
    """All templates of the database as a list of Template, equivalents resolved."""
    papers = _read_papers(root)
    originals = {}
    equivs = []
    for path in sorted(glob.glob(os.path.join(root, "*-templates.xml"))):
        for el in ET.parse(path).getroot().iter("Template"):
            brand, part = el.get("brand"), el.get("part")
            if not brand or not part:
                continue
            if el.get("equiv"):
                equivs.append((brand, part, el.get("equiv")))
                continue
            label = None
            for child in el:
                if child.tag.startswith("Label-"):
                    label = child
                    break
            if label is None:
                continue
            size = el.get("size", "")
            if size == "roll":
                paper = Paper("roll", "Roll", length(el.get("roll_width")), 0.0)
            elif size == "other":
                paper = Paper("other", "Custom size", length(el.get("width")), length(el.get("height")))
            else:
                paper = papers.get(size)
                if paper is None:
                    continue
            w, h = _label_size(label, label.tag)
            if label.tag == "Label-path":
                w, h = length(el.get("width")), length(el.get("height"))
            if w <= 0 or h <= 0:
                continue
            t = Template(
                brand=brand, part=part,
                description=el.get("_description") or el.get("description") or "Labels",
                paper=paper, shape=label.tag[len("Label-"):],
                width=w, height=h,
                corner=length(label.get("round")),
                hole=length(label.get("hole")),
                is_roll=(size == "roll"),
                base_part=part, element=label,
                categories=[m.get("category") for m in el.findall("Meta") if m.get("category")],
            )
            for mm in label.findall("Markup-margin"):
                t.margin = length(mm.get("size"), length(mm.get("x_size")))
            for lay in label.findall("Layout"):
                t.layouts.append(Layout(int(lay.get("nx", "1")), int(lay.get("ny", "1")),
                                        length(lay.get("x0")), length(lay.get("y0")),
                                        length(lay.get("dx")), length(lay.get("dy"))))
            originals[(brand, part)] = t

    result = list(originals.values())
    for brand, part, base in equivs:
        seen = set()
        src = originals.get((brand, base))
        # An equivalent of an equivalent: follow the chain
        while src is None and (brand, base) not in seen:
            seen.add((brand, base))
            nxt = [e for e in equivs if e[0] == brand and e[1] == base]
            if not nxt:
                break
            base = nxt[0][2]
            src = originals.get((brand, base))
        if src is None:
            continue
        copy = Template(**{**src.__dict__, "part": part, "base_part": src.part})
        result.append(copy)

    # Unique, stable page names inside a brand
    taken = {}
    for t in sorted(result, key=lambda x: (x.brand, x.part)):
        key = (t.brand_slug, t.slug)
        taken[key] = taken.get(key, 0) + 1
        if taken[key] > 1:
            t.slug_override = "%s-%d" % (t.slug, taken[key])
    result.sort(key=lambda x: (x.brand.lower(), natural(x.part)))
    return result, _read_categories(root)


def natural(text):
    return [int(p) if p.isdigit() else p.lower() for p in re.split(r"(\d+)", text)]
