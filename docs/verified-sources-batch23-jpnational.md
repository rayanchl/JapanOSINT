# Batch 23 — beat `jpnational` (Source Agent J1): 127 rows

Japanese NATIONAL government and public-service data that was not yet in the
tree, authored against a fence of 3,796 URLs (every `*.jp` URL registered in
`native/collectors`, `lib`, `core` plus every row of the batch-23/24/25 sibling
manifests and the batch-25 review files):

* **National Diet Library APIs and feeds (39 rows)** — the SRU 1.2 `searchRetrieve` API
  as a `{q}` pivot returning full DC-NDL records (different API from the
  registered OpenSearch endpoint); **OAI-PMH `ListRecords` per state organ**
  (21 sets read from `ListSets`: the Diet, both houses, the Board of Audit,
  the Cabinet Secretariat, the National Personnel Authority, the Cabinet
  Office, twelve ministries, the Courts and the Incorporated Administrative
  Agencies — the NDL's harvest of what each institution publishes online,
  `from=2025-01-01`, resumption-token walk); the **Collaborative Reference
  Database** (`crd.ndl.go.jp`) API as two `{q}` pivots (reference cases and
  research guides) and its new-cases RSS; NDL Lab's announcements feed; 14 NDL feeds (Research Navi, Mina Search, NDL Search service news, library / publisher / general / accessibility news, the International Library of Children's Literature, the four Research and Legislative Reference Bureau series) that became shippable once the core session's XML fix decoded their entity-encoded titles;
* **JMA (6)** — the 120-volcano master register, and the five all-stations
  "latest observations" CSVs (1 h / 24 h precipitation, daily max / min
  temperature, daily max wind) behind the `stats/data/mdrr` ranking tables:
  1,284 / 914 stations each, Shift_JIS transcoded, refreshed hourly;
* **e-Stat Statistics Dashboard `getData` (17)** — national time series that
  need no appId: population, foreign residents, births, unemployment rate, job
  openings ratio, industrial production, bankruptcies, M2, Nikkei 225, CPI
  (level and y/y, 2025 base), GDP in USD, housing starts, penal-code offences,
  traffic accidents, suicides, exports — 693 to 2,086 values each;
* **e-Gov data portal walked per ministry (23)** — `package_search?fq=organization:org_XXXX`
  for every publishing organisation on `data.e-gov.go.jp` (13 to 2,479
  datasets each), which the registered portal-wide listing and keyword pivot
  do not attribute;
* **MOE WBGT forecast file (1)** — the heat-index forecast for all 840 stations;
* **agency and ministry feeds (35)** — PMDA (14 of the 19 feeds that carry
  items), MHLW pandemic-influenza (3) and emergency information, FDMA (4), FSA
  procurement, SESC reports, CAA, Customs, JETRO events, JICA, NICT (2), BoJ
  statistics (EN), NIMS, JPX (4);
* **HTML release indexes (5)** — the CAA recall site's three result listings, the
  MOD notices index, the MAFF press index (Kokusen passed every gate here but
  was taken by the jppolice beat meanwhile; yielded).

Manifest (the source of truth): `docs/candidate-sources-batch23.jpnational.txt`.
Generated table: `native/collectors/pivot/table/hp3b23_jpnational.c` (127 rows,
`gen_hp_batch.py --prefix hp3b23 --batch 23`, never hand-edited). Rejects as
data: `docs/rejected-sources-batch23-jpnational.tsv` (58 rows).

Every row was fetched over the wire from the WSL build host, parsed in its
declared mode, run through the **real binary** (`audit_batch_emit.py`), run
again against a **fresh per-row scratch database** (`audit_registry_emit.py`,
house rule 4b), and — beyond what the tools check — had its **stored titles
read back out of a scratch database** for every fast row (92 rows), which is
where 18 rows that had passed every gate were caught (below).

## Attrition

