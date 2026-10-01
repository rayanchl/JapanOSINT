# Slice 02 — 270 sources

All 270 live in one file, `native/collectors/sources/gnews_world_topics_2.c`, which
contains **no logic**: every source is one `RSSX(...)` macro whose `run()` is a single
call to `rss_collect()` in **`native/lib/rss_atom.c`**. So every defect is either in
`lib/rss_atom.c` (READ-ONLY for this slice) or a fact about what Google actually serves.

**Method:** all 270 re-run against the `a1` binary, one throwaway DB each, reading
*every* persisted row as raw bytes (`sqlite3 text_factory=bytes`) so bad UTF-8 could not
abort the read the way it aborts `verify_one.py`. Then all 270 upstream feeds re-fetched
to compare Google's response against what reaches `intel_items`.

## Summary

| verdict | count |
|---|---|
| DATA | 199 |
| DB_ERROR (invalid UTF-8 persisted) | 63 |
| EMPTY (honest — upstream returns 0 items) | 8 |
| KEY_GATED / WAF_BLOCKED / RC_ERROR / TIMEOUT / CRASH | 0 |

**Geo: 0 of 270 sources emit any lat/lon or geometry.** The bulk sweep reported 8
DB_ERROR; the real number is **63** — the sweep only decodes the first rows it dumps.

Five most important findings, most severe first:

1. `lib/rss_atom.c:172-173` truncates `summary` at 240 *bytes* with no UTF-8 boundary
   check → 404 corrupt rows across 63 sources per cycle.
2. `lib/rss_atom.c:165-170` builds `properties_json` in a fixed `char props[512]` and
   `snprintf`-truncates it → malformed JSON on 2,421 rows over 177 sources per cycle.
3. `published_at` persisted as raw RFC-822 → text-sorts ahead of every ISO-8601 row in
   the whole database.
4. 75 of 270 sources are byte-identical duplicate feeds; 9 of those are mislabelled.
5. Payload is a link list, not article content; the publisher field upstream provides is
   dropped.

## Fixes applied

None — and that is the correct outcome.

Every defect is in `lib/rss_atom.c`, and **all 270 sources need the same change**; the
brief's explicit instruction for that case is to report, not patch. The slice's own file
is a pure declaration table. All 270 `RSSX(...)` expansions were statically verified:
**0 duplicate ids, 0 duplicate symbols, 0 duplicate URLs, 0 mismatches** between each
id's country/topic and its URL's `gl=`, `ceid=`, topic path, `lang`, `tags_json`,
category, collector, interval. There is no edit to that file that fixes anything below
without fabricating data.

## Findings not fixed (with reason)

### 1. `summary` truncated at 240 bytes mid-codepoint — `lib/rss_atom.c:172-173`

404 rows across 63 sources in one cycle. Corrupts `intel_items.summary` *and* the FTS
mirror (`core/intel.c:214` feeds the same buffer to `fts_write`); any API read raises
`Could not decode to UTF-8`. Predicted from the raw feed and confirmed exactly — 21 bad
rows predicted for `gnews-jp-science`, the DB had 21/57.

**Not script-specific**: the en-US feed produced a corrupt row too, so all 199 "DATA"
sources are one unlucky headline away from the same fault.

```c
      char summ[256] = {0};
      if (desc) {
        size_t L = strlen(desc);
        if (L > 240) {
          L = 240;
          while (L > 0 && ((unsigned char)desc[L] & 0xC0) == 0x80) L--;
        }
        memcpy(summ, desc, L); summ[L] = 0;
      }
```

That stops the corruption, but the summary is still a 240-byte prefix of raw HTML —
`<ol><li><a href="https://news.google.com/rss/articles/CBMi…` with zero readable text.
Worth replacing with text via `lib/htmlparse.c`.

*Why not fixed:* `lib/` is read-only for this slice and the fix is shared by every
`rss_collect` caller in the repo.

### 2. `properties_json` truncated into malformed JSON — `lib/rss_atom.c:165-170`

Google News guids reach 1,930 chars on Arabic editions; the buffer is `char props[512]`.
Measured directly in the DB: 33/61 rows for `gnews-lb-health`, 31/48 for
`gnews-sa-science` end mid-base64 with no closing `"}`. Slice-wide: **2,421 rows over
177 sources per cycle.** Any `cJSON_Parse` of `properties` fails.

Fix: heap-allocate from `cJSON_PrintUnformatted` instead of copying into a fixed buffer.

*Why not fixed:* same read-only `lib/` reason.

### 3. `published_at` persisted as raw RFC-822 — leaks well beyond this slice

`lib/rss_atom.c:180` (its own comment concedes `/* RFC822→ISO norm: P5 refine */`);
`core/intel.c:197` binds it verbatim. Stored value: `"Fri, 31 Jul 2026 16:16:00 GMT"`.

