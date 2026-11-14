#!/usr/bin/env python3
"""Does a Socrata dataset's `:id` survive the publisher's refresh?

WHY IT MATTERS. `:id` is Socrata's own row identifier and it reaches the ceiling
on every dataset measured, so it is the obvious key for a dataset that publishes
no identifier of its own — 158 rows in this tree are keyed on it. But a row id
is minted when the ROW is created, and many publishers refresh a dataset by
REPLACING it wholesale: every row is deleted and re-inserted, so every row is
new and every `:id` is new. A source keyed on `:id` then re-keys its whole
dataset on each refresh — every record arrives as a new uid, nothing ever
updates, and the table grows without bound with no error anywhere. That is the
hash-fallback trap `lib/geojson.c` records for `hudson_rock_jp.c`, reached from
the opposite direction: a key chosen BECAUSE it looked like the stable one.

WHAT IS MEASURED AND WHAT IS INFERRED. `:created_at` is measured: 40 of the 155
`:id` rows read on 2026-11-14 serve a sample in which every row was created
within the previous two days. That those rows' ids are therefore new is an
INFERENCE about Socrata, and this tool exists to replace it with a reading:

    snapshot   record, for a deterministic slice of each dataset, a hash of each
               row's CONTENT (system fields removed) beside its `:id`
    compare    re-read the same slice later and, for every row whose content is
               unchanged, ask whether its `:id` is

A row whose content is identical and whose `:id` differs is the churn, observed.
A dataset whose ids all survive is stable and its `:id` key is fine.

    python3 tools/socrata_id_churn.py snapshot --out ../docs/socrata-id-snapshot-<date>.tsv
    python3 tools/socrata_id_churn.py compare  --snapshot ../docs/socrata-id-snapshot-<date>.tsv
    python3 tools/socrata_id_churn.py --self-test
"""
import argparse
import hashlib
import json
import subprocess
import sys
import time

UA = 'Mozilla/5.0 (compatible; JapanOSINT/1.0)'

# (label, resource base, a business-field order that is stable across a
# refresh). One row this tree DELIBERATELY did not key on `:id`, one it already
# does, and the control: a dataset whose rows were all created in 2015.
DATASETS = [
    ('WA_CONTRACTOR_INSURANCE  (replaced daily; NOT keyed on :id)',
     'https://data.wa.gov/resource/ciwg-agsx.json',
     'contractorlicensenumber,insurancepolicyno,effectivedate'),
    ('nam-nyc-restaurant-inspections  (replaced daily; NOT keyed on :id)',
     'https://data.cityofnewyork.us/resource/43nn-pn8j.json',
     'camis,inspection_date,violation_code'),
    ('JO29A_CAM_US_HI_HONOLULU  (CONTROL: rows created 2015; keyed on :id)',
     'https://data.honolulu.gov/resource/cat5-2v98.json',
     'description'),
]


def content_hash(row):
    """A row's content with Socrata's system fields removed, so two readings of
    the same logical record hash alike whatever happened to its row object."""
    core = {k: v for k, v in row.items() if not k.startswith(':')}
    return hashlib.sha1(json.dumps(core, sort_keys=True,
                                   ensure_ascii=False).encode()).hexdigest()[:20]


def fetch(base, order, limit):
    url = '%s?$select=*,:id,:created_at&$order=%s&$limit=%d' % (base, order, limit)
    time.sleep(1.0)
    r = subprocess.run(['curl', '-sSg', '--max-time', '90', '-A', UA, url],
                       capture_output=True)
    if r.returncode != 0:
        return None
    try:
        d = json.loads(r.stdout.decode('utf-8', 'replace'))
    except Exception:
        return None
    return d if isinstance(d, list) else None


def judge(old, new):
    """old/new: {content_hash: (id, created_at)}. -> (same_content, id_kept,
    id_changed). Only rows whose CONTENT survived can say anything."""
    kept = changed = 0
    for h, (oid, _c) in old.items():
        if h in new:
            if new[h][0] == oid:
                kept += 1
            else:
                changed += 1
    return kept + changed, kept, changed


def self_test():
    bad = 0

    def check(label, got, want):
        nonlocal bad
        if got != want:
            bad += 1
        print('  %-4s %-58s got %-14s want %s' % ('ok' if got == want else 'FAIL',
                                                  label, got, want))
    a = {':id': 'row-1', ':created_at': 'x', 'n': 1}
    b = {':id': 'row-9', ':created_at': 'y', 'n': 1}
    check('system fields do not enter the content hash',
          content_hash(a) == content_hash(b), True)
    check('a changed business field changes it',
          content_hash(a) == content_hash({':id': 'row-1', 'n': 2}), False)
    old = {'h1': ('row-1', 'd1'), 'h2': ('row-2', 'd1'), 'h3': ('row-3', 'd1')}
    check('same content, new ids -> churn observed',
          judge(old, {'h1': ('row-7', 'd2'), 'h2': ('row-8', 'd2')}), (2, 0, 2))
    check('CONTROL: ids that survive are counted as kept',
          judge(old, {'h1': ('row-1', 'd1'), 'h2': ('row-2', 'd1')}), (2, 2, 0))
    check('CONTROL: content that changed says nothing',
          judge(old, {'zz': ('row-1', 'd1')}), (0, 0, 0))
    print('self-test: %s' % ('%d FAILURES' % bad if bad else 'ok'))
    return 1 if bad else 0


def main():
    if '--self-test' in sys.argv[1:]:
        return self_test()
    ap = argparse.ArgumentParser()
    ap.add_argument('mode', choices=['snapshot', 'compare'])
    ap.add_argument('--out')
    ap.add_argument('--snapshot')
    ap.add_argument('--limit', type=int, default=500)
    a = ap.parse_args()

    if a.mode == 'snapshot':
        lines = ['dataset\tbase\torder\tcontent_hash\tid\tcreated_at']
        for label, base, order in DATASETS:
            rows = fetch(base, order, a.limit)
            if rows is None:
                print('UNMEASURED  %s' % label); continue
            for r in rows:
                lines.append('\t'.join([label, base, order, content_hash(r),
                                        str(r.get(':id', '')),
                                        str(r.get(':created_at', ''))]))
            days = sorted({str(r.get(':created_at', ''))[:10] for r in rows})
            print('%4d rows  created on %s  %s' % (len(rows), days[-3:], label))
        if a.out:
            open(a.out, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
        return 0

    old = {}
    for l in open(a.snapshot, encoding='utf-8').read().splitlines()[1:]:
        label, base, order, h, rid, created = l.split('\t')
        old.setdefault((label, base, order), {})[h] = (rid, created)
    churn = 0
    for (label, base, order), o in old.items():
        rows = fetch(base, order, a.limit)
        if rows is None:
            print('UNMEASURED  %s' % label); continue
        new = {content_hash(r): (str(r.get(':id', '')), str(r.get(':created_at', '')))
               for r in rows}
        same, kept, changed = judge(o, new)
        days = sorted({c[:10] for _i, c in new.values()})
        if same == 0:
            v = 'NO OVERLAP — no row kept its content, so this reading says nothing'
        elif changed == 0:
            v = 'STABLE — every surviving row kept its :id'
        else:
            v = 'CHURN OBSERVED — %d of %d unchanged rows carry a NEW :id' % (changed, same)
            churn += 1
        print('%s\n    snapshot rows %d, re-read %d (created %s), unchanged content %d: '
              'id kept %d, id changed %d\n    -> %s'
              % (label, len(o), len(rows), days[-2:], same, kept, changed, v))
    return 1 if churn else 0


if __name__ == '__main__':
    sys.exit(main())
