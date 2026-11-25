#!/usr/bin/env python3
# build.py
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

"""Builds the gLabels Neo website (plain static files, no dependencies beyond the Python standard library).

    python3 site/build.py [output-folder]          (default: site/dist)

    SITE_ORIGIN   scheme and host the site is served from  (default https://schneiderwastaken.github.io)
    SITE_BASE     path prefix, "" for a custom domain       (default /glabels-neo)

The pages are:
  index.html                       the landing page (site/src/index.html)
  templates/                       search, then one page per maker and one per label template,
                                   generated from templates/*.xml, each with a starter .glabels file
  404.html, robots.txt, sitemap.xml
"""

import html
import json
import os
import shutil
import sys
from collections import defaultdict
from xml.etree import ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

import gl_templates as gt  # noqa: E402

ORIGIN = os.environ.get("SITE_ORIGIN", "https://schneiderwastaken.github.io").rstrip("/")
BASE = os.environ.get("SITE_BASE", "/glabels-neo").rstrip("/")
REPO = "https://github.com/SchneiderWasTaken/glabels-neo"
DOWNLOAD = REPO + "/releases/latest/download/"
SITE_NAME = "gLabels Neo"
OG_IMAGE = "/assets/img/og.png"

esc = html.escape


def url(path):
    """Site-relative path -> href"""
    return BASE + path


def absolute(path):
    return ORIGIN + BASE + path


# ---------------------------------------------------------------------------------------------
# Lengths for people
# ---------------------------------------------------------------------------------------------

def _trim(value, places):
    text = ("%." + str(places) + "f") % value
    return text.rstrip("0").rstrip(".") if "." in text else text


def inches(pt):
    """2.625 in -> '2-5/8 in' when it is a sixteenth, else '2.625 in'."""
    v = pt / gt.PT_PER_IN
    sixteenths = round(v * 16)
    if abs(v * 16 - sixteenths) < 0.03 and sixteenths % 16 != 0:
        whole, rem = divmod(sixteenths, 16)
        a, b = rem, 16
        while a % 2 == 0 and b % 2 == 0:
            a //= 2
            b //= 2
        return ("%d-%d/%d in" % (whole, a, b)) if whole else ("%d/%d in" % (a, b))
    return _trim(v, 3) + " in"


def millimetres(pt):
    return _trim(pt / gt.PT_PER_MM, 1) + " mm"


def both(pt, metric_first):
    return ("%s (%s)" % (millimetres(pt), inches(pt))) if metric_first else ("%s (%s)" % (inches(pt), millimetres(pt)))


def dims(t):
    """'2-5/8 x 1 in' for titles."""
    if t.shape in ("round", "cd"):
        return inches(t.width) + " diameter" if not _metric(t) else millimetres(t.width) + " diameter"
    if _metric(t):
        return "%s x %s" % (_trim(t.width / gt.PT_PER_MM, 1), millimetres(t.height))
    a, b = inches(t.width), inches(t.height)
    return a[:-3] + " x " + b if a.endswith(" in") and b.endswith(" in") else a + " x " + b


def _metric(t):
    return t.paper.id not in ("US-Letter", "US-Legal", "US-Executive", "other") and t.paper.id != "roll" \
        and not t.paper.id.startswith("US-")


SHAPE_NAMES = {"rectangle": "Rectangular", "round": "Round", "ellipse": "Oval", "cd": "CD/DVD",
               "continuous": "Continuous roll", "path": "Die-cut"}


# ---------------------------------------------------------------------------------------------
# Page frame
# ---------------------------------------------------------------------------------------------

NAV = [
    ("Features", "/#features"),
    ("Templates", "/templates/"),
    ("Download", "/#download"),
    ("FAQ", "/#faq"),
]

THEME_INIT = ("try{var t=localStorage.getItem('gn-theme');"
              "if(t)document.documentElement.setAttribute('data-theme',t)}catch(e){}")