Every consumer sorts it as **text** — `exportapi.c:494,499`, `intelapi.c:293,302`,
`simhash.c:552,655`, `schema.sql:598`. So ordering is by weekday name; and since RFC-822
starts with a letter and ISO-8601 with a digit, in a `DESC` sort **every one of the
~12,000 gnews rows outranks every correctly-dated row from every other collector.**

*Why not fixed:* `lib/` + `core/` are read-only here. Worth checking whether other
RSS-backed slices hit this too.

### 4. 75 of 270 sources (28%) are byte-identical duplicates; 9 are mislabelled

Google ignores `gl=` for topic sections when the edition doesn't exist and silently
serves `hl=en-US&gl=US` — it says so in the channel header (`<language>en-US</language>`,
`<link>…ceid=US:en</link>`), which the collector ignores.

`gnews-ir-technology` is registered "Google News Technology — Iran", tagged `"iran"`,
stamps `language="fa"` on every row, and returns *"First Googlebooks from Lenovo leak —
9to5google.com"*. Same for `hr` and `is` (9 sources). The other 66 share a language-wide
edition:

- 8 Arabic → one `hl=ar` feed: `dz eg iq jo kw ma qa tn` (×3 topics)
- 8 Spanish → one `es-419` feed: `bo cr do ec gt pa py uy` (×3 topics)
- English `kh lk mm np`; Russian `am az kz ru uz`; `dk`+`no`

Because `uid = source_id|guid`, each group multiplies every article into N rows hourly.
Recommend retiring the redundant 75.

*Why not fixed:* `hl=fa&gl=IR&ceid=IR:fa` *is* the correct documented URL; Google falls
back on its own. Retiring 75 registered sources is a product decision, and
`core/source_registry.gen.c` is generated.

### 5. Payload is a link list, not article content; publisher is thrown away

`body` is Google's raw HTML `<ol><li><a href="…base64…">` block (1–2 KB of opaque
redirect URLs per row) — no article text, because Google supplies none. Meanwhile
**every** item carries `<source url="https://www.9to5google.com">9to5Google</source>`
(70/70 items checked) and `rss_collect` never reads it, so publisher name and real domain
are both dropped while `author` is NULL on all 270 sources. `link` is a Google redirect,
never the article's true URL.

This is the "labels, not data" class — honest about what upstream gives, but thinner than
it looks.

### 6. Zero geo on all 270

News articles have no upstream coordinates. Pinning to a country centroid would be
inventing coordinates — forbidden. Needs real geocoding of article text if pins are
wanted.

### 7. Language-code divergence (flagged only)

`gnews-il-*` use `lang="iw"`, the deprecated code for Hebrew, while
`collectors/sources/reg_il_data.c:117` already uses `"he"`. Safe in isolation, but `"iw"`
appears identically in `gnews_world_topics_1.c`, `gnews_world_top.c`,
`gnews_osint_monitors.c` and is emitted by the generator
`collectors/gen_world_sources.py:86,212` — none in this slice. Changing 3 of 10 would
deepen the divergence; needs a global pass including the generator.

`gnews-cn/tw/hk-*` all declare `lang="zh"`, losing Hans/Hant. Cosmetic.

## The 8 EMPTY are honest — do not "fix" them

`ee-science ee-technology fi-science fi-technology lt-science lv-health lv-science
th-science`

Upstream returns HTTP 200 with a ~1.2 KB channel containing **zero `<item>` elements**;
`rss_collect` returns `n=0` and `run()` returns `0`, not `-1`, so the anomaly detector is
correctly not tripped.

## The 63 DB_ERROR sources

`ae-health ae-science am-science az-science bd-health bg-science bg-technology cn-health
cn-science cn-technology cz-science dz-science dz-technology eg-science eg-technology
hk-health hk-science hk-technology hu-science hu-technology il-health il-science
il-technology iq-science iq-technology it-technology jo-science jo-technology jp-health
jp-science jp-technology kr-health kr-science kr-technology kw-science kw-technology
kz-science lb-health lb-science lb-technology lt-technology ma-science ma-technology
pt-health qa-science qa-technology rs-technology ru-science sa-health sa-science
sa-technology sk-technology th-health th-technology tn-science tn-technology tr-science
tw-health tw-science tw-technology ua-science ua-technology uz-science` (all `gnews-`
prefixed)

Worst ratios: `th-health` 18/29, `tw-science` 10/38, `tw-technology` 10/46, `uz-science`
9/49, `th-technology` 9/70, `jp-science` 21/57, `jp-technology` 24/67.

## Bottom line

None of these 270 sources is dead, key-gated, blocked or crashing — the fetch layer
works. What is broken is everything between the fetch and the row: a byte-truncation that
corrupts 404 rows, a fixed buffer that malforms 2,421 JSON payloads, a date format that
mis-sorts the entire global feed, a dropped publisher field, and 75 sources that are the
same feed under 75 names. Four of those five are single-function fixes in
`lib/rss_atom.c` and would benefit every `rss_collect` caller in the repo.
