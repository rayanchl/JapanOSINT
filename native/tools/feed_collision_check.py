#!/usr/bin/env python3
"""Measure a feed row's COLLISION verdict against the feed itself.

    python3 tools/feed_collision_check.py MANIFEST EMIT_TSV [--jobs 8]

For every row audit_registry_emit.py called COLLISION (emitted > stored), fetch
the feed once and count: items, distinct keys (guid, else link, else id), keys
that repeat, and how many of the repeats are BYTE-IDENTICAL items. A collision
whose surplus is byte-identical is the upstream's own duplicate and the sink
collapsing it is correct; a repeat that DIFFERS is a key the row has wrong
(CLAUDE.md rule 4b: a collision verdict is a question, not a diagnosis).
Measured over batches 43–48: every collision in 25,863 feed rows was the
upstream's own duplicate, and 20 French rows that could not be re-fetched at
the time were re-measured the same way on 2026-10-19.

Output, one line per COLLISION row: id, emitted, stored, reading. A row the
server will not serve right now (non-200, or HTML where the feed was) is
UNVERIFIABLE_NOW, never a verdict."""
import sys, re, subprocess, collections, hashlib, argparse, concurrent.futures as cf

# Shelling out to curl still goes through THE trust set (jo_tls, rule 4c):
# system roots + session CA + the intermediates the engine loads.
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
import jo_tls as _jo_tls
JO_CURL_TLS = _jo_tls.curl_args()


# The request is the ENGINE's (tools/jo_http.py): JO_USER_AGENT, or the
# core/host_agents.def agent for a listed host, and the engine's Accept.
# This tool used to send an agent of its own (one of eight spellings in
# this directory, 2026-11-07), so a host walled against one agent read
# differently here than in the engine.
import jo_http as _jo_http


def tag_text(item, tag):
    m = re.search(r"<%s[^>]*>(.*?)</%s>" % (tag, tag), item, re.S)
    return m.group(1).strip() if m else ""


def key_of(item):
    return tag_text(item, "guid") or tag_text(item, "link") or tag_text(item, "id")


def measure(url):
    p = subprocess.run(["curl", *JO_CURL_TLS, "-sS", "-L", "--compressed", "--max-time", "30", *_jo_http.curl_header_args(url),
                        "-o", "-", "-w", "\n%{http_code}", url], capture_output=True)
    b = p.stdout; i = b.rfind(b"\n")
    code = b[i + 1:].decode(errors="ignore"); t = b[:i].decode("utf-8", "replace")
    html = "<html" in t[:500].lower()
    if code != "200" or html:
        return f"UNVERIFIABLE_NOW ({code}, html={html})"
    items = re.findall(r"<(?:item|entry)[\s>].*?</(?:item|entry)>", t, re.S)
    keys = [key_of(i) for i in items]
    ident = dupk = 0
    for k, c in collections.Counter(keys).items():
        if c > 1:
            dupk += c - 1
            d = [i for i in items if key_of(i) == k]
            if len(set(hashlib.md5(i.encode()).hexdigest() for i in d)) == 1:
                ident += c - 1
    return (f"items {len(items)} distinct_keys {len(set(keys))} repeated_keys {dupk} "
            f"byte_identical_surplus {ident}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("manifest")
    ap.add_argument("emit_tsv")
    ap.add_argument("--jobs", type=int, default=8)
    a = ap.parse_args()
    rows = {}
    for l in open(a.manifest, encoding="utf-8").read().splitlines():
        if l.startswith("#") or not l.strip():
            continue
        rows[l.split("|", 1)[0]] = l.split("|")
    coll = [l.split("\t") for l in open(a.emit_tsv, encoding="utf-8").read().splitlines()[1:]
            if l.split("\t")[1] == "COLLISION" and l.split("\t")[0] in rows]

    def one(r):
        sid, e, s = r[0], r[4], r[5]
        return sid, e, s, measure(rows[sid][9])

    with cf.ThreadPoolExecutor(a.jobs) as ex:
        for sid, e, s, v in ex.map(one, coll):
            print(f"{sid}\t{e}\t{s}\t{v}")


if __name__ == "__main__":
    main()
