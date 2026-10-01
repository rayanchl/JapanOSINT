#!/usr/bin/env python3
"""Copy the live SQLite database out of the WSL disk onto C: with the online
backup API, guarded by free space.

WHY THIS EXISTS. On 2026-09-19 the `Ubuntu` WSL distro's LocalState folder was
wiped (the signature of a Store-app Reset) and the live database inside it --
6.78M intel_items rows -- went with it. Nothing outside the distro had a copy.
A database that lives only inside a VHDX that Windows can delete as "app data"
needs a copy that Windows treats as a file.

WHAT IT DOES. `sqlite3.Connection.backup()` -- consistent even while the server
is writing, no WAL tricks -- into <dest>/japanmap.backup.tmp, then an atomic
rename over <dest>/japanmap.backup.db. One rolling copy, because the database
is tens of GB and C: is not.

THE GUARD. It refuses to run if C: would be left with less than
JO_BACKUP_MIN_FREE_GB (default 15) after the copy, and says so in the log,
because the last time C: filled up WSL started throwing I/O errors. A skipped
backup is logged loudly; a backup that fills the disk takes WSL down with it.

Run by hand:   python3 native/tools/db-backup.py
Scheduled:     Windows Task Scheduler task "JapanOSINT DB backup" (weekly) runs
               wsl.exe -d Ubuntu-24.04 -- python3 <this file>
"""
import os
import shutil
import sqlite3
import sys
import time

SRC = os.environ.get("JO_DB", "/home/rayan/jodata/japanmap.db")
DEST_DIR = os.environ.get("JO_BACKUP_DIR",
                          "/mnt/c/Users/rayan/sources/repos/OSINTsaas/data/backup")
LOG = os.path.expanduser("~/jodata/backup.log")
MIN_FREE_GB = float(os.environ.get("JO_BACKUP_MIN_FREE_GB", "15"))


def log(msg):
    line = "%s %s" % (time.strftime("%Y-%m-%dT%H:%M:%S"), msg)
    print(line)
    try:
        os.makedirs(os.path.dirname(LOG), exist_ok=True)
        with open(LOG, "a") as f:
            f.write(line + "\n")
    except OSError:
        pass


def main():
    if not os.path.exists(SRC):
        log("SKIP: source db missing: %s" % SRC)
        return 2
    os.makedirs(DEST_DIR, exist_ok=True)
    size = os.path.getsize(SRC)
    wal = SRC + "-wal"
    if os.path.exists(wal):
        size += os.path.getsize(wal)
    free = shutil.disk_usage(DEST_DIR).free
    final = os.path.join(DEST_DIR, "japanmap.backup.db")
    # the old copy is replaced by rename, so its space comes back after; the
    # peak is old + new, which is what must fit.
    need = size + MIN_FREE_GB * (1 << 30)
    if free < need:
        log("SKIP: C: has %.1f GB free, copy needs %.1f GB plus %.0f GB headroom"
            % (free / 2**30, size / 2**30, MIN_FREE_GB))
        return 3
    # TWO HOPS, NOT ONE. The backup API writes page by page with a journal and
    # an fsync per step; pointed straight at /mnt/c (9p/DrvFs) that measured
    # 16 MB in six minutes. So: backup onto the distro's own ext4 next to the
    # source (fast), verify there, then stream the finished file across with
    # one sequential copy, which the 9p mount handles at disk speed.
    local_tmp = SRC + ".backup-staging"
    tmp = os.path.join(DEST_DIR, "japanmap.backup.tmp")
    t0 = time.time()
    src = sqlite3.connect("file:%s?mode=ro" % SRC, uri=True, timeout=120)
    try:
        for p in (local_tmp, tmp):
            if os.path.exists(p):
                os.unlink(p)
        dst = sqlite3.connect(local_tmp)
        dst.execute("PRAGMA journal_mode=OFF")
        dst.execute("PRAGMA synchronous=OFF")
        # pages=-1: one step under one read transaction. A stepped backup is
        # restarted from page 1 every time another connection writes the
        # source, and the scheduler writes it every few seconds -- the first
        # run of this script never finished for that reason. WAL mode means the
        # read lock does not block the server's writers.
        src.backup(dst, pages=-1)
        dst.close()
    finally:
        src.close()
    chk = sqlite3.connect(local_tmp)
    ok = chk.execute("PRAGMA quick_check(1)").fetchone()[0]
    n = chk.execute("select count(*) from intel_items").fetchone()[0]
    chk.close()
    if ok != "ok":
        log("FAIL: quick_check on the copy: %s" % ok)
        os.unlink(local_tmp)
        return 4
    t1 = time.time()
    shutil.copyfile(local_tmp, tmp)
    os.unlink(local_tmp)
    os.replace(tmp, final)
    log("staged in %.0fs, copied to C: in %.0fs" % (t1 - t0, time.time() - t1))
    log("OK: %s  %.2f GB  intel_items=%d  %.0fs"
        % (final, os.path.getsize(final) / 2**30, n, time.time() - t0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
