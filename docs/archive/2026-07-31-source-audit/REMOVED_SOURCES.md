# Removed sources — evidence log

## Registry: 2,201 → 2,014 (−187)

| batch | n | note |
|---|---|---|
| pure portal-probe stubs | 53 | reachability only, no measurement possible |
| "key-gated" stubs with no implementation | 8 | no request beyond `probe_head`; a credential would change nothing |
| `bom-au-warnings` | 1 | replacement API's own response forbids use |
| unfixable scrapers | 5 | JS-shell or Akamai-blocked, see below |
| duplicate Google News feeds | 120 | empirically derived, see below |

**A trap worth knowing:** deleting a collector `.c` does **not** drop the source.
The stale `.o` keeps its `REGISTER_SOURCE` constructor in the link, and make
never relinks because no rule fires when a prerequisite *disappears*. Both have
to be forced — `tests/audit/prune_orphan_objs.sh` plus `rm bin/japanosint`. The
first removal batch silently had no effect until this was found.

## Duplicate Google News feeds — 120 retired, disable → backfill → drop

The duplicate map was derived **empirically, not from the audit prose**: Google
discards `gl=` for locales it has no edition for, and `hl=is` / `hl=fa` both land
on en-US, so the URL does not predict which sources collide. Every one of the
540 gnews feeds was fetched and grouped by its sorted guid set
(`gnews_dupes.py` → `gnews_dupes.json`). Result: **23 groups, 120 redundant
sources** (the audit's prose estimate was 129).

1. **DISABLE** — quarantined via `sources.quarantined_until`, the mechanism the
   scheduler already honours (`core/scheduler.c:129`). Stops new duplicate rows
   with no deploy and no code change, reversible through the existing
   `/api/admin/sources/<id>/unquarantine` route. Verified on a throwaway DB: 120
   quarantined, keepers untouched.
2. **BACKFILL** — every existing row from a retired source gets
   `properties.duplicate_of = <keeper>`, e.g.
   `{"guid":"x","duplicate_of":"gnews-no-world"}`. Rows are deliberately **not**
   re-keyed onto the keeper: `uid = source_id|guid`, so re-keying would collide
   with the keeper's own uids and destroy rows.
3. **DROP** — 120 RSSX definitions removed (48 from `gnews_world_topics_1.c`,
   72 from `gnews_world_topics_2.c`), each replaced by a comment naming the
   keeper. Keepers re-verified live afterwards: `gnews-no-world` 70 rows,
   `gnews-us-business` 57, `gnews-eg-science` 46.

## Mislabelled survivors — resolved: 6 retired, 5 renamed

Where no member's country code matched the edition Google served, the first pass
kept the alphabetically-first id, leaving 11 survivors labelled as a country
whose edition they do not serve. Each was re-probed and resolved on evidence
(`gnews_served.py` → `gnews_mislabel_actions.json`):

**Retired as duplicates (6)** — they serve an edition a *correctly named* source
already collects, so renaming them would have created a second id for the same
feed:

| retired | serves | already collected by | jaccard |
|---|---|---|---|
| `gnews-am-business` | ru | `gnews-ru-business` | 0.97 |
| `gnews-am-science` | ru | `gnews-ru-science` | 1.00 |
| `gnews-am-technology` | ru | `gnews-ru-technology` | 1.00 |
| `gnews-am-world` | ru | `gnews-ru-world` | 0.97 |
| `gnews-hr-health` | us | `gnews-us-health` | 1.00 |
| `gnews-hr-world` | us | `gnews-us-world` | 1.00 |

**Renamed (5)** — the `gnews-bo-*` / `gnews-cr-science` group serves `ceid=US`
but in **es-419** (Latin American Spanish) and shares *nothing* with the English
`us-*` feeds (jaccard 0.000). A real, distinct edition that belongs to no single
country — Bolivia was an arbitrary member of an 8-country group:

`gnews-bo-{world,business,health,technology}` and `gnews-cr-science`
→ **`gnews-es419-{world,business,health,technology,science}`**,
"Google News <topic> — Latin America (es-419)". `gnews-es-*` already exists and
is Spain (`hl=es-ES`), so there is no collision.

The rename migration rewrites `source_id` across **all 11 id-bearing tables**
*and* the `uid` prefix in `intel_items` (`uid = "<source_id>|<guid>"`) plus the
FTS uid maps. Verified end-to-end: `gnews-bo-world|guid123` →
`gnews-es419-world|guid123`, `fetch_log` moved too, 0 rows left pointing at the
old id.

> A bug worth recording: the first version of that migration reported "0
> id-bearing tables" and silently skipped everything except `intel_items`,
> because it ran `PRAGMA table_info` on the same cursor that was mid-iteration
> over `sqlite_master` — the inner execute destroys the outer result set. The
> test caught it. Without the test the rename would have orphaned `fetch_log`,
> `entity_mentions`, `collector_anomaly` and eight other tables.

## Second pass — 11 more duplicates

Exact guid-set equality is timing-sensitive: two fetches of a live feed seconds
apart can differ by one item and split a group. That is *how* the 6 above
survived as keepers. A second pass over the remaining 414 sources using
Jaccard ≥ 0.8 with single-linkage clustering found **8 more groups, 11 more
redundant sources** — including `gnews-bo-science`, which the keeper preference
correctly resolved in favour of the newly-renamed `gnews-es419-science`.

Retired: `lb-health`, `sa-science`, `bo-science`, `nz-science`, `ph-science`,
`pk-science`, `my-science`, `ng-science`, `dk-business`, `dk-technology`,
`kz-science`.

The first pass therefore under-counted, which is the safe direction — a group
only formed when sets matched exactly, so nothing was dropped that was not a
genuine duplicate.


Recorded so nobody re-adds these from the source list without knowing what was
already tried. Every removal below was probed live on 2026-08-01.

## The six "broken scrapers" — outcome: 1 fixed, 5 removed

I had kept these back from the probe-stub sweep on the grounds that they
contained real extraction code worth reviving. Probing each against its live
target showed that was true for exactly one of them.

### FIXED — `tenki-jp`

The homepage no longer contains `weather-telop` anywhere; the site moved to
`forecast-map-entry` anchors on the regional forecast pages. Rewrote the parser
to read those: city, weather telop (icon `alt=`), max/min temperature,
precipitation probability, and a per-city link.

**1 portal row → 14 real observations**, e.g.
`千代田区 晴のち曇 36℃/26℃`, properties
`{"city":"千代田区","telop":"晴のち曇","max_temp_c":36,"min_temp_c":26,"precip_prob":"40%"}`.

A guard rejects the `---` placeholders the page ships for blocks it fills in
client-side — a placeholder is not an observation.

### REMOVED — `jr-east-delay`

`traininfo.jreast.co.jp/delay_certificate/` returns 200 with 23.5 KB of HTML,
but every one of its 19 `delaycertificate-table__routename` cells is **empty**:
the values are injected client-side. The loader that fills them,
`/train_info/js/getinformation-jp-delaycertificate.js`, returns **403 Access
Denied** (Akamai). There is no static path to the delay data.

### REMOVED — `jr-west-delay`

`trafficinfo.westjr.co.jp` is a Next.js app. No `__NEXT_DATA__` payload and no
server-rendered status: the only occurrences of `遅延` in 91 KB of HTML are the
labels on a "遅延証明書" button and a banner image's `alt`. Nothing to parse.

### REMOVED — `jr-central-delay`

`traininfo.jr-central.co.jp/zairaisen/index.html` is a 5.8 KB shell whose entire
body is a language-detection redirect script. No content.

### REMOVED — `shinkansen-status`

`traininfo.jreast.co.jp/shinkansen/` returns **403 Access Denied** (Akamai) to
both the collector UA and a full browser UA.

### REMOVED — `bosai-volcano-cam`

This one was already honest about itself. Its header documents that JMA serves
volcano-camera imagery only through an interactive viewer with no key-free
machine-readable listing, and it deliberately refuses to derive pins from
volcano summit coordinates because observation cameras sit kilometres from the
summit — inventing a precision upstream does not publish.

That reasoning is correct and worth preserving: **do not re-add this source with
summit coordinates standing in for camera positions.** But a row that carries
only `reachable` is not data, so the source is gone. If a real listing appears
(a NIED V-net camera endpoint, or the JMA viewer's backing data), emit each
camera through `camera_upsert(db, sink, cam_make_feature(...), "jma_volcano_cam")`
so the rows land in the shared camera keyspace — not through
`geojson_emit_features`, which `camera_fc_json` cannot see.

## Earlier in the same pass

- **53 pure portal-probe stubs** — one row of `{operator, reachable}`, title and
  summary hardcoded in the .c, no measurement possible.
- **8 "key-gated" stubs with no implementation** — `intelx-leaks`,
  `psbdmp-pastes`, `securitytrails-history`, `strava-segments-jp`,
  `twitch-jp-streams`, `vrchat-active-jp`, `whoisxml-reverse`,
  `gitlab-bitbucket-leaks`. None makes any request beyond `probe_head`; a
  credential would not have changed their output.
- **`bom-au-warnings`** — feed 404s; the only live replacement API's own response
  metadata reads *"You must not use, copy or share it."*
- **`luup-private`** — kept and fixed, not removed: it has a real bbox table and
  a 165-line `run()` against a live API. Now logs its gate and emits nothing.
