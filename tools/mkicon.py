#!/usr/bin/env python3
"""Makes the launcher icons from res/icon into what pilrc reads:

  OUTDIR/icon-*.bmp   one picture per icon file and depth
  RCP                 ICONFAMILYEX and SMALLICONFAMILYEX for arashi.rcp

The large icon is 32x22 at 1 and 2 bits (OS 3 before 3.5 reads only those),
32x32 at 4 and 8 bits and 64x64 at 8 bits double density. The small one
(list view) is 15x9 at 1, 2, 4 and 8 bits and 30x18 at 8 bits double
density. The colour members keep clear pixels magenta for pilrc's
TRANSPARENCY.

Each pixel of an icon PNG is a palette index, a row of res/icon/palette.txt,
which says what it is on each kind of screen: colour (R G B, picked from
Palm's 8-bit palette), grey on 16-grey screens (0 white to 15 black), grey on
4-grey screens (0 to 3) and 1 bit (0 white, 1 black, c a checker).
Same format and rendering as PalmBolo's tools/art2palm.py. The PNGs are
drawn by tools/drawicon.py.

Usage: mkicon.py ICONDIR OUTDIR RCP
"""
import os
import sys

from PIL import Image


def palm_palette():
    """The colours of Palm OS's 8-bit system palette."""
    cols = [(r, g, b) for r in range(0xFF, -1, -0x33) for b in range(0xFF, -1, -0x33)
            for g in range(0xFF, -1, -0x33)]
    cols += [(v, v, v) for v in (0x11, 0x22, 0x44, 0x55, 0x77, 0x88, 0xAA, 0xBB, 0xDD, 0xEE)]
    cols += [(0xC0, 0xC0, 0xC0), (0x80, 0, 0), (0x80, 0, 0x80), (0, 0x80, 0), (0, 0x80, 0x80)]
    return cols


PALM = palm_palette()


def nearest(rgb):
    return min(PALM, key=lambda c: sum((c[i] - rgb[i]) ** 2 for i in range(3)))


def load_palette(path):
    mats = {}
    for line in open(path):
        f = line.split("#")[0].split()
        if f:
            mats[int(f[0])] = (f[1], (int(f[2]), int(f[3]), int(f[4])), int(f[5]), int(f[6]), f[7])
    return mats


def mono_at(code, x, y):
    if code in "01":
        return int(code)
    if code == "c":
        return (x + y) & 1
    raise SystemExit("unknown 1-bit code " + code)


def render(src, mats, depth, clear_white):
    """src: indexed image. Returns an RGB image as that depth shows it."""
    w, h = src.size
    out = Image.new("RGB", (w, h))
    sp, op = src.load(), out.load()
    for y in range(h):
        for x in range(w):
            i = sp[x, y]
            if i not in mats:
                raise SystemExit("palette index %d at %d,%d is not in palette.txt" % (i, x, y))
            name, rgb, g16, g4, mono = mats[i]
            if name == "clear":
                op[x, y] = (255, 255, 255) if clear_white else (255, 0, 255)
            elif depth == 8:
                op[x, y] = nearest(rgb)
            elif depth == 4:
                # pilrc maps the magenta transparency colour to grey 5, so
                # grey 5 would turn clear.
                if g16 == 5 and not clear_white:
                    raise SystemExit("%s uses grey 5, the 4-bit transparent index" % name)
                v = 255 - g16 * 17
                op[x, y] = (v, v, v)
            elif depth == 2:
                v = 255 - g4 * 85
                op[x, y] = (v, v, v)
            else:
                v = 0 if mono_at(mono, x, y) else 255
                op[x, y] = (v, v, v)
    return out


# (png name, size, depth, double density, transparent)
LARGE = [("icon-22", (32, 22), 1, False, False), ("icon-22", (32, 22), 2, False, False),
         ("icon-32", (32, 32), 4, False, True), ("icon-32", (32, 32), 8, False, True),
         ("icon-64", (64, 64), 8, True, True)]
SMALL = [("icon-15", (15, 9), 1, False, False), ("icon-15", (15, 9), 2, False, False),
         ("icon-15", (15, 9), 4, False, True), ("icon-15", (15, 9), 8, False, True),
         ("icon-30", (30, 18), 8, True, True)]


def family(src, out, mats, members):
    lines = []
    for name, size, depth, dd, transparent in members:
        im = Image.open(os.path.join(src, name + ".png"))
        if im.mode != "P":
            raise SystemExit("%s.png must be an indexed PNG" % name)
        if im.size != size:
            raise SystemExit("%s.png is %dx%d, expected %dx%d" % ((name,) + im.size + size))
        img = render(im, mats, depth, clear_white=not transparent)
        f = os.path.join(out, "%s-%d.bmp" % (name, depth))
        (img.convert("1") if depth == 1 else img).save(f)
        # OS 3.0 reads only uncompressed 1 and 2-bit members.
        lines.append('  BITMAP "%s" BPP %d%s%s%s\n' % (
            f, depth, " DENSITY 2" if dd else "", " COMPRESS" if depth >= 4 else "",
            " TRANSPARENCY 255 0 255" if transparent else ""))
    return lines


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mkicon.py ICONDIR OUTDIR RCP")
    src, out, rcp = sys.argv[1:]
    os.makedirs(out, exist_ok=True)
    mats = load_palette(os.path.join(src, "palette.txt"))
    r = ["// Made by tools/mkicon.py from res/icon. Don't edit.\n", "ICONFAMILYEX\nBEGIN\n"]
    r += family(src, out, mats, LARGE)
    r.append("END\n\nSMALLICONFAMILYEX\nBEGIN\n")
    r += family(src, out, mats, SMALL)
    r.append("END\n")
    with open(rcp, "w") as fh:
        fh.write("".join(r))


if __name__ == "__main__":
    main()
