#!/usr/bin/env python3
"""Joins the records of a Palm database (.pdb) into one text file.

Usage: pdbtext.py DB.pdb OUT.txt [--strip-comments]
--strip-comments leaves out lines starting with '#'.
"""
import struct
import sys


def records(data):
    if len(data) < 78:
        raise SystemExit('too short for a Palm database')
    count, = struct.unpack('>H', data[76:78])
    offsets = [struct.unpack('>I', data[78 + 8 * i:82 + 8 * i])[0] for i in range(count)]
    offsets.append(len(data))
    return [data[offsets[i]:offsets[i + 1]] for i in range(count)]


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if len(args) != 2:
        raise SystemExit(__doc__.strip().splitlines()[-2])
    text = b''.join(records(open(args[0], 'rb').read())).decode('latin-1')
    if '--strip-comments' in sys.argv:
        text = ''.join(l for l in text.splitlines(True) if not l.startswith('#'))
    with open(args[1], 'w', encoding='latin-1') as f:
        f.write(text)


if __name__ == '__main__':
    main()
