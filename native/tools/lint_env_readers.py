#!/usr/bin/env python3
"""lint_env_readers.py — there is ONE environment reader (lib/joenv.h).

Fails (exit 1) when native/core, native/lib or native/collectors grows

  * a private helper that parses the environment into a number or bool:
    `static ... env_int(`, `env_long(`, `env_ll(`, `env_double(`, `env_bool(`,
    `env_truthy(`, `env_pos(`, `env_nonempty(` (and `<prefix>_env_int` forms), or
  * a numeric parse applied straight to getenv(): `atoi(getenv("X"))`,
    `strtol(getenv("X"), ...)`, `atof(getenv(...))`, or to the pointer it was
    just assigned to: `const char *e = getenv("X"); ... atoi(e)` (within 5 lines).

Use jo_env_long / jo_env_int / jo_env_double / jo_env_bool / jo_env_str
(lib/joenv.h): unset/empty -> default, garbage -> default + one stderr line,
out of range -> clamped (or default for the _strict forms). Deliberate
exception: put `/* env-reader-ok: <reason> */` on the flagged line.

  tools/lint_env_readers.py            # exit 1 on any finding
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
ROOTS = ["core", "lib", "collectors"]
EXTS = (".c", ".h", ".inc")
SKIP = {os.path.join("lib", "joenv.c"), os.path.join("lib", "joenv.h")}

DEF = re.compile(
    r'^\s*(?:static\s+)(?:inline\s+)?[\w\s\*]*?\b(\w*env_(?:int|long|ll|double|bool|pos|truthy|nonempty|str))\s*\(',
    re.M)
PARSE = r'\b(?:atoi|atol|atoll|atof|strtol|strtoll|strtoul|strtod)\s*\('
ASSIGN = re.compile(r'(?:const\s+)?char\s*\*\s*(\w+)\s*=\s*(?:getenv|jo_env)\s*\(')
DIRECT = re.compile(r'\b(?:atoi|atol|atoll|atof|strtol|strtoll|strtoul|strtod)\s*\(\s*getenv\s*\(')


def _assigned(roots):
    out = []
    for r in roots:
        for d, _, fs in os.walk(os.path.join(NATIVE, r)):
            for f in sorted(fs):
                if not f.endswith(EXTS):
                    continue
                p = os.path.join(d, f)
                rel = os.path.relpath(p, NATIVE)
                if rel in SKIP:
                    continue
                L = open(p, encoding="utf-8", errors="replace").read().split("\n")
                for i, line in enumerate(L):
                    m = ASSIGN.search(line)
                    if not m or "env-reader-ok" in line:
                        continue
                    win = "\n".join(L[i + 1:i + 6])
                    if re.search(PARSE + r'\s*' + m.group(1) + r'\b', win):
                        out.append("%s:%d: numeric parse of a getenv() pointer — use lib/joenv.h" % (rel, i + 1))
    return out


def main():
    bad = []
    for r in ROOTS:
        for d, _, fs in os.walk(os.path.join(NATIVE, r)):
            for f in sorted(fs):
                if not f.endswith(EXTS):
                    continue
                p = os.path.join(d, f)
                rel = os.path.relpath(p, NATIVE)
                if rel in SKIP:
                    continue
                text = open(p, encoding="utf-8", errors="replace").read()
                lines = text.split("\n")
                for rx, what in ((DEF, "private env reader"), (DIRECT, "numeric parse of getenv()")):
                    for m in rx.finditer(text):
                        ln = text.count("\n", 0, m.start()) + 1
                        if m.group(0).startswith("\n"):
                            ln += 1
                        if "env-reader-ok" in lines[ln - 1]:
                            continue
                        bad.append("%s:%d: %s — use lib/joenv.h" % (rel, ln, what))
    bad.extend(_assigned(ROOTS))
    for b in bad:
        sys.stderr.write(b + "\n")
    if bad:
        sys.stderr.write("lint_env_readers: %d finding(s)\n" % len(bad))
        return 1
    print("lint_env_readers: 0 findings (one environment reader: lib/joenv.h)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
