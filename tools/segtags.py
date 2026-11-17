#!/usr/bin/env python3
# Fails when a source file that tags some of its functions with a SEG_
# segment macro has a function definition without one. Such a function lands
# in .text (the first segment) and every call to it from another segment is
# a pc-relative branch into the wrong place.
#
# This reads the C sources, not the objects: the COFF objects built without
# -g have no function type on their symbols, and .text also holds the const
# tables, so a symbol in .text can't be told apart from a table there.
# A file with no SEG_ macro at all (the Palm backend files meant for .text)
# is left alone.
#
# Usage: segtags.py FILE.c ...
import re
import sys


def strip(src):
    src = re.sub(r"/\*.*?\*/", lambda m: re.sub(r"[^\n]", " ", m.group(0)), src, flags=re.S)
    src = re.sub(r"//[^\n]*", "", src)
    out = []
    for line in src.split("\n"):
        out.append("" if line.lstrip().startswith("#") else line)
    return "\n".join(out)


def definitions(src):
    """Yields (line number, header text) of every function definition."""
    depth, start, head, line = 0, 1, [], 1
    paren = 0
    for ch in src:
        if ch == "\n":
            line += 1
        if depth == 0:
            if ch == "(":
                paren += 1
            elif ch == ")":
                paren -= 1
            if ch == ";" and paren == 0:
                head, start = [], line
                continue
            if ch == "{":
                text = " ".join("".join(head).split())
                if (text.endswith(")") and "=" not in text
                        and not re.match(r"(typedef|struct|union|enum)\b", text)):
                    yield start, text
                head, start = [], line
                depth = 1
                continue
            if not head and ch.isspace():
                start = line
                continue
            head.append(ch)
        else:
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    head, start = [], line


def main():
    bad = 0
    for path in sys.argv[1:]:
        src = strip(open(path, encoding="latin-1").read())
        defs = list(definitions(src))
        if not any("SEG_" in text for _, text in defs):
            continue
        for line, text in defs:
            if "SEG_" not in text:
                print("segtags: %s:%d: no segment tag: %s" % (path, line, text[:70]))
                bad += 1
    if bad:
        print("segtags: %d functions without a segment tag" % bad)
    return 1 if bad else 0


sys.exit(main())
