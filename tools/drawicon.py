#!/usr/bin/env python3
"""Draws the launcher icon PNGs in res/icon: the tube seen down its length
with the claw on the bottom rim. tools/mkicon.py turns them into the PRC's
icon families; only rerun this to change the art.

The big icons are drawn with lines and polygons, the claws and the 15x9 by
hand. Each pixel is a material (a row of palette.txt, written here too), so
one PNG gives every depth: the 1-bit tube is black with white lanes and claw.

Usage: drawicon.py ICONDIR
"""
import math
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mkicon import nearest  # noqa: E402


class Mats:
    """Materials: (char, name, rgb, g16, g4, mono). Index 0 is clear."""

    def __init__(self, rows):
        self.rows = rows
        self.by_char = {r[0]: i for i, r in enumerate(rows)}
        for r in rows:
            if nearest(r[2]) != tuple(r[2]):
                raise SystemExit("%s is not a Palm palette colour" % r[1])

    def palette_txt(self):
        s = ("# Materials of the icon PNGs (written by tools/drawicon.py): index,\n"
             "# name, colour (R G B), grey on 16-grey screens (0 white to 15\n"
             "# black), grey on 4-grey screens (0 to 3), 1 bit (0 white, 1 black).\n"
             "# idx name          R   G   B   g16 g4 mono\n")
        for i, r in enumerate(self.rows):
            s += "%-3d %-12s %4d %3d %3d %4d %2d  %s\n" % (i, r[1], r[2][0], r[2][1], r[2][2], r[3], r[4], r[5])
        return s


class Grid:
    def __init__(self, w, h, mats):
        self.w, self.h, self.m = w, h, mats
        self.im = Image.new("P", (w, h), 0)
        self.d = ImageDraw.Draw(self.im)

    def c(self, ch):
        return self.m.by_char[ch]

    def line(self, pts, ch):
        self.d.line(pts, fill=self.c(ch))

    def poly(self, pts, ch, outline=None):
        self.d.polygon(pts, fill=self.c(ch) if ch else None,
                       outline=self.c(outline) if outline else None)

    def px(self, x, y, ch):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.im.putpixel((x, y), self.c(ch))

    def get(self, x, y):
        return self.m.rows[self.im.getpixel((x, y))][0]

    def stamp(self, x0, y0, art):
        for y, row in enumerate(art):
            for x, ch in enumerate(row):
                if ch != ".":
                    self.px(x0 + x, y0 + y, ch)

    def png(self):
        im = self.im.copy()
        pal = []
        for r in self.m.rows:
            pal += list(r[2])
        im.putpalette(pal + [0] * (768 - len(pal)))
        return im


def from_ascii(art, mats):
    rows = art.strip().split("\n")
    g = Grid(len(rows[0]), len(rows), mats)
    g.stamp(0, 0, rows)
    return g

# Materials. The rim is black at 1 bit so the tube keeps its shape on a
# white launcher.
M = Mats([
    (".", "clear", (255, 0, 255), 0, 0, "0"),
    ("#", "tube", (0, 0, 0), 15, 3, "1"),
    ("r", "rim", (102, 153, 255), 4, 1, "1"),
    ("l", "lane", (51, 102, 255), 7, 1, "0"),
    ("d", "lane_far", (0, 51, 204), 10, 2, "1"),
    ("y", "claw", (255, 255, 0), 0, 0, "0"),
    ("p", "claw_hi", (255, 255, 153), 0, 0, "0"),
    ("g", "claw_mid", (255, 204, 0), 2, 0, "0"),
    ("o", "claw_dk", (255, 153, 0), 6, 1, "0"),
    ("w", "white", (255, 255, 255), 0, 0, "0"),
])


def octagon(x0, y0, x1, y1, cx, cy=None):
    cy = cx if cy is None else cy
    return [(x0 + cx, y0), (x1 - cx, y0), (x1, y0 + cy), (x1, y1 - cy),
            (x1 - cx, y1), (x0 + cx, y1), (x0, y1 - cy), (x0, y0 + cy)]


def mids(p):
    return [((p[i][0] + p[(i + 1) % 8][0]) / 2, (p[i][1] + p[(i + 1) % 8][1]) / 2) for i in range(8)]


