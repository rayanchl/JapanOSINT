# Page-walker unification (s_walker)

Branch: worktree-agent-a74a89f11e4cf3c9f (merged `claude/peaceful-ritchie-i7e0hj` first, as instructed; baseline and all
before/after numbers were captured on the merged tree — the first baseline on the old base was discarded).

## Before: what each copy did (a–f)

| | jsonlist (VJSON) | geojson (VGEO) | pw_walk | hpengine |
|---|---|---|---|---|
| a declared total | 29-key dotted list, case-insens., + `d.__count` | 4 root keys (`count` too), >0 | 16 root keys + meta.total + result.count/totalResults; **no bare `count`** | JSON: jsonlist's; XML: own element list |
| b next page | links list (relative resolved), PAGERS paired size→cursor on a full page, page cursor on declared total, page-1 probe | exceededTransferLimit, rel=next verbatim, declared total + cursor by records held, `$limit/$offset` literal | absolute-only link list, unpaired size/offset/page lists, stride=declared size | page_param arithmetic, `{page}`, next_path; page_walk = jsonlist_next_page; own relative-link resolver + cross-host rule; own size list |
| c repeat guard | first-8-record fp, probe set test | n + first-8 fp, **emitted the re-served page before stopping** | whole-doc FNV | raw-body hash + jsonlist fp/fps |
| d ceiling | `JO_PAGE_MAX` / `JO_JSONLIST_PAGE_MAX` / 20 | **`JO_GEOJSON_PAGE_MAX` only, ignored `JO_PAGE_MAX`** | `jo_page_max_for` | `jo_page_max_for`, `jsonlist_page_max` |
| e later-page failure | any failure = cut | any failure = cut | any failure = cut | `hp_later_page_cut` + `hp_cut_reason` |
| f notice | hand-built, key `truncation`, tags plain | hand-built (fewer fields) | `jo_truncation_notice_ex` | hand-built, scoped key, window/cycle fields |

## Design (all in `native/lib/pagewalk.{c,h}`)

