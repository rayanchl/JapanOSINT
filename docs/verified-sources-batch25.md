# Batch 25 — Japan, wide: police, national government, banks, prefectures, town halls, infrastructure, archives, the cyber surface, licensing registries, regional media

**1,128 rows on 563 hosts, 528 of them absent from the tree.** Every row was
fetched over the wire on 2026-08-29 twice — once by the discovery agent with
curl, once by `tools/probe_hp_batch.py --check-filter` — and only rows that
answered 2xx, parsed in their declared mode and carried ≥2 real records are
here. **2,495 candidates were tried and rejected**, each with its reason in
`rejected-sources-batch25-<beat>.tsv`. Per-row verdicts, item counts and byte
sizes: `verified-sources-batch25.tsv`.

**Generated and emit-audited** (2026-08-30): the ten tables plus `jprecovered`
are in `native/collectors/pivot/table/hp3b25_*.c`; every row went through the
real binary — see "Emit audit" below. Rows that could not be made to emit were
removed and are in the rejected TSVs with the reason.

| beat | rows | pivots | what it is |
| --- | --- | --- | --- |
| `jpmuni2` | 178 | 0 | town-hall CMS feeds on 80 new hosts (all 24 Osaka wards, Kawasaki ×5, Fukuoka ×4), 27 disaster-mail archives, 14 municipal CKAN APIs, 2 ArcGIS Hubs |
| `jpnatgov` | 149 | 2 | the 行政事業レビュー REST API (30,239 programme sheets, org tree, full-text pivot), ten regional finance-bureau registration feeds, e-Gov CKAN, JTSB accident indexes, Diet session-221 registers, 51 labour-bureau feeds |
| `jpmedia` | 136 | 9 | 43 local-TV news rows, 17 trade papers, 12 regional dailies, Sankei sections, 27 Mastodon/Misskey public timelines, 10 Wikimedia JA rows, PR wires |
| `jppolice` | 117 | 0 | 77 NPA-standard per-incident crime CSVs at street-block level (13 prefectures), 11 police/JCG feeds, 4 CKAN catalogues, 12 prefectural mail-alert archives |
| `jppref` | 110 | 1 | 47 prefectures: press feeds, assembly (県議会) feeds ×20, tender feeds, Kanagawa (811 datasets) and Tottori open-data catalogues |
| `jparchive` | 108 | 65 | 56 JAIRO Cloud WEKO3 repositories + UTokyo/NII/Kyoto KURENAI (255,765 items) as `{q}` pivots, NDL WARP web-archive search, Cultural Japan aggregator, 28 Wayback CDX rows for defunct hosts (Geocities, nifty, Yahoo blog, pre-2001 ministries, Mt.Gox, Yamaichi, LTCB…) |
| `jpbizreg` | 105 | 5 | 官公需 KKJ procurement API (2 pivots + 47 per-prefecture daily feeds), Osaka/Kawasaki/Utsunomiya licensee-level food, hygiene, pharmacy and waste registers, Hokkaido medical registers with 法人番号, MEXT school codes, JC-NET bankruptcy feeds |
| `jpfinance` | 93 | 0 | regional-bank release archives (Musashino 2,240 items, Hokuto 866, Toho 729), 5 crypto-exchange public APIs, TSR bankruptcy RSS, second-tier-bank monthly stats, payment-operator registrations |
| `jpdeep` | 68 | 2 | JP incident feeds (gate02: 429 write-ups), JPRS advisories, 748-ASN Japan BGP table + filter-verified AS pivot, named-victim leak feed, antiphishing council lists, open2ch boards, plain-text blocklists |
| `jpinfra` | 64 | 0 | Haneda live flight boards, JEPX half-hourly power prices, NEXCO closure feeds, Osaka Metro archive + 13 city bureau feeds, QZSS advisories, Docomo/KDDI/IIJ/NTT Com outage feeds, Hiroshima waterworks ×7 |

84 rows are entity pivots; 1,044 are scheduled. Modes: 494 RSS/Atom, 218 HTML
anchor lists, 126 CSV, 232 JSON/XML API envelopes.

## How it was gated

1. **Exclusion.** 5,121 hosts already referenced anywhere in `native/collectors`
   or an earlier manifest were dumped to a list; a candidate on one of them was
   kept only if the exact URL was absent from the tree.