def page(title, description, path, body, jsonld=None, extra_head="", body_class="", noindex=False):
    canonical = absolute(path)
    og = absolute(OG_IMAGE)
    ld = ""
    for block in (jsonld or []):
        ld += '<script type="application/ld+json">%s</script>\n' % json.dumps(block, separators=(",", ":"))
    nav = "".join('<a href="%s">%s</a>' % (esc(url(p)), esc(n)) for n, p in NAV)
    robots = '<meta name="robots" content="noindex">\n' if noindex else ""
    return """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<meta name="description" content="{desc}">
<link rel="canonical" href="{canonical}">
{robots}<meta name="theme-color" content="#4f46e5">
<meta property="og:type" content="website">
<meta property="og:site_name" content="{site}">
<meta property="og:title" content="{title}">
<meta property="og:description" content="{desc}">
<meta property="og:url" content="{canonical}">
<meta property="og:image" content="{og}">
<meta name="twitter:card" content="summary_large_image">
<link rel="icon" type="image/svg+xml" href="{favicon}">
<link rel="stylesheet" href="{css}">
<script>{theme_init}</script>
{extra_head}{ld}</head>
<body class="{body_class}">
<a class="skip" href="#main">Skip to content</a>
<header class="top">
  <div class="wrap top-in">
    <a class="brand" href="{home}" aria-label="{site} home">
      <img class="logo-light" src="{logo}" alt="" width="150" height="40">
      <img class="logo-dark" src="{logo_dark}" alt="" width="150" height="40">
    </a>
    <nav class="nav" aria-label="Main">{nav}</nav>
    <button class="mode" id="mode" type="button" aria-label="Switch between light and dark">
      <svg viewBox="0 0 24 24" width="18" height="18" aria-hidden="true"><path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round"/></svg>
    </button>
    <a class="btn btn-small" href="{dl}">Download</a>
  </div>
</header>
<main id="main">
{body}
</main>
<footer class="foot">
  <div class="wrap foot-in">
    <div>
      <img class="logo-light" src="{logo}" alt="{site}" width="150" height="40">
      <img class="logo-dark" src="{logo_dark}" alt="{site}" width="150" height="40">
      <p>Free, open-source label software for Windows, macOS and Linux.</p>
    </div>
    <div>
      <h2>Product</h2>
      <a href="{features}">Features</a>
      <a href="{templates}">Label templates</a>
      <a href="{dl}">Download</a>
      <a href="{repo}/releases">Release notes</a>
    </div>
    <div>
      <h2>Project</h2>
      <a href="{repo}">Source code on GitHub</a>
      <a href="{repo}/issues">Report a problem</a>
      <a href="{repo}/blob/main/docs/CONTRIBUTING.md">Contribute</a>
      <a href="{repo}/blob/main/LICENSE">GPL-3.0-or-later license</a>
    </div>
  </div>
  <div class="wrap fine">
    <p>gLabels Neo is an independent fork of <a href="https://github.com/j-evins/glabels-qt">gLabels</a> by Jaye Evins and
    contributors and is not endorsed by them. The label template database is
    &copy; 2001-2026 Jaye Evins and is used under its own permissive license.
    Product names such as Avery, Dymo, Brother, Herma and Zweckform appear only to describe compatible label stock;
    no affiliation or endorsement is implied.</p>
  </div>
</footer>
<script src="{js}" defer></script>
</body>
</html>
""".format(title=esc(title), desc=esc(description), canonical=esc(canonical), og=esc(og), site=SITE_NAME,
           robots=robots, body_class=body_class, extra_head=extra_head, ld=ld, theme_init=THEME_INIT,
           favicon=esc(url("/assets/favicon.svg")), css=esc(url("/assets/style.css")), js=esc(url("/assets/app.js")),
           home=esc(url("/")), logo=esc(url("/assets/logo-horizontal.svg")),
           logo_dark=esc(url("/assets/logo-horizontal-onDark.svg")),
           nav=nav, dl=esc(url("/#download")), body=body, features=esc(url("/#features")),
           templates=esc(url("/templates/")), repo=REPO)


