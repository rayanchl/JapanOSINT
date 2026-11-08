#!/usr/bin/env python3
"""Re-fetch feed rows whose URL changed AFTER validation and rewrite their
kind, opts and description from what the server serves NOW.

    python3 tools/feed_reclassify.py MANIFEST IDS_FILE [--jobs 12]

A URL changed after validation is unvalidated (CLAUDE.md): an unescaped
`&amp;` fixed in 534 French rows made Joomla serve Atom where the rows declared
RSS, so every one of them was re-fetched through this before it shipped. The
manifest is rewritten in place; ids that could not be re-read land in
IDS_FILE.bad with the reason, and are the caller's to drop."""
import sys, re, subprocess, datetime, collections, argparse, concurrent.futures as cf

# Shelling out to curl still goes through THE trust set (jo_tls, rule 4c):
# system roots + session CA + the intermediates the engine loads.
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
import jo_tls as _jo_tls
import manifest
JO_CURL_TLS = _jo_tls.curl_args()


# The request is the ENGINE's (tools/jo_http.py): JO_USER_AGENT, or the
# core/host_agents.def agent for a listed host, and the engine's Accept.
# This tool used to send an agent of its own (one of eight spellings in
# this directory, 2026-11-07), so a host walled against one agent read
# differently here than in the engine.
import jo_http as _jo_http
DATE = re.compile(rb"<(pubDate|dc:date|updated|published)>([^<]{8,40})<")


def get(url):
    r = subprocess.run(["curl", *JO_CURL_TLS, "-sS", "-L", "--compressed", "--max-time", "25", *_jo_http.curl_header_args(url),
                        "-o", "-", "-w", "\n%{http_code}", url], capture_output=True)
    b = r.stdout; i = b.rfind(b"\n")
    return b[i + 1:].decode(errors="ignore"), b[:i]


def classify(body):
    head = body[:3000].lower()
    if b"<feed" in head: return "entry", rb"<entry[\s>]", "atom"
    if b"<rdf:rdf" in head: return "item", rb"<item[\s>]", "rss1"
    if b"<rss" in head: return "channel.item", rb"<item[\s>]", "rss2"
    return None, None, None


def opts_for(kind, ap, interval, extra):
    if kind == "atom":
        o = f"array_path={ap};id_keys=id,link.@href;title_keys=title;date_keys=updated,published;link_keys=link.@href;interval={interval};timeout_ms=30000"
    else:
        o = f"array_path={ap};id_keys=guid,link;title_keys=title;date_keys=pubDate,dc:date;link_keys=link;interval={interval};timeout_ms=30000"
    return o + extra


def main():
    a = argparse.ArgumentParser()
    a.add_argument("manifest"); a.add_argument("ids_file"); a.add_argument("--jobs", type=int, default=12)
    a = a.parse_args()
    ids = set(l.strip() for l in open(a.ids_file) if l.strip())
    lines = open(a.manifest, encoding="utf-8").read().splitlines()

    def one(idx):
        f = lines[idx].split("|")
        try:
            http, body = get(f[9])
        except Exception:
            return idx, "ERR", None
        if http != "200": return idx, "HTTP_" + http, None
        ap, tag, kind = classify(body)
        if not ap: return idx, "NOT_FEED", None
        n = len(re.findall(tag, body))
        if n == 0: return idx, "EMPTY", None
        m = DATE.search(body)
        newest = m.group(2).decode("utf-8", "ignore").strip().replace("|", " ") if m else ""
        # keep what the row already carries beyond the feed opts: its interval
        # (measured, possibly re-tuned) and any documentation opt
        iv = re.search(r"(?:^|;)interval=(\d+)", f[12])
        # manifest.split_opts, never a bare split(";"): a header value carries
        # `\;` (the compat User-Agent does), and splitting on it wrote back a
        # half User-Agent with the next opt glued on — the swallowed-opt shape
        # gen_hp_batch.py rejects. Re-escape on the way out.
        extra = "".join(";" + p.replace(";", "\\;") for p in manifest.split_opts(f[12])
                        if p.split("=", 1)[0] in ("pagination_ok", "header1", "header2", "header3", "adaptive_interval"))
        f[12] = opts_for(kind, ap, iv.group(1) if iv else "3600", extra)
        f[11] = re.sub(r"^Fetched \S+: \d+ items, \w+ feed, newest [^.]*\.",
                       f"Fetched {datetime.date.today()}: {n} items, {kind.upper()} feed, newest {newest or '(no date field)'}.",
                       f[11])
        return idx, "OK", "|".join(f)

    todo = [i for i, l in enumerate(lines) if not l.startswith("#") and l.split("|", 1)[0] in ids]
    c = collections.Counter(); bad = []
    with cf.ThreadPoolExecutor(a.jobs) as ex:
        for idx, v, new in ex.map(one, todo):
            c[v] += 1
            if new: lines[idx] = new
            else: bad.append(lines[idx].split("|", 1)[0] + "\t" + v)
    open(a.manifest, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    open(a.ids_file + ".bad", "w").write("\n".join(bad) + ("\n" if bad else ""))
    print(dict(c), file=sys.stderr)


if __name__ == "__main__":
    main()
