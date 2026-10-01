#!/usr/bin/env python3
"""audit_page_param.py — rows whose URL already binds their own page parameter.

WHY THIS EXISTS. lib/hpengine.c used to build the next page by APPENDING
`&<page_param>=<n>` to the row's URL. For a row whose URL already carried that
parameter — the shape of every API that *requires* it on the first request
(PNCP answers 400 without `pagina`) — the request became
`…&pagina=1&pagina=2`, and a server that binds the FIRST occurrence of a
repeated parameter (Spring and JAX-RS both do) answered with page 1 again. The
walk then emitted N identical pages, the sink stored one, and the run exited 0:
BR_PNCP_CONTRATOS lost 4,499 of 5,000 records with every gate green.

The engine now replaces the parameter in place (hp_url_set_param, pinned by
tests/hpengine_test.c "9f-bis"), so this is no longer a live defect. It stays as
a lint because the shape is worth seeing: a row in this list is one whose paging
depends entirely on that replacement being correct, and — more useful day to day
— a duplicated parameter in a hand-written URL is often a sign the author meant
to declare `page_start` instead.

    python3 native/tools/audit_page_param.py [root]      # default: native/collectors

Exit code is 0 either way: this reports, it does not gate. Measured 2026-09-11:
1,431 rows declare page_param, 103 of them carry it in the URL as well.
"""
import os
import re
import sys

ID_RE = re.compile(r'\.id\s*=\s*"([^"]+)"')
PP_RE = re.compile(r'\.page_param\s*=\s*"([A-Za-z0-9_]+)"')
URL_RE = re.compile(r'\.url\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+)')
LIT_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def scan(root):
    hits, total, unreadable = [], 0, 0
    for dirpath, _dirs, files in os.walk(root):
        for f in files:
            if not f.endswith((".c", ".inc")):
                continue
            path = os.path.join(dirpath, f)
            text = open(path, encoding="utf-8", errors="replace").read()
            marks = list(ID_RE.finditer(text))
            for i, m in enumerate(marks):
                end = marks[i + 1].start() if i + 1 < len(marks) else min(len(text), m.start() + 6000)
                block = text[m.start():end]
                pm = PP_RE.search(block)
                if not pm:
                    continue
                total += 1
                um = URL_RE.search(block)
                if not um:
                    unreadable += 1          # runtime-composed url; not judged
                    continue
                url = "".join(LIT_RE.findall(um.group(1)))
                key = pm.group(1)
                if re.search(r"[?&]" + re.escape(key) + r"=", url):
                    hits.append((path, m.group(1), key, url))
    return hits, total, unreadable


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "native/collectors"
    if not os.path.isdir(root):
        print(f"no such directory: {root}", file=sys.stderr)
        return 2
    hits, total, unreadable = scan(root)
    for path, sid, key, url in sorted(hits):
        print(f"{sid}\tpage_param={key}\t{path}")
        print(f"    {url[:160]}")
    print()
    print(f"rows declaring page_param            : {total}")
    print(f"rows whose URL also carries that key : {len(hits)}")
    print(f"rows whose url is composed at runtime: {unreadable}  (not judged)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