2. **Discovery probe.** Each agent fetched every candidate with curl, read the
   first 1,500 bytes, and rejected error envelopes, empty result sets, bot walls
   and login pages. Every `{q}` row was also asked about `zzqxv9`.
3. **Static gates** over the ten files as one set: `batch_exclusions.py --bin`
   (14,728 registered ids) → 0 collisions after 3 cross-beat duplicates were
   dropped; `audit_batch_reachable.py` → 0 rows that can never run;
   `audit_batch_pagination.py` → 0 findings.
4. **`probe_hp_batch.py --check-filter`** over all rows: 1,077 PASS on the
   first pass. The 51 non-PASS were re-fetched one at a time with curl using
   each row's own headers and POST body:
   * 14 fediverse rows returned 200 + JSON — the probe tool does not POST
     (`post_body` rows are not its to judge); kept.
   * 27 Wayback CDX rows returned 200 with 200 captures each under a serial
     6-second-gap fetch; the probe's four parallel workers had been rate-limited.
   * 3 METI/gov-online rows answer 403 to a Python UA and 200 to a browser UA;
     `header1=User-Agent:` added to the row. 7 rows were transient DNS/read
     errors and returned 200.
   * **Dropped**: 2 `FILTER_IGNORED` CKAN `q=` pivots (Utsunomiya, Kobe —
     rule 4d), 4 Centrair flight files behind a Cloudflare challenge, Kagin
     (Akamai 403), Miyagi radiation feed (broken certificate), 3 gaccom.jp
     pages (DH key too small for OpenSSL 3), and 12 CDX wildcards on huge hosts
     (2ch, teacup, infoseek, NAVER…) that archive.org answers 503/504 even
     serially after 90 s.

## What the manifest could not express (recurring, worth an engine change)

* **Fixed-width and `<>`-delimited text** — JPNIC `as-numbers.txt` (1,052
  ASNs with org names), 2ch.sc `subject.txt`. No `csv_delim` covers them.
* **Directory-relative hrefs** (`../`, `./meisai/…`) — `base=` prefixes
  root-relative hrefs only. Sangiin, courts.go.jp disclosure lists, EC-CUBE and
  Yamaha advisories all lost pages to this; an engine-side relative-URL
  resolver rescues every one.
* **Path-segment pagination** (`/tosan/p/N`, `/list/<station>?page=`) and
  fediverse `max_id`/`untilId` cursors — rows take page 1 / the current window
  and say so in `pagination_ok`.
* **CSV with a title line above the header** — `csv_no_header=1` emits the
  title and header as two junk records (MEXT, Kawasaki, Saitama).
* **OAI-PMH `resumptionToken`** — KURENAI bulk takes 100 + a truncation notice.
* **xlsx/zip-only registers** — 青森 旅館業, WAM 障害福祉, 医療情報ネット
  zip, 登録支援機関: unreachable to the engine as it stands.

## Where the beats hit their ceilings

The national professional registers (日弁連, 税理士, 司法書士, 医師資格確認,
BIT court auctions, 裁判例) are POST/JS applications; every prefectural 条例
database is a 403-walled vendor host; the FSA lender register and JVCEA member
list are session-bound forms; 18 regional banks reset TLS connections from this
network; JR East, Tokyo Metro, JAL, TEPCO and Kansai airports are bot-walled;
the beat's core cyber hosts (JVN, JPCERT, IPA, 5ch, Telegram, ransomware.live)
were already in the tree; 13 prefectures' open data lives only on the 403-ing
`data.bodik.jp`. These are in the rejected TSVs with reasons, not padded over.

## Other-session files in the same namespace

`candidate-sources-batch25.{jpacademic,jpbiz2,jpgeospatial,jpkaigo,jpkeizai,jpmedia2,jpmunireserve}.txt`,
`review-sources-batch25.*.txt` and `hp3b25_*.c` were produced concurrently by
another session on this checkout. They were treated as taken hosts/URLs by the
agents above and are not counted here.

## Emit audit (rules 4 and 4b) — 2026-08-30

Probe PASS proves an endpoint answers. This section is the second proof: every
row was run through the real binary and its records counted on the way into
`intel_items`. Three instruments, in order of authority:

