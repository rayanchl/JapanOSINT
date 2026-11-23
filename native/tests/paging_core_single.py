#!/usr/bin/env python3
"""tests/paging_core_single.py — the paging core stays ONE copy.

lib/pagewalk.c owns the declared-total key list, the pager table, the cursor
lists and the next-link list; lib/jsonlist.c, lib/geojson.c and lib/hpengine.c
call it. The history this guards (CLAUDE.md): the same paging fix landed in one
copy and missed the others at least four times — the uid-collision guard, the
page ceiling, the declared-total lists, the repeat-page guard. Reintroducing a
second list is how it happens again, so it fails the build.

Heuristic and deliberately narrow: a SECOND LIST is a lib/ or core/ file other
than pagewalk.c that carries several of the identifying string literals of one
list. A single mention (a comment, a URL, one `.page_param`) is fine; three
members of the same family is a table. The fixtures below run first, and a
check that stops seeing its own case exits 2 — a gate that cannot fail is not
a gate.

  python3 tests/paging_core_single.py [--root native]
"""
import os
import re
import sys

TOTAL_KEYS = ['"totalResults"', '"total_count"', '"totalCount"', '"numFound"',
              '"numberMatched"', '"totalElements"', '"recordsTotal"',
              '"number_of_results"', '"nhits"', '"totalHits"', '"totalItems"',
              '"@odata.count"', '"response.numFound"', '"result.count"',
              '"totalFeatures"', '"matchedCount"']
PAGER_KEYS = ['"per_page"', '"page_size"', '"pageSize"', '"perPage"', '"itemsPerPage"',
              '"resultRecordCount"', '"maxRecords"', '"maxFeatures"', '"$limit"',
              '"$offset"', '"$skip"', '"resultOffset"', '"startIndex"']
LINK_KEYS = ['"next_url"', '"nextUrl"', '"nextPageUrl"', '"@odata.nextLink"',
             '"next_page"', '"nextLink"', '"next_page_url"', '"nextRecordsUrl"',
             '"meta.pagination.next"', '"pagination.next_page"']
# core/pagination_probe.c used to keep its own size/offset name lists; it now
# reads lib/pagewalk.c (pw_declared_size_param, pw_url_has_cursor). No exemptions.
EXEMPT = {}
# The probe's retired SIZE_PARAMS / OFF_PARAMS, plus any array under those names.
PROBE_SIZE_KEYS = ['"limit"', '"rows"', '"$limit"', '"resultRecordCount"', '"page_size"',
                   '"per_page"', '"pageSize"', '"$top"', '"size"', '"maxRecords"',
                   '"count"', '"retmax"', '"itemsPerPage"']
LISTS = [('declared-total list', TOTAL_KEYS, 3),
         ('probe size list', PROBE_SIZE_KEYS, 6),
         ('pager table', PAGER_KEYS, 4),
         ('next-link list', LINK_KEYS, 3)]


def hits(text, keys):
    return sorted({k for k in keys if k in text})


def scan(files):
    bad = []
    for path, text in files:
        for name, keys, need in LISTS:
            h = hits(text, keys)
            if len(h) >= need:
                bad.append((path, name, h))
    return bad


def selftest():
    # a second declared-total list is seen ...
    t = 'static const char *K[] = {"totalResults", "numFound", "recordsTotal", NULL};'
    assert scan([('x.c', t)]), 'second total list not detected'
    # ... as is a second pager table ...
    t = '{"per_page","page"},{"$limit","$offset"},{"resultRecordCount","resultOffset"}'
    assert scan([('x.c', t)]), 'second pager table not detected'
    # ... the probe's old inline size array is seen ...
    t = 'static const char *const SIZE_PARAMS[] = {"limit", "rows", "$limit", "resultRecordCount", "page_size", "per_page", NULL};'
    assert scan([('x.c', t)]), 'second size list not detected'
    # ... and one mention is not a list.
    t = 'cJSON *v = get(doc, "totalResults"); /* per_page */'
    assert not scan([('x.c', t)]), 'a single mention flagged'


def main():
    selftest()
    root = 'native'
    if '--root' in sys.argv:
        root = sys.argv[sys.argv.index('--root') + 1]
    elif os.path.isdir('lib') and os.path.isdir('core'):
        root = '.'
    files = []
    for d in ('lib', 'core'):
        for fn in sorted(os.listdir(os.path.join(root, d))):
            if not fn.endswith('.c') or fn == 'pagewalk.c':
                continue
            p = os.path.join(root, d, fn)
            with open(p, encoding='utf-8', errors='replace') as f:
                text = f.read()
            # a comment is documentation, not a list
            text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
            files.append((p, text))
    # and pagewalk.c must still HOLD each list (the scan above would pass if it
    # had been deleted and nothing replaced it)
    with open(os.path.join(root, 'lib', 'pagewalk.c'), encoding='utf-8') as f:
        pw = f.read()
    for name, keys, need in LISTS:
        if len(hits(pw, keys)) < need:
            print('FAIL: lib/pagewalk.c no longer holds the %s' % name)
            return 1
    bad = scan(files)
    for path, name, h in list(bad):
        rel = os.path.relpath(path, root)
        if EXEMPT.get(rel) == name:
            print('NOTE: %s keeps its own %s (known, not unified)' % (rel, name))
            bad.remove((path, name, h))
    for path, name, h in bad:
        print('FAIL: %s carries a second %s (%s) — it belongs in lib/pagewalk.c'
              % (path, name, ', '.join(h)))
    if bad:
        return 1
    print('paging core: one declared-total list, one pager table, one next-link list')
    return 0


if __name__ == '__main__':
    sys.exit(main())
