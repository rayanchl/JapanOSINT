#!/usr/bin/env python3
"""Say WHY a row emitted nothing, and propose the keys that fix it.

`audit_batch_emit.py` proves the defect exists — "fetched 36,166, emitted 0" —
but not its cause, and the cause turns out not to be one thing. Running it over
the 59 batch-18 rows that drop everything separates three populations:

  NUMERIC_ID    the record HAS a conventional identifier, as a JSON number.
                `hp_pick` tests `cJSON_IsString`, so `{"asn": 3215}` matches
                nothing and the record is discarded as shape noise. No manifest
                edit can fix this — a number is a perfectly good identifier and
                the engine is wrong to insist on a string.
  UNCONVENTIONAL the record is labelled, under a name no fallback list knows
                (CelesTrak's OBJECT_NAME, USGS's SiteName). This is what
                title_keys/id_keys exist for; the fix is per-row.
  UNLABELLED    the record genuinely carries no title and no id — a bare array
                of values, or a scalar list. Neither an engine change nor a key
                declaration helps; the row needs a different array_path, or it
                is not a record source at all.

The three want different fixes, so guessing at title_keys for all 59 would have
"fixed" rows whose real defect was the engine's string-only test, and left the
underlying bug in place for every future batch.

Usage:
  diagnose_emit_keys.py MANIFEST... [--emit-tsv audit_batch_emit output]
                                    [--only ID,ID] [--jobs N] [--out t.tsv]
"""
import argparse
import io
import json
import os
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "collectors"))

from gen_hp_batch import load, split_opts          # noqa: E402
from probe_hp_batch import fetch, row_headers      # noqa: E402

# Mirrored from lib/hpengine.c. Kept in sync by hand; the test below prints the
# engine's own lists so a drift shows up as a diff rather than a wrong verdict.
TITLE_FALLBACK = [
    "name", "title", "legalName", "legal_name", "companyName", "company_name",
    "fullName", "full_name", "display_name", "displayName", "label", "caption",
    "nom", "nom_complet", "navn", "nome", "nombre", "denomination", "subject",
    "headline", "description", "summary",
]
ID_FALLBACK = [
    "id", "uid", "uuid", "number", "company_number", "companyNumber", "lei",
    "cik", "siren", "registrationNumber", "registration_number",
    "organisasjonsnummer", "businessId", "doc_id", "docId", "accession",
    "identifier", "key", "code",
]

# Names that read as a label to a human but appear in no fallback list. Ordered
# most-specific first: a record with both OBJECT_NAME and NAME should title on
# the more precise one.
TITLE_HINTS = [
    "object_name", "sitename", "site_name", "station_name", "stationname",
    "facility_name", "org_name", "orgname", "entity_name", "actor",
    "threat_actor", "malware", "family", "common_name", "short_name",
    "shortname", "long_name", "longname", "product", "vessel_name",
    "airport", "airline", "callsign", "host", "hostname", "domain",
    "url", "ip", "address", "prefix", "asn_name", "as_name", "holder",
    "descr", "text", "message", "event", "type", "category", "status",
]
ID_HINTS = [
    "norad_cat_id", "object_id", "icao24", "icao", "iata", "site_no", "siteno",
    "station_id", "stationid", "asn", "as_number", "asnumber", "prefix",
    "cve", "cve_id", "sha256", "md5", "hash", "doi", "orcid", "pmid",
    "accession_number", "cik_str", "ticker", "symbol", "mmsi", "imo",
    "callsign", "ip", "domain", "url", "gid", "fid", "objectid",
]


def flatten(obj, prefix="", out=None, depth=0):
    """One-level-dotted flatten, matching hp_flat_get's last-segment lookup."""
    if out is None:
        out = {}
    if depth > 3:
        return out
    if isinstance(obj, dict):
        for k, v in obj.items():
            key = "%s.%s" % (prefix, k) if prefix else k
            if isinstance(v, (dict, list)):
                flatten(v, key, out, depth + 1)
            else:
                out[key] = v
    elif isinstance(obj, list):
        for i, v in enumerate(obj[:4]):
            key = "%s.%d" % (prefix, i) if prefix else str(i)
            if isinstance(v, (dict, list)):
                flatten(v, key, out, depth + 1)
            else:
                out[key] = v
    return out


def find_records(doc, array_path=None):
    """The array the engine would iterate: array_path if declared, else the
    longest list of objects anywhere in the document."""
    if array_path:
        cur = doc
        for seg in array_path.split("."):
            if isinstance(cur, dict) and seg in cur:
                cur = cur[seg]
            else:
                cur = None
                break
        if isinstance(cur, list) and cur:
            return cur
    if isinstance(doc, list):
        return doc
    best = []
    stack = [doc]
    while stack:
        cur = stack.pop()
        if isinstance(cur, dict):
            stack.extend(cur.values())
        elif isinstance(cur, list):
            if cur and isinstance(cur[0], dict) and len(cur) > len(best):
                best = cur
            stack.extend(x for x in cur[:3] if isinstance(x, (dict, list)))
    return best


