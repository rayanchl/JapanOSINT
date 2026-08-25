# Slice 01 — 270 sources

All 270 live in `native/collectors/sources/gnews_world_topics_1.c`: 90 countries ×
{WORLD, NATION, BUSINESS} Google News RSS editions. Every source is a one-line `RSSX(...)`
macro delegating to `rss_collect()` in `native/lib/rss_atom.c`, so every defect is either
a **wrong URL** (in-slice, fixable) or a **`lib/rss_atom.c` defect** (read-only →
reported).

## Summary

Counts by verdict, after the fixes below; all 270 re-run individually against
`bin/japanosint-a0`:

| verdict | count |
|---|---|
| DATA | **270** |
| EMPTY | **0** (was 27) |
| KEY_GATED / WAF_BLOCKED / RC_ERROR / TIMEOUT / CRASH | 0 |

Slice totals: **16,159 rows, 0 with lat/lon, 0 with geometry.** No credential is involved
in any of these, so nothing is legitimately key-gated.

Quality split: **189 real+country-scoped · 27 fixed here · 54 real but NOT country-scoped
(duplicates).**

Two column-level integrity defects ride on top of the DATA verdicts: **608 rows / 80
sources have invalid UTF-8 in `summary`**, and **2,771 rows / 188 sources have invalid
JSON in `properties`**. Both originate in `lib/rss_atom.c`.

Five most important findings, most severe first:

1. 54 sources are byte-identical duplicates of a sibling, mislabelled with the wrong
   country.
2. `lib/rss_atom.c:172-173` splits UTF-8 sequences → 608 corrupt rows.
3. `lib/rss_atom.c:165-171` truncates `properties` to 511 chars → 2,771 rows of invalid
   JSON.
4. `published_at` stored as raw RFC-822, and it is the feed's primary sort key — the
   intel timeline is not chronological.
5. Publisher dropped; `body`/`summary` are markup, not article text.

## Fixes applied

| file | source(s) | bug | fix | re-test result |
|---|---|---|---|---|
| `gnews_world_topics_1.c` | 27 × `gnews-*-nation` | `topic/NATION` returns a valid RSS channel with zero `<item>`s for these editions — permanently 0 rows. `gnews-ir-nation` and `gnews-mm-nation` were worse: they fetched **1.85 MB of HTML** (not RSS) every hour. | Repointed to the country-scoped `news.google.com/rss/search?q=<country name in the feed language>` feed, keeping each source's existing `hl`/`gl`/`ceid`, percent-encoding non-ASCII names, and updating `.description` to say so. | **0 rows → 98–106 rows each, `rc=0`, 2,720 rows total.** All 27 fingerprints unique (no collapse), each in the right language. |

Upstream emptiness was confirmed by direct `curl`, and `headlines/section/geo/<Country>`
checked as equally empty, so this was not a wrong-topic guess. No hardcoded data — every
row still comes from the fetch.

Sample rows after the fix:

- `gnews-lk-nation` → *"Sri Lanka ex-police chief sentenced to death over 2019 Easter…"*
- `gnews-kz-nation` → *"Reuters: Казахстан ведет переговоры с Россией…"*
- `gnews-tn-nation` → *"لماذا خرجت المظاهرات في تونس ضد الرئيس قيس سعيد؟ - BBC"*

Sources fixed:
`gnews-{am,az,bo,cr,dk,do,dz,ec,gt,hr,iq,ir,is,jo,kh,kw,kz,lk,ma,mm,np,pa,py,qa,tn,uy,uz}-nation`

## Findings not fixed (with reason)

### 1. 54 sources are byte-identical duplicates, mislabelled with the wrong country

`topic/WORLD` and `topic/BUSINESS` are scoped by `hl` *language*, not `gl`. Where `hl`
names a locale Google doesn't publish, it discards the locale and serves a generic
edition. Verified at the persisted-row level (`link` set overlap, back-to-back runs, each
with a control):

| family | sources | identical to | Jaccard | control |
|---|---|---|---|---|
| → en-US | `{hr,ir,is,kh,lk,mm,np}-business` | `gnews-us-business` | **1.00** | `gnews-gb-business` 0.01 |
| → en-US | `{hr,ir,is,kh,lk,mm,np}-world` | `gnews-us-world` | 0.82 | `gnews-gb-world` 0.03 |
| `hl=da` → **Norwegian** | `dk-{world,business}` | `gnews-no-*` | **1.00** | `gnews-se-world` 0.00 |
| `hl=ru` | `{am,az,kz,uz}-{world,business}` | `gnews-ru-*` | **1.00** | `gnews-ua-world` 0.00 |
| `hl=ar` | `{dz,iq,jo,kw,ma,qa,tn}-{world,business}` | `gnews-eg-*` | **1.00** | `gnews-sa-world` 0.34 |
| `hl=es-419` | `{bo,cr,do,ec,gt,pa,py,uy}-{world,business}` | each other, 8-way | **1.00** | `gnews-mx-world` 0.04 |