| stage | in | out | dropped |
| --- | --- | --- | --- |
| pre-screen over the wire (230 URLs: known API endpoints + 110 agency homepages for RSS autodiscovery; then 58 discovered feeds; then 46 feeds harvested from 13 "RSS index" pages; then 48 targeted checks) | ~380 URLs | 133 rows authored | 404 guesses (MLIT/BoJ/MOF/MOE/NRA/e-Stat feeds, 12 KSJ GeoJSON paths), DNS (`registry-catalog.registries.digital.go.jp`, `carinf.mlit.go.jp`), 403 (MOFA, MOD root, JAXA feed dir), TLS (`www.gsi.go.jp` legacy renegotiation), empty feeds (5 PMDA, 2 JPX, SESC other, current.ndl), bare-string CKAN registers on data.go.jp, Shugiin bill lists whose anchors read 本文, e-Gov `lawlists/2-4` (subsets of the registered `lawlists/1`) |
| `probe_hp_batch.py --check-filter` | 133 | 131 | OAI set B00007 `noRecordsMatch`; METI Atom 403 to the engine UA |
| `batch_exclusions.py` | 131 | 129 | NAOJ Atom, JAXA press — `DUP-ENDPOINT` (`vsrc2_space_1.c`) |
| pagination, reachable | 129 | 129 | — |
| `audit_batch_emit.py` | 129 | 129 | `OK=128 EMPTY_UPSTREAM=1` (the CRD manual pivot, which the tool runs without an entity) |
| rule 4b + title read-back | 129 | 111 | 14 NDL feeds whose titles are numeric character references stored undecoded; 4 feeds whose CDATA titles are stored as `feed-item <link>` fallbacks; 7 `COLLISION` rows re-keyed (not dropped); 1 SRU pivot re-keyed |
| **after the core session's XML fix** (CDATA unwrapped, character references decoded): the 18 restored, worktree re-rsynced, clean rebuild, `batch_exclusions.py` re-run against the 16,248-id registry | 129 | **127** | `JPN_IPA_NEWS_ONLY_RSS -> already in hp3b23_jpcyber.c`, `JPN_KOKUSEN_NEWS_RELEASES -> already in hp3b28_jppolice.c` (taken by sibling beats meanwhile; yielded) |

Registry: 16,248 → **16,375** seeds in the built binary (`lint-sources` counts
`REGISTER_SOURCE` only and stays at 10,439).

## Gate results (quoted)

