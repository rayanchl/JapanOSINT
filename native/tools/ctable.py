#!/usr/bin/env python3
"""THE reader for hpengine tables (`hp_source NAME[] = { {…}, … }`) in the C.

Why one module. Seven tools in this directory read rows out of the C — the
maintained copy, see CLAUDE.md's batch-tooling section — and each brought its
own regex: `\\{ \\.id = "` to find a row, `t.find("{ .id =")` to find the next,
`\\.url\\s*=\\s*"([^"]+)"` for a field. Those parsers disagreed with each other
and with C in ways that read as findings:

  * a row was "everything up to the next `{ .id =`", so a commented-out row, a
    nested initialiser or the LAST row of a table (whose span ran to the end of
    the file, past the closing `};` and into the next table) lent its fields to
    a neighbour;
  * a field was its FIRST string literal, so a `.url` split over several
    adjacent literals — one string in C — was read as a fragment
    (audit_page_two probed 26 rows at a truncated URL for exactly that);
  * a field spelled through a `#define` string macro was invisible;
  * a `.page_resume = 1` inside a COMMENT counted as declared.

This is rule 4c's lesson again — two tools reading one source must read it the
same way — so there is one reader: comments stripped (string-aware), simple
string macros substituted, tables found by their declaration, rows split by
brace counting (as tools/lint_sources.py's _hp_rows does), and each field's
value read as C reads it: adjacent literals joined and unescaped, integers as
ints, a brace list (`.headers = { "A: b", … }`) as a list.

It is NOT a C preprocessor. A field whose value is an expression this cannot
read is kept as its raw text under the same name, so a caller can tell "absent"
from "present but not a literal" — which is the distinction audit_page_param.py
reports as `unreadable`.

    python3 tools/ctable.py --self-test
    python3 tools/ctable.py --count          # rows / tables / files in the tree
"""
import bisect
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NATIVE = os.path.dirname(HERE)
COLLECTORS = os.path.join(NATIVE, "collectors")

_TABLE_RE = re.compile(r"\bhp_source\s+([A-Za-z_]\w*)\s*\[\s*\w*\s*\]\s*=\s*\{")
_DEFINE_RE = re.compile(r'^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)(\(?)[ \t]*'
                        r'((?:[^\n\\]|\\\n|\\[^\n])*)', re.M)
_STR_TOKEN = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
_IDENT = re.compile(r"[A-Za-z_]\w*")
_ESC = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", '"': '"', "'": "'",
        "0": "\0", "a": "\a", "b": "\b", "f": "\f", "v": "\v", "?": "?"}


def unescape(s):
    """A C string literal's body -> its characters (the escapes URLs and key
    lists actually carry; an unknown escape keeps its character)."""
    if "\\" not in s:
        return s
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            nx = s[i + 1]
            if nx == "x":
                m = re.match(r"[0-9A-Fa-f]{1,2}", s[i + 2:])
                if m:
                    out.append(chr(int(m.group(0), 16)))
                    i += 2 + len(m.group(0))
                    continue
            out.append(_ESC.get(nx, nx))
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


_COMMENT_SCAN = re.compile(r'"(?:[^"\\\n]|\\.)*"?|\'(?:[^\'\\\n]|\\.)*\'?|/\*.*?(?:\*/|\Z)|//[^\n]*',
                           re.S)


def strip_comments(text):
    """Remove /* */ and // comments, leaving string and char literals alone
    and every NEWLINE in place, so offsets stay on the same line."""
    def blank(m):
        t = m.group(0)
        if t[0] in "\"'":
            return t
        return re.sub(r"[^\n]", " ", t)
    return _COMMENT_SCAN.sub(blank, text)


def _concat(tokens_text, macros):
    """Adjacent string literals and string-macro names -> one string, or None
    when anything else is in the run."""
    pos, parts, t = 0, [], tokens_text
    while True:
        while pos < len(t) and t[pos].isspace():
            pos += 1
        if pos >= len(t):
            break
        m = _STR_TOKEN.match(t, pos)
        if m:
            parts.append(unescape(m.group(1)))
            pos = m.end()
            continue
        m = _IDENT.match(t, pos)
        if m and m.group(0) in macros:
            parts.append(macros[m.group(0)])
            pos = m.end()
            continue
        return None
    return "".join(parts) if parts else None


def string_macros(text):
    """{NAME: value} for every object-like `#define NAME "lit" [NAME2 "lit"]…`
    whose body is only string literals and other string macros (resolved in
    definition order, with a second pass for a forward reference)."""
    raw = []
    for m in _DEFINE_RE.finditer(text):
        if m.group(2):                       # function-like: not a string
            continue
        raw.append((m.group(1), m.group(3).replace("\\\n", " ")))
    macros = {}
    for _ in range(3):
        grew = False
        for name, body in raw:
            if name in macros:
                continue
            v = _concat(body, macros)
            if v is not None:
                macros[name] = v
                grew = True
        if not grew:
            break
    return macros


