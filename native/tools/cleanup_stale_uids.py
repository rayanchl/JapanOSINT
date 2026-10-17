#!/usr/bin/env python3
"""Find, and on request delete, the rows that recent identity fixes left behind.

WHY THIS EXISTS (docs/audit-2026-10-02.md, "Still open", housekeeping)
Ten hand-written collectors used to resolve a shared key first-come-plain: the
first record to arrive kept the plain key as its uid and every later one got
`key|<discriminator>`. Since d07e8181 (lib/keyqual.h) EVERY member of a
colliding group is qualified, and FRA's crossings do the same since ffd82f76.
Before that, FRA, like the reg2 Socrata registers without a key2 field, keyed
a recurring id on the id alone and kept whichever row came last. A database
that ran the old code therefore still holds the old plain row of each
formerly colliding group. That row is a second copy of a record that now
lives under its qualified uid. wayback_jp moved twice: from a digest-only
uid, through a digest qualified per run, to digest+timestamp+URL (d3610739).
Its rows in the older shapes duplicate captures that are now stored under the
new uid.

WHAT COUNTS AS STALE. A row is deleted only when its replacement is PROVEN to
exist (house rule 2: remove duplicates, never the only copy of a record):

  plain    `<source>|<key>`, with the source's base-key shape, where qualified
           rows `<source>|<key>|...` exist. Its replacement must be a qualified
           sibling fetched AFTER it (one fetched before it means the key became
           unique again and this plain row is the current copy). It must also
           be the same record, shown one of two ways:
             exact    the uid the current code gives this record,
                      `<key>|<discriminator>`, with the discriminator read from
                      the row's OWN stored properties, exists;
             content  every property the old row stored is present, with the
                      same value and type, in the sibling, so nothing the old
                      row knew is lost. Later code versions add fields; a value
                      renamed, retyped or changed upstream fails the proof and
                      the row stays.
           A plain key with no qualified siblings is a normal record and is
           never looked at.
  shape    wayback-jp: a capture row whose uid is not the uid the current code
           derives from its own properties (digest|timestamp|URL). The
           replacement is that derived uid, and it must exist. Pages are walked
           over ~200 days, so most old captures have NOT been re-read yet; they
           are the only copy and stay.
           netherlands-rdw-parking-garages: the old repeat shape
           `<areamanagerid>|<areaid>`, reversed from today's `<areaid>|<manager>`.

Everything else is reported and left alone: no proof (`kept-unproven`), a
plain row newer than every sibling (`kept-newer`), an old capture not yet
re-read (`kept-no-replacement`), and a stale row that a case pin, an
annotation, an alert event, an evidence capture or a media asset points at
(`kept-referenced`). Deleting the row would leave those pointing at nothing.

NOT COVERED. EMODnet's and NGA's old REPEAT rows carried a hash of the raw
upstream record. What was stored cannot recompute it, so those rows cannot be
told from a current hash-qualified row and are not judged. The commit message
of d07e8181 counts at most 3 such groups for EMODnet and 10 for NGA. The other
collectors' old repeats used the discriminator the current code uses, so it
rewrites them under the same uid. Only their rare fallbacks for a missing
discriminator differ (meteoalarm's `entry-N`, telegeography's literal
`feature`, safecast's empty capture time), and those are not judged. EPA GHGRP
also goes through keyqual,
but its old code already qualified every member of a group (a0687d31), so it
left nothing behind.

DELETING. Only with --delete, in ONE transaction. Inside it, each proof is
checked again: the row must be unchanged since the scan, and its replacement
must still be there, still newer, and still holding what the row held. The
scan does not hold the write lock, so a collector can write in between. A row
that fails the check is left and named. A stale row goes with its
search-index row (intel_items_fts, via intel_items_fts_uid_map), its entity
mentions and extraction state, and its embedding bookkeeping (intel_vec_done,
intel_vec_failed, and intel_vec when the vec0 module can be loaded here). If it
cannot be loaded, intel_vec and intel_vec_done are left as they are, and the
tool says how many vectors remain. /api/intel/semantic reports those as
index_orphans. Every removed uid is printed with its replacement.

  python3 native/tools/cleanup_stale_uids.py "$JO_DB"            # report
  python3 native/tools/cleanup_stale_uids.py "$JO_DB" --delete   # remove
  python3 native/tools/cleanup_stale_uids.py --self-test         # scratch DB

The source ids and uid shapes below were read from the collectors named next
to each entry. Re-read them before adding a source; do not guess a shape.
"""
import argparse
import hashlib
import io
import json
import os
import shutil
import sqlite3
import sys
import tempfile

# ── how each collector writes a discriminator, mirrored from its C ──────────


def _sv(v):
    """jo_sv()/pstr(): a non-empty JSON string, else None."""
    return v if isinstance(v, str) and v else None


def _num_or_sv(fmt):
    """A number formatted with `fmt` (as snprintf would), or jo_sv()."""
    def f(v):
        if isinstance(v, bool):
            return None
        if isinstance(v, (int, float)):
            return fmt % v
        return _sv(v)
    return f


def _prop(name, conv=_sv):
    return lambda p: conv(p.get(name))


def _fra_disc(p):
    # fra_disc(): the row's agencyid, or the literal "no-agencyid" when it has
    # none. An absent agencyid only means "none upstream" when the row was
    # written by the code that keeps every Form 71 column (00df8a2d), and the
    # raw `crossingid` column is the mark of that. A row from before that
    # stored only readable names, and its agencyid is unknown.
    a = _sv(p.get("agencyid"))
    if a:
        return a
    return "no-agencyid" if "crossingid" in p else None