# ---------------------------------------------------------------------------------------------
# Sheet diagram (inline SVG)
# ---------------------------------------------------------------------------------------------

def diagram(t, width=420):
    """The sheet with every label drawn on it, or one label for a roll."""
    if t.is_roll or t.shape in ("continuous", "path") or t.paper.height <= 0:
        w, h = t.width, t.height
        scale = width / max(w, h) * 0.7
        pad = 14
        vw, vh = w * scale + 2 * pad, h * scale + 2 * pad
        return ('<svg class="sheet" viewBox="0 0 %.1f %.1f" role="img" aria-label="One %s label, %s">'
                '<rect x="%d" y="%d" width="%.1f" height="%.1f" rx="%.1f" class="lab"/></svg>'
                % (vw, vh, esc(t.brand + " " + t.part), esc(dims(t)), pad, pad, w * scale, h * scale,
                   min(t.corner * scale, 40)))
    pw, ph = t.paper.width, t.paper.height
    items = []
    for lay in t.layouts:
        for iy in range(lay.ny):
            for ix in range(lay.nx):
                x = lay.x0 + ix * lay.dx
                y = lay.y0 + iy * lay.dy
                if t.shape in ("round", "cd"):
                    cx, cy, r = x + t.width / 2, y + t.height / 2, t.width / 2
                    items.append('<circle cx="%.1f" cy="%.1f" r="%.1f" class="lab"/>' % (cx, cy, r))
                    if t.shape == "cd" and t.hole:
                        items.append('<circle cx="%.1f" cy="%.1f" r="%.1f" class="hole"/>' % (cx, cy, t.hole))
                elif t.shape == "ellipse":
                    items.append('<ellipse cx="%.1f" cy="%.1f" rx="%.1f" ry="%.1f" class="lab"/>'
                                 % (x + t.width / 2, y + t.height / 2, t.width / 2, t.height / 2))
                else:
                    items.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="%.1f" class="lab"/>'
                                 % (x, y, t.width, t.height, t.corner))
    return ('<svg class="sheet" viewBox="0 0 %.1f %.1f" role="img" aria-label="%s: %d labels on a %s sheet">'
            '<rect width="%.1f" height="%.1f" class="paper"/>%s</svg>'
            % (pw, ph, esc(t.brand + " " + t.part), t.per_sheet, esc(t.paper.name), pw, ph, "".join(items)))


# ---------------------------------------------------------------------------------------------
# Starter file
# ---------------------------------------------------------------------------------------------

def starter_file(t):
    """A blank gLabels project on this template, openable in gLabels Neo."""
    if t.is_roll or t.shape in ("continuous", "path"):
        return None
    label = ET.tostring(t.element, encoding="unicode")
    # The element keeps its source indentation as tail text; normalise to a tidy block.
    lines = [line.strip() for line in label.strip().splitlines() if line.strip()]
    label = "\n".join(("    " if line.startswith(("<Label-", "</Label-")) else "      ") + line for line in lines)
    size_attr = ' size="%s"' % t.paper.id
    if t.paper.id == "other":
        size_attr = ' size="other" width="%.2fpt" height="%.2fpt"' % (t.paper.width, t.paper.height)
    return ('<?xml version="1.0"?>\n<Glabels-document version="4.0">\n'
            '  <Template brand="%s" description="%s" part="%s"%s>\n%s\n  </Template>\n'
            '  <Objects id="0" rotate="false"/>\n  <Data/>\n</Glabels-document>\n'
            % (esc(t.brand, True), esc(t.description, True), esc(t.part, True), size_attr, label))


# ---------------------------------------------------------------------------------------------
# Template pages
# ---------------------------------------------------------------------------------------------

def signature(t):
    return (t.paper.id, round(t.paper.width, 0), round(t.paper.height, 0), t.shape,
            round(t.width, 0), round(t.height, 0),
            tuple((l.nx, l.ny, round(l.x0, 0), round(l.y0, 0), round(l.dx, 0), round(l.dy, 0)) for l in t.layouts))