_BRACE_SCAN = re.compile(r'"(?:[^"\\]|\\.)*"|\'(?:[^\'\\]|\\.)*\'|[{}]', re.S)


def _brace_end(text, i):
    """Index just past the `}` matching the `{` at text[i], string-aware."""
    depth = 0
    for m in _BRACE_SCAN.finditer(text, i):
        t = m.group(0)
        if t == "{":
            depth += 1
        elif t == "}":
            depth -= 1
            if depth == 0:
                return m.end()
    return len(text)


def split_rows(block):
    """[(offset, row_text)] for each top-level `{ … }` inside a table's
    initialiser `{ … }` (the block passed in starts at its opening brace).
    One pass over the braces, string and char literals skipped whole."""
    rows, depth, start = [], 0, None
    for m in _BRACE_SCAN.finditer(block):
        t = m.group(0)
        if t == "{":
            depth += 1
            if depth == 2:
                start = m.start()
        elif t == "}":
            if depth == 2 and start is not None:
                rows.append((start, block[start:m.end()]))
                start = None
            depth -= 1
            if depth == 0:
                break
    return rows


_FIELD_AT = re.compile(r"\s*\.([A-Za-z_]\w*)\s*=\s*")


def _top_level_fields(row):
    """[(name, raw_value)] for every `.name = value` directly inside the row's
    braces, the value running to the next top-level comma."""
    out, i, n = [], 1, len(row) - 1          # inside the outer { }
    while i < n:
        m = _FIELD_AT.match(row, i)
        if not m:
            # skip a stray token (a positional initialiser, a trailing comma)
            j = i
            depth = 0
            while j < n:
                c = row[j]
                if c in "\"'":
                    k = j + 1
                    while k < n and row[k] != c:
                        k += 2 if row[k] == "\\" else 1
                    j = k + 1
                    continue
                if c in "({[":
                    depth += 1
                elif c in ")}]":
                    depth -= 1
                elif c == "," and depth == 0:
                    break
                j += 1
            i = j + 1
            continue
        name, j, depth = m.group(1), m.end(), 0
        start = j
        while j < n:
            c = row[j]
            if c in "\"'":
                k = j + 1
                while k < n and row[k] != c:
                    k += 2 if row[k] == "\\" else 1
                j = k + 1
                continue
            if c in "({[":
                depth += 1
            elif c in ")}]":
                depth -= 1
            elif c == "," and depth == 0:
                break
            j += 1
        out.append((name, row[start:j].strip()))
        i = j + 1
    return out


_INT = re.compile(r"^[-+]?(0[xX][0-9a-fA-F]+|\d+)[uUlL]*$")
_ARITH = re.compile(r"^[\d\s+\-*/()uUlL]+$")


def read_value(raw, macros=None):
    """A field's raw C text -> str (joined literals, or an identifier such as
    HP_JSON), int, list of str (a brace list), or the raw text when it is
    none of those."""
    macros = macros or {}
    s = raw.strip()
    if not s:
        return ""
    v = _concat(s, macros)
    if v is not None:
        return v
    if _INT.match(s):
        body = re.sub(r"[uUlL]+$", "", s)
        return int(body, 0)
    if _ARITH.match(s) and re.search(r"\d", s):
        try:
            return int(eval(re.sub(r"(\d)[uUlL]+", r"\1", s), {"__builtins__": {}}, {}))
        except Exception:
            return s
    if s.startswith("{") and s.endswith("}"):
        items, inner, cur, depth, j = [], s[1:-1], [], 0, 0
        while j < len(inner):
            c = inner[j]
            if c == '"':
                k = j + 1
                while k < len(inner) and inner[k] != '"':
                    k += 2 if inner[k] == "\\" else 1
                cur.append(inner[j:k + 1])
                j = k + 1
                continue
            if c == "," and depth == 0:
                items.append("".join(cur))
                cur = []
            else:
                depth += c in "({["
                depth -= c in ")}]"
                cur.append(c)
            j += 1
        items.append("".join(cur))
        vals = [_concat(x, macros) for x in items if x.strip() and x.strip() != "NULL"]
        if all(x is not None for x in vals):
            return vals
        return s
    if _IDENT.fullmatch(s):
        return macros.get(s, s)
    return s


class Row(dict):
    """A row's fields by name, plus where it came from (`.file`, `.table`,
    `.line`). Missing fields read as None through .get(), as for any dict."""
    file = table = ""
    line = 0

    def s(self, name, default=""):
        """The field as a string ("" when absent or not a string)."""
        v = self.get(name)
        return v if isinstance(v, str) else default

    def i(self, name, default=0):
        """The field as an int (default when absent or not an int)."""
        v = self.get(name)
        return v if isinstance(v, int) and not isinstance(v, bool) else default


