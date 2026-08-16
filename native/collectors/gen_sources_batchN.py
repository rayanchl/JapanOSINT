#!/usr/bin/env python3
"""Agent-discovered endpoint specs -> a candidate manifest, for any batch.

This is gen_sources_batch16.py generalised. That script hard-coded "batch16" in
its tags and its side-car filename, so batch 17 would have meant a copy with two
strings changed -- which is how a tree ends up with five near-identical
generators that drift apart. One script, a --batch flag.

The contract is unchanged and is the important part: this does NOT trust the
discovering agent. It normalises rows, drops anything that duplicates a URL or
id already in the tree, drops anything gated on a credential we do not hold, and
hands the rest to tools/promote_candidates.py which re-probes every one. Then --
and this is the step batch 16 added -- the generated collectors are RUN, and
only the ones that actually emit intel rows are kept. "Answers HTTP 200" and
"produces a row" are different claims and only the second one ships.

Usage:
  gen_sources_batchN.py --specs specs.json --batch 17 \
      --out ../docs/candidate-sources-batch17.tsv \
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

DETAIL_TAG = "detail-hop"

LANG_BY_COUNTRY = {
    "JP": "ja", "FR": "fr", "DE": "de", "IT": "it", "ES": "es", "NL": "nl",
    "BE": "nl", "PL": "pl", "SE": "sv", "DK": "da", "NO": "no", "FI": "fi",
    "AT": "de", "CH": "de", "PT": "pt", "CZ": "cs", "GR": "el", "RO": "ro",
    "HU": "hu", "SK": "sk", "SI": "sl", "HR": "hr", "BG": "bg", "EE": "et",
    "LV": "lv", "LT": "lt", "UA": "uk", "RU": "ru", "TR": "tr", "IL": "he",
    "SA": "ar", "AE": "ar", "EG": "ar", "MA": "ar", "TN": "ar", "DZ": "ar",
    "QA": "ar", "KW": "ar", "JO": "ar", "IQ": "ar", "LB": "ar",
    "BR": "pt", "MX": "es", "AR": "es", "CL": "es", "CO": "es", "PE": "es",
    "EC": "es", "UY": "es", "PY": "es", "BO": "es", "VE": "es", "CR": "es",
    "PA": "es", "GT": "es", "DO": "es", "CU": "es", "HN": "es",
    "KR": "ko", "TW": "zh", "HK": "zh", "CN": "zh", "MO": "zh", "MN": "mn",
    "ID": "id", "PH": "en", "VN": "vi", "TH": "th", "MY": "ms", "SG": "en",
    "KH": "km", "LA": "lo", "MM": "my",
    "IN": "en", "PK": "en", "BD": "bn", "LK": "si", "NP": "ne",
    "GE": "ka", "AM": "hy", "AZ": "az", "KZ": "kk", "UZ": "uz", "KG": "ky",
    "RS": "sr", "BA": "bs", "MK": "mk", "AL": "sq", "ME": "sr", "MD": "ro",
    "SN": "fr", "CI": "fr", "CM": "fr", "BJ": "fr", "BF": "fr", "ML": "fr",
    "CD": "fr", "MG": "fr", "MU": "en",
}

INTERVAL_BY_CATEGORY = {
    "government": 86400, "legal": 43200, "corporate": 86400,
    "procurement": 21600, "health": 43200, "environment": 10800,
    "transport": 3600, "research": 86400, "civilian": 21600,
    "infrastructure": 43200, "energy": 10800, "statistics": 86400,
}


def slug(s):
    s = re.sub(r"[^a-zA-Z0-9]+", "-", (s or "").lower()).strip("-")
    return re.sub(r"-{2,}", "-", s)[:64]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--specs", required=True)
    ap.add_argument("--batch", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--exclude-urls")
    ap.add_argument("--exclude-ids")
    a = ap.parse_args()
    tag = "batch" + str(a.batch)

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

    rows = []
    skipped = {"dup-url": 0, "dup-id": 0, "no-url": 0, "needs-key": 0,
               "no-label-field": 0}
    for s in specs:
        url = (s.get("url") or "").strip()
        sid = slug(s.get("id") or "")
        if not url.startswith("http") or not sid:
            skipped["no-url"] += 1
            continue
        # The agent was asked to name the field that will label each record.
        # No named field means it did not look at a real record, and a record
        # the parser cannot label produces nothing - that was 44% of the
        # previous batch. Drop it here rather than discover it after generating
        # a collector that emits zero forever.
        if "label_field" in s and not (s.get("label_field") or "").strip():
            skipped["no-label-field"] += 1
            continue
        p = urllib.parse.urlsplit(url)
        norm = "%s://%s%s%s" % (p.scheme, p.netloc.lower(), p.path.rstrip("/"),
                                ("?" + p.query) if p.query else "")
        if url in seen_urls or norm in seen_urls:
            skipped["dup-url"] += 1
            continue
        if sid in seen_ids:
            skipped["dup-id"] += 1
            continue
        if (s.get("key_env") or "").strip():
            skipped["needs-key"] += 1
            continue
        seen_urls.add(url)
        seen_urls.add(norm)
        seen_ids.add(sid)

        cc = (s.get("country") or "").upper()
        cat = (s.get("category") or "government").lower()
        tags = [cc.lower(), cat, tag, "high-penetrancy"]
        if s.get("detail_verified") and s.get("detail_url"):
            tags.append(DETAIL_TAG)

        desc = (s.get("description") or "").strip()
        # Never quote the detail URL here: VJSON fetches one url and has no
        # detail_url concept, so naming it would claim a hop the collector
        # cannot make - and a bare URL in prose is indistinguishable from a
        # fetched endpoint to anything that greps the tree.
        if s.get("detail_verified") and s.get("detail_url"):
            desc += ("  A per-record detail endpoint was verified for this "
                     "source; see the batch's detail-hops side-car. This "
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

    hop_path = os.path.splitext(a.out)[0] + ".detail-hops.tsv"
    hops = [s for s in specs if s.get("detail_verified") and s.get("detail_url")]
    with io.open(hop_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("id\tlist_url\tdetail_url\tdetail_key\n")
        for s in hops:
            fh.write("%s\t%s\t%s\t%s\n" % (slug(s.get("id") or ""), s.get("url") or "",
                                           s.get("detail_url") or "",
                                           s.get("detail_key") or ""))

    ndetail = sum(1 for r in rows if DETAIL_TAG in r["tags"])
    sys.stderr.write("candidates: %d (%d with a verified detail hop)\n" % (len(rows), ndetail))
    sys.stderr.write("skipped: %s\n" % skipped)
    sys.stderr.write("detail hops -> %s (%d)\n" % (hop_path, len(hops)))
    sys.stderr.write("-> %s\n" % a.out)


if __name__ == "__main__":
    main()