def template_path(t):
    return "/templates/%s/%s/" % (t.brand_slug, t.slug)


def title_for(t):
    if t.is_roll or t.shape == "continuous":
        return "%s %s Label Template (%s) | %s" % (t.brand, t.part, dims(t) if t.width and t.height else "roll", SITE_NAME)
    plural = "label" if t.per_sheet == 1 else "labels"
    return "%s %s Label Template - %d %s per sheet | %s" % (t.brand, t.part, t.per_sheet, plural, SITE_NAME)


def describe(t):
    name = "%s %s" % (t.brand, t.part)
    if t.is_roll or t.shape == "continuous":
        return ("%s label template: %s label for a %s roll. Design and print it free with gLabels Neo "
                "on Windows, macOS or Linux." % (name, dims(t), millimetres(t.paper.width)))
    return ("%s label template: %d %s label%s per %s sheet, each %s. Download a ready-made starter file "
            "and print it free with gLabels Neo." % (name, t.per_sheet, SHAPE_NAMES.get(t.shape, "").lower(),
                                                       "" if t.per_sheet == 1 else "s", t.paper.name, dims(t)))


def template_page(t, same_group, same_layout, related, brand_count):
    name = "%s %s" % (t.brand, t.part)
    metric = _metric(t)
    kind = SHAPE_NAMES.get(t.shape, "Rectangular").lower()
    path = template_path(t)
    starter = starter_file(t)

    rows = [("Maker", esc(t.brand)), ("Part number", esc(t.part)), ("Type", esc(t.description))]
    if t.is_roll:
        rows += [("Roll width", both(t.paper.width, True)), ("Label size", both(t.width, True) + " x " + both(t.height, True))]
    else:
        if t.paper.id == "other":
            paper_text = esc(t.paper.name) + " (%s x %s)" % (millimetres(t.paper.width), millimetres(t.paper.height))
        elif metric:
            paper_text = esc(t.paper.name) + " (%s x %s)" % (_trim(t.paper.width / gt.PT_PER_MM, 1), millimetres(t.paper.height))
        else:
            paper_text = esc(t.paper.name) + " (%s x %s)" % (inches(t.paper.width)[:-3], inches(t.paper.height))
        rows += [("Paper", paper_text),
                 ("Labels per sheet", str(t.per_sheet))]
        if t.layouts and t.shape != "continuous":
            lay = t.layouts[0]
            if len(t.layouts) == 1:
                rows.append(("Layout", "%d across x %d down" % (lay.nx, lay.ny)))
        rows.append(("Shape", SHAPE_NAMES.get(t.shape, "Rectangular")))
        if t.shape in ("round", "cd"):
            rows.append(("Diameter", both(t.width, metric)))
            if t.hole:
                rows.append(("Centre hole", both(2 * t.hole, metric) + " diameter"))
        else:
            rows.append(("Label width", both(t.width, metric)))
            rows.append(("Label height", both(t.height, metric)))
        if t.corner > 0.5 and t.shape == "rectangle":
            rows.append(("Corner radius", both(t.corner, metric)))
        if t.layouts:
            lay = t.layouts[0]
            rows.append(("Left margin", both(lay.x0, metric)))
            rows.append(("Top margin", both(lay.y0, metric)))
            if lay.nx > 1:
                rows.append(("Horizontal pitch", both(lay.dx, metric)))
            if lay.ny > 1:
                rows.append(("Vertical pitch", both(lay.dy, metric)))
    spec = "".join("<tr><th scope=\"row\">%s</th><td>%s</td></tr>" % (k, v) for k, v in rows)

    steps = ["Open gLabels Neo and choose <b>File &gt; New</b>.",
             "Search for <b>%s</b> in the product list and select it. The sheet layout, margins and safe area are set for you." % esc(t.part),
             "Add text, barcodes, images and shapes. Use <b>Data &gt; Fill</b> to print a whole list from a CSV or Excel file.",
             "Choose <b>Print</b>. Print on plain paper first and hold it against the sheet to check the alignment."]
    if starter:
        steps[0] = ("Download the <a href=\"%s\" download>starter file for %s</a> and open it in gLabels Neo, or choose <b>File &gt; New</b> and search for <b>%s</b>."
                    % (esc(url(path + "%s-%s.glabels" % (t.brand_slug, t.slug))), esc(name), esc(t.part)))

    others = [x for x in same_group if x.key != t.key]
    also = ""
    if others:
        also = ('<h2>Same layout, other part numbers</h2><p>%s is the same label as these %s part numbers, so the same template applies:</p>'
                '<ul class="chips">%s</ul>'
                % (esc(name), esc(t.brand), "".join('<li><a href="%s">%s</a></li>' % (esc(url(template_path(x))), esc(x.part)) for x in others[:40])))
    cross = ""
    if same_layout:
        cross = ('<h2>Other makers with an identical sheet layout</h2><p>These sheets have the same label size and positions as %s:</p>'
                 '<ul class="chips">%s</ul>'
                 % (esc(name), "".join('<li><a href="%s">%s %s</a></li>' % (esc(url(template_path(x))), esc(x.brand), esc(x.part)) for x in same_layout[:24])))
    rel = ""
    if related:
        rel = ('<h2>More %s labels</h2><ul class="chips">%s</ul><p><a href="%s">All %d %s templates</a></p>'
               % (esc(t.brand), "".join('<li><a href="%s">%s</a></li>' % (esc(url(template_path(x))), esc(x.part)) for x in related),
                  esc(url("/templates/%s/" % t.brand_slug)), brand_count, esc(t.brand)))

    lead = ("<b>%s</b> is a %s label for %s." % (esc(name), kind,
            ("a %s roll" % millimetres(t.paper.width)) if t.is_roll else ("%s sheets" % esc(t.paper.name))))
    if not t.is_roll:
        lead += (" Each sheet holds <b>%d</b> label%s, each <b>%s</b>%s." %
                 (t.per_sheet, "" if t.per_sheet == 1 else "s", esc(dims(t)), " with rounded corners" if t.corner > 0.5 and t.shape == "rectangle" else ""))

    faq = [
        ("How many labels are on a %s sheet?" % name,
         "%d per sheet%s." % (t.per_sheet, "" if t.is_roll else ", laid out %d across and %d down" % (t.layouts[0].nx, t.layouts[0].ny) if len(t.layouts) == 1 else "")),
        ("What size is a %s label?" % name, "%s." % (dims(t) if t.shape not in ("round", "cd") else "%s across" % dims(t))),
        ("What software prints %s labels?" % name,
         "gLabels Neo has the %s template built in, so you pick the part number and design. It is free and open source for Windows, macOS and Linux." % name),
    ]
    faq_html = "".join("<details><summary>%s</summary><p>%s</p></details>" % (esc(q), esc(a)) for q, a in faq)

    dl = ""
    if starter:
        dl = ('<a class="btn" href="%s" download>Download the %s starter file</a> ' % (esc(url(path + "%s-%s.glabels" % (t.brand_slug, t.slug))), esc(name)))
    body = """
<section class="wrap crumbs"><nav aria-label="Breadcrumb"><a href="{home}">Home</a> / <a href="{tpl}">Label templates</a> / <a href="{brand_url}">{brand}</a> / {name}</nav></section>
<article class="wrap tpl">
  <header class="tpl-head">
    <h1>{name} label template</h1>
    <p class="lead">{lead}</p>
    <p class="actions">{dl}<a class="btn btn-ghost" href="{get}">Get gLabels Neo (free)</a></p>
  </header>
  <div class="tpl-grid">
    <figure class="sheet-fig">{svg}<figcaption>{name}: layout on the sheet (to scale)</figcaption></figure>
    <table class="spec"><caption>{name} specifications</caption><tbody>{spec}</tbody></table>
  </div>
  <h2>Print {name} labels for free</h2>
  <ol class="steps">{steps}</ol>
  {also}{cross}{rel}
  <h2>Questions</h2>
  <div class="faq">{faq}</div>
</article>
""".format(home=esc(url("/")), tpl=esc(url("/templates/")), brand_url=esc(url("/templates/%s/" % t.brand_slug)),
           brand=esc(t.brand), name=esc(name), lead=lead, dl=dl, get=esc(url("/#download")), svg=diagram(t), spec=spec,
           steps="".join("<li>%s</li>" % s for s in steps), also=also, cross=cross, rel=rel, faq=faq_html)

    ld = [
        {"@context": "https://schema.org", "@type": "BreadcrumbList", "itemListElement": [
            {"@type": "ListItem", "position": 1, "name": "Home", "item": absolute("/")},
            {"@type": "ListItem", "position": 2, "name": "Label templates", "item": absolute("/templates/")},
            {"@type": "ListItem", "position": 3, "name": t.brand, "item": absolute("/templates/%s/" % t.brand_slug)},
            {"@type": "ListItem", "position": 4, "name": name},
        ]},
        {"@context": "https://schema.org", "@type": "FAQPage", "mainEntity": [
            {"@type": "Question", "name": q, "acceptedAnswer": {"@type": "Answer", "text": a}} for q, a in faq]},
    ]
    return page(title_for(t), describe(t), path, body, ld)