1. `tools/audit_batch_emit.py` per beat (`--jobs 4 --timeout 240`) — the
   engine's own `emitted N of M` line. 1,128 rows + the 4 `jprecovered` rows.
2. hand re-runs (`JO_DB=… ./bin/japanosint --run <ID> <entity>`) for every row
   the auditor could not judge — the run line prints emitted AND stored.
3. `tools/audit_registry_emit.py --match '^JP25_' --scheduled --jobs 6
   --timeout 240` over the registry: 1,036 scheduled rows, `stored` read back
   out of each run's own database, independent of the run line.

The TSV now carries three extra columns: `emitted`, `stored`, `emit_verdict`.
`stored` is DISTINCT-uid as the sink counts it; a blank means *not measured*
(a pivot the auditor passed and nobody re-ran by hand), never zero. 1,109 of
1,132 rows have a measured `stored`.

### Result, per beat (rows after rejection)

| beat | rows | OK | OK_PAGE_CEILING | OK_UPSTREAM_DUP | OK_SLOW | COLLISION | rejected at emit |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| jppolice | 117 | 94 | | 23 | | | |
| jpnatgov | 148 | 147 | | | 1 | | 1 (JBIC duplicate, lint) |
| jpfinance | 93 | 93 | | | | | |
| jppref | 110 | 110 | | | | | |
| jpmuni2 | 178 | 177 | | | | 1 | |
| jpinfra | 63 | 63 | | | | | 1 |
| jparchive | 108 | 64 | 42 | | 1 (floor) | 1 | |
| jpdeep | 59 | 56 | | 2 | 1 | | 9 |
| jpbizreg | 104 | 100 | | 1 | 1 | 2 | 1 |
| jpmedia | 136 | 131 | | 5 | | | |
| jprecovered | 4 | 2 | | | 1 (floor) | 1 | |
| **total** | **1,120** | **1,037** | **42** | **31** | **5** | **5** | **12** |

Registry after this pass: **15,786** sources (was 15,797 with the 11 rows
that are now rejected). Gates on the final tree: `make` 0 warnings,
`make hptest` all passed, `make audit-sources` strict set 0, `make lint-sources`
OK, `make selftest` PASS.

Verdicts:

* **OK** — `stored == emitted`, every record that came back is in the table.
* **OK_PAGE_CEILING** — the 42 WEKO (JAIRO Cloud) repositories: `emitted 1000
  of 1000 across 10 page(s) (TRUNCATED)`, with the truncation notice stored as
  the 1001st row. The page ceiling is the engine's, not the row's; each repo
  has more.
* **OK_UPSTREAM_DUP** — `stored < emitted` and the difference is exactly the
  number of byte-identical lines / same-URL anchors in the upstream file. Checked
  by re-fetching and counting: GIFU_2024_ZITENSYATOU 1,630 rows / 1,615
  distinct lines / 15 collapsed; NIIGATA_LATEST_SYAZYOUNERAI 282/274/8;
  SHIZUOKA_2025_ZIDOUHANBAIKINERAI 90/84/6; CYBERCRIME_TRACKER_ALL_CSV
  22,700/22,697/3; GIFU_LIST 68 raw hrefs -> 63 resolved URLs / 5 collapsed;
  PSIA_NAIGAI 40 -> 37 / 3; NBC_NAGASAKI 50 items / 45 distinct links / 5;
  ANTIPHISHING_INFO 247 -> 243 / 4. The 23 police CSV rows are one family (the
  same publisher tooling emits duplicate lines); the rest were checked one by
  one. This is the sink deduplicating, not discarding — the same reading as the
  NZ collisions in 7dd608d.
* **OK_SLOW** — killed at the auditor's 240 s and re-run alone to the end:
  POTAROO_AUTNUMS 122,241 = 122,241 (259 s); EGOV_CKAN_ALL 18,141 = 18,141
  (624 s); KANSAI_KAIGO 43,630 = 43,630 (181 s, see below). Two rows are
  recorded as a **floor** because a full run did not finish inside the session:
  KURENAI_OAI (Kyoto U. OAI-PMH) >= 24,900 stored at 240 s and still walking;
  KANPOUAI_TOSAN_PAGED >= 28,192 stored at 240 s. Their `emitted` is blank —
  a run killed mid-walk has no total, and 0 would be a lie.
