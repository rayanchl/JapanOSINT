#!/usr/bin/env python3
"""Turn agent-discovered, live-probed endpoint specs into a batch-16 manifest.

Batch 15 discovered breadth: open-data portals, probed for platform, expanded
along the platform's fixed contract. That got ~1,000 catalogue-shaped sources.

Batch 16 is the other axis - PENETRANCY. A search endpoint that answers "does
this name appear?" is one hop deep. What an investigation needs is the record
behind the hit: the officers behind a company, the violations behind a facility,
the subawards under a contract, the docket entries behind a case. So each spec
here may carry a detail_url/detail_key pair, and rows that have one are worth
far more than rows that do not.

Input is JSON emitted by the discovery agents, each row of which the agent
claims to have personally fetched. This script does NOT trust that claim - it
normalises the rows into the standard candidate TSV and hands them to
tools/promote_candidates.py, which re-probes every one. An agent that recalled
an endpoint from documentation instead of fetching it loses the row there.

Usage:
  gen_sources_batch16.py --specs specs.json --out ../docs/candidate-sources-batch16.tsv \
      --exclude-urls <tree urls> --exclude-ids <tree ids>
"""
import argparse
import io
import json
import os
import re
import sys
import urllib.parse

COLUMNS = ["id", "url", "kind", "items", "name", "name_ja", "collector",
           "category", "lang", "tags", "interval", "description",
           "verified", "verified_note"]

# A source that reaches a second hop is the point of this batch, so the two
# extra columns ride in `tags` rather than forcing a schema change on the
# shared manifest format that batches 13-15 also use.
DETAIL_TAG = "detail-hop"

LANG_BY_COUNTRY = {
    "JP": "ja", "FR": "fr", "DE": "de", "IT": "it", "ES": "es", "NL": "nl",
    "BE": "nl", "PL": "pl", "SE": "sv", "DK": "da", "NO": "no", "FI": "fi",
    "AT": "de", "CH": "de", "PT": "pt", "CZ": "cs", "GR": "el", "RO": "ro",
    "HU": "hu", "SK": "sk", "SI": "sl", "HR": "hr", "BG": "bg", "EE": "et",
    "LV": "lv", "LT": "lt", "NO": "no",
}

# Scheduled cadence by category. A registry that changes on filing does not
# need the cadence of a hazard feed, and hammering a government API is how a
# source gets blocked for everyone.
INTERVAL_BY_CATEGORY = {
    "government": 86400, "legal": 43200, "corporate": 86400,
    "procurement": 21600, "health": 43200, "environment": 10800,
    "transport": 3600, "research": 86400, "civilian": 21600,
}