def brand_page(brand, items):
    slug = items[0].brand_slug
    by_paper = defaultdict(list)
    for t in items:
        by_paper[t.paper.name].append(t)
    sections = ""
    for paper in sorted(by_paper, key=lambda p: (-len(by_paper[p]), p)):
        rows = "".join(
            '<tr><td><a href="%s">%s</a></td><td>%s</td><td>%s</td><td class="num">%d</td></tr>'
            % (esc(url(template_path(t))), esc(t.part), esc(t.description), esc(dims(t)), t.per_sheet)
            for t in by_paper[paper])
        sections += ('<h2>%s (%d)</h2><table class="list"><thead><tr><th>Part</th><th>Type</th><th>Label size</th>'
                     '<th class="num">Per sheet</th></tr></thead><tbody>%s</tbody></table>' % (esc(paper), len(by_paper[paper]), rows))
    body = """
<section class="wrap crumbs"><nav aria-label="Breadcrumb"><a href="{home}">Home</a> / <a href="{tpl}">Label templates</a> / {brand}</nav></section>
<article class="wrap">
  <h1>{brand} label templates</h1>
  <p class="lead">gLabels Neo includes {n} {brand} label templates. Pick your part number, design your labels and print them free, on Windows, macOS or Linux.</p>
  {sections}
</article>""".format(home=esc(url("/")), tpl=esc(url("/templates/")), brand=esc(brand), n=len(items), sections=sections)
    title = "%s Label Templates - %d free templates | %s" % (brand, len(items), SITE_NAME)
    desc = "Free %s label templates for printing: %d part numbers including %s. Built into gLabels Neo for Windows, macOS and Linux." % (
        brand, len(items), ", ".join(t.part for t in items[:3]))
    return page(title, desc, "/templates/%s/" % slug, body)