* **COLLISION** — `stored < emitted` and not proven upstream duplication:
  WIKISOURCE_JA_SEARCH 1,000/997, KKJ_QUERY 100/99, KKJ_ORG 100/94,
  NISHITOKYO_MUNICIPAL_UPDATE_FEED 52/50 (the feed had moved to 50 unique guids
  by the time it was re-fetched), YAMAHA_RT_SECURITY 67/66. All <= 6 %, all
  keyed on the upstream's own id (pageid / Key / guid), left in place with the
  numbers.

### What the beat audit reported vs what was true

The auditor's first pass listed 136 non-OK rows. Split before believing it:

* **69 were the auditor's own artifact** — every pivot whose probe URL carries a
  percent-encoded Japanese entity (`%E7%A0%94%E7%A9%B6` = 研究) is re-run with
  the encoded string as the entity and encoded again. 58 WEKO repositories, the
  three MediaWiki searches, the Mastodon tag pivots, RSSYSTEM x2, KKJ x2,
  TOKYO_TAKKEN, SAITAMA/CHIBA datasets, DBpedia: all re-run by hand with the
  decoded entity, all emit. (The memory note `emit-audit-false-failures` from
  batch 23 predicted this exactly.)
* **28 were archive.org contention** — the 14 CDX rows failed 503/transport under
  four parallel jobs and passed one at a time; a second 14 had "passed" with
  exactly one record each, which was the real defect (next section).
* **The rest were real**, and were fixed or rejected as below.

### Fixes (manifest edits, regenerated, re-run, `stored == emitted` confirmed)

| rows | defect | fix |
| --- | --- | --- |
| 28 `JPARCH_CDX_*` | CDX `output=json` is an array-of-arrays; the engine saw ONE record holding the whole response and every row "passed" with `emitted 1` — a confident wrong answer | `csv` mode on the plain CDX output with `csv_delim=ws;csv_no_header=1;title_keys=col2;id_keys=col5;date_keys=col1;link_keys=col2` -> 200/200 each (YAHOOBLOG 4, MTGOX 92: honest small archives); plus `timeout_ms=180000` (archive.org regularly needs more than `HP_HTTP_TIMEOUT` 20 s) |
| 12 `JPMEDIA_MISSKEY_*` | HTTP 415 on every run: `content_type=application/json` in the manifest is emitted as a whole header line, so the engine sent the bare string "application/json" and curl dropped it | opt removed — the engine's default IS `Content-Type: application/json` -> 100/100 each |
| 9 csv rows (MEXT_SCHOOLCODE x4, KOSHIGAYA_SANPAI_YURYO, SAITAMA_JOKASO, KAWASAKI_PHARMACY/TENPO/OROSHI) | `csv_no_header=1;title_keys=colN` emitted the title line and the real header as two junk records | `csv_skip_lines=1` + header-named keys (学校名/学校コード, 業者名称/許可番号, 清掃業者名/番号, 施設名称/許可番号) -> MEXT_1 28,907 = 28,907; no title-line record in any of the nine |
| `JPBIZ_KANSAI_KAIGO` | 11 MB CSV at ~180 KB/s died at the 20 s default; then 7,307 of 43,630 collapsed (a facility x service-type row is the record, no single column is unique) | `timeout_ms=300000;body_keys=サービスの種類`; under the re-synced engine (csv collision guard hashing the flattened record) 43,630 = 43,630 |
| `JPARCH_CODH_PMJT_BOOKS` | `href_must=/pmjt/book/` is tested on the RAW href and the index links are relative (`200003080/`), so 0 anchors; and `base=https://codh.rois.ac.jp` resolved them to the wrong URL | `href_must=2000;base=https://codh.rois.ac.jp/pmjt/book/` -> 2,844 = 2,844 |
| `JPARCH_JA_DBPEDIA_LABEL` | 406 from Virtuoso: the engine's forced `Accept: application/json`; the endpoint offers only `application/sparql-results+json` (same shape as ESTAT_LOD_SPARQL in batch 23) | `header1=Accept: application/sparql-results+json` -> 100/100 |
| 58 `JPARCH_WEKO_*` | six repositories die at the 20 s default on slow pages | `timeout_ms=120000` on all WEKO rows; the six re-run to the 10-page ceiling |
| `JPPREF_AKITA_OD_NEWS`, `_CKAN_NEWS` | reported PARTIAL 8/9 and 20/21; `id_keys=link,guid` with two items sharing `link=https://opendata.pref.akita.lg.jp/` | `id_keys=guid,link` (guid is unique). The 9th "item" is an empty `<item/>` shell with no guid, link or title — dropping it is correct, so 8 of 9 is the honest count |