def slug(s):
    s = re.sub(r"[^a-zA-Z0-9]+", "-", (s or "").lower()).strip("-")
    return re.sub(r"-{2,}", "-", s)[:64]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--specs", required=True, help="JSON array of discovered specs")
    ap.add_argument("--out", required=True)
    ap.add_argument("--exclude-urls")
    ap.add_argument("--exclude-ids")
    a = ap.parse_args()

    specs = json.load(io.open(a.specs, encoding="utf-8"))
    if isinstance(specs, dict):
        specs = specs.get("sources", [])

    seen_urls = set()
    if a.exclude_urls and os.path.exists(a.exclude_urls):
        seen_urls = {l.strip() for l in io.open(a.exclude_urls, encoding="utf-8",
                                                errors="replace") if l.strip()}
    seen_ids = set()
    if a.exclude_ids and os.path.exists(a.exclude_ids):
        seen_ids = {l.strip() for l in io.open(a.exclude_ids, encoding="utf-8",
                                               errors="replace") if l.strip()}

    rows, skipped = [], {"dup-url": 0, "dup-id": 0, "no-url": 0, "needs-key": 0}
    for s in specs:
        url = (s.get("url") or "").strip()
        sid = slug(s.get("id") or "")
        if not url.startswith("http") or not sid:
            skipped["no-url"] += 1
            continue
        # Normalise for the dedup check the same way the lint check does, so a
        # row that only differs by a trailing slash is caught here rather than
        # becoming the 532nd duplicate endpoint.
        p = urllib.parse.urlsplit(url)
        norm = "%s://%s%s%s" % (p.scheme, p.netloc.lower(), p.path.rstrip("/"),
                                ("?" + p.query) if p.query else "")
        if url in seen_urls or norm in seen_urls:
            skipped["dup-url"] += 1
            continue
        if sid in seen_ids:
            skipped["dup-id"] += 1
            continue
        # A row we could not probe without a credential we do not hold is not a
        # verified row. It is recorded in the rejects, not the manifest.
        if (s.get("key_env") or "").strip():
            skipped["needs-key"] += 1
            continue
        seen_urls.add(url)
        seen_urls.add(norm)
        seen_ids.add(sid)

        cc = (s.get("country") or "").upper()
        cat = (s.get("category") or "government").lower()
        tags = [cc.lower(), cat, "batch16", "high-penetrancy"]
        if s.get("detail_verified") and s.get("detail_url"):
            tags.append(DETAIL_TAG)

        desc = (s.get("description") or "").strip()
        # DO NOT put the detail URL in the description.
        #
        # Two reasons, both learned the hard way. First, it is a claim the code
        # cannot honour: these rows render as VJSON, and that macro fetches ONE
        # url - it has no detail_url concept at all. A description saying
        # "Second hop: <url>" would promise a depth the collector does not have,
        # which is precisely the kind of statement house rule 1 exists to stop.
        # Second, a bare URL sitting in prose is indistinguishable from a fetched
        # endpoint to anything that greps the tree - it tripped the
        # dup-endpoint lint check with 12 phantom collisions that were really
        # two agents quoting the same detail hop in two descriptions.
        #
        # The verified hop is not lost: it is recorded in the manifest column
        # below and in docs/verified-sources-batch16.md, which is where the
        # future hpengine rows that CAN follow it will be built from.
        if s.get("detail_verified") and s.get("detail_url"):
            desc += ("  A per-record detail endpoint was verified for this "
                     "source; see docs/verified-sources-batch16.md. This "
                     "collector fetches the list endpoint only.")

        rows.append(dict(zip(COLUMNS, [
            sid, url, s.get("kind") or "", s.get("items") or "",
            s.get("name") or sid, "",
            "%s_%s" % (cc.lower() or "xx", cat),
            cat, s.get("lang") or LANG_BY_COUNTRY.get(cc, "en"),
            ",".join(tags),
            str(INTERVAL_BY_CATEGORY.get(cat, 86400)),
            desc.replace("\t", " ").replace("\n", " "),
            "no", "pending-independent-probe",
        ])))

    with io.open(a.out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\t".join(COLUMNS) + "\n")
        for r in rows:
            fh.write("\t".join(str(r[c]).replace("\t", " ") for c in COLUMNS) + "\n")

    # Side-car of the proven second hops. gen_verified_sources.py renders the
    # 12 standard manifest columns and would drop anything extra, so the hops
    # ride alongside rather than inside. This file is the input for the hpengine
    # rows that can actually follow them - without it the single most valuable
    # thing this batch learned (which endpoints have a working detail hop, and
    # what key drives it) would be thrown away at the generator seam.
    hop_path = os.path.splitext(a.out)[0] + ".detail-hops.tsv"
    hops = [s for s in specs if s.get("detail_verified") and s.get("detail_url")]
    with io.open(hop_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("id\tlist_url\tdetail_url\tdetail_key\n")
        for s in hops:
            fh.write("%s\t%s\t%s\t%s\n" % (slug(s.get("id") or ""), s.get("url") or "",
                                           s.get("detail_url") or "",
                                           s.get("detail_key") or ""))
    sys.stderr.write("detail hops -> %s (%d)\n" % (hop_path, len(hops)))

    ndetail = sum(1 for r in rows if DETAIL_TAG in r["tags"])
    sys.stderr.write("candidates: %d (%d with a verified detail hop)\n" % (len(rows), ndetail))
    sys.stderr.write("skipped: %s\n" % skipped)
    sys.stderr.write("-> %s\n" % a.out)


if __name__ == "__main__":
    main()
