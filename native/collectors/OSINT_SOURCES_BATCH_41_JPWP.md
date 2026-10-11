# Batch 41 — Japanese organisations that publish through the WordPress REST API

**437 rows, every one measured end to end against the live upstream.** Probe,
`array_path` resolution, emit and **stored** readings all ran for real in the
authoring session, so each row in `docs/verified-sources-batch41.tsv` carries an
emitted-AND-stored number rather than a claim. Registry 18,883 → **19,320**.

| Beat | Rows | What |
|---|---:|---|
| `hp3b41_jpcorpwp.c` | 380 | Listed companies (東証/名証/札証/福証), keyed by 証券コード |
| `hp3b41_jpuniwp.c` | 27 | Universities (`ac.jp`) |
| `hp3b41_jpbankwp.c` | 15 | Banks, shinkin, credit cooperatives, insurers |
| `hp3b41_jpmuniwp.c` | 15 | Municipalities, keyed by 総務省 団体コード |

Scheduled feeds (`interval=21600`), not entity pivots. `body_keys=content.rendered`
keeps each organisation's own notice text, so these are record sources, not
headline lists: 87,639 records emitted and 87,640 stored across one pass of all
437 (stored exceeds emitted by the per-run disclosure notices).

## How these were found, and why the beat is mostly `co.jp`

Full method and the negative results: `docs/jp-endpoint-discovery-2026-10.md`.
In short — 6,720 hosts from fetched open-data organisation lists, probed for four
*specifications* (WordPress REST, CKAN, ArcGIS, OAI-PMH). Only WordPress REST
answered at any scale, and the rate splits in a way that contradicts the obvious
assumption:

| Type | Probed | Live | Rate |
|---|---:|---:|---:|
| Listed companies | 3,816 | 403 | 10.6% |
| Universities | 572 | 28 | 4.9% |
| Financial institutions | 707 | 18 | 2.5% |
| Municipalities + prefectures | 1,788 | 16 | 0.9% |

Corporate Japan runs WordPress; local government does not. A batch aimed at
`.lg.jp` would have come back nearly empty — all 47 prefecture roots returned
nothing on all four conventions, and `www.pref.aichi.jp` answers `/wp-json/wp/v2/posts`
with **403**, meaning the API is present and deliberately closed.

Also established: CKAN, ArcGIS and OAI returned **zero** at these hostnames;
7,152 derived open-data hostnames (`opendata.`/`data.`/`odcs.`/`catalog.`)
answered 4 times; and of 155 national-API candidates probed, 91 were live and
essentially all were already registered here (JMA, BODIK, 国会会議録, MyJVN,
and WordPress REST itself in 37 files). At 18,643 sources the reachable
Japanese surface is largely harvested; sweeps no longer find it.

## Three things this batch had to get right

**`array_path=.` is mandatory, not tidiness.** WordPress returns a BARE ROOT
array and every post carries `_links.wp:term`, itself an array of objects. On a
site whose first page holds 2 posts that TIES with the root, and the
densest-array heuristic can mine the link stubs instead of the posts — the
hex.pm hijack in CLAUDE.md. Every row declares it.

**`pagination_ok` prose must not contain a `;`.** The first probe run rejected
all four manifests: a semicolon inside the prose split the opts field and the
following text became a token with no `=`. That is rule 4c's swallowed-opt trap,
caught by the tool exactly as designed.

**Discovery with TLS verification off inflates the count by 4.5%.** The sweep
script ran `CERT_NONE`; 21 of its 465 hits then failed
`CERTIFICATE_VERIFY_FAILED` or reset the connection under `probe_hp_batch.py`,
which verifies properly. They are in `docs/rejected-sources-batch41.*.tsv`.
A discovery pass that skips verification is not a proof of life.

## One upstream defect worth naming: a site that loses its own records

`www.ckd.co.jp` (証券コード 6407) reports `X-WP-Total: 2329` across 24 pages and
then **serves overlapping page contents deterministically**. Measured: 12 pages ×
100 posts yielded **612 distinct** with `orderby=date` and **549** with
`orderby=id`, while each individual page is byte-identical across repeated
fetches. So it is not an unstable sort we could pin — posts that exist are never
served at all. It surfaced only as `emitted=2000 stored=791` in the registry
sweep, which is rule 4b doing its job: the probe passed, the first three pages
were clean, and nothing short of the stored count could see it.

