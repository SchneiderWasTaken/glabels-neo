#!/usr/bin/env python3
# check.py
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

"""Checks a built site: every internal link and image exists, every page has one h1, a unique
title and a description, every starter file is well-formed XML, and the sitemap matches the pages.

    python3 site/check.py <built-folder> [base-path]

Exits with status 1 and lists the problems when anything is wrong.
"""

import os
import re
import sys
from html.parser import HTMLParser
from urllib.parse import unquote, urldefrag, urlparse
from xml.etree import ElementTree as ET


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links, self.title, self.h1, self.desc, self.canonical, self.ids = [], "", 0, "", "", set()
        self._in_title = False

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if "id" in a:
            self.ids.add(a["id"])
        if tag in ("a", "link") and a.get("href"):
            self.links.append(a["href"])
        if tag in ("img", "script") and a.get("src"):
            self.links.append(a["src"])
        if tag == "h1":
            self.h1 += 1
        if tag == "title":
            self._in_title = True
        if tag == "meta" and a.get("name") == "description":
            self.desc = a.get("content", "")
        if tag == "link" and a.get("rel") == "canonical":
            self.canonical = a.get("href", "")

    def handle_endtag(self, tag):
        if tag == "title":
            self._in_title = False

    def handle_data(self, data):
        if self._in_title:
            self.title += data


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    root = os.path.abspath(sys.argv[1])
    base = (sys.argv[2] if len(sys.argv) > 2 else "").rstrip("/")
    problems, titles, pages = [], {}, {}

    for dirpath, _dirs, files in os.walk(root):
        for name in files:
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, root).replace(os.sep, "/")
            if name.endswith(".html"):
                p = Page()
                with open(full, encoding="utf-8") as f:
                    p.feed(f.read())
                pages[rel] = p
            elif name.endswith(".glabels"):
                try:
                    ET.parse(full)
                except ET.ParseError as e:
                    problems.append("%s: not well-formed XML (%s)" % (rel, e))

    for rel, p in pages.items():
        if rel != "404.html":
            if p.h1 != 1:
                problems.append("%s: %d h1 elements" % (rel, p.h1))
            if not p.title or len(p.title) > 110:
                problems.append("%s: bad title length (%d)" % (rel, len(p.title)))
            if not p.desc or len(p.desc) > 320:
                problems.append("%s: bad description length (%d)" % (rel, len(p.desc)))
            if p.title in titles:
                problems.append("%s: same title as %s" % (rel, titles[p.title]))
            titles[p.title] = rel
        for href in p.links:
            u = urlparse(href)
            if u.scheme in ("http", "https", "mailto") or href.startswith("#") or href.startswith("data:"):
                continue
            path, frag = urldefrag(unquote(href))
            if not path.startswith("/"):
                problems.append("%s: relative link %s" % (rel, href))
                continue
            if base and not path.startswith(base + "/") and path != base:
                problems.append("%s: link outside base %s" % (rel, href))
                continue
            local = path[len(base):] if base else path
            target = os.path.join(root, *local.strip("/").split("/")) if local.strip("/") else root
            if os.path.isdir(target):
                target = os.path.join(target, "index.html")
            if not os.path.isfile(target):
                problems.append("%s: broken link %s" % (rel, href))
            elif frag and target.endswith(".html"):
                t = pages.get(os.path.relpath(target, root).replace(os.sep, "/"))
                if t is not None and frag not in t.ids:
                    problems.append("%s: missing anchor %s" % (rel, href))

    sm = os.path.join(root, "sitemap.xml")
    if not os.path.isfile(sm):
        problems.append("sitemap.xml missing")
    else:
        locs = re.findall(r"<loc>([^<]+)</loc>", open(sm, encoding="utf-8").read())
        for loc in locs:
            path = urlparse(loc).path
            local = path[len(base):] if base and path.startswith(base) else path
            target = os.path.join(root, *local.strip("/").split("/")) if local.strip("/") else root
            if not os.path.isfile(os.path.join(target, "index.html")):
                problems.append("sitemap: %s has no page" % loc)
        indexable = [r for r in pages if r != "404.html"]
        if len(locs) != len(indexable):
            problems.append("sitemap lists %d URLs but there are %d pages" % (len(locs), len(indexable)))

    if problems:
        print("\n".join(problems[:60]))
        print("%d problem(s)" % len(problems))
        sys.exit(1)
    print("site OK: %d pages" % len(pages))


if __name__ == "__main__":
    main()