def _src(sid, coll, base_pipes, disc=None):
    return {"id": sid, "collector": coll, "base_pipes": base_pipes, "disc": disc}


REG2 = "collectors/sources/reg2_socrata_registries.c"
SOURCES = [
    # key = platformid, else name; disc = the WFS feature id, stored since
    # d07e8181 as wfs_feature_id (old rows lack it -> content proof).
    _src("emodnet-offshore-platforms",
         "collectors/sources/mar_emodnet_offshore_platforms.c", 0,
         _prop("wfs_feature_id")),
    # key = the register's key_f value, else a 20-hex hash of service+title.
    # Only us-chicago-contracts declares key2_f (revision_number, via
    # soc_field: a string, or a number as %.10g). The others qualify by a hash
    # of the raw row, which the stored properties cannot recompute.
    _src("us-co-business-entities", REG2, 0),
    _src("us-tx-franchise-taxpayers", REG2, 0),
    _src("us-ct-business-registry", REG2, 0),
    _src("us-nyc-business-licenses", REG2, 0),
    _src("us-nyc-procurement-notices", REG2, 0),
    _src("us-chicago-contracts", REG2, 0,
         _prop("revision_number", _num_or_sv("%.10g"))),
    _src("us-chicago-business-licenses", REG2, 0),
    _src("us-la-business-registry", REG2, 0),
    _src("us-sf-business-locations", REG2, 0),
    _src("us-seattle-business-licenses", REG2, 0),
    _src("co-secop-contracts", REG2, 0),
    # key = areaid, else areadesc; disc = areamanagerid (area_manager_id).
    _src("netherlands-rdw-parking-garages",
         "collectors/sources/trn_netherlands_rdw_parking_garages.c", 0,
         _prop("area_manager_id")),
    # key = "<CAP identifier>|<EMMA_ID>" (or "<title>|<areaDesc>"): ONE pipe.
    # disc = the Atom entry id, stored since d07e8181 as atom_id.
    _src("meteoalarm-cap", "collectors/sources/geoeo_meteoalarm_cap.c", 1,
         _prop("atom_id")),
    # key = "<volume>|<featureNumber>|<name:80>|<position:40>": THREE pipes.
    # disc = charNo (a number as %.15g, or a string), stored since d07e8181.
    _src("nga-list-of-lights", "collectors/sources/mar_nga_list_of_lights.c", 3,
         _prop("charNo", _num_or_sv("%.15g"))),
    # key = id_pdc_itinerance, else id_station_itinerance, else nom_station;
    # disc = datagouv_resource_id (stored under that name before and after).
    _src("france-irve-charging-stations",
         "collectors/sources/trn_france_irve_charging_stations.c", 0,
         _prop("datagouv_resource_id")),
    # key = cnaID, else shortName; disc = shortName (short_name).
    _src("cve-cna-directory", "collectors/sources/cert_cve_program.c", 0,
         _prop("short_name")),
    # key = "landing|<id or name>" / "cable|<id or name>": ONE pipe.
    # disc = feature_id (the upstream properties are stored verbatim).
    _src("telegeography-cables", "collectors/sources/telegeography_cables.c", 1,
         _prop("feature_id")),
    # key = device_urn; disc = when_captured.
    _src("safecast-realtime-devices", "collectors/sources/eni_safecast_devices.c", 0,
         _prop("when_captured")),
    # key = crossingid; disc = agencyid, else "no-agencyid" (see _fra_disc).
    _src("fra-grade-crossing-inventory",
         "collectors/sources/trn_fra_grade_crossing_inventory.c", 0, _fra_disc),
]
WAYBACK = "wayback-jp"                         # collectors/sources/wayback_jp.c
RDW = "netherlands-rdw-parking-garages"
NOTICE = "collector-truncation-notice"

# Tables whose rows point at an intel_items uid and that a person or a later
# step relies on. A stale row they reference is kept, never deleted from under
# them: (table, uid column, extra predicate).
REFERENCES = [
    ("case_items", "ref_id", "ref_type='intel_item'"),
    ("annotations", "ref_id", "ref_type='intel_item'"),
    ("alert_events", "item_uid", None),
    ("evidence", "item_uid", None),
    ("media_assets", "item_uid", None),
]


# ── wayback_jp's current uid, exactly as wayback_jp.c builds it ─────────────

def feed_hash_key(*parts):
    """lib/feedlib.c feed_hash_key(): sha1 over each part + "|", 20 hex."""
    h = hashlib.sha1()
    for p in parts:
        if p is None:
            continue
        h.update(p.encode("utf-8"))
        h.update(b"|")
    return h.hexdigest()[:20]


def wayback_key(props):
    """The remote key wayback_jp.c gives a capture with these properties, or
    None when they do not determine one."""
    host = _sv(props.get("target"))
    ts = _sv(props.get("timestamp"))
    orig = _sv(props.get("original"))
    dig = _sv(props.get("digest"))
    if not ts and not orig and not dig:
        return None                           # the collector skips such a row
    if orig and len(orig.encode("utf-8")) <= 360:
        rk = "%s|%s|%s" % (dig or "", ts or "", orig)
    else:
        if not orig and not host:
            return None
        rk = "%s|%s|url-sha1:%s" % (dig or "", ts or "", feed_hash_key(orig or host))
    if len(rk.encode("utf-8")) > 479:         # rk[480]: the C would truncate it
        return None
    return rk