Dropped, with the measurement, rather than shipped with 60% loss or "fixed" by
widening `id_keys` away from the post id. One more row (`JO41_WP_CORP_9900`,
emitted 823 / stored 822) was dropped for the same reason at much smaller scale,
so the batch ships with **no COLLISION verdicts**.

## The page ceiling: measured, then removed

The beat first shipped at `page_max=20`, and **six rows hit it** — a truncation
notice is filed, so it was disclosed rather than silent, but a disclosed discard
is still a discard (house rule 2). Measuring `X-WP-Total` on all 437 rows showed
how much:

| row | X-WP-Total | pages | lost at page_max=20 |
|---|---:|---:|---:|
| `JO41_WP_CORP_8508` | 3,481 | 35 | 1,481 |
| `JO41_WP_CITY_473014` | 2,927 | 30 | 927 |
| `JO41_WP_CORP_4712` | 2,406 | 25 | 406 |
| `JO41_WP_CORP_3807` | 2,400 | 24 | 400 |
| `JO41_WP_UNIV_HITU` | 2,170 | 22 | 170 |
| `JO41_WP_CORP_7419` | 2,004 | 21 | 4 |

**3,388 posts per pass.** The distribution across the 429 rows that answered the
header is heavily skewed — median **1** page, p95 10, p99 21, max 35 — so a
ceiling is nearly free for almost every row and decisive for a handful:

```
page_max=20   6 rows truncated, 3,388 posts lost/pass
page_max=25   2 rows truncated, 1,408
page_max=30   1 row  truncated,   481
page_max=40+  0 rows truncated,     0
```

Now **`page_max=60`**: it clears the whole measured distribution with ~70%
headroom over today's maximum, which these news feeds will consume as they grow,
and it still bounds a runaway. It costs the median row nothing — the walk stops
when a page comes back short, so the ceiling only ever binds on the large sites.

Re-measured after the change, each of the six now emits **exactly** its
`X-WP-Total`, with no collisions:

```
JO41_WP_CORP_7419    emitted=2004  stored=2004  db=2004
JO41_WP_CORP_3807    emitted=2400  stored=2401  db=2400
JO41_WP_CORP_4712    emitted=2406  stored=2406  db=2406
JO41_WP_CORP_8508    emitted=3481  stored=3481  db=3481
JO41_WP_UNIV_HITU    emitted=2170  stored=2170  db=2170
JO41_WP_CITY_473014  emitted=2927  stored=2927  db=2927
          15,388 emitted / 15,388 stored / 0 lost to uid collision
```

## Gates

```
make                       0 new warnings (3 pre-existing in embed_pod.c/vuln_world.c)
make audit-sources         0 findings, strict set 299 files
make lint-sources          OK, 19320 registered
make hptest selftest unit authtest pagewalktest htmlparsetest   all pass
source-floor/registry-floor  re-recorded at 19320, 0 duplicates, 0 dropped
batch_exclusions.py --check  0 collisions against all 18,643 prior sources
audit_batch_pagination.py    0 of 461 paged rows undeclared
audit_batch_reachable.py     0 of 461 can never run
probe_hp_batch.py            439 PASS / 21 NET_ERR / 1 UNPARSEABLE
audit_registry_emit.py       437 OK, 2 COLLISION (both dropped)
```

`--check-filter` does not apply: these are scheduled feeds with no entity token,
so there is no filter to ignore (rule 4d).

## Re-measuring later

```sh
cd native
python3 tools/audit_registry_emit.py --bin ./bin/japanosint --match '^JO41_' \
        --jobs 6 --timeout 200 --out sweep38.tsv
```

The ceiling is now 60 pages (6,000 posts) and nothing in the batch reaches it —
today's largest is 35 pages. A site that grows past 60 will disclose a truncation
notice naming how much it held, so the next revisit is visible in the data rather
than needing this paragraph. Re-measure with the command above and compare each
row's `emitted` against its `X-WP-Total`.
