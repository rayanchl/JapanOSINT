#!/usr/bin/env python3
"""Find run() entry points in slice-05 files whose return value is a row count."""
import os, re, sys

BASE = '/mnt/c/Users/rayan/sources/repos/OSINTsaas/native'
SLICE = os.path.join(BASE, 'tests/audit/slice_05.md')

files = sorted(set(re.findall(r'`native/(collectors/sources/[a-z0-9_]+\.c)`',
                              open(SLICE, encoding='utf-8').read())))
print('slice files:', len(files))

# collect the function names used as .run = X
BAD = re.compile(r'return\s+(?!0\s*;|-1\s*;|rc\s*;)([A-Za-z_][A-Za-z0-9_]*\s*[;(]|[a-z_]+\s*[><=!]|.*\?\s*1\s*:|.*\?\s*[a-z_]+\s*:)')
for rel in files:
    path = os.path.join(BASE, rel)
    if not os.path.exists(path):
        print('MISSING', rel); continue
    src = open(path, encoding='utf-8', errors='replace').read()
    runs = set(re.findall(r'\.run\s*=\s*([A-Za-z_][A-Za-z0-9_]*)', src))
    hits = []
    for fn in sorted(runs):
        m = re.search(r'^\s*static\s+int\s+' + re.escape(fn) + r'\s*\(', src, re.M)
        if not m:
            continue
        # crude brace matching from the opening brace
        i = src.find('{', m.end())
        d = 0
        j = i
        while j < len(src):
            if src[j] == '{': d += 1
            elif src[j] == '}':
                d -= 1
                if d == 0:
                    break
            j += 1
        body = src[i:j]
        for r in re.finditer(r'return\s+([^;]+);', body):
            expr = r.group(1).strip()
            if expr in ('0', '-1'):
                continue
            if re.fullmatch(r'-?\d+', expr):
                hits.append((fn, expr, 'literal'))
            elif re.search(r'\?\s*1\s*:', expr) or re.search(r'\?\s*[a-z_]+\s*:\s*0', expr):
                hits.append((fn, expr, 'bool/count'))
            elif re.fullmatch(r'[a-z_][a-z0-9_]*', expr) and expr not in ('rc', 'r'):
                hits.append((fn, expr, 'bare var'))
            elif re.fullmatch(r'[a-z_][a-z0-9_]*\s*\(.*\)', expr, re.S):
                hits.append((fn, expr[:60], 'call result'))
            elif expr.startswith('rc') or expr.startswith('r '):
                pass
            else:
                hits.append((fn, expr[:70], 'other'))
    if hits:
        print('== %s' % rel)
        for h in hits:
            print('    %-22s return %-45s [%s]' % h)
