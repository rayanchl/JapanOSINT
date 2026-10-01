# Batch 24 — three engine fixes, and the sources they unlock

Batch 23 rejected four families. Three of those rejections were engine
limitations rather than properties of the sources, so this batch fixes the
engine. The fourth turned out to be a **mistake in the batch 23 write-up**, and
that correction is the most important line here.

Registry: **14,115 → 14,135** (+20). Build: 0 warnings. Every gate green.

## Engine changes

### 1. `array_path` can now cross an array

`hp_path()` walks each segment with `cJSON_GetObjectItem()`, which returns NULL
on an array. A document that is a top-level array of objects, each holding the
real record list, was unreachable: the only expressible alternative was a
positional index like `0.timeSeries.0.areas`, which reaches one block and
silently discards every other — the discard house rule 2 forbids.

`hp_path_multi()` walks the same segments but, when the node is an array and the
segment is neither an index nor a `key=value` selector, **maps the segment
across every element** and concatenates. A node that is itself an array is
spliced, so the result is a flat record list. It returns a reference array the
caller deletes; the borrowed records are untouched.

It is wired in as a **fallback only** — tried when the plain walk already
returned nothing — so no row that resolves today changes behaviour, and a
genuinely absent `array_path` still emits nothing rather than letting the new
walk mine something else. Both halves are asserted in `make hptest`.

### 2. hpengine transcodes legacy Japanese encodings

`lib/feedlib.c` transcodes a non-UTF-8 body from a `.jp` host, with a documented
rationale. hpengine never used that path, so every hp row on a legacy `.jp` host
stored mojibake. hpengine now does the same, before the parse — after it, a
Shift_JIS title is bytes nobody can recover a string from.

The gate is duplicated rather than shared on purpose: `tests/hpengine_test.c`
links hpengine **without** feedlib, precisely so the engine test does not drag in
OpenSSL. `lib/csv.c` is already on that link line.

### 3. …and a row can declare its own charset

The host gate is right for what it is for, and wrong for these boards:
**machi.to is a Tonga domain and open2ch.net is `.net`**. Both are Japanese
sites serving Shift_JIS, and the `.jp` test correctly refused them. The encoding
is a property of the endpoint, not of its TLD, so a row now declares
`charset=sjis` (or `cp932`, `shift_jis`, `euc-jp`) and is transcoded whatever its
host. `csv_decode_sjis()` was generalised to `csv_decode_charset(buf, len, from)`
and kept as a one-line wrapper, so every existing caller is unchanged.

**CP932, not SHIFT_JIS.** This one only showed up in the measurement. glibc's
`SHIFT_JIS` is strict JIS X 0208 and *rejects* the NEC/IBM extension rows at
0x81A1–0x879C — which is where ■ ★ ☆ ① live. `csv_decode_charset` fails closed,
so **one decorative character in one thread title returned a verbatim copy and
lost the entire document.** That is exactly why open2ch transcoded cleanly and
machi.to did not, from the same code path and the same commit. CP932 is a strict
superset of Shift_JIS, so the change can only turn a failure into a success.

Six new assertions in `make hptest` cover all of it, including the two that keep
the fixes honest: the same bytes from a non-`.jp` host are **not** transcoded
(Latin-1 `Z\xfcrich` is valid Shift_JIS and a blanket transcode would turn
European feeds into kanji), and a `.jp` host already serving UTF-8 passes through
untouched.

## The correction

Batch 23 recorded that **56 verified JMA forecast offices were dropped because
`hp_path()` could not cross an array.** The engine limitation was real and is now
fixed. The conclusion drawn from it was wrong.

All 112 JMA forecast and warning endpoints are **already registered**, in
`vsrc13_jp_local_1.c` and `vsrc14_jp_prefectural_3.c`. `batch_exclusions --check`
reported 112 of the 114 candidate rows as duplicate endpoints. And they are not
under-reading: the existing rows store 2 records per office, one per top-level
block, and each record's `properties` carries **the whole nested `timeSeries`
array**. Nothing is discarded — the difference is record granularity, not
coverage.