# ── reading the database ────────────────────────────────────────────────────

def _props(text):
    try:
        v = json.loads(text) if text else {}
    except ValueError:
        return None
    return v if isinstance(v, dict) else None


def _canon(v):
    return json.dumps(v, sort_keys=True, ensure_ascii=False)


def props_contained(old, new):
    """Every property `old` stored is in `new` with the same value (types
    compared too: 1 is not "1", true is not 1)."""
    if not old or new is None:
        return False
    for k, v in old.items():
        if k not in new or _canon(new[k]) != _canon(v):
            return False
    return True


def table_exists(con, name):
    return con.execute("SELECT 1 FROM sqlite_master WHERE name=?", (name,)).fetchone() is not None


class Row:
    __slots__ = ("uid", "fetched_at", "text", "props")

    def __init__(self, uid, fetched_at, props_text):
        self.uid, self.fetched_at, self.text = uid, fetched_at or "", props_text
        self.props = _props(props_text)


def verdict_of(row, kind, verdict, rep, how, **extra):
    r = {"source": None, "uid": row.uid, "kind": kind, "verdict": verdict,
         "replacement": rep, "proof": how, "fetched_at": row.fetched_at, "text": row.text}
    r.update(extra)
    return r


def plain_candidates(con, src):
    """Plain rows of `src` (base-key shape) that have qualified siblings."""
    sid = src["id"]
    pre = sid + "|"
    q = """
      SELECT p.uid, p.fetched_at, p.properties FROM intel_items p
       WHERE p.source_id = ?1
         AND substr(p.uid, 1, ?2) = ?3
         AND COALESCE(p.record_type, '') <> ?5
         AND length(substr(p.uid, ?2 + 1))
             - length(replace(substr(p.uid, ?2 + 1), '|', '')) = ?4
         AND EXISTS (SELECT 1 FROM intel_items s
                      WHERE s.uid >= p.uid || '|' AND s.uid < p.uid || '}'
                        AND s.source_id = p.source_id)
       ORDER BY p.uid"""
    return [Row(*r) for r in con.execute(q, (sid, len(pre), pre, src["base_pipes"], NOTICE))]


def siblings(con, sid, uid):
    """Rows `<uid>|...` of the same source ('}' is the byte after '|')."""
    return [Row(*r) for r in con.execute(
        "SELECT uid, fetched_at, properties FROM intel_items "
        "WHERE uid >= ?1 AND uid < ?2 AND source_id = ?3 ORDER BY uid",
        (uid + "|", uid + "}", sid))]


def prove(row, sibs, disc_uid):
    """(verdict, replacement uid, how) for a row with candidate replacements."""
    newer = [s for s in sibs if s.fetched_at > row.fetched_at]
    if not newer:
        return "kept-newer", None, None
    if disc_uid is not None:
        for s in newer:
            if s.uid == disc_uid:
                return "stale", s.uid, "exact"
    if row.props:
        for s in newer:
            if props_contained(row.props, s.props):
                return "stale", s.uid, "content"
    return "kept-unproven", None, None


def judge_plain(con, src):
    out = []
    for row in plain_candidates(con, src):
        sibs = siblings(con, src["id"], row.uid)
        d = src["disc"](row.props) if (src["disc"] and row.props is not None) else None
        verdict, rep, how = prove(row, sibs, row.uid + "|" + d if d is not None else None)
        out.append(verdict_of(row, "plain", verdict, rep, how, source=src["id"]))
    return out


def judge_wayback(con):
    """Capture rows not in the shape the current collector writes. Narrowed in
    SQL to rows whose uid is not the plain digest|timestamp|URL form (or whose
    URL is missing); each is then checked exactly in Python."""
    pre = WAYBACK + "|"
    q = """
      SELECT uid, fetched_at, properties FROM intel_items
       WHERE source_id = ?1 AND record_type = ?1
         AND CASE WHEN json_valid(properties) THEN
               json_extract(properties, '$.original') IS NULL
               OR uid <> ?2 || COALESCE(json_extract(properties, '$.digest'), '') || '|'
                       || COALESCE(json_extract(properties, '$.timestamp'), '') || '|'
                       || json_extract(properties, '$.original')
             ELSE 0 END
       ORDER BY uid"""
    out = []
    for r in con.execute(q, (WAYBACK, pre)).fetchall():
        row = Row(*r)
        if row.props is None:
            continue
        rk = wayback_key(row.props)
        if rk is None or pre + rk == row.uid:
            continue                          # undetermined, or already current
        want = pre + rk
        hit = con.execute("SELECT uid FROM intel_items WHERE uid=?1 AND source_id=?2",
                          (want, WAYBACK)).fetchone()
        out.append(verdict_of(row, "wayback", "stale" if hit else "kept-no-replacement",
                              want if hit else None, "exact" if hit else None,
                              source=WAYBACK, wanted=want))
    return out