def templates_index(by_brand, total):
    cards = "".join(
        '<li><a href="%s"><b>%s</b><span>%d templates</span></a></li>'
        % (esc(url("/templates/%s/" % items[0].brand_slug)), esc(brand), len(items))
        for brand, items in sorted(by_brand.items(), key=lambda kv: (-len(kv[1]), kv[0].lower())))
    body = """
<section class="wrap">
  <h1>Label templates</h1>
  <p class="lead">Search {total} built-in label templates by part number. Every one works with gLabels Neo out of the box, and each page has a ready-made starter file.</p>
  <div class="finder">
    <label for="q" class="sr">Part number or maker</label>
    <input id="q" type="search" placeholder="Try 5160, L7160 or Online Labels" autocomplete="off" data-index="{index}">
    <ul id="results" class="results" aria-live="polite"></ul>
  </div>
  <h2>Browse by maker</h2>
  <ul class="brands">{cards}</ul>
  <h2>Your label is not here?</h2>
  <p>gLabels Neo's <b>Product Template Designer</b> builds a custom sheet or roll template in a few steps, with round, oval, rectangular and CD shapes.</p>
</section>""".format(total="{:,}".format(total), cards=cards, index=esc(url("/templates/search.json")))
    ld = [{"@context": "https://schema.org", "@type": "CollectionPage", "name": "Label templates", "url": absolute("/templates/")}]
    return page("Label Templates for Printing - Avery, Online Labels and more | " + SITE_NAME,
                "Search %d free label templates by part number: Avery, Online Labels, Herma, Hisago and more. Print them with gLabels Neo on Windows, macOS or Linux." % total,
                "/templates/", body, ld)