### Rejected at emit (11 rows removed from the manifests; reasons in `rejected-sources-batch25-<beat>.tsv`)

* `JPDEEP_OPEN2CH_*_HTML` x7 — **BOT_WALL**: 403 with a 90 KB challenge body
  to every UA tried, browser UA + Accept/Accept-Language included.
* `JPDEEP_NCA_CSIRT_MEMBERS_HTML` — **NOT_ANCHORS**: the member list is
  `<li onclick="window.location.href='…/corporate_detail/…'">`; there is no
  `<a>`. The probe counted the strings, not anchors.
* `JPDEEP_CYBERCRIME_TRACKER_CCAM_HTML` — **IMG_ONLY_ANCHORS**: 444 VirusTotal
  anchors wrap an `<img>` and have no text; `hp_run_html` drops `text_len < 3`.
  The hash lives only in the href.
* `JPINFRA_QZSS_NAQU` — **SCALAR_LIST**: `search/naqu` returns
  `<search><id>2026234</id>…` x100, bare scalars with no fields; the engine
  cannot key them, so the `detail_url` fan-out is never reached (emitted 0 of 100).
* `JPBIZ_KKJ_PREF_24_MIE` — **OVERSIZE**: `LG_Code=24&Count=100` answers 206 MB
  where `LG_Code=23` answers 1 MB; upstream ignores `Count` for Mie and the
  body exceeds `JO_HTTP_MAX_BODY` (64 MB).

### Engine findings handed back (not fixable from a manifest)

1. **`hp_run_html` filters on the raw href, resolves afterwards.** A relative
   listing can never satisfy an absolute `href_must`; CODH had to be filtered on
   `2000`. Testing `href_must` against the resolved link (or offering both) would
   make the opt mean what it says.
2. **No way to title an anchor from its href.** Image-only anchors whose only
   payload is the URL (CCAM: 444 malware hashes) are dropped as text-less. A
   `title_from_href=1` (or basename) opt would recover this class.
3. **Scalar arrays are unkeyable.** An XML/JSON list of bare ids (QZSS NAQU) is
   the classic search-then-detail shape and the engine already has
   `detail_url`/`detail_key`; it only needs to treat a scalar item as
   `{id: <scalar>}`.
4. **`content_type=` is a whole header line, not a media type.** Twelve rows
   carried the natural reading and silently got HTTP 415. Either accept a bare
   media type or have `gen_hp_batch.py` reject a value without a colon.
5. **`tools/lint_sources.py` counted `.base` hosts as endpoints** — fixed in
   this pass (strips `.base = "…"` like `.portal`); the two jc3.or.jp / ppc.go.jp
   findings were different pages on one host.
6. **`audit_batch_emit.py` double-encodes non-ASCII pivot entities** (69 false
   failures here, 19 in batch 23). Decoding the probe entity once before the
   run would remove the whole class.
7. **`audit_registry_emit.py`'s `emitted` is the scheduler's `records=`**, which
   excludes notice-typed records, so a news feed reports `emitted 0, stored 50`.
   The merge here trusted it only when non-zero; the tool should read the
   `hp:` line or count both.

### Not done / caveats

* 23 rows have no measured `stored` (pivots the auditor passed on emit and nobody
  re-ran by hand); their `emitted` is the engine's own count.
* KURENAI_OAI and KANPOUAI_TOSAN_PAGED are floors, see OK_SLOW.
* `hp3b25_jpacademic.c` (other session) needs the in-flight `hpengine.h`
  (`timeout_ms`) and was built with it here; not audited by this pass.
* The 4 `jprecovered` rows were added after the probe pass and appear in the TSV
  with emit evidence only (probe columns blank, note says so).