def rows_in_text(text, path=""):
    """Every row of every hp_source table in `text` (a whole translation unit:
    pass the text with any `.inc` it includes already appended)."""
    clean = strip_comments(text)
    macros = string_macros(clean)
    nl = [m.start() for m in re.finditer("\n", clean)]
    out = []
    for m in _TABLE_RE.finditer(clean):
        open_at = m.end() - 1
        block = clean[open_at:_brace_end(clean, open_at)]
        for off, rt in split_rows(block):
            r = Row()
            for name, raw in _top_level_fields(rt):
                if name not in r:                    # C: a repeat is an error; keep the first
                    r[name] = read_value(raw, macros)
            r.file = path
            r.table = m.group(1)
            r.line = bisect.bisect_left(nl, open_at + off) + 1
            out.append(r)
    return out


def rows_in_file(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            return rows_in_text(fh.read(), path)
    except OSError:
        return []


def iter_rows(tree=COLLECTORS, need=None):
    """Every hp_source row under `tree` (recursive, `.c` files). `need`, a
    substring, skips files that cannot contain a matching row — a speed
    prefilter only, applied to the raw text."""
    for f in sorted(glob.glob(os.path.join(tree, "**", "*.c"), recursive=True)):
        if os.sep + "obj" + os.sep in f:
            continue
        try:
            with open(f, encoding="utf-8", errors="replace") as fh:
                t = fh.read()
        except OSError:
            continue
        if "hp_source" not in t or (need and need not in t):
            continue
        for r in rows_in_text(t, f):
            yield r


def host_of(url):
    m = re.match(r"\s*[A-Za-z][A-Za-z0-9+.-]*://([^/?#:]+)", url or "")
    return m.group(1).lower() if m else ""


# ── fixtures ──────────────────────────────────────────────────────────────
_FIXTURE = r'''
#define BASE "https://ex.org/api"
#define BASE2 BASE "/v2"
/* static const hp_source GHOST[] = { { .id = "commented-out" } }; */
static const hp_source T1[] = {
  { .id = "a1", .url = BASE2 "/items?x=1"
      "&y={q}", .mode = HP_JSON,
    .headers = { "Accept: text/csv", "X-K: a,b" },
    .interval = 86400 * 7, .page_resume = 1,
    .id_keys = "a+b,c", /* .page_size = 99, */ .title_keys = "t \"q\"" },
  { .id = "a2", .url = "https://ex.org/{ .id = \"not-a-row\" }", .interval = 0 },
  // { .id = "line-commented" },
  { .id = "a3", .url = "http://x", .page_param = "page%5Bnumber%5D" },
};
static const hp_source T2[] = { { .id = "b1", .url = "https://y" } };
'''


def self_test():
    rows = rows_in_text(_FIXTURE, "fx.c")
    by = {r["id"]: r for r in rows}
    checks = [
        ("four rows, none from comments", sorted(by), ["a1", "a2", "a3", "b1"]),
        ("macro + adjacent literals join",
         by["a1"].s("url"), "https://ex.org/api/v2/items?x=1&y={q}"),
        ("identifier value", by["a1"].s("mode"), "HP_JSON"),
        ("brace list", by["a1"].get("headers"), ["Accept: text/csv", "X-K: a,b"]),
        ("arithmetic int", by["a1"].i("interval"), 604800),
        ("commented field absent", by["a1"].get("page_size"), None),
        ("escaped quote unescaped", by["a1"].s("title_keys"), 't "q"'),
        ("braces inside a literal are not a row", by["a2"].s("url"),
         'https://ex.org/{ .id = "not-a-row" }'),
        ("zero int", by["a2"].i("interval", -1), 0),
        ("percent-encoded page_param", by["a3"].s("page_param"), "page%5Bnumber%5D"),
        ("table name", by["b1"].table, "T2"),
        ("line number", by["a3"].line, 13),
    ]
    bad = 0
    for label, got, want in checks:
        if got != want:
            bad += 1
            print("ctable SELF-TEST FAIL  %-40s got=%r want=%r" % (label, got, want))
    return bad == 0


def main(argv):
    if "--self-test" in argv:
        ok = self_test()
        print("ctable fixtures: %s" % ("all pass" if ok else "FAILED"))
        return 0 if ok else 2
    if "--count" in argv:
        files, tables, n = set(), set(), 0
        for r in iter_rows():
            files.add(r.file)
            tables.add((r.file, r.table))
            n += 1
        print("%d rows in %d tables in %d files" % (n, len(tables), len(files)))
        return 0
    print(__doc__)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