def last_seg(k):
    return k.rsplit(".", 1)[-1]


def classify(rec):
    """Return (verdict, title_keys, id_keys, note)."""
    if not isinstance(rec, dict):
        return ("UNLABELLED", "", "", "record is %s, not an object"
                % type(rec).__name__)
    flat = flatten(rec)
    low = {last_seg(k).lower(): (k, v) for k, v in flat.items()}

    def hit(names, want_string):
        for n in names:
            e = low.get(n.lower())
            if not e:
                continue
            k, v = e
            if want_string and isinstance(v, str) and v:
                return k
            if not want_string and isinstance(v, (int, float)) and \
                    not isinstance(v, bool):
                return k
        return None

    # what the engine finds today
    has_title = hit(TITLE_FALLBACK, True)
    has_id = hit(ID_FALLBACK, True)
    if has_title or has_id:
        return ("ALREADY_OK", "", "",
                "engine should already emit (title=%s id=%s)" % (has_title, has_id))

    # a conventional identifier that is a number, not a string
    num_id = hit(ID_FALLBACK, False)
    num_title = hit(TITLE_FALLBACK, False)
    if num_id or num_title:
        return ("NUMERIC_ID", "", "",
                "conventional key holds a number: %s" % (num_id or num_title))

    # an unconventional but real label
    t = hit(TITLE_HINTS, True)
    i = hit(ID_HINTS, True) or hit(ID_HINTS, False)
    if t or i:
        return ("UNCONVENTIONAL", last_seg(t) if t else "",
                last_seg(i) if i else "",
                "labelled under a name no fallback knows")

    # nothing recognisable — report the widest string field so a human can judge
    strs = [(k, v) for k, v in flat.items() if isinstance(v, str) and v]
    if strs:
        strs.sort(key=lambda kv: -len(kv[1]))
        return ("UNCONVENTIONAL", last_seg(strs[0][0]), "",
                "no known name; widest string field is %s" % strs[0][0])
    return ("UNLABELLED", "", "", "record has no non-empty string field")


def run_one(r):
    rid = r["id"]
    opts = {}
    for kv in split_opts(r["opts"]):
        k, _, v = kv.partition("=")
        opts[k.strip()] = v.strip()
    try:
        status, ctype, raw = fetch(r["probe"], row_headers(r))
    except Exception as e:
        return (rid, r["mode"], "FETCH_FAIL", "", "", str(e)[:90], "")
    if r["mode"] != "json":
        return (rid, r["mode"], "NOT_JSON", "", "",
                "mode %s — inspect by hand" % r["mode"], "")
    try:
        doc = json.loads(raw.decode("utf-8", "replace"))
    except Exception as e:
        return (rid, r["mode"], "PARSE_FAIL", "", "", str(e)[:90], "")
    recs = find_records(doc, opts.get("array_path"))
    if not recs:
        return (rid, r["mode"], "UNLABELLED", "", "",
                "no record array found", "")
    verdict, tk, ik, note = classify(recs[0])
    sample = ",".join(sorted(last_seg(k) for k in
                             list(flatten(recs[0]).keys())[:14]))
    return (rid, r["mode"], verdict, tk, ik, note, sample[:160])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("manifests", nargs="+")
    ap.add_argument("--emit-tsv", help="audit_batch_emit.py output; diagnose "
                                       "its DROPS_EVERYTHING rows")
    ap.add_argument("--only")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--out")
    a = ap.parse_args()

    rows = load(a.manifests)
    want = None
    if a.emit_tsv:
        want = set()
        for line in io.open(a.emit_tsv, encoding="utf-8"):
            f = line.rstrip("\n").split("\t")
            if len(f) > 1 and f[1] == "DROPS_EVERYTHING":
                want.add(f[0])
    if a.only:
        want = set(x.strip() for x in a.only.split(","))
    if want is not None:
        rows = [r for r in rows if r["id"] in want]
    sys.stderr.write("diagnosing %d rows\n" % len(rows))

    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        res = list(ex.map(run_one, rows))

    if a.out:
        with io.open(a.out, "w", encoding="utf-8", newline="\n") as fh:
            fh.write("id\tmode\tverdict\ttitle_keys\tid_keys\tnote\tfields\n")
            for x in res:
                fh.write("\t".join(x) + "\n")

    tally = {}
    for x in res:
        tally[x[2]] = tally.get(x[2], 0) + 1
    print("\n" + "  ".join("%s=%d" % kv for kv in sorted(tally.items())))
    for verdict in ("NUMERIC_ID", "UNCONVENTIONAL", "UNLABELLED",
                    "ALREADY_OK", "NOT_JSON", "PARSE_FAIL", "FETCH_FAIL"):
        sel = [x for x in res if x[2] == verdict]
        if not sel:
            continue
        print("\n%s (%d)" % (verdict, len(sel)))
        for x in sel:
            print("  %-32s %s" % (x[0], x[5]))
            if x[3] or x[4]:
                print("  %-32s   -> title_keys=%s id_keys=%s" % ("", x[3], x[4]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
