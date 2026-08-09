#!/usr/bin/env python3
"""Extract id -> url from an RSS-macro source file for the given ids."""
import re, sys
path = sys.argv[1]
want = set(sys.argv[2:]) if len(sys.argv) > 2 else None
txt = open(path, encoding='utf-8', errors='replace').read()
blocks = re.split(r'\n(?=[A-Z_]+\()', txt)
for b in blocks:
    if not re.match(r'[A-Z_]+\(', b):
        continue
    strs = re.findall(r'"((?:[^"\\]|\\.)*)"', b)
    if not strs:
        continue
    sid = strs[0]
    url = next((s for s in strs if s.startswith('http')), None)
    if not url:
        continue
    if want and sid not in want:
        continue
    print(sid, url)
