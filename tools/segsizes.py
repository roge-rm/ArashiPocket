#!/usr/bin/env python3
"""Prints the size of each code segment of a linked prc-tools program."""
import re
import subprocess
import sys

out = subprocess.run(['m68k-palmos-objdump', '-h', sys.argv[1]], check=True,
                     capture_output=True, text=True).stdout.splitlines()
sizes = []
for i, line in enumerate(out):
    m = re.match(r'\s*\d+\s+(\S+)\s+([0-9a-f]+)\s', line)
    if m and 'CODE' in out[i + 1]:
        sizes.append('%s %d' % (m.group(1), int(m.group(2), 16)))
print('segments (bytes): ' + ', '.join(sizes))