def judge_rdw_old_repeat(con):
    """RDW's old repeat uid was "<areamanagerid>|<areaid>", the reverse of
    today's "<areaid>|<areamanagerid>". It is recognised from the row's own
    properties. If the two ids were equal, the shape would not tell old from
    new, so such a row is left alone."""
    pre = RDW + "|"
    q = """
      SELECT uid, fetched_at, properties FROM intel_items
       WHERE source_id = ?1 AND CASE WHEN json_valid(properties) THEN
             json_extract(properties, '$.area_manager_id') IS NOT NULL
             AND json_extract(properties, '$.area_id') IS NOT NULL
             AND json_extract(properties, '$.area_manager_id') <> json_extract(properties, '$.area_id')
             AND uid = ?2 || json_extract(properties, '$.area_manager_id') || '|'
                     || json_extract(properties, '$.area_id')
             ELSE 0 END
       ORDER BY uid"""
    out = []
    for r in con.execute(q, (RDW, pre)).fetchall():
        row = Row(*r)
        aid, mgr = _sv(row.props.get("area_id")), _sv(row.props.get("area_manager_id"))
        if not aid or not mgr:
            continue
        base = pre + aid
        cands = siblings(con, RDW, base)
        plain = con.execute("SELECT uid, fetched_at, properties FROM intel_items "
                            "WHERE uid=?1 AND source_id=?2", (base, RDW)).fetchone()
        exact = base + "|" + mgr
        if plain:
            p = Row(*plain)
            if exact not in {c.uid for c in cands} and p.props \
                    and _sv(p.props.get("area_manager_id")) == mgr:
                exact = p.uid                 # unique again: the plain areaid is it
            cands.append(p)
        verdict, rep, how = prove(row, [c for c in cands if c.uid != row.uid], exact)
        out.append(verdict_of(row, "rdw-repeat", verdict, rep, how, source=RDW))
    return out


def references(con, uid):
    refs = []
    for table, col, extra in REFERENCES:
        if not table_exists(con, table):
            continue
        sql = "SELECT COUNT(*) FROM %s WHERE %s=?1%s" % (table, col, " AND " + extra if extra else "")
        n = con.execute(sql, (uid,)).fetchone()[0]
        if n:
            refs.append("%s:%d" % (table, n))
    return refs


def find(con, only=None):
    rows = []
    for src in SOURCES:
        if only and src["id"] not in only:
            continue
        rows += judge_plain(con, src)
    if not only or RDW in only:
        rows += judge_rdw_old_repeat(con)
    if not only or WAYBACK in only:
        rows += judge_wayback(con)
    # A replacement that is itself about to be deleted proves nothing: the row
    # is kept, never deleted along a chain.
    doomed = {r["uid"] for r in rows if r["verdict"] == "stale"}
    for r in rows:
        if r["verdict"] == "stale" and r["replacement"] in doomed:
            r["verdict"], r["replacement"], r["proof"] = "kept-unproven", None, None
    for r in rows:
        if r["verdict"] == "stale":
            refs = references(con, r["uid"])
            if refs:
                r["verdict"], r["refs"] = "kept-referenced", refs
    return rows


# ── reporting ───────────────────────────────────────────────────────────────

VERDICTS = ["stale", "kept-referenced", "kept-unproven", "kept-newer", "kept-no-replacement"]


def report(rows, sample, out=sys.stdout):
    by = {}
    for r in rows:
        by.setdefault(r["source"], []).append(r)
    total = sum(1 for r in rows if r["verdict"] == "stale")
    print("stale rows (replacement proven): %d across %d source(s)" %
          (total, len({r["source"] for r in rows if r["verdict"] == "stale"})), file=out)
    for sid in sorted(by):
        rs = by[sid]
        counts = {v: sum(1 for r in rs if r["verdict"] == v) for v in VERDICTS}
        proofs = {}
        for r in rs:
            if r["verdict"] == "stale":
                proofs[r["proof"]] = proofs.get(r["proof"], 0) + 1
        print("\n%s: %d stale%s; %s" % (
            sid, counts["stale"],
            " (%s)" % ", ".join("%d %s" % (n, k) for k, n in sorted(proofs.items())) if proofs else "",
            ", ".join("%d %s" % (counts[v], v) for v in VERDICTS[1:] if counts[v]) or "nothing kept"),
            file=out)
        for v in VERDICTS:
            shown = [r for r in rs if r["verdict"] == v][:sample]
            for r in shown:
                tail = ""
                if r.get("replacement"):
                    tail = "  -> %s (%s)" % (r["replacement"], r["proof"])
                elif r.get("wanted"):
                    tail = "  (no %s yet)" % r["wanted"]
                if r.get("refs"):
                    tail += "  referenced by %s" % ", ".join(r["refs"])
                print("  %-19s %s%s" % (v, r["uid"], tail), file=out)
    return total


# ── deleting ────────────────────────────────────────────────────────────────

def vec_available(con):
    if not table_exists(con, "intel_vec"):
        return True                           # nothing to keep consistent
    try:
        con.execute("SELECT uid FROM intel_vec LIMIT 0").fetchall()
        return True
    except sqlite3.OperationalError:
        pass
    try:                                      # the pip `sqlite-vec` package, if present
        import sqlite_vec                     # noqa: F401
        con.enable_load_extension(True)
        sqlite_vec.load(con)
        con.enable_load_extension(False)
        con.execute("SELECT uid FROM intel_vec LIMIT 0").fetchall()
        return True
    except Exception:
        return False


def recheck(con, r):
    """Why a proof found by the scan no longer holds, or None. Run inside the
    write transaction: the scan does not hold the write lock (it can take a
    minute on a large database and the collectors keep writing), so a row can
    be re-fetched, or its replacement rewritten or removed, in between."""
    cur = con.execute("SELECT fetched_at, properties FROM intel_items WHERE uid=?",
                      (r["uid"],)).fetchone()
    if not cur or (cur[0] or "") != r["fetched_at"] or cur[1] != r["text"]:
        return "the row was rewritten since the scan"
    rep = con.execute("SELECT fetched_at, properties FROM intel_items WHERE uid=?",
                      (r["replacement"],)).fetchone()
    if not rep:
        return "its replacement %s is gone" % r["replacement"]
    if r["kind"] != "wayback" and not (rep[0] or "") > r["fetched_at"]:
        return "its replacement is no longer newer than it"
    if r["proof"] == "content" and not props_contained(_props(r["text"]), _props(rep[1])):
        return "its replacement no longer holds every property it stored"
    if references(con, r["uid"]):
        return "something references it now"
    return None