def rnd(p):
    return (int(math.floor(p[0] + 0.5)), int(math.floor(p[1] + 0.5)))


def tube(w, h, outer, inner, lanes16=True, far=0.0, hole="#"):
    """far: the share of each lane next to the hole drawn dim."""
    g = Grid(w, h, M)
    g.poly(outer, "#")
    os_, is_ = (outer + mids(outer), inner + mids(inner)) if lanes16 else (outer, inner)
    for o, i in zip(os_, is_):
        g.line([rnd(i), rnd(o)], "l")
        if far:
            m = (i[0] + (o[0] - i[0]) * far, i[1] + (o[1] - i[1]) * far)
            g.line([rnd(i), rnd(m)], "d")
    g.poly(inner, hole, "d" if far else "l")
    g.poly(outer, None, "r")
    return g


def claw_on(g, x0, y0, art, gap=True):
    """Stamps a claw with a ring of tube black round it so it stands off the
    lanes."""
    a = art.strip().split("\n")
    if gap:
        for y, row in enumerate(a):
            for x, ch in enumerate(row):
                if ch != ".":
                    for dx in (-1, 0, 1):
                        for dy in (-1, 0, 1):
                            X, Y = x0 + x + dx, y0 + y + dy
                            if 0 <= X < g.w and 0 <= Y < g.h and g.get(X, Y) in "ld":
                                g.px(X, Y, "#")
    g.stamp(x0, y0, a)


CLAW32 = """
p..........p
yp........py
yyp..pp..pyy
yyyppyyppyyy
oyyyyggyyyyo
.oyyo..oyyo.
..oo....oo..
"""

CLAW64 = """
pp....................pp
ypp..................ppy
yypp................ppyy
yyypp..............ppyyy
gyyyyp............pyyyyg
gyyyyyp..........pyyyyyg
ogyyyyyp........pyyyyygo
.ogyyyyyp......pyyyyygo.
..ogyyyyyp....pyyyyygo..
..oggyyyyyyyyyyyyyyggo..
...oggggyyyyyyyyggggo...
....oooooggggggooooo....
"""

CLAW30 = """
p........p
yp..pp..py
oyyyyyyyyo
.oyy..yyo.
..oo..oo..
"""

CLAW22 = """
w..........w
ww........ww
www..ww..www
wwwwwwwwwwww
.wwww..wwww.
..ww....ww..
"""

ICON15 = """
.....rrrrr.....
....r#l#l#r....
...rll###llr...
...r#######r...
...ry#####yr...
...ryy#y#yyr...
...r#oyyyo#r...
....r#o#o#r....
.....rrrrr.....
"""


def build():
    G = {}
    # The launcher draws the large icon from its top row and puts the name
    # under row 22, so the 32x32 and 64x64 keep to their top 22 (44) rows.
    g = tube(32, 32, octagon(5, 0, 26, 21, 6), octagon(12, 2, 19, 9, 2), lanes16=False)
    claw_on(g, 10, 14, CLAW32)
    G["icon-32"] = g
    g = tube(64, 64, octagon(10, 0, 53, 43, 12), octagon(25, 4, 38, 17, 4), far=0.45)
    claw_on(g, 20, 30, CLAW64)
    g.poly([(31.5, 33), (35, 39), (28, 39)], "y")
    g.line([(31, 33), (28, 38)], "p")
    G["icon-64"] = g
    g = tube(32, 22, octagon(5, 0, 26, 21, 6), octagon(12, 2, 19, 9, 2), lanes16=False)
    claw_on(g, 10, 14, CLAW22)
    G["icon-22"] = g
    g = tube(30, 18, octagon(6, 0, 23, 17, 5), octagon(12, 2, 17, 7, 2), lanes16=False)
    claw_on(g, 10, 11, CLAW30)
    G["icon-30"] = g
    G["icon-15"] = from_ascii(ICON15, M)
    return G


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: drawicon.py ICONDIR")
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for name, g in build().items():
        g.png().save(os.path.join(out, name + ".png"))
    with open(os.path.join(out, "palette.txt"), "w") as fh:
        fh.write(M.palette_txt())


if __name__ == "__main__":
    main()
