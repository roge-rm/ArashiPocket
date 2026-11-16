#!/usr/bin/env python3 -I
"""Copies the Think C sources and headers the port uses into one flat folder,
fixed up so m68k-palmos-gcc can read them: CR line ends to LF, MacRoman to
UTF-8, Pascal "\\p" strings to length-prefixed C strings, and Think C
"extern T name : 0xADDR;" low-memory globals to plain externs.

usage: prep.py OUTDIR SRCDIR... (earlier SRCDIRs win on a name clash;
all .h files are copied, .c files only from SRCDIRs given as "c:DIR")."""
import os, re, sys

out = sys.argv[1]
os.makedirs(out, exist_ok=True)

def pascal(m):
    body = m.group(1)
    # count bytes of the escaped text
    raw = re.sub(r'\\(x[0-9a-fA-F]{1,2}|[0-7]{1,3}|.)', 'X', body)
    n = len(raw.encode('latin-1', 'replace'))
    return '"\\%03o%s"' % (n, body)

def fix(text):
    text = text.replace('\r\n', '\n').replace('\r', '\n')
    text = re.sub(r'"\\p((?:[^"\\\n]|\\.)*)"', pascal, text)
    text = re.sub(r'^(\s*(?:extern\s+)?[A-Za-z_][\w\s\*]*?)\s*:\s*0x[0-9a-fA-F]+\s*;',
                  r'\1;', text, flags=re.M)
    return text

seen = set()
for arg in sys.argv[2:]:
    cfiles = arg.startswith('c:')
    d = arg[2:] if cfiles else arg
    for name in sorted(os.listdir(d)):
        p = os.path.join(d, name)
        if not os.path.isfile(p):
            continue
        ext = os.path.splitext(name)[1]
        if ext != '.h' and not (cfiles and ext == '.c'):
            continue
        if name in seen:
            continue
        seen.add(name)
        data = open(p, 'rb').read().decode('mac_roman')
        dst = os.path.join(out, name)
        new = fix(data)
        if not os.path.exists(dst) or open(dst, encoding='utf-8').read() != new:
            open(dst, 'w', encoding='utf-8').write(new)