So the 114 JMA rows were dropped a second time, for the right reason this time,
and `docs/verified-sources-batch23.md` has been corrected in place. Fixing the
engine did not recover them because they were never missing.

Two further rows were dropped the same way: the Tokyo and Sapporo whole-catalogue
sweeps are already in `vsrc13_jp_local_2.c`.

## The 20 rows

| beat | rows | measured |
| --- | --- | --- |
| `jpbbs` | 18 | 15 Machi BBS boards + 3 open2ch boards |
| `jpbulk` | 2 | data.go.jp dataset index, J-STAGE corpus sweep |

**jpbbs** — the 2ch-family boards, live and now legible. Machi BBS is the oldest
surviving Japanese regional forum and its boards are organised strictly by
geography: threads about named streets, shops, schools, redevelopments and local
officials, which no national platform carries and no municipal site would
publish. Note the two different field separators, both measured rather than
assumed: machi.to uses a comma (0 of 639 titles contain a second one) and open2ch
uses `<>`.

5ch.net and jbbs.shitaraba.net still answer 403 to this host and remain absent.
That is a refusal, not an encoding problem, and no engine change addresses it.

**jpbulk** — two endpoints whose *filter* is ignored, registered as what they
actually are. An API that accepts a filter, ignores it and returns the whole
collection is unusable as an entity pivot — it attributes thousands of unrelated
records to whatever was asked about — but it is perfectly good as a bulk source,
where no per-entity claim is made:

* `JPOD_DATAGOJP_INDEX` — data.go.jp is worse than filter-ignoring. Measured:
  `package_search` ignores `fq`, `q`, **`rows` and `start` alike** and returns
  the same ten records whatever you ask, so all 23 per-organisation candidates
  and every paged form were rejected outright. `package_list` is the one endpoint
  that answers honestly, and it returns all **18,147** dataset identifiers — a
  complete inventory of what the state publishes.
* `JSTAGE_CORPUS_BULK` — `service=3` accepts `text=` and `article=` and ignores
  both (20 of 20 entries for a term that cannot exist). `totalResults` is
  2,637,915 and `start=` was verified to advance to at least 10,001, so the row is
  deliberately bounded by `max_items` and emits a `collector-truncation-notice`
  stating how much of the corpus it took — bounded and in-band rather than
  silently partial.

## Measured, against the real binary

```
JPBBS_MACHI_TOKYO          records=639    stored=639     0 mojibake
JPBBS_MACHI_KOUSINETU      records=1553   stored=1553    0 mojibake
JPBBS_OPEN2CH_LIVEJUPITER  records=150    stored=150     0 mojibake
JPOD_DATAGOJP_INDEX        records=18147  stored=18147
JSTAGE_CORPUS_BULK         records=1001   stored=1001    (bounded, notice emitted)
```

Titles were read back out of the database as raw bytes and decoded, not eyeballed
in a terminal — a terminal will happily render mojibake as plausible-looking
glyphs. Before the fix the same query returned
`b'\x82\xdc\x82\xbfBBS \x93\x8c\x8b\x9e23...'`.

## Gates

```
$ probe_hp_batch.py ../docs/candidate-sources-batch24.*.txt      20/20 PASS
$ audit_batch_reachable.py                                       0 of 20 can never run
$ audit_batch_pagination.py                                      0 of 20 look paged
$ batch_exclusions.py --check --bin --skip-prefix hp3b24         collisions: 0
$ make                    warnings/errors: 0        14,135 sources
$ make selftest           PASS
$ make unit               OK
$ make hptest             all passed  (6 new assertions)
$ make lint-sources       OK
$ make audit-sources      strict set 0 findings, tree 0 findings
```

The full suite was re-run from `rm -rf obj bin` after the engine changes, because
both changes are in shared code that every one of the 14,135 sources runs through.