`gnews-dk-world` literally returns `<language>no</language>` / `Verden - Siste - Google
Nyheter`. `gnews-is-world` returns `<language>en-US</language>` / `World - Latest - Google
News`. Since `uid` is `source_id|guid` there is **no cross-source dedupe**, so each
article is stored 7–8 times, each copy tagged a different country.

*Why not fixed:* deleting or renaming+repointing 54 registered IDs is a product call, not
a local safe edit — unlike NATION, "the World section of the Iceland edition" has no
honest equivalent. Recommend delete, or repoint to `rss/search?q=<country>` **and rename**.

### 2. `lib/rss_atom.c:172-173` — UTF-8 split on a byte boundary

`char summ[256]; strncpy(summ, desc, 240)` truncates on a **byte** boundary, splitting
UTF-8 sequences. 608 rows / 80 sources. This is exactly the sweep's `DB_ERROR`;
`verify_one.py` itself dies on `gnews-jp-nation` with `Could not decode to UTF-8 column
'summary'`. Hits every non-Latin edition.

Fix: walk back while `(c & 0xC0) == 0x80`, or just `strndup`.

*Why not fixed:* `lib/` is read-only for this slice and the fix is shared by every
`rss_collect` caller.

### 3. `lib/rss_atom.c:165-171` — `properties` truncated to invalid JSON

`char props[512]` + `snprintf` truncates `properties` to 511 chars. Google News guids
reach 731 chars. **2,771 rows / 188 sources** fail `json.loads`. Pre-existing and
independent of the NATION change (untouched `gnews-ec-world`: 31 of 66 rows broken).

Fix: keep `cJSON_PrintUnformatted`'s heap string.

*Why not fixed:* read-only `lib/`.

### 4. `published_at` stored as raw RFC-822 — and it is the primary sort key

`rss_collect` assigns it verbatim (its own TODO says `/* RFC822→ISO norm: P5 refine */`),
`core/intel.c:197` binds it unchanged → `Fri, 31 Jul 2026 12:33:00 GMT`. Other collectors
write ISO-8601 to the same column (`airquality_world.c:345`, `earthquake_monitor.c:87`,
`court_records.c:272`). `core/intelapi.c:293`, `core/exportapi.c:494`,
`core/simhash.c:552` all `ORDER BY COALESCE(published_at, fetched_at)` as **text**.

Since `'F' > '2'`, every RSS row outranks every ISO row regardless of date, and RSS rows
sort among themselves by weekday name. **The intel timeline is not chronological.**

*Why not fixed:* `lib/` + `core/` are read-only here.

### 5. Publisher dropped; `body`/`summary` are markup, not article text

Every item carries `<source url="https://www.euronews.com">Euronews.com</source>` (52/52,
53/53, 97/97 in the feeds dissected). `rss_collect` never reads it → `author` NULL on all
16,159 rows, publisher only recoverable by splitting the title on `" - "`. The
`<description>` is an HTML list of *related links*, so `body` (~2 KB) and `summary` are
`<ol><li><a href=…>` markup with no prose. `properties` carries only `guid`.

## Smaller items

- **`tests/audit/agent_build.sh` reported success on a failed build.** It piped `make`
  through `grep … || true` then only ran `test -x`, so it printed `[slot a0] OK` while
  `bin/japanosint-a0` was 40 minutes stale (a concurrent edit to `geohazard_extra.c`
  broke the link). Other auditors may have been verifying against stale binaries.
- **`rss_collect` cannot distinguish "not a feed" from "empty feed"** — both return 0.
  That is how `gnews-{ir,mm}-nation` hid a 1.85 MB HTML response for so long. A
  content-type/root-element check in `lib/` would surface this fleet-wide.
- **Hebrew language code divergence:** `gnews-il-*` uses the deprecated `"iw"`, as do
  `gnews_world_top.c:101`, `gnews_osint_monitors.c:221`,
  `gnews_world_topics_2.c:269,273,277`, while `reg_il_data.c:117` already uses `"he"`.
  Converging only the 3 in this slice would increase divergence; needs one uniform change
  across all 9 files.
- **Geo:** 0/16,159 rows pin, and that is correct here — a headline is not a physical
  thing and a country centroid would be fabricated. But confirm the downstream geocode
  stage (`geom_source`/`geom_at`/`geom_failed`) actually runs over `record_type='article'`,
  or this slice will never pin.

## Per-source structure

Consistent across all 270: `record_type=article`, `sub_source_id=NULL`,
`tags=["news","<section>","<country>","google-news"]`, `properties={"guid":…}` — uniform,
just thin.

Only `gnews_world_topics_1.c` was modified; no `core/` or `lib/` file was touched, and no
git commit/checkout/stash was run.
