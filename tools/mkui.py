#!/usr/bin/env python3
"""Makes the menu art the core draws with (src/core/ui.c):

  UIFT 128  the menu font, low density, from tools/ui/font.txt
  UIFT 129  the same font at double density (Scale2x of 128)
  UIBM 128  the Arashi logo, filled and shaded, about 124 pixels wide
  UIBM 129  the same, about 248 pixels wide

Font: u8 rows, u8 ascent (rows down to the baseline), u8 first char,
u8 count, then per char u8 width, u8 0, u16 offset of its rows, then the
rows, (width + 7) / 8 bytes each, leftmost pixel in the high bit.

Bitmap: u16 width, u16 height, 16 colours of r, g, b, grey (grey 0 black
to 15 white, picked by hand for grey screens; colour 0 is clear), then the
rows at 4 bits a pixel, (width + 1) / 2 bytes each, high nibble first.

Usage: mkui.py FONT.txt LOGO.bin OUTDIR
"""
import struct
import sys


def read_font(path):
    glyphs = {}
    name, rows = None, []
    for line in open(path):
        line = line.rstrip("\n")
        if line.startswith("#") and name is None:
            continue
        if line.startswith("== "):
            if name is not None:
                glyphs[name] = rows
            c = line[3:]
            name = " " if c == "space" else c
            rows = []
        elif line:
            rows.append([ch == "#" for ch in line])
    if name is not None:
        glyphs[name] = rows
    for c, r in glyphs.items():
        if len(r) != 9 or len({len(x) for x in r}) != 1:
            sys.exit("font.txt: glyph %r needs 9 rows of one length" % c)
    return glyphs


def scale2x(rows):
    """Scale2x (EPX): doubles a 1 bit image, rounding the diagonals."""
    h, w = len(rows), len(rows[0])

    def px(x, y):
        return rows[y][x] if 0 <= x < w and 0 <= y < h else False
    out = [[False] * (w * 2) for _ in range(h * 2)]
    for y in range(h):
        for x in range(w):
            p = px(x, y)
            a, b, c, d = px(x, y - 1), px(x + 1, y), px(x - 1, y), px(x, y + 1)
            e0 = a if (c == a and c != d and a != b) else p
            e1 = b if (a == b and a != c and b != d) else p
            e2 = c if (d == c and d != b and c != a) else p
            e3 = d if (b == d and b != a and d != c) else p
            out[2 * y][2 * x] = e0
            out[2 * y][2 * x + 1] = e1
            out[2 * y + 1][2 * x] = e2
            out[2 * y + 1][2 * x + 1] = e3
    return out


def pack_font(glyphs, rows, ascent):
    first, count = 32, 95
    head = struct.pack(">BBBB", rows, ascent, first, count)
    table, bits = b"", b""
    for i in range(count):
        g = glyphs[chr(first + i)]
        w = len(g[0])
        stride = (w + 7) // 8
        table += struct.pack(">BBH", w, 0, len(bits))
        for r in g:
            b = bytearray(stride)
            for x, on in enumerate(r):
                if on:
                    b[x >> 3] |= 0x80 >> (x & 7)
            bits += bytes(b)
    return head + table + bits


# Logo colours: r, g, b, grey. 1 outline, 2 shadow, 3 to 8 the fill from
# top to bottom, 9 the lit top edge.
LOGO_COLOURS = [
    (0, 0, 0, 0),
    (8, 0, 40, 0),
    (150, 0, 170, 5),
    (255, 250, 150, 15),
    (255, 224, 40, 15),
    (255, 190, 0, 14),
    (255, 140, 0, 12),
    (250, 90, 0, 11),
    (225, 30, 40, 10),
    (255, 255, 255, 15),
]


def logo_loops(data):
    n = struct.unpack(">h", data[:2])[0]
    segs = [struct.unpack(">4h", data[2 + i * 8:10 + i * 8]) for i in range(n)]
    loops, cur = [], []
    for x, y, dx, dy in segs:
        cur.append((x, y))
        if (x + dx, y + dy) == cur[0]:
            loops.append(cur)
            cur = []
    if cur:
        loops.append(cur)
    return loops


def inside(loops, px, py):
    hit = False
    for pts in loops:
        n = len(pts)
        for i in range(n):
            x1, y1 = pts[i]
            x2, y2 = pts[(i + 1) % n]
            if (y1 > py) != (y2 > py):
                xc = x1 + (py - y1) * (x2 - x1) / (y2 - y1)
                if xc > px:
                    hit = not hit
    return hit


def make_logo(data, width, d):
    loops = logo_loops(data)
    xs = [p[0] for l in loops for p in l]
    ys = [p[1] for l in loops for p in l]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    sc = (width - 2 * d - d) / (x1 - x0)
    fw = int((x1 - x0) * sc + 0.5) + 1
    fh = int((y1 - y0) * sc + 0.5) + 1
    w, h = fw + 3 * d, fh + 3 * d
    fill = [[False] * w for _ in range(h)]
    for y in range(fh):
        for x in range(fw):
            if inside(loops, x0 + (x + 0.5) / sc, y0 + (y + 0.5) / sc):
                fill[y + d][x + d] = True
    img = [[0] * w for _ in range(h)]

    def on(a, x, y):
        return 0 <= x < w and 0 <= y < h and a[y][x]
    body = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if fill[y][x]:
                img[y][x] = 3 + min(5, (y - d) * 6 // fh)
                body[y][x] = True
                if not on(fill, x, y - 1) or (d > 1 and not on(fill, x, y - 2)):
                    img[y][x] = 9
    for y in range(h):
        for x in range(w):
            if fill[y][x]:
                continue
            if any(on(fill, x + i, y + j) for i in range(-d, d + 1)
                   for j in range(-d, d + 1) if abs(i) + abs(j) <= d):
                img[y][x] = 1
                body[y][x] = True
    for y in range(h):
        for x in range(w):
            if not body[y][x] and on(body, x - d, y - d):
                img[y][x] = 2
    out = struct.pack(">HH", w, h)
    cols = LOGO_COLOURS + [(0, 0, 0, 0)] * (16 - len(LOGO_COLOURS))
    for c in cols:
        out += struct.pack(">BBBB", *c)
    for row in img:
        b = bytearray((w + 1) // 2)
        for x, v in enumerate(row):
            b[x >> 1] |= v << (4 if x % 2 == 0 else 0)
        out += bytes(b)
    return out


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    font, logo, outdir = sys.argv[1:]
    glyphs = read_font(font)
    big = {c: scale2x(r) for c, r in glyphs.items()}
    files = {
        "55494654_128.bin": pack_font(glyphs, 9, 7),
        "55494654_129.bin": pack_font(big, 18, 14),
    }
    data = open(logo, "rb").read()
    files["5549424d_128.bin"] = make_logo(data, 124, 1)
    files["5549424d_129.bin"] = make_logo(data, 248, 2)
    for name, b in files.items():
        open("%s/%s" % (outdir, name), "wb").write(b)


main()
