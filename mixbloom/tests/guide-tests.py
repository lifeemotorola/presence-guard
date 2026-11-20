#!/usr/bin/env python3
# MixBloom for OBS Studio - Copyright (C) 2026 Emmanuel Suah.
# GPL-2.0-or-later; no warranty.
"""Validate the offline manual shared by the Qt resource and package."""
from html.parser import HTMLParser
from pathlib import Path
import sys

EXPECTED = {
    "quick-start", "sources", "level-mute", "routing", "monitoring",
    "meter", "troubleshooting", "installation", "privacy",
}


class GuideParser(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.anchors = []
        self.links = []
        self.errors = []
        self.in_style = False

    def handle_starttag(self, tag, attributes):
        attrs = dict(attributes)
        if tag in {"script", "iframe", "object", "embed", "base", "link"}:
            self.errors.append(f"unexpected active or external element: {tag}")
        if "src" in attrs:
            self.errors.append(f"unexpected resource reference on {tag}: {attrs['src']}")
        if tag == "meta" and "http-equiv" in attrs:
            self.errors.append("unexpected HTTP-equivalent meta tag")
        if tag == "a" and "name" in attrs:
            self.anchors.append(attrs["name"])
        if "href" in attrs:
            href = attrs["href"]
            if tag != "a" or not href.startswith("#") or len(href) < 2:
                self.errors.append(f"unexpected non-section link: {href}")
            else:
                self.links.append(href[1:])
        if tag == "style":
            self.in_style = True
        if "style" in attrs and ("url(" in attrs["style"].lower() or "@import" in attrs["style"].lower()):
            self.errors.append("unexpected external style resource")

    def handle_endtag(self, tag):
        if tag == "style":
            self.in_style = False

    def handle_data(self, data):
        if self.in_style and ("url(" in data.lower() or "@import" in data.lower()):
            self.errors.append("unexpected external style resource")


def main():
    path = Path(__file__).resolve().parent.parent / "data/help/user-guide.html"
    parser = GuideParser()
    parser.feed(path.read_text(encoding="utf-8"))
    parser.close()
    if set(parser.anchors) != EXPECTED or len(parser.anchors) != len(EXPECTED):
        parser.errors.append(f"section anchors are missing or duplicated: {parser.anchors}")
    if set(parser.links) != EXPECTED or len(parser.links) != len(EXPECTED):
        parser.errors.append(f"section links are missing or duplicated: {parser.links}")
    if parser.errors:
        for error in parser.errors:
            print(f"FAIL  {error}", file=sys.stderr)
        return 1
    print("Offline guide: 9 section links resolve; no external or active content")
    return 0


if __name__ == "__main__":
    sys.exit(main())
