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

UA = "Mozilla/5.0 (compatible; JapanOSINT/1.0; +https://github.com/rayanchl/JapanOSINT)"
DATE = re.compile(rb"<(pubDate|dc:date|updated|published)>([^<]{8,40})<")


def get(url):
    r = subprocess.run(["curl", "-sS", "-L", "--compressed", "--max-time", "25", "-A", UA,
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
        extra = "".join(";" + p for p in f[12].split(";")
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
