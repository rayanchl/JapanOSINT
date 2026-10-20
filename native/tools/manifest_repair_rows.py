#!/usr/bin/env python3
"""Re-join manifest rows that a multi-line field split across lines.

    python3 tools/manifest_repair_rows.py MANIFEST ID_PREFIX

A feed title or description that spans lines (a <title> with a newline in it,
seen in batches 43–45 before build_feed_manifest.py normalised whitespace)
writes one logical row as two physical lines, and every reader then sees a
row with too few fields followed by a line that is not a row. A line that is
neither a comment nor starts with ID_PREFIX is glued onto the row above it,
whitespace-normalised. Rewrites in place and reports how many rows still do
not carry exactly 12 pipes — those are a different defect (a pipe inside a
field) and are named, not repaired."""
import sys, re


def main():
    p, prefix = sys.argv[1], sys.argv[2]
    out = []
    for line in open(p, encoding="utf-8").read().split("\n"):
        if line.startswith("#") or line.startswith(prefix) or not out:
            out.append(line)
        else:
            out[-1] = re.sub(r"\s+", " ", out[-1] + " " + line)
    L = [l for l in out if l]
    bad = [l for l in L if not l.startswith("#") and l.count("|") != 12]
    open(p, "w", encoding="utf-8").write("\n".join(L) + "\n")
    print(len([l for l in L if not l.startswith("#")]), "rows;", len(bad), "malformed")
    for l in bad:
        print("  ", l.split("|", 1)[0], l.count("|"), "pipes", file=sys.stderr)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