def delete(con, rows, out=sys.stdout):
    stale = [r for r in rows if r["verdict"] == "stale"]
    if not stale:
        print("\nnothing to delete", file=out)
        return 0
    vec_ok = vec_available(con)
    con.execute("BEGIN IMMEDIATE")
    try:
        left = []
        for r in stale:
            why = recheck(con, r)
            if why:
                left.append((r, why))
        stale = [r for r in stale if r not in [x for x, _ in left]]
        con.execute("CREATE TEMP TABLE IF NOT EXISTS stale_uids(uid TEXT PRIMARY KEY)")
        con.execute("DELETE FROM stale_uids")
        con.executemany("INSERT OR IGNORE INTO stale_uids(uid) VALUES (?)", [(r["uid"],) for r in stale])
        side = {}
        if table_exists(con, "intel_items_fts"):
            mapped = 0
            if table_exists(con, "intel_items_fts_uid_map"):
                mapped = con.execute(
                    "DELETE FROM intel_items_fts WHERE rowid IN (SELECT m.rowid FROM "
                    "intel_items_fts_uid_map m JOIN stale_uids s ON s.uid=m.uid)").rowcount
                con.execute("DELETE FROM intel_items_fts_uid_map WHERE uid IN (SELECT uid FROM stale_uids)")
            # Index rows with no map entry (written before the map existed):
            # one scan for all of them, not one per uid.
            loose = con.execute("DELETE FROM intel_items_fts WHERE uid IN (SELECT uid FROM stale_uids)").rowcount
            side["intel_items_fts"] = mapped + max(loose, 0)
        for table, col in (("entity_mentions", "item_uid"), ("entity_extraction_state", "item_uid"),
                           ("intel_vec_failed", "uid")):
            if table_exists(con, table):
                side[table] = con.execute("DELETE FROM %s WHERE %s IN (SELECT uid FROM stale_uids)"
                                          % (table, col)).rowcount
        vec_left = 0
        if vec_ok:
            for table in ("intel_vec", "intel_vec_done"):
                if table_exists(con, table):
                    side[table] = con.execute("DELETE FROM %s WHERE uid IN (SELECT uid FROM stale_uids)"
                                              % table).rowcount
        elif table_exists(con, "intel_vec_done"):
            vec_left = con.execute("SELECT COUNT(*) FROM intel_vec_done WHERE uid IN "
                                   "(SELECT uid FROM stale_uids)").fetchone()[0]
        n = con.execute("DELETE FROM intel_items WHERE uid IN (SELECT uid FROM stale_uids)").rowcount
        if n != len({r["uid"] for r in stale}):
            raise RuntimeError("expected to delete %d intel_items rows, deleted %d" % (len(stale), n))
        con.execute("COMMIT")
    except BaseException:
        con.execute("ROLLBACK")
        raise
    if left:
        print("\nnot deleted, because the database changed after the scan:", file=out)
    for r, why in left:
        print("  left    %s: %s" % (r["uid"], why), file=out)
    print("\ndeleted %d stale row(s) in one transaction:" % n, file=out)
    for r in stale:
        print("  removed %s  -> kept %s (%s)" % (r["uid"], r["replacement"], r["proof"]), file=out)
    print("  with: " + (", ".join("%s %d" % kv for kv in sorted(side.items())) or "no dependent rows"), file=out)
    if vec_left:
        print("  intel_vec: the vec0 module is not loadable here, so %d embedding(s) of removed rows "
              "stay (intel_vec and intel_vec_done untouched); /api/intel/semantic counts them as "
              "index_orphans" % vec_left, file=out)
    return n


def open_db(path, write):
    if not os.path.exists(path):
        sys.exit("no database at %s" % path)
    uri = "file:%s?mode=%s" % (os.path.abspath(path), "rw" if write else "ro")
    con = sqlite3.connect(uri, uri=True, timeout=30, isolation_level=None)
    if not table_exists(con, "intel_items"):
        sys.exit("%s has no intel_items table" % path)
    return con


# ── self-test ───────────────────────────────────────────────────────────────

def fts_uids(c):
    return {r[0] for r in c.execute("SELECT uid FROM intel_items_fts")}


