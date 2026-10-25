#!/usr/bin/env python3
"""Copy the live SQLite database out of the WSL disk onto D: with the online
backup API, guarded by free space.

WHY THIS EXISTS. On 2026-09-19 the `Ubuntu` WSL distro's LocalState folder was
wiped (the signature of a Store-app Reset) and the live database inside it --
6.78M intel_items rows -- went with it. Nothing outside the distro had a copy.
A database that lives only inside a VHDX that Windows can delete as "app data"
needs a copy that Windows treats as a file.

WHAT IT DOES. `sqlite3.Connection.backup()` -- consistent even while the server
is writing, no WAL tricks -- into <dest>/japanmap.backup.tmp, then an atomic
rename over <dest>/japanmap.backup.db. One rolling copy, because the database
is tens of GB.

THE GUARD. It refuses to run if the DESTINATION drive would be left with less
than JO_BACKUP_MIN_FREE_GB (default 15) after the copy, and says so in the log,
because the last time C: filled up WSL started throwing I/O errors. A skipped
backup is logged loudly; a backup that fills the disk takes WSL down with it.

WHY THE DESTINATION IS D: (changed 2026-10-24). It was
`/mnt/c/.../OSINTsaas/data/backup`, and the guard above meant it could never
run: the copy needs db + wal, which was 62 GB + 98 GB, against a 466 GB C:
that holds the 255 GB vhdx the database lives inside. So the guard was
protecting WSL exactly as designed and skipping the backup every week, and
the newest copy on disk was 1.9 GB dated 2026-09-20 while the live database
had grown past 6.78M rows. The 2026-10-21 scheduled run failed outright
(task result -2147020576) with C: at zero. A backup whose destination is the
drive the source is already filling is not a backup. D: has 3.8 TB. Note what
changed with the 2026-10-24 migration though: the distro itself now lives on
D: too (/mnt/d/wsl/Ubuntu-24.04 as the distro sees it), so this copy is on a
different FILESYSTEM from the ext4 it comes out of -- which is the property
that protects against a distro reset, the failure that actually happened here
-- but it is NO LONGER on a different physical drive. A second copy off this
machine is still the only answer to losing the disk.

Do not write a Windows path with backslashes in this docstring. A backslash
followed by a capital U opens a unicode escape, so spelling the distro's new
location in Windows form makes the WHOLE FILE a SyntaxError ("truncated
unicode escape") -- a docstring is not raw. That is how the first version of
this note broke the script, and the second version broke it again while
describing the problem. Write the /mnt/d form and there is nothing to escape.

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
DEST_DIR = os.environ.get("JO_BACKUP_DIR", "/mnt/d/jo-backup")
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
        # Name the DESTINATION, not "C:". The old wording said C: while the
        # path was configurable, so a skip on any other drive reported the
        # wrong disk -- and this message is the only record that a weekly
        # backup did not happen.
        log("SKIP: %s has %.1f GB free, copy needs %.1f GB plus %.0f GB headroom"
            % (DEST_DIR, free / 2**30, size / 2**30, MIN_FREE_GB))
        return 3
    # TWO HOPS, NOT ONE. The backup API writes page by page with a journal and
    # an fsync per step; pointed straight at a DrvFs mount (/mnt/c when this
    # was measured, /mnt/d now -- same 9p path, same cost) that measured
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
    log("staged in %.0fs, copied to %s in %.0fs"
        % (t1 - t0, DEST_DIR, time.time() - t1))
    log("OK: %s  %.2f GB  intel_items=%d  %.0fs"
        % (final, os.path.getsize(final) / 2**30, n, time.time() - t0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
