#!/usr/bin/env python3
"""Probe every RSSX() feed URL in the given .c files with curl, two ways:
   (a) exactly like rss_collect does (UA japanosint-collector, 8s, 2 retries-ish)
   (b) with a browser UA + following redirects
so we can tell 404-moved from 403-WAF from UA-gated."""
import re, subprocess, sys, concurrent.futures

COLLECTOR_UA = "japanosint-collector"
BROWSER_UA = ("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
              "(KHTML, like Gecko) Chrome/124.0 Safari/537.36")

RSSX = re.compile(r'RSSX\(\s*\w+\s*,\s*"([^"]+)"\s*,(?:[^;]*?)\n?\s*"(https?://[^"]+)"', re.S)


def probe(ua, url, follow):
    cmd = ["curl", "-sS", "-o", "/dev/null", "-m", "20",
           "-w", "%{http_code} %{url_effective} %{size_download}",
           "-H", "Accept: application/rss+xml,*/*", "-A", ua, url]
    if follow:
        cmd.insert(1, "-L")
    try:
        p = subprocess.run(cmd, capture_output=True, timeout=30, text=True)
        return (p.stdout or p.stderr.strip()[:80]).strip()
    except Exception as e:
        return "ERR %s" % e


def main():
    ids = []
    for f in sys.argv[1:]:
        src = open(f, encoding="utf-8").read()
        for m in RSSX.finditer(src):
            ids.append((m.group(1), m.group(2)))
    print("# %d feeds" % len(ids), file=sys.stderr)

    def work(t):
        sid, url = t
        a = probe(COLLECTOR_UA, url, False)
        b = probe(BROWSER_UA, url, True)
        return sid, url, a, b

    with concurrent.futures.ThreadPoolExecutor(max_workers=12) as ex:
        for sid, url, a, b in ex.map(work, ids):
            print("%-22s | asis=%-40s | browserL=%s\n    %s" % (sid, a[:40], b[:110], url))


main()