# ---------------------------------------------------------------------------------------------
# Landing page artwork (generated so the page source stays readable)
# ---------------------------------------------------------------------------------------------

POPULAR = [("Avery", p) for p in ("5160", "5163", "5164", "5167", "5195", "8160", "L7160", "L7163", "22806", "5262", "3274.2", "5293")]


def _rand(seed):
    state = seed
    while True:
        state = (state * 1103515245 + 12345) & 0x7FFFFFFF
        yield state / 0x7FFFFFFF


def hero_grid():
    lines = []
    for x in range(100 + 22, 540, 22):
        lines.append('<line x1="%d" y1="134" x2="%d" y2="374"/>' % (x, x))
    for y in range(132 + 22, 376, 22):
        lines.append('<line x1="102" y1="%d" x2="538" y2="%d"/>' % (y, y))
    return "".join(lines)


def hero_datamatrix():
    rnd = _rand(7)
    n, size, x0, y0 = 11, 7, 408, 196
    cells = []
    i = 0
    for r in range(n):
        for c in range(n):
            solid = (c == 0) or (r == n - 1) or (c % 2 == 0 and r == 0) or (r % 2 == 0 and c == n - 1)
            if solid or (0 < c < n - 1 and r < n - 1 and next(rnd) > 0.5 and r > 0):
                cells.append('<rect class="cell" style="--i:%d" x="%d" y="%d" width="%d" height="%d"/>' % (i, x0 + c * size, y0 + r * size, size, size))
                i += 1
    return "".join(cells)


def hero_barcode():
    rnd = _rand(21)
    x, bars, i = 136, [], 0
    while x < 300:
        w = 1.5 + int(next(rnd) * 3) * 1.5
        bars.append('<rect style="--i:%d" x="%.1f" y="304" width="%.1f" height="46"/>' % (i, x, w))
        x += w + 1.5 + int(next(rnd) * 2) * 1.5
        i += 1
    return "".join(bars)

# ---------------------------------------------------------------------------------------------
# Site
# ---------------------------------------------------------------------------------------------

def write(out, rel, text, binary=False):
    path = os.path.join(out, *rel.strip("/").split("/"))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb" if binary else "w", **({} if binary else {"encoding": "utf-8", "newline": "\n"})) as f:
        f.write(text)


def copy_tree(src, dst):
    if os.path.isdir(src):
        shutil.copytree(src, dst, dirs_exist_ok=True)


