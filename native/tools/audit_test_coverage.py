#!/usr/bin/env python3
"""OPS-07: which core/ and lib/ modules does no test touch?

A module counts as TESTED when some test file (tests/unit/*.c, tests/*.c)
  - #includes its .c or its header (basename match), or
  - calls one of the non-static functions its .c defines.
Offline, no build.  A module neither tested nor listed in
tools/test-coverage-allow.txt ("<module> <reason>" per line, '#' comments)
is a failure, so the gap cannot silently reopen.

  python3 tools/audit_test_coverage.py            # list; exit 1 on a gap
  python3 tools/audit_test_coverage.py --all      # also list tested modules
  python3 tools/audit_test_coverage.py --no-allow # ignore the allow-list
  python3 tools/audit_test_coverage.py --self-test
"""
import os, re, sys, glob, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)  # native/


def read(p):
    with open(p, encoding="utf-8", errors="replace") as f:
        return f.read()


def modules(root):
    out = []
    for d in ("core", "lib"):
        out += sorted(glob.glob(os.path.join(root, d, "*.c")))
    return out


def exported(src):
    """Non-static function definitions: column-0 declarator whose '{' comes
    before the next ';' within a few lines."""
    names = set()
    lines = src.split("\n")
    for i, ln in enumerate(lines):
        if not ln or ln[0] in " \t#/*}{" or ln.startswith(("static", "typedef")):
            continue
        m = re.match(r'^[A-Za-z_][\w \t\*]*?[\s\*]([A-Za-z_]\w*)\s*\(', ln)
        if not m:
            continue
        blob = "\n".join(lines[i:i + 12])
        k, s = blob.find("{"), blob.find(";")
        if k >= 0 and (s < 0 or k < s):
            names.add(m.group(1))
    names -= {"main", "if", "while", "for", "switch", "return", "sizeof"}
    return names


def load_allow(path):
    allow = {}
    if os.path.exists(path):
        for ln in read(path).splitlines():
            ln = ln.strip()
            if not ln or ln.startswith("#"):
                continue
            parts = ln.split(None, 1)
            allow[parts[0]] = parts[1] if len(parts) > 1 else ""
    return allow


def analyse(root, allow_path):
    tests = sorted(glob.glob(os.path.join(root, "tests", "unit", "*.c")) +
                   glob.glob(os.path.join(root, "tests", "*.c")))
    blob = "\n".join(read(t) for t in tests)
    stripped = re.sub(r'/\*.*?\*/|//[^\n]*', '', blob, flags=re.S)
    incs = set(re.findall(r'#\s*include\s+"(?:[^"]*/)?(\w+\.[ch])"', stripped))
    res = []
    for m in modules(root):
        rel = os.path.relpath(m, root)
        base = os.path.basename(m)[:-2]
        why = None
        if base + ".c" in incs or base + ".h" in incs:
            why = "includes " + base
        else:
            for fn in sorted(exported(read(m))):
                if re.search(r'\b' + re.escape(fn) + r'\s*\(', stripped):
                    why = "calls " + fn
                    break
        res.append((rel, why))
    return res, load_allow(allow_path)


def main(argv):
    if "--self-test" in argv:
        return self_test()
    res, allow = analyse(ROOT, os.path.join(HERE, "test-coverage-allow.txt"))
    if "--no-allow" in argv:
        allow = {}
    untested = [r for r, w in res if not w]
    gap = [r for r in untested if r not in allow]
    stale = [a for a in allow if a not in untested]
    if "--all" in argv:
        for r, w in res:
            print("%-28s %s" % (r, w or "UNTESTED"))
    print("modules: %d  tested: %d  untested: %d  allow-listed: %d"
          % (len(res), len(res) - len(untested), len(untested), len(untested) - len(gap)))
    for r in untested:
        print("UNTESTED %s%s" % (r, "  [allow: %s]" % allow[r] if r in allow else ""))
    for a in stale:
        print("STALE-ALLOW %s (module is tested or gone; remove the line)" % a)
    return 1 if gap or stale else 0


def self_test():
    with tempfile.TemporaryDirectory() as d:
        for sub in ("core", "lib", "tests/unit"):
            os.makedirs(os.path.join(d, sub))

        def w(p, s):
            with open(os.path.join(d, p), "w") as f:
                f.write(s)
        w("core/a.c", "int a_fn(int x)\n{\n return x;\n}\n")
        w("core/b.c", "static int hid(void) { return 1; }\nint b_fn(void)\n{\n return 2;\n}\n")
        w("lib/c.c", "int c_fn(void)\n{\n return 3;\n}\n")
        w("lib/d.c", "int d_fn(void)\n{\n return 3;\n}\n")
        w("tests/unit/test_x.c", '#include "../../core/a.c"\n// c_fn(\nint main(void){ return b_fn(); }\n')
        got = dict(analyse(d, os.path.join(d, "none"))[0])
        assert got["core/a.c"], got
        assert got["core/b.c"] == "calls b_fn", got
        assert got["lib/c.c"] is None, "comment must not count"
        assert got["lib/d.c"] is None
    print("audit_test_coverage self-test ok")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
