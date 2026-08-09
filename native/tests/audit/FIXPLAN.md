# Fix plan — OSINTsaas source audit, 2026-07-31

## FINAL OUTCOME — 2026-08-01, like-for-like on the 1,980 sources common to both runs

Pre-fix baseline (19:26, before any change) vs final (330 s harness timeout,
matched to the new Overpass budget):

| verdict | pre-fix | final | Δ |
|---|---|---|---|
| DATA | 1113 | **1236** | **+123** |
| RC_ERROR | 345 | 292 | −53 |
| EMPTY | 418 | 419 | +1 |
| NO_TITLE | 30 | **3** | −27 |
| TIMEOUT | 56 | 29 | −27 |
| DB_ERROR | 17 | **1** | −16 |
| CRASH | 1 | **0** | −1 |

**rows 194,576 → 491,303 (+296,727, 2.5×)   geo 144,833 → 432,725 (+287,892, 3.0×)**

Transitions into DATA: 79 from RC_ERROR, 62 from EMPTY, 26 from NO_TITLE, 16 from
DB_ERROR.

Two counter-movements, both understood and neither a defect:

- **DATA → EMPTY (49)** — ~40 are arXiv. Their feed for `Sat, 01 Aug 2026` ships
  `<skipDays>` and zero items; arXiv does not publish at weekends. Identical for
  the old UA and plain curl, so upstream, not us. `rss_collect` correctly returns
  0, so they read EMPTY rather than quarantining.
- **DATA → RC_ERROR (23)** — Overpass mirror contention. The sweep runs 10
  queries in parallel against shared public mirrors and starves its own requests;
  the sources that win the race differ run to run. `[overpass] … no endpoint
  answered` after 90–150 s is the honest signal, and it is a *harness* artifact:
  production runs a single scheduler process, not ten concurrent probes.

**The harness timeout was itself distorting the picture.** At 120 s (shorter than
the 300 s Overpass budget set in Phase 3) every Overpass source read TIMEOUT
whether or not it worked. Re-running at 330 s moved TIMEOUT 83 → 29 and added
**+256,183 rows / +255,482 geo** in one change. Any future sweep must use a
timeout ≥ the Overpass budget or it will understate coverage by roughly the size
of the Overpass fleet.

**3 NO_TITLE are newly *visible*, not new.** `odpt-station` (9,220 rows),
`vending-machines` (13,904) and `soramame` (151) always emitted title-less rows;
they were masked because the source timed out before persisting anything. Real
findings, now surfaced.

---

## Earlier checkpoint — re-swept 2026-07-31 23:0x

| verdict | pre-fix 19:26 | post-fix 23:0x | Δ |
|---|---|---|---|
| DATA | 1287 | **1468** | **+181** |
| EMPTY | 425 | 377 | −48 |
| RC_ERROR | 312 | 253 | −59 |
| TIMEOUT | 96 | 89 | −7 |
| NO_TITLE | 29 | **1** | −28 |
| DB_ERROR | 39 | **1** | −38 |
| CRASH | 1 | **0** | −1 |

Same 2,189 sources, same harness, same machine. Phase 4 geo verification confirms every
fabricated-pin removal now persists as absent geometry rather than the string `"null"`:
`peeringdb-jp` 1139 rows / **56** geo (facilities only, was 1139/1139 invented),
`spamhaus-drop` 2085 / **0**, `cisa-kev-jp` 46 / **0**, `poc-in-github` 94 / **0**,
`mastodon-jp-instances` 51 / **0**, `nws-alerts-us` 302 / **36** real polygons.
Sources that *should* pin still do: `jma-intensity` 89/89, `wolfx-eqlist` 50/50,
`flight-adsb` 124/124.

Not executed, by decision — see `DECISIONS.md`: retiring the 129 duplicate Google News IDs
and the source renames (both orphan existing rows/alerts — migration, not fix), pivot-type
registry field (feature), 73 portal-probe rewrites (product voice), `bom-au-warnings`
(declined: its replacement API forbids use).

---


Ordered by (blast radius × confidence) ÷ risk. Phases 1–2 are the ones no auditor was
allowed to touch, and several of them **gate** work already done: e.g. the fake-pin removals
in Phase 4 silently regress until `lib/geojson.c` stops serialising JSON null.

Every phase ends with `tests/audit/build_check.sh` plus a named re-run that proves the fix on
a real source.

