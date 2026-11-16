#!/usr/bin/env python3
"""Say WHY a row emitted nothing, and propose the keys that fix it.

`audit_batch_emit.py` proves the defect exists — "fetched 36,166, emitted 0" —
but not its cause, and the cause is not one thing. This fetches the row's
probe URL with the row's own request conditions and asks, of each record, what
lib/hpengine.c would do with it — through tools/hpjson.py, the shared model of
the engine's JSON path, so the answer is the ENGINE's and not this tool's:

  PATH_MISSING   the declared array_path does not resolve: the engine EMITS
                 NOTHING from the body (no densest-array guess). Fix the path.
  UNLABELLED     the records carry no non-empty scalar at all: dropped as shape
                 noise. Neither a key declaration nor an engine change helps;
                 the row needs a different array_path, or it is not a record
                 source at all.
  UNCONVENTIONAL the records ARE emitted, but keyed and titled on their FIRST
                 scalar (hp_first_scalar's last resort) because no declared or
                 fallback key matches; the proposed title_keys/id_keys name a
                 field that reads as a label. Per-row.
  ALREADY_OK     a declared or fallback key matches: if this row still emits
                 nothing the cause is elsewhere (status, wall, a later page).
  ERROR_DOC / HTTP_ERR / BOT_WALL / CHALLENGE / RATE_LIMITED / PARSE_FAIL /
  NOT_JSON / FETCH_FAIL  the body never reached the record path.

REBASED 2026-11-07. This tool modelled an older engine: a NUMERIC_ID verdict
for "the identifier is a JSON number" (hp_scalar_str has keyed on numbers
since batch 18, so every such verdict was a false finding), no first-scalar
fallback (so it called rows UNCONVENTIONAL/UNLABELLED that the engine stores),
a record finder that walked dict keys only and fell back to the densest array
where the engine emits nothing, and a flatten of depth 3 / 4 list elements
where a LATER field won a shared last segment (the engine: depth 8, 256
elements, FIRST in document order). Its docstring promised "the test below
prints the engine's own lists" and there was no test; the lists are now PARSED
from lib/hpengine.c and --self-test pins every behaviour above.

Usage:
  diagnose_emit_keys.py MANIFEST... [--emit-tsv audit_batch_emit output]
                                    [--only ID,ID] [--jobs N] [--out t.tsv]
  diagnose_emit_keys.py --self-test
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

from gen_hp_batch import load                      # noqa: E402
from manifest import parse_opts                    # noqa: E402  (THE opts parser)
import urllib.error                                # noqa: E402
from probe_hp_batch import fetch, row_headers      # noqa: E402
import hpjson                                      # noqa: E402  (THE engine model)
import jo_http                                     # noqa: E402

# The engine's own fallback lists, parsed out of lib/hpengine.c (never copied:
# a hand-kept copy here had drifted into a claim about a test that did not
# exist). self_test() fails if the parse finds nothing.
TITLE_FALLBACK = hpjson.TITLE_FALLBACK
ID_FALLBACK = hpjson.ID_FALLBACK

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


def last_seg(k):
    return k.rsplit(".", 1)[-1]


def _hint(flat, names):
    """The first hint name present as a non-empty scalar (hp_flat_get's
    rules) -> the flat key it resolved to, or None."""
    for n in names:
        for k, v in flat:
            if last_seg(k).lower() == n.lower() and hpjson.scalar_str(v) is not None:
                return k
    return None


def classify(rec, id_keys=None, title_keys=None):
    """Return (verdict, title_keys, id_keys, note) for one record, judged by
    the engine's own key resolution (hpjson.record_key)."""
    flat = hpjson.json_flat(rec, (id_keys or "", title_keys or ""))
    basis, val, key = hpjson.record_key(flat, id_keys, title_keys)
    if basis == "empty":
        return ("UNLABELLED", "", "", "record has no non-empty scalar: the "
                "engine drops it as shape noise (%s)" % type(rec).__name__)
    if basis != "first_scalar":
        return ("ALREADY_OK", "", "", "engine keys it via %s (%s=%s)"
                % (basis, key or "title", (val or "")[:40]))
    t = _hint(flat, TITLE_HINTS)
    i = _hint(flat, ID_HINTS)
    if t or i:
        return ("UNCONVENTIONAL", last_seg(t) if t else "", last_seg(i) if i else "",
                "emitted, but keyed and titled on its first scalar %s; "
                "labelled under a name no fallback knows" % key)
    strs = [(k, v) for k, v in flat if isinstance(v, str) and v]
    if strs:
        strs.sort(key=lambda kv: -len(kv[1]))
        return ("UNCONVENTIONAL", last_seg(strs[0][0]), "",
                "emitted, keyed on its first scalar %s; no known name, widest "
                "string field is %s" % (key, strs[0][0]))
    return ("UNCONVENTIONAL", "", last_seg(key),
            "emitted, keyed on its first scalar %s; no string field to title on" % key)


