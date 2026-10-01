#!/usr/bin/env python3
"""Build the audit inventory.

Static parsing of source_def initialisers is useless here: most of the 2201
sources are registered through X-macros (RSSX(...), country tables, …), so
`.id = "..."` literals cover barely a quarter of them. Instead we let the
binary do it — db_open() seeds the whole registry into the `sources` table —
and read that back, joining `reads_entity` from a grep of the defining file
where we can attribute one.

Writes inventory.jsonl next to this script.
"""
import json, os, re, subprocess, sys, sqlite3, tempfile, shutil

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(os.path.dirname(HERE))
SRCDIR = os.path.join(NATIVE, "collectors", "sources")


LIST_RE = re.compile(r"^(\S+)\s+collector=(\S+)\s+interval=(-?\d+)\s*$")


def seed_db():
    """--list-sources opens the DB (which seeds the registry into `sources`)
    and prints collector+interval, which the table does not carry. One run
    gives us both halves of the inventory."""
    tmp = tempfile.mkdtemp(prefix="joinv_")
    db = os.path.join(tmp, "inv.db")
    env = dict(os.environ, JO_DB=db, JO_SCHEMA=os.path.join(NATIVE, "core", "schema.sql"))
    p = subprocess.run([os.path.join(NATIVE, "bin", "japanosint"), "--list-sources"],
                       capture_output=True, env=env, cwd=NATIVE, timeout=300)
    listing = {}
    for line in p.stdout.decode("utf-8", "replace").splitlines():
        m = LIST_RE.match(line)
        if m:
            listing[m.group(1)] = (m.group(2), int(m.group(3)))
    return tmp, db, listing


def entity_files():
    """Files whose collector code reads ctx->entity — i.e. the OSINT pivot."""
    out = set()
    for fn in os.listdir(SRCDIR):
        if fn.endswith((".c", ".inc")):
            try:
                if "ctx->entity" in open(os.path.join(SRCDIR, fn), encoding="utf-8",
                                        errors="replace").read():
                    out.add(fn)
            except OSError:
                pass
    return out


def id_to_file():
    """Best-effort id -> defining file. Catches both literal `.id = "X"` and
    X-macro invocations that pass the id as a bare "X" string argument."""
    m = {}
    lit = re.compile(r'\.id\s*=\s*"([^"]+)"')
    arg = re.compile(r'^\s*[A-Z_][A-Z0-9_]*\(\s*[A-Za-z_][A-Za-z0-9_]*\s*,\s*"([^"]+)"', re.M)
    for fn in sorted(os.listdir(SRCDIR)):
        if not fn.endswith((".c", ".inc")):
            continue
        try:
            t = open(os.path.join(SRCDIR, fn), encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        for rx in (lit, arg):
            for mm in rx.finditer(t):
                m.setdefault(mm.group(1), fn)
    return m


def main():
    tmp, db, listing = seed_db()
    con = sqlite3.connect(db)
    cols = [r[1] for r in con.execute("PRAGMA table_info(sources)")]
    rows = con.execute("SELECT %s FROM sources" % ",".join(cols)).fetchall()
    con.close()
    shutil.rmtree(tmp, ignore_errors=True)

    ef, i2f = entity_files(), id_to_file()
    want = ["id", "name", "category", "type", "url", "status"]
    n_unattributed = 0
    with open(os.path.join(HERE, "inventory.jsonl"), "w", encoding="utf-8") as fh:
        for r in rows:
            d = dict(zip(cols, r))
            sid = d.get("id")
            f = i2f.get(sid)
            if not f:
                n_unattributed += 1
            rec = {k: d.get(k) for k in want if k in d}
            coll, iv = listing.get(sid, (None, None))
            rec["collector"], rec["interval"] = coll, iv
            rec["file"] = f
            rec["reads_entity"] = bool(f and f in ef)
            fh.write(json.dumps(rec, ensure_ascii=False) + "\n")
    print("inventory rows: %d  (file unattributed: %d)" % (len(rows), n_unattributed),
          file=sys.stderr)


if __name__ == "__main__":
    main()
