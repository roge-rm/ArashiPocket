#!/usr/bin/env python3
"""Check the extracted game data.

  check-data.py FORK DATADIR PRC

For every resource tools/rsrc.py ships: the .bin in DATADIR must be
byte-identical to the resource in the source fork, and the PRC must hold the
same type and ID with the same bytes. Then decodes a few fields and compares
them with the Arashi Script text file. Prints a table; exits 1 on any failure.
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rsrc  # noqa: E402

SCRIPT = ('reference/Arashi/Arashi/Game Source/Full Arashi Script')


def read_prc(path):
    d = open(path, 'rb').read()
    n = struct.unpack('>H', d[76:78])[0]
    ents = []
    for i in range(n):
        t, rid, off = struct.unpack('>4sHI', d[78 + 10 * i:88 + 10 * i])
        ents.append((t.decode('mac_roman'), rid, off))
    out = {}
    for i, (t, rid, off) in enumerate(ents):
        end = ents[i + 1][2] if i + 1 < n else len(d)
        out[(t, rid)] = d[off:end]
    return out


def main(argv):
    fork, datadir, prc = argv[1:4]
    res = rsrc.parse(rsrc.fork_of(fork))
    prcres = read_prc(prc)
    fails = []
    rows = []
    src = {}
    for t, rid, name, data in res:
        src[(t, rid)] = data
        if not rsrc.wanted(t, rid):
            continue
        status = 'ok'
        try:
            bin_ = open(os.path.join(datadir, rsrc.fname(t, rid)), 'rb').read()
        except OSError:
            bin_ = None
        if bin_ != data:
            status = 'BIN MISMATCH'
        elif prcres.get((t, rid)) is None:
            status = 'NOT IN PRC'
        elif prcres[(t, rid)] != data:
            status = 'PRC MISMATCH'
        if status != 'ok':
            fails.append('%s %d: %s' % (t, rid, status))
        rows.append((t, rid, len(data), name, status))
    # Title pictures: LOGO is made from the PICT with the same number.
    picts = {rid: data for (t, rid), data in src.items() if t == 'PICT'}
    for t, rid, pid in rsrc.LOGOS:
        want = rsrc.logo_segments(picts[pid])
        status = 'ok'
        try:
            bin_ = open(os.path.join(datadir, rsrc.fname(t, rid)), 'rb').read()
        except OSError:
            bin_ = None
        if bin_ != want:
            status = 'BIN MISMATCH'
        elif prcres.get((t, rid)) != want:
            status = 'NOT IN PRC' if prcres.get((t, rid)) is None else 'PRC MISMATCH'
        if status != 'ok':
            fails.append('%s %d: %s' % (t, rid, status))
        rows.append((t, rid, len(want), 'from PICT %d' % pid, status))
    print('%-4s %5s %6s  %-6s %s' % ('type', 'id', 'size', 'status', 'name'))
    total = 0
    for t, rid, size, name, status in sorted(rows):
        total += size
        print('%-4s %5d %6d  %-6s %s' % (t, rid, size, status, name))
    print('%d resources, %d bytes' % (len(rows), total))

    # SPCE shapes: the segment count at offset 0 must match "NN Segs" in names.
    names = {(t, rid): n for t, rid, n, d in res}
    checked = 0
    for (t, rid), data in src.items():
        m = re.search(r'(\d+) Segs', names[(t, rid)])
        if t == 'SPCE' and m:
            checked += 1
            if struct.unpack('>h', data[:2])[0] != int(m.group(1)):
                fails.append('SPCE %d: numsegs does not match name' % rid)
    print('SPCE numsegs vs resource name: %d checked' % checked)

    # Script text: every line of TEXT 2128 + 2129 should be in the source
    # script file, except the header and copyright comments.
    def lines(b):
        t = b.decode('mac_roman').replace('\r\n', '\n').replace('\r', '\n')
        return [l.strip() for l in t.split('\n') if l.strip()]
    if os.path.exists(SCRIPT):
        f = set(lines(open(SCRIPT, 'rb').read()))
        mine = lines(src[('TEXT', 2128)]) + lines(src[('TEXT', 2129)])
        missing = [l for l in mine if l not in f]
        bad = [l for l in missing if not l.startswith(('Arashi', 'Copyright'))]
        print('script lines in TEXT 2128+2129: %d, in Full Arashi Script: %d, '
              'only in resource: %d' % (len(mine), len(mine) - len(missing),
                                        len(missing)))
        for l in bad:
            fails.append('script line not in source file: ' + l)
        nlev = sum(1 for l in mine if re.match(r'level \d+', l))
        print('level statements in resource script: %d' % nlev)
    for f in fails:
        print('FAIL', f)
    print('check-data: %s' % ('FAILED' if fails else 'ok'))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