---

## Phase 1 — shared-code one-liners — ✅ DONE 2026-07-31 22:15, all four verified live

All in `core/` + `lib/`, all single-expression changes, each independently verified by 2+
auditors.

| # | file:line | change | proves it | unblocks |
|---|---|---|---|---|
| 1.1 | `core/httpclient.c:129` | add `CURLOPT_NOBODY` for HEAD | any portal probe returns `reachable:true` where `curl -I` returns 200 | 73 portal-probe sources, each currently burning 8 s |
| 1.2 | `core/httpclient.c:123` | `CONNECTTIMEOUT_MS = min(timeout_ms, 20000)` | `GDELT_TV` gets past the 10.8 s TLS handshake | GDELT_TV, NEWS_AGGREGATOR |
| 1.3 | `lib/geojson.c:235` | skip `cJSON_IsNull(geom)` | `nws-alerts-us` stores NULL, not `"null"` | every GeoJSON source; **prevents Phase 4 regressing** |
| 1.4 | `core/camera_store.c:232` | set `it.link = m_url`, `it.published_at = m_first` | `cam-camscape` rows carry an openable URL | all 14 camera sources fleet-wide |

**Risk**: 1.1 changes HEAD semantics — verify no caller depends on reading a HEAD body.
1.4 uses `m_first` (stable first-seen) rather than "now", so the timeline doesn't churn on
every poll.

### Verified results

| # | before | after |
|---|---|---|
| 1.1 | `nied-mowlas` → `reachable:false`, ~8,500 ms (the probe timeout) | **`reachable:true`, 1,962 ms** |
| 1.2 | `GDELT_TV` unreachable — died at 10,813 ms with `status=0` | **`rc=0, records=25`, 13,653 ms** |
| 1.3 | `nws-alerts-us` 243/256 rows held the string `"null"` in `geometry` | **284 rows, 28 with geometry** — only the real polygons |
| 1.4 | `cam-camscape` 32/32 rows with NULL `link` | **32/32 carry the camera page URL** + `published_at` |

Note 1.2 surfaces the Phase 6 item immediately: GDELT_TV now trips `duration_outlier`
(13,653 ms > 10,000 ms) because it finally does real work. Raise that threshold with Phase 6,
not before — it is currently the only thing flagging genuinely slow sources.

## Phase 2 — `lib/rss_atom.c` (one file, ~800 sources downstream)

| # | line | change | proves it |
|---|---|---|---|
| 2.1 | `:172-173` | walk back off UTF-8 continuation bytes before terminating `summ` | `gnews-jp-science` reads back clean; the 63-source DB_ERROR set clears |
| 2.2 | `:165-171` | keep `cJSON_PrintUnformatted`'s heap string instead of `snprintf` into `props[512]` | `gnews-lb-health` properties parse as JSON |
| 2.3 | `:180` | normalise RFC-822 → ISO-8601 before assigning `published_at` | a mixed query orders by real date |
| 2.4 | `:157` | `tag_text()` nesting-aware, or unwrap `<name>` | `saigai-info` author is `山形地方気象台`, not `<name>…</name>` |
| 2.5 | — | unwrap CDATA | China Daily titles stop persisting as `<![CDATA[…]]>`; 100 emitted → 100 persisted |
| 2.6 | — | max-items cap (config, default ~500) | `msrc-blog` stops pulling 4,995 items / 3,539 rows every run |
| 2.7 | `:119` | UA → descriptive string + contact URL | ReliefWeb 406 clears **without** browser spoofing |

**2.3 is the highest-value single change in the audit** and the most delicate: it rewrites a
column three consumers sort on. Sequence: add the parser, backfill existing rows in a
migration, *then* switch the write path — otherwise the DB holds two formats at once and
ordering gets worse before it gets better.

## Phase 3 — `lib/overpass.c` + the ENV_BLOCKED correction

| # | change | proves it |
|---|---|---|
| 3.1 | overall time budget across all 4 endpoints (not per-endpoint) | `airport-infra` returns inside budget instead of 144 s |
| 3.2 | return `0` for an honest empty, `-1` only on transport failure | Overpass sources stop opening `status_bad` anomalies |
| 3.3 | `seen_has()` → hash set instead of linear scan over 200k slots | large tiled queries stop being O(n²) |
| 3.4 | raise the client budget past 60 s | slice 06 proved endpoints answer at 100–162 s |
| 3.5 | **re-run all ~90 ENV_BLOCKED sources** | the IP-ban theory was wrong; most should return data |