- a: `pw_declared_total(doc, flags)` — ONE table (`PW_TOTAL_KEYS`, union of all three lists, bare `count`/`meta.count` gated by
  `PW_TOT_COUNT`); `pw_declared_total_xml` (hpengine's XML list moved in); `pw_odata_v2_count` kept.
- b: `PW_PAGERS` (one pager table, per-row `uses` bits JSON/DECL/GEO/HP), one cursor table (`PW_CURSORS`), `pw_next_link`
  (union link list), `pw_resolve_url`, `pw_host_of`, `pw_same_host`, `pw_strip_secret` (cross-host rule), `pw_next_page`
  (= jsonlist_next_page), `pw_next_page_geo`, `pw_page_one_retry`, `pw_seed_cursor`, `pw_declared_size[_below]`.
- c: `pw_fnv_str`, `pw_text_hash`, `pw_body_fp`, `pw_page_fp`, `pw_page_fps`, `pw_fps_has`.
- d: `pw_page_ceiling(legacy_env, declared)`, `pw_ceiling_name`; `jo_page_max*` are wrappers.
- e: `pw_later_page_cut`, `pw_cut_reason`, `pw_cut_remedy`, `pw_failed_page_reason` (hp's functions moved in unchanged).
- f: `pw_notice_emit(sink, &pw_notice)` — every engine calls it; same property names; per-engine `avail_style`/`key_style`/tags/title.
- Loops: `pw_walk_list(PW_FLAVOR_JSON|GEO, …, ops)` replaces the jsonlist and geojson loops (both are now ~30-line drivers
  with fetch/records/emit hooks); `pw_walk_max` is the callback-flavoured loop. hpengine keeps its own loop and calls the core for
  a, b (page_walk decision, resolve/cross-host), c, d, e, f.
- `jsonlist_*` names remain as one-line aliases (hpengine, tests and macros unchanged).

## Lines (before -> after)
jsonlist.c 2169 -> 1400 (-769) · geojson.c 703 -> 478 (-225) · hpengine.c 6608 -> 6358 (-250) · pagewalk.c 666 -> 1785 (+1119, the core) ·
pagewalk.h +112 · tests: pagewalk_test.c +165, new tests/paging_core_single.py. Net lib/: -125 lines, and four copies became one.

## Disagreements between copies and how resolved
1. Bare `count`: pw refused, jsonlist/geojson accepted -> gated flag; each caller keeps its behaviour.
2. Declared-total precedence (when one doc carries two spellings): jsonlist's order kept (serves ~2,000 rows); lookup now case-insensitive for all; geojson `>0` became `>=0`.
3. Next-link list: union; precedence jsonlist's; an empty/unusable `next` no longer shadows a later usable link (pw hid it); union widens each engine slightly (e.g. VJSON now follows `nextLink`/`next_page_url`/`nextRecordsUrl`; pw_walk follows `nextUrl`, `meta.next`…).
4. geojson emitted a page it had just recognised as a re-serve (double-counting `emitted`) -> now checks before emitting like jsonlist (CLAUDE.md 2026-09-05).
5. geojson fingerprint mixed in the array length -> jsonlist's first-8 fp used.
6. geojson ignored `JO_PAGE_MAX` -> `JO_PAGE_MAX` now brakes it (legacy `JO_GEOJSON_PAGE_MAX` still honoured). Operator-visible.
7. Relative links: jsonlist resolved `?q`/`/p`; hp also `//host` and bare paths. One resolver; bare paths opt-in (`PW_RESOLVE_PATHS`, hp only) so a JSON `"next":"<opaque token>"` is never glued onto a base. `//host` now resolved for jsonlist (before it was mangled).
8. Pager/size lists: jsonlist PAGERS, pw's three lists, geojson's size/cursor lists, hp's `hp_url_declared_size` list -> one table with `uses` bits; where lists differed (rp, maxFeatures JSON-only; retmax, rpp DECL-only; Count/take/pagesize HP-only) the bits keep each engine's set exactly. Table order, not each list's order, decides when a URL carries two size parameters.
9. Wording: hp's "next-page link pointed back…" now uses the shared `PW_REASON_NEXT_LINK_SELF` (adds "…the same records"); VJSON/GEO notices unchanged (they already named `$JO_JSONLIST_PAGE_MAX` / `$JO_GEOJSON_PAGE_MAX`). No test assertion needed changing.

## Not unified (and why)
- `core/pagination_probe.c` keeps its own candidate pager list (core/ out of scope); recorded as a known exception in `tests/paging_core_single.py`.
- Notice schema differences kept on purpose (jocore.h says they are a data migration): unknown total = string for list walks vs null for pw; remote_key `truncation` (list walks) vs `<id>|truncation:<q>` (pw, hp) — changing either would orphan stored notices. The core builder takes `avail_style`/`key_style`.
- Failed-page reason: two wordings (`pw_failed_page_reason` plain, `pw_cut_reason` classified, hp only — it knows "unreadable 200").
- pw_walk (declared-size, unpaired advance) vs jsonlist (paired advance) vs geo (advance by records held) are three advance disciplines reading the same tables; merging them would change fetches.
- hpengine's own loop (page_param/`{page}`/page_resume/window_days/detail hops/date_back) stays.
- CLAUDE.md not edited (outside the allowed files); `tools/audit_source_exhaustiveness` regex `pw_walk\w*` already matches `pw_walk_list`, so no walker-name edit was needed.

## 40-source before/after (merged tree; baseline = untouched build, final = this commit; fresh DB per run)
Sweep note: docs/audit-2026-11-14/sweep.tsv holds 1,053 rows and only one hand-written pw_walk user, so the 10 PW rows are hand-written pw users not in the sweep (ods-*, ckan-*); 3 hp rows (ihsn x2, KKJ Toyama) are page_walk / window_days rows outside the "OK" set (Toyama is WINDOW_EMPTY by design).

SEE TABLE BELOW (records / stored / notices / requests, base/new)

| class | id | records b/n | stored b/n | notices b/n | requests b/n | verdict |
|---|---|---|---|---|---|---|
| VJSON | gr-diavgeia-dictionary-cpv | 10357/10357 | 10357/10357 | -/- | 1/1 | same |
| VJSON | vn-admin-wards | 10051/10051 | 10051/10051 | -/- | 1/1 | same |
| VJSON | si-podatki-gov-si-tags | 9156/9156 | 9156/9156 | -/- | 1/1 | same |
| VJSON | unhcr-asylum-applications | 6092/6092 | 6092/6092 | -/- | 7/7 | same |
| VJSON | bh-quarterly-fdi-stocks | 4460/4460 | 4460/4460 | -/- | 1/1 | same |
| VJSON | ifrc-go-district | 3553/3553 | 3553/3553 | -/- | 18/18 | same |
| VJSON | il-patent-applicants | 2000/2000 | 2002/2002 | 2/2 | 20/20 | same |
| VJSON | au-apvma-pubcris-products | 2000/2000 | 2002/2002 | 2/2 | 20/25 | DIFF |
| VJSON | il-private-investigators | 1561/1561 | 1562/1562 | 1/1 | 16/16 | same |
| VJSON | my-flood-warning-stations | 1276/1276 | 1276/1276 | -/- | 1/1 | same |
| VGEO | afr-grid3-grid3-cod-schools-v1-0-grid3-cod-schools-v1--0 | 10000/10000 | 10001/10001 | 1/1 | 20/20 | same |
| VGEO | nam-edmonton-traffic-disruptions | 1946/1946 | 1946/1946 | -/- | 5/5 | same |
| VGEO | sci-stac-inpe-mod13q1-6-1 | 300/200 | 301/201 | 1/1 | 8/5 | DIFF |
| VGEO | nam-ags-maine-stream-habitat-viewer-dams | 973/973 | 973/973 | -/- | 2/2 | same |
| VGEO | nam-ags-usa-current-wildfires-california-current-incidents | 286/286 | 286/286 | -/- | 1/1 | same |
| VGEO | nam-michigan-map-annual-hospitalization | 747/747 | 747/747 | -/- | 2/2 | same |
| VGEO | nam-texas-texas-commission-on-environmental-qualit-2 | 3139/3139 | 3139/3139 | -/- | 8/8 | same |
| VGEO | id-jakarta-emission-test-firms | 269/269 | 269/269 | -/- | 1/1 | same |
| VGEO | id-jakarta-sip-occupancy | 1170/1170 | 1170/1170 | -/- | 2/2 | same |
| VGEO | id-jakarta-imb-building-permits | 13137/13137 | 13137/13137 | -/- | 14/14 | same |
| PW | ods-be-namur | 294/294 | 294/294 | -/- | 3/3 | same |
| PW | ods-ch-basel | 364/364 | 364/364 | -/- | 4/4 | same |
| PW | ods-be-brussels | 206/206 | 206/206 | -/- | 3/3 | same |
| PW | ods-ch-thurgau | 470/470 | 470/470 | -/- | 5/5 | same |
| PW | ods-fr-rennes | 673/673 | 673/673 | -/- | 7/7 | same |
| PW | ckan-ch-zurich | 942/942 | 942/942 | -/- | 5/5 | same |
| PW | ckan-energydata-info | 1191/1191 | 1191/1191 | -/- | 6/6 | same |
| PW | ckan-london-datastore | 1310/1310 | 1311/1311 | 1/1 | 2/2 | same |
| PW | ods-paris-velib | 1522/1522 | 1522/1522 | -/- | 16/16 | same |
| PW | fedidb-software | 618/618 | 618/618 | -/- | 1/1 | same |
| HP | OR_ACTIVE_NONPROFIT_CORPORATIONS | 20000/20000 | 20001/20001 | 1/1 | 20/20 | same |
| HP | FAA_NASR_PENDING_RUNWAY | 20000/20000 | 20001/20001 | 1/1 | 20/20 | same |
| HP | UK_HYDROLOGY_MEASURES | 10000/10000 | 10001/10001 | 1/1 | 20/20 | same |
| HP | W3C_SPECIFICATION_SERIES | 1495/1495 | 1495/1495 | -/- | 15/15 | same |
| HP | PSE_CRB_ROZL | 1000/1000 | 1001/1001 | 1/1 | 22/20 | DIFF |
| HP | ihsn-microdata-sn | 50/50 | 50/50 | -/- | 51/51 | same |
| HP | ihsn-microdata-mu | 50/50 | 50/50 | -/- | 51/51 | same |
| HP | JP25_JPBIZ_KKJ_PREF_16_TOYAMA | 0/0 | 1/1 | 1/1 | 1/1 | same |
| HP | CO_CONTRALORIA_FUNCIONARIOS | 6636/6636 | 6636/6636 | -/- | 8/8 | same |
| HP | JO40_CNSAR_HKMA_HIBOR_DAILY | 7000/7000 | 7001/7001 | 1/1 | 8/8 | same |

DIFF rows: 3


3 rows differed on the first pass; all three are live variance, not behaviour: `sci-stac-inpe-mod13q1-6-1` flaps with the SAME binary
(base alone gave 200, 1, 300 records across runs; new gave 200, 500, 100; every run ends "a later page failed"), `au-apvma-pubcris-products`
and `PSE_CRB_ROZL` were 20/20 requests for both binaries on re-run (first-pass 25 / 22 were retries against a flaky host).
Every other source: identical records, stored, notices and requests. Re-run evidence: `scratchpad/rerun3.txt`.

## Tests (all in `native/`, `JO_DB=$PWD/.scratch.db`, `make -j2`)
- `make -j2` full build: 0 warnings (`grep -c "warning:"` of both build logs = 0).
- `make hptest` all passed (incl. LIB-03a/b/c/d cross-host + relative-link cases from the audit branch) — unchanged assertions.
- `make pagewalktest` all passed — existing assertions unchanged; ~50 new cases (declared-total union incl. gating, link reader/resolver/same-host, ceiling, later-page ruling, pager table, notice fields, `pw_walk_list` JSON flavour: link walk, repeat guard + declared-short notice, ceiling brake, failed page) and it now also runs `tests/paging_core_single.py`.
- `make htmlparsetest`, `make rsstest` all passed.
- `tests/paging_core_single.py` (fails if a second declared-total list / pager table / next-link list appears in lib/ or core/; self-tested; `core/pagination_probe.c` exempted by name).
- `make lint-sources` OK; `make audit-sources`: strict set 397 files 0 findings (one pre-existing finding in collectors/sources/bird_flu_outbreaks.c, untouched, outside the gated set).
- Not run: `make unit` (84 tests, ~8 min) and TSan/ASan.