def diagnose_doc(doc, opts):
    """(verdict, title_keys, id_keys, note, fields) for a parsed body."""
    ap = opts.get("array_path")
    recs, how = hpjson.select_records(doc, ap or None)
    if how == "declared-missing":
        return ("PATH_MISSING", "", "", "array_path %r does not resolve: the "
                "engine emits nothing from this body" % ap, "")
    if how == "root-record" and hpjson.json_error_doc(doc):
        return ("ERROR_DOC", "", "", "HTTP 200 carrying an error document", "")
    flats = [r for r in recs if r is not None]
    if not flats:
        return ("UNLABELLED", "", "", "the record array is empty", "")
    verdict, tk, ik, note = classify(flats[0], opts.get("id_keys"), opts.get("title_keys"))
    fields = ",".join(sorted({last_seg(k) for k, _ in hpjson.json_flat(flats[0])})[:14])
    return verdict, tk, ik, "%s [%s]" % (note, how), fields[:160]


def run_one(r):
    rid = r["id"]
    opts, _dups, _junk = parse_opts(r["opts"])
    if r["mode"] != "json":
        return (rid, r["mode"], "NOT_JSON", "", "",
                "mode %s — inspect by hand" % r["mode"], "")
    try:
        status, ctype, raw = fetch(r["probe"], row_headers(r), mode=r["mode"])
    except urllib.error.HTTPError as e:
        try:
            eb = e.read(256 * 1024).decode("utf-8", "replace")
        except Exception:
            eb = ""
        wk = jo_http.wall_kind(eb, e.code)
        return (rid, r["mode"], wk or "HTTP_ERR", "", "", "HTTP %d" % e.code, "")
    except Exception as e:
        return (rid, r["mode"], "FETCH_FAIL", "", "", str(e)[:90], "")
    text = raw.decode("utf-8", "replace")
    doc = hpjson.parse_body(text)
    if doc is hpjson.MISSING:
        wk = jo_http.wall_kind(text, status)
        return (rid, r["mode"], wk or "PARSE_FAIL", "", "", text[:90].replace("\n", " "), "")
    v, tk, ik, note, fields = diagnose_doc(doc, opts)
    return (rid, r["mode"], v, tk, ik, note, fields)


def self_test():
    """Each case is a disagreement the old model had with the engine."""
    cases = [
        ("a numeric id is keyed, not NUMERIC_ID", {"items": [{"asn": 1, "id": 3215}]},
         {"array_path": "items"}, "ALREADY_OK"),
        ("no known name: emitted on its first scalar",
         {"r": [{"OBJECT_NAME": "ISS", "NORAD_CAT_ID": 25544}]}, {"array_path": "r"},
         "UNCONVENTIONAL"),
        ("a declared path that does not resolve emits nothing",
         {"other": [{"id": 1}]}, {"array_path": "results"}, "PATH_MISSING"),
        ("a record of nulls is dropped", {"r": [{"a": None, "b": ""}]}, {"array_path": "r"},
         "UNLABELLED"),
        ("an id at depth 5 is reached (old flatten stopped at 3)",
         {"r": [{"a": {"b": {"c": {"d": {"id": "x"}}}}}]}, {"array_path": "r"}, "ALREADY_OK"),
        ("a declared id_keys is honoured", {"r": [{"OBJ": "a", "ZZ": "b"}]},
         {"array_path": "r", "id_keys": "ZZ"}, "ALREADY_OK"),
    ]
    bad = 0
    if not (TITLE_FALLBACK and ID_FALLBACK and ID_FALLBACK[0] == "id"):
        bad += 1
        print("diagnose SELF-TEST FAIL  fallback lists not parsed from lib/hpengine.c")
    for label, doc, opts, want in cases:
        got = diagnose_doc(doc, opts)[0]
        if got != want:
            bad += 1
            print("diagnose SELF-TEST FAIL  %-58s got=%s want=%s" % (label, got, want))
    # The proposal picks the FIRST of two fields sharing a last segment, as
    # hp_flat_get does (the old flatten let the later one win).
    if hpjson.flat_get(hpjson.json_flat({"a": {"x": "first"}, "b": {"x": "second"}}), "x") != "first":
        bad += 1
        print("diagnose SELF-TEST FAIL  shared last segment: first in document order must win")
    return bad == 0


def main():
    if "--self-test" in sys.argv[1:]:
        ok = self_test()
        print("diagnose fixtures: %s" % ("all pass" if ok else "FAILED"))
        return 0 if ok else 2
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
    for verdict in ("PATH_MISSING", "UNCONVENTIONAL", "UNLABELLED",
                    "ALREADY_OK", "ERROR_DOC", "HTTP_ERR", "BOT_WALL", "CHALLENGE",
                    "RATE_LIMITED", "NOT_JSON", "PARSE_FAIL", "FETCH_FAIL"):
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