`overpass.osm.ch` must **not** be added to `ENDPOINTS` — it is a Switzerland-only extract and
returns 0 elements for a Tokyo query.

## Phase 4 — verify the fabricated-geo removals actually landed

Nine sources had invented coordinates removed. Because of defect 1.3 some replaced the pin
with a JSON null that persisted as the string `"null"`. After Phase 1.3, re-run and assert
`geometry IS NULL`:

`peeringdb-jp`, `cisa-kev-jp`, `poc-in-github`, `classifieds`, `mastodon-jp-instances`,
`trickest-cve`, `osm-changesets-jp`, `flight-adsb`, `spamhaus-drop`, `feodo-tracker-jp`,
`npa-missing-persons`, `wolfx-eqlist`, `nws-alerts-us`.

Then decide `hudson-rock-jp` (34 rows on one Tokyo point) and `npa-special-fraud` (25
national statistics pinned at NPA HQ).

## Phase 5 — `_jp_osint.inc` (134 collectors)

| # | change |
|---|---|
| 5.1 | charset sniff + Shift_JIS→UTF-8 transcode in `jo_emit_anchors()` (removes the per-collector workaround in `gov_money.c`) |
| 5.2 | left-trim anchor text (currently right-trim only — UK registry titles carry ~40 leading spaces) |
| 5.3 | stop hardcoding `it.lang = "ja"` |
| 5.4 | audit the 26 `href_must=NULL` call sites across 18 files — that is what filed `本文へ移動` as a financial penalty |

## Phase 6 — quarantine-on-success sweep-back

17 fixed. Remaining work is the *latent* form: `return n > 0 ? 0 : -1` quarantines on an
honest empty. Grep the fleet for `? 0 : -1` and split "fetch failed" from "fetch OK, nothing
to emit" per collector. Known: `data-go-jp-ckan`, `egov-laws`, `hatena-bookmark-extended`,
`japan-reit`, `jma-earthquake`, `jshis-seismic`, `sakura-front`.

Also raise the `duration_outlier` threshold (10 s) or make it per-source: `wifi-hotspots-
freespot` (74 s), `suumo-rental-density` (87 s), `jaxa-earth` (47 s), `SUBDOMAIN_FINDER`
(279 s), `SSL_ANALYZER` (59 s), `wayback-jp` (47 s) are all legitimately slow.

## Phase 7 — registry / product decisions (need you)

| decision | scope |
|---|---|
| retire the 129 duplicate Google News sources | Google serves one generic edition for locales it doesn't publish; each article stored 7–8× under 7–8 country tags |
| declare a **pivot type** per on-demand source in the registry | ~50 false EMPTY verdicts came from the sweep guessing the wrong entity kind |
| build out / retire / re-label the ~73 portal-probe sources | they count as DATA while carrying no measurement |
| `bom-au-warnings` | only live replacement API says *"You must not use, copy or share it."* |
| rename `police-crime` (emits police *stations*, registered as `crime`) | |
| `record_type` NULL on 14+ sources | rows unclassifiable by API/UI |
| dedupe pairs | `kyiv-independent`/`-2`, `scmp-china`/`-china3`, `the-diplomat`/`-2`, `google-my-maps`/`famous-places`, `DNS_RECORDS`/`DOH_RESOLVE` |

## Phase 8 — dead upstreams (~70 sources)

Auditors already found and verified replacements where they exist. The remainder split into:
genuinely retired (`nato.int` has no RSS at all; `BGPVIEW`/`PATENTSVIEW` NXDOMAIN; `tuoitre`
TLS expired), WAF-blocked to any UA (~50), and key-gated successors (MLIT reinfolib,
USPTO ODP). Each needs an individual call: repoint, retire, or accept as gated.

## Phase 9 — re-sweep and compare

Re-run `sweep.sh` over all 2,189 and diff against tonight's `results_*.jsonl` to confirm the
verdict mix moved in the right direction and nothing regressed. Do this from an IP that has
not been rate-limited — tonight's sweep earned throttles on PeeringDB, Blockchair and Stooq.

---

## Sequencing note

Phases 1 and 2 are independent of each other and of 3. Phase 4 **must** follow 1.3. Phase 9
must be last. Phases 7 and 8 are decision-gated, not code-gated.