def self_test():
    """Builds a scratch DB from core/schema.sql holding old and new rows, and
    checks that exactly the true stale rows are reported and deleted."""
    here = os.path.dirname(os.path.abspath(__file__))
    schema = os.path.join(here, "..", "core", "schema.sql")
    d = tempfile.mkdtemp(prefix="jo-stale-")
    path = os.path.join(d, "jo.db")
    con = sqlite3.connect(path, isolation_level=None)
    con.executescript(open(schema, encoding="utf-8").read())
    T0, T1, T2 = "2026-10-01T00:00:00.000Z", "2026-10-06T00:00:00.000Z", "2026-10-09T00:00:00.000Z"

    def put(uid, sid, props, at, rtype=None):
        con.execute("INSERT INTO intel_items(uid,source_id,title,fetched_at,properties,record_type) "
                    "VALUES (?,?,?,?,?,?)", (uid, sid, uid, at, json.dumps(props), rtype))
        con.execute("INSERT INTO intel_items_fts(uid,title) VALUES (?,?)", (uid, uid))
        rid = con.execute("SELECT last_insert_rowid()").fetchone()[0]
        con.execute("INSERT INTO intel_items_fts_uid_map(uid,rowid) VALUES (?,?)", (uid, rid))

    want_stale, want_kept = set(), {}

    # Chicago contracts: plain C1 held revision 1; C1|1 and C1|2 now exist.
    put("us-chicago-contracts|C1", "us-chicago-contracts", {"revision_number": "1", "award_amount": "5"}, T0)
    put("us-chicago-contracts|C1|1", "us-chicago-contracts", {"revision_number": "1", "award_amount": "5"}, T1)
    put("us-chicago-contracts|C1|2", "us-chicago-contracts", {"revision_number": "2", "award_amount": "9"}, T1)
    want_stale.add("us-chicago-contracts|C1")
    # Plain C2 holds revision 5, which fell out of the window: the ONLY copy.
    put("us-chicago-contracts|C2", "us-chicago-contracts", {"revision_number": "5"}, T0)
    put("us-chicago-contracts|C2|6", "us-chicago-contracts", {"revision_number": "6"}, T1)
    put("us-chicago-contracts|C2|7", "us-chicago-contracts", {"revision_number": "7"}, T1)
    want_kept["us-chicago-contracts|C2"] = "kept-unproven"
    # Proven stale, but pinned into a case: kept.
    put("us-chicago-contracts|C3", "us-chicago-contracts", {"revision_number": "1"}, T0)
    put("us-chicago-contracts|C3|1", "us-chicago-contracts", {"revision_number": "1"}, T1)
    put("us-chicago-contracts|C3|2", "us-chicago-contracts", {"revision_number": "2"}, T1)
    con.execute("INSERT INTO tenants(id,slug,name) VALUES ('tA','ta','A')")
    con.execute("INSERT INTO cases(id,tenant_id,name) VALUES ('k1','tA','k')")
    con.execute("INSERT INTO case_items(case_id,ref_type,ref_id) VALUES ('k1','intel_item','us-chicago-contracts|C3')")
    want_kept["us-chicago-contracts|C3"] = "kept-referenced"

    # Colorado (no key2: hash-qualified): content proof.
    co = "us-co-business-entities"
    put(co + "|E1", co, {"entityid": "E1", "entityname": "Acme"}, T0)
    put(co + "|E1|aaaaaaaaaaaaaaaaaaaa", co, {"entityid": "E1", "entityname": "Acme", "entitystatus": "Good"}, T1)
    put(co + "|E1|bbbbbbbbbbbbbbbbbbbb", co, {"entityid": "E1", "entityname": "Acme Two"}, T1)
    want_stale.add(co + "|E1")
    # Different content from every sibling: unproven.
    put(co + "|E2", co, {"entityid": "E2", "entityname": "Old name"}, T0)
    put(co + "|E2|cccccccccccccccccccc", co, {"entityid": "E2", "entityname": "New"}, T1)
    want_kept[co + "|E2"] = "kept-unproven"
    # The key became unique again: the plain row is newer than its siblings.
    put(co + "|E3", co, {"entityid": "E3", "entityname": "Same"}, T2)
    put(co + "|E3|dddddddddddddddddddd", co, {"entityid": "E3", "entityname": "Same"}, T1)
    want_kept[co + "|E3"] = "kept-newer"
    # A plain key with no qualified siblings: a normal record, never listed.
    put(co + "|E4", co, {"entityid": "E4"}, T0)
    # 1 is not "1": a type change is not proof.
    put(co + "|E5", co, {"entityid": "E5", "n": 1}, T0)
    put(co + "|E5|eeeeeeeeeeeeeeeeeeee", co, {"entityid": "E5", "n": "1"}, T1)
    want_kept[co + "|E5"] = "kept-unproven"
    # An empty old property set proves nothing.
    put(co + "|E6", co, {}, T0)
    put(co + "|E6|ffffffffffffffffffff", co, {"entityid": "E6"}, T1)
    want_kept[co + "|E6"] = "kept-unproven"

    # Safecast: the reading the plain row held is (U1) / is not (U2) stored.
    sc = "safecast-realtime-devices"
    put(sc + "|note:dev:1", sc, {"device_urn": "note:dev:1", "when_captured": "2026-10-01T00:00:00Z", "cpm": 30}, T0)
    put(sc + "|note:dev:1|2026-10-01T00:00:00Z", sc, {"device_urn": "note:dev:1", "when_captured": "2026-10-01T00:00:00Z", "cpm": 31}, T1)
    put(sc + "|note:dev:1|2026-10-06T00:00:00Z", sc, {"device_urn": "note:dev:1", "when_captured": "2026-10-06T00:00:00Z", "cpm": 33}, T1)
    want_stale.add(sc + "|note:dev:1")
    put(sc + "|note:dev:2", sc, {"device_urn": "note:dev:2", "when_captured": "2026-09-30T00:00:00Z"}, T0)
    put(sc + "|note:dev:2|2026-10-06T00:00:00Z", sc, {"device_urn": "note:dev:2", "when_captured": "2026-10-06T00:00:00Z"}, T1)
    put(sc + "|note:dev:2|2026-10-06T00:15:00Z", sc, {"device_urn": "note:dev:2", "when_captured": "2026-10-06T00:15:00Z"}, T1)
    want_kept[sc + "|note:dev:2"] = "kept-unproven"

    # MeteoAlarm: the base key has ONE pipe. "ID" alone is not a base shape.
    ma = "meteoalarm-cap"
    put(ma + "|2.49.0.1|NO001", ma, {"identifier": "2.49.0.1", "event": "wind"}, T0)
    put(ma + "|2.49.0.1|NO001|https://feeds/x?index_polygon=0", ma, {"identifier": "2.49.0.1", "event": "wind", "atom_id": "https://feeds/x?index_polygon=0"}, T1)
    put(ma + "|2.49.0.1|NO001|https://feeds/x?index_polygon=1", ma, {"identifier": "2.49.0.1", "event": "wind", "atom_id": "https://feeds/x?index_polygon=1"}, T1)
    want_stale.add(ma + "|2.49.0.1|NO001")
    put(ma + "|2.49.0.1", ma, {"identifier": "2.49.0.1", "event": "wind"}, T0)   # wrong shape: never judged

    # FRA: agencyid exact; no agencyid with the raw columns -> "no-agencyid".
    fra = "fra-grade-crossing-inventory"
    put(fra + "|022128G", fra, {"crossingid": "022128G", "agencyid": "1141", "revisiondate": "2019"}, T0)
    put(fra + "|022128G|1141", fra, {"crossingid": "022128G", "agencyid": "1141", "revisiondate": "2024"}, T1)
    put(fra + "|022128G|no-agencyid", fra, {"crossingid": "022128G", "revisiondate": "2023"}, T1)
    want_stale.add(fra + "|022128G")
    put(fra + "|033333H", fra, {"crossingid": "033333H", "revisiondate": "2018"}, T0)
    put(fra + "|033333H|230", fra, {"crossingid": "033333H", "agencyid": "230", "revisiondate": "2024"}, T1)
    put(fra + "|033333H|no-agencyid", fra, {"crossingid": "033333H", "revisiondate": "2024"}, T1)
    want_stale.add(fra + "|033333H")

    # RDW: plain areaid (exact by manager) and the old reversed repeat.
    rdw = RDW
    put(rdw + "|599_HART", rdw, {"area_id": "599_HART", "area_manager_id": "2448"}, T0)
    put(rdw + "|2459|599_HART", rdw, {"area_id": "599_HART", "area_manager_id": "2459"}, T0)
    put(rdw + "|599_HART|2448", rdw, {"area_id": "599_HART", "area_manager_id": "2448"}, T1)
    put(rdw + "|599_HART|2459", rdw, {"area_id": "599_HART", "area_manager_id": "2459"}, T1)
    want_stale |= {rdw + "|599_HART", rdw + "|2459|599_HART"}
    # A current qualified row is never mistaken for the reversed shape.
    put(rdw + "|700_X|31", rdw, {"area_id": "700_X", "area_manager_id": "31"}, T1)
    put(rdw + "|700_X|32", rdw, {"area_id": "700_X", "area_manager_id": "32"}, T1)

    # wayback-jp: v0 plain digest re-read (stale), v0 plain digest not yet
    # re-read (only copy), v0 repeat, long URL (hashed form), current rows,
    # a truncation notice.
    wb = WAYBACK
    cap = lambda dig, ts, url, host="mod.go.jp": {"target": host, "timestamp": ts, "original": url,
                                                  "mimetype": "text/html", "statuscode": "200", "digest": dig}
    put(wb + "|DIGA", wb, cap("DIGA", "20070101000000", "http://www.mod.go.jp/"), T0, wb)
    put(wb + "|DIGA|20070101000000|http://www.mod.go.jp/", wb,
        dict(cap("DIGA", "20070101000000", "http://www.mod.go.jp/"), cdx_page=0), T1, wb)
    want_stale.add(wb + "|DIGA")
    put(wb + "|DIGB", wb, cap("DIGB", "20080101000000", "http://www.mod.go.jp/b"), T0, wb)
    want_kept[wb + "|DIGB"] = "kept-no-replacement"
    put(wb + "|DIGC|kantei.go.jp|20000101000000", wb,
        cap("DIGC", "20000101000000", "http://www.kantei.go.jp/", "kantei.go.jp"), T0, wb)
    put(wb + "|DIGC|20000101000000|http://www.kantei.go.jp/", wb,
        cap("DIGC", "20000101000000", "http://www.kantei.go.jp/", "kantei.go.jp"), T1, wb)
    want_stale.add(wb + "|DIGC|kantei.go.jp|20000101000000")
    long_url = "http://www.mhlw.go.jp/" + "x" * 400
    put(wb + "|DIGD", wb, cap("DIGD", "20100101000000", long_url, "mhlw.go.jp"), T0, wb)
    put(wb + "|DIGD|20100101000000|url-sha1:" + hashlib.sha1((long_url + "|").encode()).hexdigest()[:20],
        wb, cap("DIGD", "20100101000000", long_url, "mhlw.go.jp"), T1, wb)
    want_stale.add(wb + "|DIGD")
    put(wb + "|truncation:mod.go.jp", wb, {"query": "mod.go.jp", "next_page": 10}, T1, NOTICE)

    # A source this tool does not cover, same shape: never touched.
    put("some-other-source|K", "some-other-source", {"a": 1}, T0)
    put("some-other-source|K|1", "some-other-source", {"a": 1}, T1)

    # Dependent rows of a stale uid and of a kept one.
    for uid in ("us-chicago-contracts|C1", "us-chicago-contracts|C2"):
        con.execute("INSERT INTO entities(entity_id,type,canonical,norm_key) VALUES (?,?,?,?)",
                    ("e-" + uid, "org", uid, uid))
        con.execute("INSERT INTO entity_mentions(entity_id,item_uid,source_id,field,extractor) "
                    "VALUES (?,?,?,?,?)", ("e-" + uid, uid, "us-chicago-contracts", "title", "test"))
        con.execute("INSERT INTO entity_extraction_state(item_uid) VALUES (?)", (uid,))
    con.close()

    failures = []

    def check(cond, what):
        if not cond:
            failures.append(what)

    before = sqlite3.connect(path).execute("SELECT COUNT(*) FROM intel_items").fetchone()[0]
    con = open_db(path, write=False)
    rows = find(con)
    con.close()
    got = {r["uid"]: r["verdict"] for r in rows}
    stale = {u for u, v in got.items() if v == "stale"}
    check(stale == want_stale, "stale set: extra %s, missing %s" %
          (sorted(stale - want_stale), sorted(want_stale - stale)))
    for uid, v in want_kept.items():
        check(got.get(uid) == v, "%s: wanted %s, got %s" % (uid, v, got.get(uid)))
    for uid in ("us-co-business-entities|E4", "meteoalarm-cap|2.49.0.1", "some-other-source|K",
                "netherlands-rdw-parking-garages|700_X|31", "wayback-jp|truncation:mod.go.jp"):
        check(uid not in got, "%s must not be judged at all" % uid)
    after = sqlite3.connect(path).execute("SELECT COUNT(*) FROM intel_items").fetchone()[0]
    check(before == after, "report mode changed the database")

    con = open_db(path, write=True)
    scanned = find(con)
    # A collector re-fetches E1 between the scan and the delete: the plain row
    # is the current copy again, and the delete must notice and leave it.
    refetched = co + "|E1"
    con.execute("UPDATE intel_items SET fetched_at=? WHERE uid=?", (T2, refetched))
    buf = io.StringIO()
    n = delete(con, scanned, out=buf)
    con.close()
    check("left    %s: the row was rewritten" % refetched in buf.getvalue(),
          "the re-fetched row is not reported as left")
    want_gone = want_stale - {refetched}
    check(n == len(want_gone), "deleted %d, wanted %d" % (n, len(want_gone)))
    c = sqlite3.connect(path)
    left = {r[0] for r in c.execute("SELECT uid FROM intel_items")}
    check(not (left & want_gone), "still present: %s" % sorted(left & want_gone))
    check(refetched in left, "the row re-fetched after the scan was deleted")
    check(after - n == len(left), "rows other than the stale ones were deleted")
    for uid in want_kept:
        check(uid in left, "kept row %s was deleted" % uid)
    for r in rows:
        if r["verdict"] == "stale":
            check(r["replacement"] in left, "replacement %s of %s is gone" % (r["replacement"], r["uid"]))
            if r["uid"] in want_gone:
                check(r["uid"] not in fts_uids(c), "index row of %s survived" % r["uid"])
    fts = fts_uids(c)
    check(fts == left, "search index out of step: %s" % sorted(fts ^ left))
    check(not c.execute("SELECT 1 FROM intel_items_fts_uid_map WHERE uid NOT IN (SELECT uid FROM intel_items)").fetchone(),
          "uid map keeps a removed uid")
    em = {r[0] for r in c.execute("SELECT item_uid FROM entity_mentions")}
    check(em == {"us-chicago-contracts|C2"}, "entity mentions: %s" % sorted(em))
    es = {r[0] for r in c.execute("SELECT item_uid FROM entity_extraction_state")}
    check(es == {"us-chicago-contracts|C2"}, "extraction state: %s" % sorted(es))
    c.close()

    con = open_db(path, write=False)
    again = [r for r in find(con) if r["verdict"] == "stale"]
    con.close()
    check(not again, "a second pass still finds %d stale rows" % len(again))

    if failures:
        for f in failures:
            print("FAIL: " + f)
        print("cleanup_stale_uids self-test: %d failure(s); scratch DB kept at %s" % (len(failures), path))
        return 1
    shutil.rmtree(d, ignore_errors=True)
    print("cleanup_stale_uids self-test: ok (%d stale found, %d deleted, one re-fetched after the "
          "scan left, %d kept for cause, report mode read-only, second pass clean)"
          % (len(want_stale), len(want_gone), len(want_kept)))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("db", nargs="?", help="path to the SQLite database (e.g. $JO_DB)")
    ap.add_argument("--delete", action="store_true",
                    help="delete the stale rows (one transaction); default is to report only")
    ap.add_argument("--source", action="append", metavar="ID",
                    help="limit to this source id (repeatable)")
    ap.add_argument("--sample", type=int, default=5, help="rows shown per verdict per source (default 5)")
    ap.add_argument("--self-test", action="store_true", help="run the scratch-DB self-test and exit")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    if not a.db:
        ap.error("a database path is required (or --self-test)")
    known = {s["id"] for s in SOURCES} | {WAYBACK}
    for s in a.source or []:
        if s not in known:
            ap.error("%s is not a source this tool covers: %s" % (s, ", ".join(sorted(known))))
    con = open_db(a.db, write=a.delete)
    rows = find(con, set(a.source) if a.source else None)
    report(rows, a.sample)
    if a.delete:
        delete(con, rows)
    else:
        print("\nreport only; nothing was changed (pass --delete to remove the stale rows)")
    con.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