| gate | result |
| --- | --- |
| `gen_hp_batch.py ../docs/candidate-sources-batch23.jpnational.txt --outdir /tmp/gen23j1c --prefix hp3b23 --batch 23` | `127 manifest rows` → `/tmp/gen23j1c/hp3b23_jpnational.c 127 rows` / `total 127 rows`; no duplicate opts, no swallowed `\;` |
| `probe_hp_batch.py --jobs 4 --check-filter` (first authoring, 133 rows) | `131 PASS`, `JPN_NDL_OAIPMH_ONLINE_PUB_B00007 UNPARSEABLE` (an OAI `noRecordsMatch` error document), `JPN_METI_RELEASES_ATOM HTTP_ERR Forbidden`. The three `{q}` pivots all `PASS` with `--check-filter`: `JPN_NDL_SEARCH_SRU_PIVOT xml:record 100`, `JPN_NDL_CRD_REFERENCE_PIVOT xml:result 200`, `JPN_NDL_CRD_MANUAL_PIVOT xml:result 185`. Measured by hand as well: SRU `numberOfRecords` 279,639 for 防災 vs a diagnostic (0) for the impossible entity; CRD `hit_num` 1,039 / 185 vs `hit_num 0`. **0 `FILTER_IGNORED`.** Re-probed after each re-key: `PASS` (SRU 100, Kokusen html-anchors 25, JPX 25/6/49, MHLW 10 ×3, FDMA 15, Customs 100) |
| `batch_exclusions.py --check … --bin ./bin/japanosint --skip-prefix hp3b23_jpnational` (binary relinked WITHOUT the table, after the sibling sessions' latest rows were rsynced in) | at 111 rows: `id set: 15888 from the binary's registry` / `collisions: 0   near-misses needing a human read: 0`. Re-run after restoring the 18 feeds, against the current worktree: `id set: 16248 from the binary's registry` / `DUP-ENDPOINT JPN_IPA_NEWS_ONLY_RSS -> already in collectors/pivot/table/hp3b23_jpcyber.c` / `DUP-ENDPOINT JPN_KOKUSEN_NEWS_RELEASES -> already in collectors/pivot/table/hp3b28_jppolice.c` / `collisions: 2` → both removed → 127 rows. The first run (14,761-id registry) had found the two `DUP-ENDPOINT` rows, removed. By-name grep: every host in the manifest was grepped against the 3,796-URL fence; hosts that appear there (`ndlsearch.ndl.go.jp`, `www.jma.go.jp`, `www.pmda.go.jp`, `www.fdma.go.jp`, `www.fsa.go.jp`, `www.jpx.co.jp`, `www.boj.or.jp`, `www.jetro.go.jp`, `www.customs.go.jp`, `www.caa.go.jp`, `www.mhlw.go.jp`, `data.e-gov.go.jp`, `dashboard.e-stat.go.jp`) are registered for OTHER endpoints (OpenSearch not SRU, `rss_015-021` not `001-025`, `getIndicatorInfo` not `getData`, `package_search?q=` not `?fq=organization:`, …) |
| `audit_batch_pagination.py` | `0 of 111 rows declare no pagination but look paged  (65 declared pagination_ok)`; the 16 restored feeds all declare `pagination_ok` |
| `audit_batch_reachable.py` | `0 of 111 rows can never run` |
| `audit_batch_emit.py --bin ./bin/japanosint --jobs 3 --timeout 400` (129 rows) | `EMPTY_UPSTREAM=1  OK=128` / `records emitted across the run: 47,519`; 0 `DROPS_EVERYTHING`, 0 `SLOW`, 0 `TRANSPORT_FAIL`. Re-run `--only` after the re-keys: `OK=3 … 57` and `OK=9 … 255` |
| **rule 4b** — `audit_registry_emit.py --match '^JPN_' --jobs 3 --timeout 400` (fresh copy of the warm template DB per row; `stored=` read off the run line AND counted back out of the database) | first pass over 115 rows: `OK=105 COLLISION=7 EMITS_NOTHING=3`. The 3 are the `{q}` pivots, which the sweep runs without an entity — run by hand below. The 7 `COLLISION` rows were feeds that reuse one `<link>` for several items; the field that IS distinct per item was measured on the wire (`guid` on the three JPX feeds; `title` on MHLW ×3, FDMA notices, Customs) and keyed first; re-swept `--only`: `OK=8 COLLISION=1` — the remaining one (IPA) turned out to be a CDATA title stored as a link fallback and was dropped. **At 111 rows: `108 OK` + 3 pivots; static rows emitted 47,168 / stored 47,178 (the +10 are the truncation-notice records of the ten ceiling rows); 0 rows with `stored < emitted`.** After the XML fix, the 17 restored rows (18 minus IPA): `audit_batch_emit.py --only … --jobs 2 --timeout 400` → `OK=17` / `records emitted across the run: 316`; `audit_registry_emit.py --only …` → `OK=17`, every row `emitted == stored == db` (20/20, 20/20, 4/4, 20/20, 20/20, 18/18, 10/10, 3/3, 10/10, 10/10, 20/20 ×4, 1/1, 50/50, 50/50). **Final 127 rows: 0 with `stored < emitted`** |
| `make` | 0 warnings (`grep -ci 'warning:'` over a clean `rm -rf obj bin && make -j8` → `0`, again after the XML-fix rsync and after every relink; `audit-sources` 0/0/strict 0, `lint-sources: OK`, `hptest` and `unit` `all passed` were re-run on the final 127-row tree) |
| `make audit-sources` | `files with findings: 0` / `findings           : 0` / `strict set (collectors/pivot/table/hp*_*.c): 0 finding(s)` |
| `make lint-sources` | `registered source_defs: 10439  (1115 direct + 9324 macro-expanded)` / `lint-sources: OK` |
| `make hptest` | `… ok    22b: page_zero_based=1 starts a {page} walk at /p/0` / `all passed` |
| `make unit` | `… ok    exactly at the ceiling converts` / `all passed` |

## Rule 4b — the three pivots, run with an entity against a fresh database

| row | entity | run line |
| --- | --- | --- |
| `JPN_NDL_SEARCH_SRU_PIVOT` | 防災 | `rc=0 records=500 1097ms stored=500` (five pages of 100; first keyed on `dcterms:identifier` it was `stored=463 UID-COLLISION: 37 of 500` — a record's first identifier is an ISBN or set id shared by volumes; only 63 of 100 first identifiers on page 1 were distinct while every `rdf:about` was — re-keyed on `rdf:about`) |
| `JPN_NDL_CRD_REFERENCE_PIVOT` | 防災 | `rc=0 records=1039 11984ms stored=1038 UID-COLLISION: 1 of 1039` — the upstream returns case `sys-id 1000088012` on two pages of the same query (verified with curl across the six pages); one byte-identical record collapsing is the dedupe working, not a wrong key |
| `JPN_NDL_CRD_MANUAL_PIVOT` | 防災 | `rc=0 records=185 3216ms stored=185` |

Emitted across the batch, static rows plus the three pivots on the probe
entity: **49,180** (stored 49,189) — 47,456 static (47,168 − Kokusen 28 + the 17 restored feeds' 316) + 1,724 from the pivots.

## Largest results

`JPN_NDL_OAIPMH_ONLINE_PUB_B00035` (独立行政法人) and `_B00027` (裁判所) 4,000
each — the 20-page ceiling on sets of 24,048 and 7,937 records, truncation
notice emitted; `JPN_ESTAT_DASHBOARD_UNEMPLOYMENT_RATE` 2,086,
`_JOB_OPENINGS_RATIO` 1,906, `_INDUSTRIAL_PRODUCTION` 1,638,
`JPN_NDL_OAIPMH_ONLINE_PUB_B00021` (農林水産省) 1,575, `_B00019` (文部科学省)
1,559, `_HOUSING_STARTS` 1,304, `JPN_JMA_LATEST_OBS_PRECIP_1H` / `_24H` 1,284,
`_B00022` (経済産業省) 1,033, seven `JPN_EGOV_DATA_ORG_*` rows at the
1,000-record ten-page ceiling (Cabinet Office 1,741 datasets, MOF 1,006, MEXT
1,986, MHLW 2,479, MAFF 2,210, MLIT 1,864, MOE 1,679 — truncation notices
emitted), `_CPI_ALL_ITEMS` 990, `JPN_MOE_WBGT_FORECAST_ALL_STATIONS` 841,
`JPN_MOD_PRESS_NOTICES` 481.

Titles were read back out of scratch databases for all 92 fast rows, for
one OAI set, and again for each of the 17 restored feeds after the XML fix
(0 of 316 titles carry `&#`, `CDATA`, a `feed-item` fallback or a replacement
character): Research Navi `歴史・地理・地域研究（日本）` / `関西館で調べよう！` / `言語`;
Issue Briefs `職場における熱中症対策の動向 調査と情報  (1371)  2026-08-27` /
`米英仏における政治資金監督・規制機関`; Foreign Legislation `EU 欧州防衛産業プログラム規則の制定`;
NDL general news `東京本館の8月の混雑について` / `NDL Newsletter（英文）No. 270を掲載`;
Mina Search `視覚障害者等用データ送信サービスの送信承認館6館、データ提供館1館が加わりました`;
ILCL `展示会「色の引き算―白・黒・いろいろ―」を開催します（付・プレスリリース）`;
NICT `「日経SDGsフェス」ジェンダーギャップ会議` / `多言語音声翻訳アプリ「VoiceTra」の修正版公開およびサービス再開について`;
MHLW emergency `管理栄養士国家試験に関する緊急情報`. From the first pass: UTF-8 Japanese throughout the shipped set, e.g. `宗谷岬（ソウヤミサキ）`
(JMA, Shift_JIS transcoded), `衆議院災害対策特別委員会ニュース` (OAI B00003),
`クマ撃退スプレーの備え方−事前の確認・準備が必須です−` (Kokusen, EUC-JP transcoded),
`[東証]上場廃止等の決定：（株）サニーサイドアップグループ` (JPX),
`予防接種_子宮頸がん、ヒブ、小児用肺炎球菌_リーフレット` (e-Gov data, MHLW),
`防災グッズを手作りしたい。` (CRD).

## Things worth knowing that the tools did not say

* **The engine's XML path used to store numeric character references and CDATA
  undecoded, and that was invisible to every gate.** (Fixed by the core session
  during this batch — CDATA unwrapped, `&#NNN;`/`&#xHHH;` decoded — after which
  17 of the 18 rejected rows were restored and re-verified; see the read-back above.) Fourteen NDL feeds write
  every item title as `&#x6b74;&#x53f2;…` and four feeds (IPA, NICT ×2, MHLW
  emergency) wrap titles in `<![CDATA[…]]>`. Probe PASS (items counted), emit
  OK, `stored == emitted` — and the stored title is either the literal
  `&#x…;` string or, for CDATA, the empty-title fallback `feed-item <link>`.
  Only a read-back of `intel_items.title` from a scratch database shows it.
  The 18 rows were rejected as `UNREADABLE_TITLES` on the first pass and
  restored once the fix landed (IPA then fell to a sibling beat's claim). One
  NIMS title kept an `&amp;` on the first pass; noted.
* **`id_keys` is first-match, not composite**, and RSS feeds that point several
  items at one section page collide on `link`. Seven feeds were caught by rule
  4b (Customs 27 of 100 collapsed, JPX site-updates 23 of 49, FDMA notices 12
  of 15). The fix is per feed: `guid` where the feed has one (JPX), otherwise
  `title` — both measured distinct on the wire before being declared.
* **NDL SRU**: `recordSchema` must be `dc` or `dcndl` (`dcndl_simple` is
  "illegal"); `recordPacking=xml` is what makes the records real elements. The
  `dc` schema carries no identifier at all (title / creator / publisher /
  language only), so the pivot uses `dcndl` and keys on the `rdf:about` of the
  `BibAdminResource`. 100 records ≈ 1.07 MB.
* **NDL OAI-PMH**: `oai_dc`, `dcndl`, `dcndl_v3` are the formats; `dc` is
  `cannotDisseminateFormat`. Sets are `B000xx` per state organ (from
  `ListSets`); a non-existent set name is `badArgument`, an empty one is
  `noRecordsMatch`. `resumptionToken` continuation works with the engine's
  `next_path=resumptionToken` + `next_tmpl=…&resumptionToken={v}` (the same
  pattern the NII repository rows use).
* **e-Stat dashboard `getData`** without `RegionCode` returns every region and
  exceeds 3 MB per indicator; `RegionCode=00000` bounds it to the national
  series. The record shape is `VALUE.@time` / `VALUE.$` under
  `GET_STATS.STATISTICAL_DATA.DATA_INF.DATA_OBJ`; titles are the period code
  (`19751000`) because the record carries nothing more readable.
* **`data.go.jp` redirects (301) to `data.e-gov.go.jp`**; its
  `organization_list` / `group_list` / `tag_list` ignore `all_fields` and
  return bare name strings, and `organization_show?include_datasets=true`
  caps at 10 packages with no paging — so per-ministry inventories have to be
  `package_search?fq=organization:`.
* **CRD API**: `results_num` up to 200, `results_get_position` pages; the
  `manual` type's fields are `theme` / `guide`, not `question` / `answer`.
  The upstream can return the same `sys-id` on two pages of one query.
* Kokusen serves **EUC-JP** (`charset=euc-jp` was needed; without it the stored
  titles were not valid UTF-8) — worth knowing for the jppolice row that now owns it. JMA's mdrr CSVs are cp932 (`charset=sjis`).
* `www.gsi.go.jp` cannot be reached from the WSL host at all
  (`UNSAFE_LEGACY_RENEGOTIATION_DISABLED`; curl exit 000), so nothing on the
  GSI main host could be proved and none was authored.
* METI's Atom feed answers 403 to `JapanOSINT/1.0` and to curl's default UA
  but 200 to a browser UA; not shipped behind a spoofed header.
* The MOD notices index carries a print-PDF link (`印刷用`) beside most notices;
  those are emitted as their own records (a few notices exist only as PDF).
  The MAFF press index shares its `/j/press/` prefix with bureau navigation
  links, which are emitted as what they are (70 release anchors + ~15 nav).
* The 21 OAI rows start at `from=2025-01-01`; they are ordered oldest-first by
  the upstream, so the two sets larger than 4,000 records (Courts, IAAs) will
  keep returning their first 4,000 until the floor is moved. Recorded here so
  nobody mistakes a stable count for freshness.

## Not verified

* The OAI ceiling rows (`B00027`, `B00035`) and the seven e-Gov ministry rows
  at 1,000 are bounded windows with truncation notices, by design of
  `page_max`; the rest of each set was not fetched.
* `JPN_PMDA_RSS_RS_JA` (1 item), `JPN_PMDA_RSS_ABOUT_EN` (2), the three
  `JPN_NDL_OAIPMH_ONLINE_PUB_B00015/B00012/B00011` (6–10 records): a run this
  small cannot show a collision defect.
* `batch_exclusions.py` was run against a binary relinked without the table
  (the tool self-collides once the table is installed); the 0-collision
  reading is that pre-install run against the 15,888-id registry.
* The `{q}` pivots were exercised with one entity (防災); the SRU `startRecord`
  walk was exercised to five pages, the CRD walk to six.
