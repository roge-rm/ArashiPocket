#!/usr/bin/env python3
"""Mac resource fork reader for the Arashi data.

The vendored source keeps the resource forks in AppleDouble files under
__MACOSX/ (the plain .rsrc files are empty). This reads either an AppleDouble
file or a raw fork.

  rsrc.py list FILE                 type, id, size, name of every resource
  rsrc.py dump FILE OUTDIR [--rcp OUT.rcp]
                                    write the resources the game loads as
                                    OUTDIR/TYPE_ID.bin, plus a pilrc fragment
"""
import os
import struct
import sys

# Resources the game code loads that the Palm build needs (type -> ids, or
# None for all ids of that type). Everything else (dialogs, pictures, icons,
# sound kit, Mac-only code) is skipped. Where each is read:
#   TEXT 2128/2129  Parser.c LoadProgram: the Arashi Script (levels)
#   TEXT 128..      Flashmain.c, Highscores.c, Highmain.c: title and score text
#   SPCE            GridTest.c, STLevelSelect.c: level shapes
#   CLOT            VABuffers.c, GridTest.c, STFlyThru.c ...: colour tables
#   SCOR 128/129    GameLoop.c, Highscores.c: high scores
#   KEYS 128        STPlayer.c: default key codes
#   MYGA 128-135    STPlayer.c LoadPlayRecord: demo recordings
#   DFLT 128        PlayOptions.c: default options
#   SVGM 128/129    loadsave.c: saved game
#   STR  128-131    STPlayer.c: bottom-of-screen messages
#   GNRL 1000+      Explosion.c, VAExplode.c: explosion shapes
#   SKIT 1001/1002  Huffman.c DecodeSounds: the sounds, written as 'snd ' 128-145
WANTED = {
    'TEXT': None, 'SPCE': None, 'CLOT': None, 'SCOR': None, 'KEYS': None,
    'MYGA': None, 'DFLT': None, 'SVGM': None, 'STR ': None, 'GNRL': None,
}


def fork_of(path):
    """Return the resource fork bytes of an AppleDouble file or raw fork."""
    d = open(path, 'rb').read()
    if d[:4] == b'\x00\x05\x16\x07':
        n = struct.unpack('>H', d[24:26])[0]
        for i in range(n):
            eid, off, ln = struct.unpack('>III', d[26 + 12 * i:38 + 12 * i])
            if eid == 2:
                return d[off:off + ln]
        raise ValueError('no resource fork in ' + path)
    return d


def parse(fork):
    """Return a list of (type, id, name, data), in map order."""
    dofs, mofs, dlen, mlen = struct.unpack('>IIII', fork[:16])
    m = fork[mofs:mofs + mlen]
    tlofs, nlofs = struct.unpack('>HH', m[24:28])
    ntypes = struct.unpack('>H', m[tlofs:tlofs + 2])[0] + 1
    out = []
    for i in range(ntypes):
        t, cnt, roff = struct.unpack('>4sHH', m[tlofs + 2 + 8 * i:tlofs + 10 + 8 * i])
        for j in range(cnt + 1):
            p = tlofs + roff + 12 * j
            rid, noff, doff = struct.unpack('>hH I', m[p:p + 8])
            doff &= 0xFFFFFF
            name = ''
            if noff != 0xFFFF:
                ln = m[nlofs + noff]
                name = m[nlofs + noff + 1:nlofs + noff + 1 + ln].decode('mac_roman')
            n = struct.unpack('>I', fork[dofs + doff:dofs + doff + 4])[0]
            data = fork[dofs + doff + 4:dofs + doff + 4 + n]
            out.append((t.decode('mac_roman'), rid, name, data))
    return out


# Opcode data sizes of the PICT version 2 opcodes the title pictures use.
PICT_FIXED = {0x00: 0, 0x03: 2, 0x04: 2, 0x05: 2, 0x06: 4, 0x07: 4, 0x08: 2, 0x09: 8,
              0x0a: 8, 0x0b: 4, 0x0c: 4, 0x0d: 2, 0x0e: 4, 0x0f: 4, 0x10: 8, 0x11: 1,
              0x12: 0, 0x13: 0, 0x14: 0, 0x15: 2, 0x16: 2, 0x17: 0, 0x18: 0, 0x19: 0,
              0x1a: 6, 0x1b: 6, 0x1c: 6, 0x1d: 6, 0x1e: 0, 0x1f: 6, 0x20: 8, 0x21: 4,
              0x22: 6, 0x23: 2}