def main():
    out = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "dist")
    # Empty the folder rather than removing it (a web server may be running in it)
    if os.path.isdir(out):
        for name in os.listdir(out):
            path = os.path.join(out, name)
            shutil.rmtree(path) if os.path.isdir(path) else os.remove(path)
    os.makedirs(out, exist_ok=True)

    # Static assets
    copy_tree(os.path.join(HERE, "assets"), os.path.join(out, "assets"))
    for name in ("logo-horizontal.svg", "logo-horizontal-onDark.svg", "favicon.svg", "mark.svg"):
        shutil.copy(os.path.join(ROOT, "brand", name), os.path.join(out, "assets", name))
    copy_tree(os.path.join(ROOT, "docs", "assets", "samples"), os.path.join(out, "samples"))

    templates, _categories = gt.read_templates(os.path.join(ROOT, "templates"))
    by_brand = defaultdict(list)
    by_group = defaultdict(list)
    by_signature = defaultdict(list)
    for t in templates:
        by_brand[t.brand].append(t)
        by_group[t.group].append(t)
        by_signature[signature(t)].append(t)

    sitemap = ["/", "/templates/"]
    search = []

    for t in templates:
        same_group = by_group[t.group]
        same_layout = [x for x in by_signature[signature(t)] if x.brand != t.brand][:24]
        # Neighbours in the same maker's list on the same paper, other than aliases of this one
        related = [x for x in by_brand[t.brand] if x.paper.id == t.paper.id and x.group != t.group][:14]
        write(out, template_path(t) + "index.html", template_page(t, same_group, same_layout, related, len(by_brand[t.brand])))
        starter = starter_file(t)
        if starter:
            write(out, template_path(t) + "%s-%s.glabels" % (t.brand_slug, t.slug), starter)
        sitemap.append(template_path(t))
        search.append([t.brand, t.part, url(template_path(t)), t.description,
                       t.per_sheet, dims(t) if not (t.is_roll or t.shape == "continuous") else millimetres(t.paper.width) + " roll"])

    for brand, items in by_brand.items():
        write(out, "/templates/%s/index.html" % items[0].brand_slug, brand_page(brand, items))
        sitemap.append("/templates/%s/" % items[0].brand_slug)

    write(out, "/templates/index.html", templates_index(by_brand, len(templates)))
    write(out, "/templates/search.json", json.dumps(search, separators=(",", ":")))

    # Landing page, 404, robots, sitemap
    with open(os.path.join(HERE, "src", "index.html"), encoding="utf-8") as f:
        landing = f.read()
    existing = {t.key: t for t in templates}
    popular = [(b, p) for b, p in POPULAR if (b, p) in existing]
    chips = "".join('<li><a href="%s">%s %s</a></li>' % (esc(url(template_path(existing[k]))), esc(k[0]), esc(k[1])) for k in popular)
    landing = (landing.replace("{{BASE}}", BASE).replace("{{DOWNLOAD}}", DOWNLOAD).replace("{{REPO}}", REPO)
               .replace("{{TEMPLATES_RAW}}", str(len(templates)))
               .replace("{{TEMPLATES}}", "{:,}".format(len(templates))).replace("{{MAKERS}}", str(len(by_brand)))
               .replace("{{GRID}}", hero_grid()).replace("{{DATAMATRIX}}", hero_datamatrix())
               .replace("{{BARCODE}}", hero_barcode()).replace("{{POPULAR}}", chips))
    with open(os.path.join(HERE, "src", "landing.json"), encoding="utf-8") as f:
        meta = json.load(f)
    ld = json.loads(json.dumps(meta["jsonld"]).replace("{{ORIGIN}}", ORIGIN + BASE).replace("{{REPO}}", REPO)
                    .replace("{{DOWNLOAD}}", DOWNLOAD))
    write(out, "/index.html", page(meta["title"], meta["description"], "/", landing, ld, body_class="home"))

    write(out, "/404.html", page("Page not found | " + SITE_NAME, "Page not found.", "/404.html",
                                 '<section class="wrap"><h1>Page not found</h1><p>That page does not exist. Try the '
                                 '<a href="%s">label template search</a> or go <a href="%s">home</a>.</p></section>'
                                 % (esc(url("/templates/")), esc(url("/"))), noindex=True))
    write(out, "/robots.txt", "User-agent: *\nAllow: /\n\nSitemap: %s\n" % absolute("/sitemap.xml"))
    urls = "".join("<url><loc>%s</loc></url>" % esc(absolute(p)) for p in sitemap)
    write(out, "/sitemap.xml", '<?xml version="1.0" encoding="UTF-8"?>\n'
                               '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">%s</urlset>\n' % urls)
    write(out, "/.nojekyll", "")

    print("%d templates, %d makers, %d URLs -> %s" % (len(templates), len(by_brand), len(sitemap), out))


if __name__ == "__main__":
    main()
