#!/usr/bin/env python3
"""Emit the ids and endpoints already present in the tree, and check a manifest.

A new batch must not re-register something the tree already has. There are
11k+ sources across 4k+ hosts, so this is not something an author can hold in
their head -- batch 18 wrote 70 rows that duplicated existing endpoints or ids
before this check existed.

Two subtleties this encodes, both learned the hard way:

  * `.portal` is documentation, never fetched. Harvesting it as an endpoint
    makes two tables that merely cite the same portal look like a duplicate
    fetch. For hp_source tables only `.url` and `.detail_url` count.
  * URL templates differ only in their placeholder: `?q={q}` and `?q=%s` are
    the same endpoint. Both normalise to `{}` before comparison.

Usage:
  batch_exclusions.py --dump-ids ids.txt --dump-urls urls.txt
  batch_exclusions.py --check MANIFEST...          # exit 1 if any row collides
"""
import argparse
import collections
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
COLLECTORS = os.path.join(NATIVE, "collectors")

URL_FIELD = re.compile(r'\.(?:url|detail_url)\s*=\s*"([^"]+)"')
ANY_URL = re.compile(r'https?://[^"\s\\]+')
ID_FIELD = re.compile(r'\.id\s*=\s*"([A-Za-z0-9_]+)"')
REG_SOURCE = re.compile(r'REGISTER_SOURCE\(\s*([A-Za-z0-9_]+)')
IS_TABLE = re.compile(r"hp_source\s+\w+\[\]")
TRAIL = '",)\\'


def norm(u):
    u = u.strip().rstrip(TRAIL)
    u = re.sub(r"^https?://", "", u)
    u = re.sub(r"\{q[a-zA-Z]*\}|%s|%d|\{v\}|\{key\}", "{}", u)
    return u.rstrip("/").lower()


def scan(skip_prefix=None):
    """-> (ids, {normalised endpoint: {file, ...}})"""
    ids = set()
    eps = collections.defaultdict(set)
    for root, _d, names in os.walk(COLLECTORS):
        if os.sep + "obj" in root:
            continue
        for f in names:
            if not f.endswith((".c", ".inc")):
                continue
            if skip_prefix and f.startswith(skip_prefix):
                continue
            p = os.path.join(root, f)
            try:
                t = io.open(p, encoding="utf-8", errors="replace").read()
            except Exception:
                continue
            ids.update(ID_FIELD.findall(t))
            ids.update(REG_SOURCE.findall(t))
            urls = URL_FIELD.findall(t) if IS_TABLE.search(t) else ANY_URL.findall(t)
            rel = os.path.relpath(p, NATIVE)
            for u in urls:
                if u.startswith("http"):
                    eps[norm(u)].add(rel)
    return ids, eps


def load_manifest(paths):
    rows = []
    for p in paths:
        for lno, line in enumerate(io.open(p, encoding="utf-8"), 1):
            s = line.rstrip("\n")
            if s.startswith("#") or s.count("|") != 12:
                continue
            f = s.split("|")
            rows.append((os.path.basename(p), lno, f[0], f[9]))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump-ids")
    ap.add_argument("--dump-urls")
    ap.add_argument("--check", nargs="*")
    ap.add_argument("--skip-prefix", default="hp3_",
                    help="collector filename prefix to treat as 'not yet in tree'")
    a = ap.parse_args()

    ids, eps = scan(a.skip_prefix)
    if a.dump_ids:
        io.open(a.dump_ids, "w", newline="\n").write("\n".join(sorted(ids)) + "\n")
        sys.stderr.write("%d ids -> %s\n" % (len(ids), a.dump_ids))
    if a.dump_urls:
        io.open(a.dump_urls, "w", newline="\n").write("\n".join(sorted(eps)) + "\n")
        sys.stderr.write("%d endpoints -> %s\n" % (len(eps), a.dump_urls))

    if a.check:
        bad = 0
        seen_new = {}
        for fname, lno, sid, url in load_manifest(a.check):
            if sid in ids:
                print("%s:%d DUP-ID       %s" % (fname, lno, sid)); bad += 1
            elif sid in seen_new:
                print("%s:%d DUP-ID-BATCH %s (also %s)" % (fname, lno, sid, seen_new[sid])); bad += 1
            else:
                seen_new[sid] = "%s:%d" % (fname, lno)
            k = norm(url)
            if k in eps:
                print("%s:%d DUP-ENDPOINT %s -> already in %s"
                      % (fname, lno, sid, sorted(eps[k])[0])); bad += 1
            else:
                eps[k].add("(this batch)")
        print("collisions: %d" % bad)
        return 1 if bad else 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