def pict_polys(d):
    """The polygons of a PICT (paint, frame, erase, invert, fill poly) as
    lists of (h, v) points. Raises on an opcode it doesn't know."""
    p = 10
    out = []
    while p < len(d):
        if d[p:p + 4] == b'\x00\x11\x02\xff':
            p += 4
            continue
        op = struct.unpack('>H', d[p:p + 2])[0]
        p += 2
        if op == 0xff:
            return out
        if op == 0x0c00:
            p += 24
        elif op == 0x01 or 0x80 <= op <= 0x84:
            p += struct.unpack('>H', d[p:p + 2])[0]
        elif 0x70 <= op <= 0x74:
            n = struct.unpack('>H', d[p:p + 2])[0]
            pts = [struct.unpack('>hh', d[p + 10 + 4 * i:p + 14 + 4 * i])
                   for i in range((n - 10) // 4)]
            out.append([(h, v) for v, h in pts])
            p += n
        elif op in PICT_FIXED:
            p += PICT_FIXED[op]
        else:
            raise ValueError('unknown PICT opcode 0x%x' % op)
    return out


def logo_segments(pict):
    """What Arashi's title does with a PICT: each polygon drawn through
    StdPoly gives one line per pair of neighbouring points, without the
    last one (the original counts (size - 14) / 4 lines). Big endian: a count,
    then x, y, dx, dy of each line as 16 bit."""
    segs = []
    for poly in pict_polys(pict):
        for a, b in zip(poly, poly[1:]):
            segs.append((a[0], a[1], b[0] - a[0], b[1] - a[1]))
    return struct.pack('>H', len(segs)) + b''.join(struct.pack('>hhhh', *s) for s in segs)


# Derived resources: type, id, source PICT.
LOGOS = (('LOGO', 131, 131), ('LOGO', 133, 133))


# The original's sounds: SKIT 1001 holds each sound's length (longs) and SKIT
# 1002 the Huffman coded sample differences (128 longs of symbol counts, then
# the bit stream, first bit highest), as the Sound Kit's Huffman.c packed
# them. Samples are 7 bits (64 is silence) at 11127 Hz. Each comes out as a
# Mac 'snd ' (format 1, one bufferCmd, 8 bits) for SndPlayResource.
SND_BASE = 128
SND_RATE = 0x2B7745D1      # 11127.27 Hz as a Fixed, the Sound Kit's KHZ11


def skit_sounds(res):
    """Return the sounds as lists of 7-bit samples, in SKIT 1001 order."""
    skit = {rid: data for t, rid, name, data in res if t == 'SKIT'}
    lens = struct.unpack('>%dI' % (len(skit[1001]) // 4), skit[1001])
    d = skit[1002]
    freq = struct.unpack('>128I', d[:512])
    bits = d[512:]
    # HuffmannTree: nodes 0-127 are the symbols; GetSmallest takes the
    # lowest count, scanning down from the end, the first found on a tie,
    # and moves the last entry into its place.
    nodes = [[v, f, None, None] for v, f in enumerate(freq)]   # value, freq, zero, one
    srt = [n for n in nodes if n[1]]

    def smallest():
        small = len(srt) - 1
        f = srt[small][1]
        for i in range(len(srt) - 2, -1, -1):
            if srt[i][1] < f:
                small, f = i, srt[i][1]
        found = srt[small]
        srt[small] = srt[-1]
        srt.pop()
        return found
    while len(srt) > 1:
        a = smallest()
        b = smallest()
        srt.append([None, a[1] + b[1], a, b])
    root = srt[0]
    out = []
    pos = 0
    delta = 64
    for n in lens:
        snd = []
        for _ in range(n):
            node = root
            while node[0] is None:
                bit = (bits[pos >> 3] >> (7 - (pos & 7))) & 1
                pos += 1
                node = node[3] if bit else node[2]
            delta = (delta + node[0]) & 0x7F
            snd.append(delta)
        out.append(snd)
    skit_sounds.spare_bits = len(bits) * 8 - pos
    return out


def snd_resource(samples):
    """A format 1 'snd ' with one bufferCmd and a standard sound header."""
    head = struct.pack('>HHHIH', 1, 1, 5, 0x80, 1)          # format, 1 synth, sampled, mono, 1 cmd
    head += struct.pack('>HHI', 0x8051, 0, len(head) + 8)  # bufferCmd with the header offset
    head += struct.pack('>IIIIIBB', 0, len(samples), SND_RATE, 0, 0, 0, 60)
    return head + bytes(min(255, v * 2) for v in samples)


def wanted(t, rid):
    if t not in WANTED:
        return False
    ids = WANTED[t]
    return ids is None or rid in ids


def fname(t, rid):
    # Type names can hold spaces and '|', so hex the type in the file name.
    return '%s_%d.bin' % (t.encode('mac_roman').hex(), rid)


def main(argv):
    if len(argv) < 3 or argv[1] not in ('list', 'dump'):
        print(__doc__)
        return 2
    res = parse(fork_of(argv[2]))
    if argv[1] == 'list':
        for t, rid, name, data in res:
            print('%-4s %6d %7d %s%s' % (t, rid, len(data), name,
                                          '' if wanted(t, rid) else '  (skipped)'))
        return 0
    outdir = argv[3]
    rcp = None
    if '--rcp' in argv:
        rcp = argv[argv.index('--rcp') + 1]
    os.makedirs(outdir, exist_ok=True)
    lines = ['// Generated by tools/rsrc.py. Do not edit.']
    picts = {rid: data for t, rid, name, data in res if t == 'PICT'}
    out = [(t, rid, data) for t, rid, name, data in sorted(res, key=lambda r: (r[0], r[1]))
           if wanted(t, rid)]
    out += [(t, rid, logo_segments(picts[src])) for t, rid, src in LOGOS]
    out += [('snd ', SND_BASE + i, snd_resource(smp)) for i, smp in enumerate(skit_sounds(res))]
    for t, rid, data in out:
        fn = fname(t, rid)
        with open(os.path.join(outdir, fn), 'wb') as f:
            f.write(data)
        lines.append('DATA "%s" ID %d "%s"' % (t, rid, os.path.join(outdir, fn)))
    if rcp:
        with open(rcp, 'w') as f:
            f.write('\n'.join(lines) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
