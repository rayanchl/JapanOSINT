#!/usr/bin/env python3
"""jo_registry.py -- the one place the audit tools reach the registry/binary.

Four tools (audit_registry_emit, audit_batch_emit, audit_cycle_health,
audit_page_ceiling) each carried their own copy of "boot the binary once
against an empty file so every run starts from the same migrated, seeded
database" and of the WAL checkpoint that must follow it; audit_registry_emit
also owned the only parser of `--list-sources`. The copies had already drifted
(one raised on a bad checkpoint, two swallowed it; two checked the exit code,
two checked the file). A scratch-database template built differently by two
tools is two different measurements, so there is one implementation here.

Manifest reading is NOT here: `manifest.py` is THE manifest parser (rule 4c).

    unlink_db(path)                    remove a sqlite file and its -wal/-shm
    checkpoint(path, strict=True)      fold the WAL in (strict=False: ignore errors)
    warm_template(binpath, path)       build the seeded template DB; returns path
    list_sources(binpath, db)          -> [(id, collector, interval)] from the binary
    parse_list_sources(text)           the same parse on text, for offline tests

Self-test (offline, uses a fake binary):  python3 jo_registry.py --self-test
"""
import os
import re
import sqlite3
import subprocess
import sys
import tempfile

# `--list-sources` line, as audit_registry_emit always parsed it.
# `kind=` is the source's DECLARED kind (feed|pivot|pod|probe, source.h
# source_def_kind). Older binaries print no kind, so it is optional.
LIST = re.compile(r"^(\S+)\s+collector=(\S+)\s+interval=(-?\d+)(?:\s+kind=(\w+))?\s*$")
KIND = {}                      # id -> declared kind, filled by parse_list_sources()


def unlink_db(path):
    for suf in ("", "-wal", "-shm"):
        try:
            os.unlink(path + suf)
        except OSError:
            pass


def checkpoint(path, strict=True):
    """Fold the WAL back into the main file. Without this a template is copied
    without whatever its last writer left in the -wal, and every run starts
    from a database missing the migrations that were just applied."""
    try:
        con = sqlite3.connect(path)
        con.execute("PRAGMA wal_checkpoint(TRUNCATE)")
        con.close()
    except sqlite3.Error:
        if strict:
            raise


def warm_template(binpath, path, timeout=900):
    """A database with the schema applied, every migration run and the sources
    table seeded -- the state a real deployment is in before a collector runs.
    Built once by booting the binary against an empty file, so every run starts
    from an IDENTICAL database and a difference between two sources is a
    difference between the sources."""
    unlink_db(path)
    env = dict(os.environ, JO_DB=path, JO_FTS_REBUILD="0")
    p = subprocess.run([binpath, "--list-sources"], capture_output=True,
                       text=True, env=env, timeout=timeout,
                       stdin=subprocess.DEVNULL)
    if p.returncode != 0 or not os.path.exists(path):
        raise SystemExit("could not build a warm template DB at %s (rc=%s)\n%s"
                         % (path, p.returncode, (p.stderr or "")[-800:]))
    checkpoint(path)
    return path


def parse_list_sources(text):
    out = []
    for line in (text or "").splitlines():
        m = LIST.match(line.rstrip())
        if m:
            out.append((m.group(1), m.group(2), int(m.group(3))))
            if m.group(4):
                KIND[m.group(1)] = m.group(4)
    return out


def list_sources(binpath, db):
    """Ask the BINARY what is registered. -> [(id, collector, interval)]"""
    env = dict(os.environ, JO_DB=db, JO_FTS_REBUILD="0")
    p = subprocess.run([binpath, "--list-sources"], capture_output=True,
                       text=True, env=env, timeout=600)
    out = parse_list_sources(p.stdout)
    if not out:
        raise SystemExit("--list-sources produced no parseable rows; refusing "
                         "to report on a source list this tool did not get.\n"
                         + (p.stderr or "")[-800:])
    return out


def self_test():
    d = tempfile.mkdtemp(prefix="jo_registry_selftest_")
    fake = os.path.join(d, "fakebin")
    with open(fake, "w") as f:
        f.write("#!/bin/sh\n"
                "python3 -c \"import os,sqlite3; c=sqlite3.connect(os.environ['JO_DB']);"
                " c.execute('create table t(x)'); c.commit()\"\n"
                "echo 'SRC_A collector=HP_JSON interval=3600'\n"
                "echo 'SRC_B collector=_probe interval=0'\n"
                "echo 'noise line that is not a row ('\n")
    os.chmod(fake, 0o755)
    db = os.path.join(d, "t.db")
    assert warm_template(fake, db) == db and os.path.exists(db)
    got = list_sources(fake, db)
    assert got == [("SRC_A", "HP_JSON", 3600), ("SRC_B", "_probe", 0)], got
    bad = os.path.join(d, "badbin")
    with open(bad, "w") as f:
        f.write("#!/bin/sh\nexit 3\n")
    os.chmod(bad, 0o755)
    try:
        warm_template(bad, os.path.join(d, "x.db"))
        raise AssertionError("warm_template accepted a failing binary")
    except SystemExit:
        pass
    checkpoint(os.path.join(d, "nonexistent-dir", "x.db"), strict=False)
    unlink_db(db)
    assert not os.path.exists(db)
    print("jo_registry self-test: ok")
    return 0


if __name__ == "__main__":
    if "--self-test" in sys.argv:
        sys.exit(self_test())
    print(__doc__)
