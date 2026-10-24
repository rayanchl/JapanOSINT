# XML replay of `hp_xml_flatten` (bdfce6a), October 2026

**What was checked:** bdfce6a ("hp_xml_flatten keeps what it used to drop, and
stamps what a bound still keeps out") changed how every XML-mode hpengine row
turns elements into records. The first check replayed 341 of 1,587 XML rows.
This second pass took every XML-mode row (1,592 at the time), fetched each one
ONCE into a cache, and replayed the same cached body through two engines:

- **before**: the engine at bdfce6a's parent (ddf13483);
- **after**: the engine on main at 7c2254dd.

Same bytes in, two engines, so a changing upstream cannot pollute the
comparison. Each row had a 360 s limit, at most 4 at a time.

**Status: partial.** The run was interrupted at 1,108 of 1,592 rows, and the
harness and its cache were lost with `/tmp` before the rest finished. Nothing
below is extrapolated; the remaining 484 rows were not compared. They were
mostly JAIRO OAI-PMH repositories (`*.repo.nii.ac.jp`), which took 20–30
minutes each and frequently failed at the transport level from this network.

## Result over the 1,080 rows compared

| measure | before | after |
| --- | --- | --- |
| records emitted | 96,198 | 96,200 |
| distinct uids | 96,167 | 96,169 |
| notices | 126 | 126 |
| fields kept | 1,539,176 | 1,870,709 (+21.5%) |
| `_fields_dropped` stamps | 21,977 | 21,977 |

- **Records lost: 0. Rows with fewer records: 0. Rows with fewer distinct uids: 0.**
- **Titles changed: 0. Links changed: 0. Published dates changed: 0.**
  **Field values changed: 0.** Every field the old engine kept, the new one
  keeps with the same value: 1,515,325 kept as-is, 1,874 moved to an indexed
  key. For example, a repeated child that used to overwrite its sibling now
  keeps both as `name` and `name.1`.
- **Records gained: 2, in 2 rows.** These are elements the old engine skipped:
  self-closing records that carry only attributes, or text-only records.
- **uids re-keyed: 846, in 5 rows.** All are records that share a key within a
  page and get a content-hash suffix to tell them apart. They carry more fields
  now, so the hash changed. Expect a one-time re-key of those rows on their
  next run, with the old rows left behind;
  `native/tools/cleanup_stale_uids.py` is the place to extend if they matter.
- **Not run on the old engine:** 2 rows, `ECFR_USML_TEXT` and
  `ECFR_ENTITY_LIST_TEXT`. Their URL carries a `{date}` token the old engine
  could not expand, so the cache key never matched. The new engine emitted 3
  and 21 records.

## Not compared

- **Unreachable (28):** 4 prefectural RSS feeds answering 404 (Ibaraki ×2,
  Shiga ×2), the UK OFSI consolidated list (transport failure), and 23 JAIRO
  repositories that failed at the transport level after 20–30 minutes.
- **Not reached before the interruption: 484 rows,** mostly JAIRO repositories.

## Conclusion

Over 1,080 rows and 96,198 records the change lost nothing. It changed no
title, link, date or value. It kept 21.5% more fields, and it re-keyed 846
records in 5 rows for the reason stated in the commit. The unverified
remainder is the slow repository set, which the first 341-row check also
skipped.
