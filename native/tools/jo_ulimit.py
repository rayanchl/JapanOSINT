#!/usr/bin/env python3
"""THE `ulimit -f` unit for this directory, measured rather than assumed.

Both emit auditors cap a run's file writes by running it under
`/bin/sh -c 'ulimit -f N; exec "$0" ...'`, so that one row cannot fill the disk
with a scratch SQLite WAL (the ceiling was added the day one did, 14 GB in two
minutes). `N` is a count of BLOCKS, and the block size is not a constant:

    measured 2026-11-12 on this image
      /bin/sh -> dash:  ulimit -f 1 allows   512 bytes   (POSIX's 512-byte block)
      /bin/bash:        ulimit -f 3 allows  3072 bytes   (1024-byte block)

`audit_registry_emit.py` carried `file_limit * 1024` under a comment reading
"`ulimit -f` counts 1K blocks". That is true of bash and false of the shell the
tool actually invokes, so on this image `--file-limit 1024` imposed **512 MB,
half the number the flag states** — and that halved ceiling is the measured
cause of 54 of the 73 SLOW verdicts in the 2026-11-12 re-read, rows killed with
76,000 to 262,000 records already stored.

So the unit is a property of the HOST's /bin/sh wearing a flag's clothing, and
the same `--file-limit 1024` means 512 MB on a Debian-style image and 1 GB
where /bin/sh is bash. It is measured once, with the same shell the auditors
use, and the probe needs no external command: the shell writes the bytes
itself, is killed by SIGXFSZ part-way, and the file it leaves behind IS the
ceiling. Python reads its size whatever the shell's exit status was.

One copy, imported by both tools, for rule 4c's reason — the same reason
`tools/jo_tls.py` exists. Two readings of one thing diverge.
"""
import os
import subprocess
import tempfile

# The conservative fallback. If the probe cannot run we assume the SMALLER
# block, which makes the computed ceiling larger than asked for rather than
# smaller: too small manufactures false SIZE_CAPPED verdicts on healthy rows,
# which is the defect this module exists to remove, while too large still
# bounds the runaway the ceiling was added for and still honours a number the
# operator chose.
FALLBACK_BLOCK_BYTES = 512

_CACHE = []


def ulimit_block_bytes(shell="/bin/sh"):
    """Bytes allowed by `ulimit -f 1` in `shell`. Measured once, then cached."""
    if _CACHE:
        return _CACHE[0]
    blk = FALLBACK_BLOCK_BYTES
    try:
        fd, path = tempfile.mkstemp(prefix="jo_ulimit.")
        os.close(fd)
        try:
            # 64 KiB of output against a one-block ceiling: the redirection is
            # the shell's, so the shell itself takes the SIGXFSZ and dies with
            # the file holding exactly one block. No dd, no head, no coreutils.
            subprocess.run(
                [shell, "-c",
                 'ulimit -f 1; i=0; while [ $i -lt 4096 ]; do '
                 'printf 0123456789abcdef; i=$((i+1)); done > "$1"',
                 shell, path],
                capture_output=True, timeout=30, stdin=subprocess.DEVNULL)
            n = os.path.getsize(path)
            # Only believe a plausible reading. A shell that wrote nothing, or
            # was not stopped at all, tells us nothing about the unit.
            if n in (512, 1024, 2048, 4096):
                blk = n
        finally:
            try:
                os.unlink(path)
            except OSError:
                pass
    except Exception:
        pass
    _CACHE.append(blk)
    return blk


def file_limit_blocks(megabytes, shell="/bin/sh"):
    """`ulimit -f` argument that really means `megabytes` MB. 0 -> 0 (no cap)."""
    if not megabytes:
        return 0
    return max(1, (int(megabytes) * 1024 * 1024) // ulimit_block_bytes(shell))


if __name__ == "__main__":
    import sys
    b = ulimit_block_bytes()
    print("/bin/sh ulimit -f block = %d bytes (measured)" % b)
    for mb in (1, 64, 1024):
        n = file_limit_blocks(mb)
        print("  --file-limit %-5d -> ulimit -f %-9d = %d MB"
              % (mb, n, n * b // (1024 * 1024)))
    sys.exit(0)
