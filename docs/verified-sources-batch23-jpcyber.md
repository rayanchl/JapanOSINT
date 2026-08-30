# Batch 23 — beat `jpcyber` (Source Agent J3): 100 rows

Japanese cyber, corporate/legal registers, infrastructure and lawful deep-web
metadata that the tree and the sibling batch-23/24/25 manifests did not hold:

* **JVN / JVN iPedia** — the four JVN listing pages (JVNVU and JVN# series,
  Japanese and English, the full past-12-months rather than the registered
  RSS's newest 20), the complete legacy JVNTR feed (262 notes), and the MyJVN
  `getVulnOverviewList` API sliced by CVSS severity (high/medium/low, Japanese;
  high/medium, English) and walked through `startItem`, plus a keyword search
  pivot in both languages;
* **JPCERT/CC** — the CyberNewsFlash archive (263), the Weekly Report year
  index, the TSUBAME quarterly-report archive back to 2012;
* **IPA, ICT-ISAC, NICTER** — IPA's third feed (news-only) and its 重要な
  セキュリティ情報 archive, every ICT-ISAC notice since 2016, the NICTER blog;
* **vendor PSIRT / security feeds** — NEC's untrimmed blog RSS (315 items),
  ELECOM's security-notice archive, Mitsubishi Electric PSIRT RSS, GMO
  Cybersecurity and Ierae, Unit 42 Japan, ruby-lang.org security advisories,
  five Cybozu status feeds (kintone/Garoon incidents);
* **JPCERT/CC phishing-URL dataset** — all 29 monthly CSVs (2024-01 … 2026-05), 2,000–8,000 confirmed phishing URLs a month with the impersonated brand;
* **JPX** — the four English RSS feeds (the Japanese editions were claimed by the `jpnational` sibling while this beat was in flight); **Teikoku
  Databank** bankruptcy bulletin RSS; the **Official Gazette** 30-day issue
  index; **OTIT** notices; **e-Gov public comment** RSS; **METI** ja/en Atom;
  **Digital Agency** en RSS; **FSA** en news; **FDMA** live disaster reports;
  **JAXA** 2026 press index; **Wikidata** TSE-listed companies (2,271);
* **PeeringDB** — the Japanese organisation and network (ASN) populations;
  **Kansai International Airport** live flight board (170 flights);
* **J-STAGE** — six search-field pivots (title, author, affiliation, keyword,
  abstract, full text) as quoted phrases, paged 100 at a time;
* **social** — Zenn per-topic articles, Togetter search, Hatena Bookmark tag
  search (all `{q}`), and thirteen open2ch board indexes not claimed by
  `jpbbs`/`jpdeep` (Shift_JIS, `csv_delim=lit:<>`).

Manifest (the source of truth): `docs/candidate-sources-batch23.jpcyber.txt`.
Generated table: `native/collectors/pivot/table/hp3b23_jpcyber.c` (87 rows,
`gen_hp_batch.py --prefix hp3b23 --batch 23`, never hand-edited). Rejects as
data: `docs/rejected-sources-batch23-jpcyber.tsv` (78 rows).

Every row was fetched over the wire from this machine, parsed in its declared
mode, filter-checked with an impossible entity where it takes one, run through
the **real binary** and read back as `emitted N of M`, and run again against a
fresh per-row database to read `stored=` off the run line and count it back out
of `intel_items` (house rule 4b). Nothing was authored from documentation.

The fence was built from the worktree as it stood (uncommitted batch 23/24/25
work included): the built binary's `--list-sources` (15,888 ids before this
table), every `docs/candidate-sources-batch2[345].*.txt` and every
`docs/review-sources-batch25.*.txt` URL. That fence took a large part of the
obvious territory (JPCERT alerts, JVN RSS, antiphishing.jp, JC3, JPRS, LAC,
piyolog, Security NEXT, Zenn/Qiita/note feeds, ja.wikipedia API rows, NDL,
TDnet, KAKEN, researchmap, CiNii, houjin/gBizINFO/invoice APIs, the carriers'
feeds, nca.jp, MyJVN alert/vendor/product) and is why the beat leans on
listing pages, secondary feeds and API slices those batches did not touch.

## Attrition

| stage | in | out | dropped |
| --- | --- | --- | --- |
| pre-screen over the wire (three passes, ~520 URLs, own `urllib` with an IPv4-preferring `sitecustomize`) | ~520 | 106 rows authored | 5ch/shitaraba/Yahoo/JPO/JCG/Xserver 403; NISC placeholder site; JAL/ANA/Narita/JPNAP/vendor PSIRT pages JS-rendered; nicovideo bot wall; IRDB OAI badArgument; KAKEN 403; ~30 hosts already in the fence — all in the rejects file |
| `probe_hp_batch.py --check-filter` | 106 | 105 | `93/106 PASS` first pass; 12 recovered by adding a browser User-Agent (NEC, ELECOM, METI ×2), an `Accept: application/sparql-results+json` header (Wikidata) and serial re-runs (7 open2ch boards rate-limited under `--jobs 4`); McAfee `TIMEOUT` on three serial attempts → dropped. **0 `FILTER_IGNORED`** |
| `batch_exclusions.py`, pagination, reachable | 105 | 105 | — |
| `audit_batch_emit.py` | 105 | 105 | `OK=104 PARTIAL=1` after the 8 non-ASCII-probe rows and 7 rate-limited boards were re-run (see below) |
| **rule 4b** `audit_registry_emit.py` | 105 | 75 | **29 JPCERT phishing-URL CSV rows** and `MYJVN_OVERVIEW_SEVERITY_LOW_EN` read back `stored < emitted` → dropped (engine finding, below) |
| core session extends the collision guard to the CSV/XML paths; re-rsync, rebuild (16,258 seeds), restore the 29 | 75+29 | 91 | `audit_batch_emit.py --jobs 2 --timeout 400` on the 29: `OK=29` / `111,401`; `audit_registry_emit.py`: **16 `OK` (stored == emitted exactly)**, 13 `COLLISION` short by 1–10 → shown to be byte-identical duplicate lines and restored (note below) |
| `batch_exclusions.py` on the re-rsynced tree | 104 | **100** | 4 `DUP-ENDPOINT … already in collectors/pivot/table/hp3b23_jpnational.c` — the Japanese JPX feeds, registered by a sibling meanwhile → dropped, English editions kept |

Registry: 16,258 → **16,358** seeds in the built binary (first pass: 15,888 → 15,963) (`lint-sources` counts
`REGISTER_SOURCE` only and stays at 10,439).

## Gate results (quoted)

| gate | result |
| --- | --- |
| `gen_hp_batch.py ../docs/candidate-sources-batch23.jpcyber.txt --outdir /tmp/gen23j3c --prefix hp3b23 --batch 23` | final: `/tmp/gen23j3c/hp3b23_jpcyber.c 87 rows` / `total 87 rows` (first pass 75). An earlier draft was rejected by the generator with `opts token 'Linux x86_64) JapanOSINT/1.0' has no '='` — the browser User-Agent I added carried a `;` and swallowed the following opt exactly as CLAUDE.md 4c describes; the shipped UA has no semicolon |
| `probe_hp_batch.py --jobs 4 --check-filter` (106 rows) | `# 93/106 PASS`, **0 `FILTER_IGNORED`**; failures `HTTP_ERR Forbidden` ×4 (NEC, ELECOM, METI ja/en — default UA), `UNPARSEABLE declared array_path but body is not JSON` (Wikidata without an Accept header), `TIMEOUT` (McAfee), `Too Many Requests` ×7 (open2ch). Serial re-probe with the fixed opts: `12/13 PASS`, McAfee `TIMEOUT` again; third attempt `PASS` for the last board, McAfee `TIMEOUT` a third time → dropped. After the probe entities of the 8 pivots were changed to ASCII (see emit): `# 8/8 PASS` with `--check-filter` |
| `batch_exclusions.py --check … --bin ./bin/japanosint --skip-prefix hp3b23_jpcyber` (before the table was installed, `bin/` and the stale object removed so the binary is truly relinked) | final: `id set: 16258 from the binary's registry` / `collisions: 0   near-misses needing a human read: 0`; earlier passes 0/0 against 14,761 and 15,888 ids; the intermediate pass after the re-rsync reported 4 `DUP-ENDPOINT` against the sibling's `hp3b23_jpnational.c` (Japanese JPX feeds), dropped. By-name grep: every host in the manifest was grepped against the registry URL dump and all sibling/review manifests; hosts that do appear (jvn.jp, jvndb.jvn.jp, jpcert.or.jp, ipa.go.jp, jpx.co.jp, tdb.co.jp, cs.cybozu.co.jp, meti.go.jp, digital.go.jp, fsa.go.jp, fdma.go.jp, jaxa.jp, peeringdb.com, query.wikidata.org, api.jstage.jst.go.jp, zenn.dev, togetter.com, b.hatena.ne.jp, open2ch.net, public-comment.e-gov.go.jp) are registered for OTHER documents/endpoints; each row's description names what is registered and what this row adds |
| `audit_batch_pagination.py` | `0 of 87 rows declare no pagination but look paged  (72 declared pagination_ok)` |
| `audit_batch_reachable.py` | `0 of 87 rows can never run` |
| `audit_batch_emit.py --bin ./bin/japanosint --jobs 3 --timeout 400` (105 rows, run as two halves) | half 1 `OK=52` / `records emitted across the run: 82,433`; half 2 `DROPS_EVERYTHING=5 EMPTY_UPSTREAM=2 HTTP_429=7 OK=38 PARTIAL=1` / `35,867`. The 7 `HTTP_429` are open2ch under load: re-run `--jobs 1` → `OK=7`, `399`. The 5 `DROPS_EVERYTHING` (J-STAGE) and 2 `EMPTY_UPSTREAM` (Togetter, Hatena) all passed when run by hand (`--run JPC_JSTAGE_AUTHOR_PIVOT 山田` → `records=1000 stored=1002`): the audit tool lifts the entity out of the probe URL and my probe entities were percent-encoded Japanese, so the engine re-encoded `%E5%9C%B0%E9%9C%87` and J-STAGE/Togetter got `%25E5…`. Probe entities changed to ASCII (`Tokyo`, `Sony`) and the 8 rows re-run: `OK=7 PARTIAL=1` / `5,183`. After the guard fix the 29 CSV rows re-ran with `--jobs 2 --timeout 400`: `OK=29` / `records emitted across the run: 111,401`. **Over the final 100 rows: `OK=99 PARTIAL=1`, 123,757 records emitted** (111,401 of them the 29 phishing-URL months) |
| **rule 4b** — `audit_registry_emit.py --match '^JPC_' --jobs 3 --timeout 400` (fresh copy of the warm template DB per row; `stored=` read off the run line AND counted back out of the database) | 105 rows: `COLLISION=30  EMITS_NOTHING=21  OK=54`. Over the final 75: **60 `OK`** (`emitted=5754 stored=6513 db=6513`; stored ≥ emitted on every row because it counts the run's own truncation/shape notice records too) and 15 `EMITS_NOTHING` which split into **11 `{q}` pivots** the sweep runs without an entity (table below), **2 open2ch boards** rate-limited even serially (`--run` by hand 20 s apart: `SOFTWARE emitted 73 of 73 … stored=73`, `STOCK emitted 51 of 51 … stored=51`), and **2 PeeringDB rows** that hit the API's anonymous rate limit after the day's repeated runs (`status=429`; earlier the same day `audit_batch_emit` measured them `OK 234` and `OK 248` — see "Not verified"). Second sweep after the core session extended `hp_collision_map()` to `hp_run_csv`/`hp_run_xml` (`--only` the 29 CSV rows + PeeringDB, `--jobs 2 --timeout 400`): **16 `OK` with `stored == emitted == db_rows` exactly** (`202401 5772/5772`, `202402 7994/7994`, `202403 6248/6248`, `202405 5166`, `202406 3906`, `202407 3561`, `202408 3752`, `202409 2416`, `202410 4729`, `202501 2582`, `202502 1790`, `202503 2344`, `202504 2068`, `202507 5118`, `202601 2830`, `202602 1625`) and 13 `COLLISION` short by 1–10 (`202404 7267/7266` … `202605 1743/1735`) — real dedupe, see the note below, restored; PeeringDB `429` again. **0 `UID-COLLISION` on any shipped row** |
| `make` | `0` warnings (`grep -c 'warning:'` on the full rebuild log, with and without the table). Note: on the FIRST rsync of the worktree the build carried 3 `-Wformat-truncation` warnings in `lib/hpengine.c` (someone else's in-progress edit); the re-rsync the coordinator asked for built at 0 |
| `make audit-sources` | `files with findings: 0` / `findings           : 0` / `strict set (collectors/pivot/table/hp*_*.c): 0 finding(s)` |
| `make lint-sources` | `registered source_defs: 10439  (1115 direct + 9324 macro-expanded)` / `lint-sources: OK` |
| `make hptest` | `all passed` (incl. `ok 22b: page_zero_based=1 starts a {page} walk at /p/0`) |
| `make unit` | `all passed` |

## Rule 4b — the eleven pivots, run with an entity against a fresh database

| row | entity | run line |
| --- | --- | --- |
| `JPC_JSTAGE_ARTICLE_TITLE_PIVOT` | Tokyo | `emitted 1000 of 1000 available across 10 page(s) (TRUNCATED)` / `records=1000 stored=1002 notices=2` |
| `JPC_JSTAGE_AUTHOR_PIVOT` | Tokyo | `emitted 119 of 127 available across 10 page(s)` / `records=119 stored=121 notices=2` (127 available, 8 duplicates across pages — J-STAGE repeats an entry on the page boundary; with 山田: `records=1000 stored=1002`) |
| `JPC_JSTAGE_AFFILIATION_PIVOT` | Tokyo | `1000 of 1000` / `records=1000 stored=1002 notices=2` |
| `JPC_JSTAGE_KEYWORD_PIVOT` | Tokyo | `1000 of 1000` / `records=1000 stored=1002 notices=2` |
| `JPC_JSTAGE_ABSTRACT_PIVOT` | Tokyo | `1000 of 1000` / `records=1000 stored=1002 notices=2` |
| `JPC_JSTAGE_FULLTEXT_PIVOT` | Tokyo | `1000 of 1000` / `records=1000 stored=1002 notices=2` |
| `JPC_MYJVN_OVERVIEW_KEYWORD_PIVOT` | Windows | `emitted 16 of 16 … [1 filtered out]` / `records=16 stored=16` — the filtered one is the API's no-match placeholder on the page past the end |
| `JPC_MYJVN_OVERVIEW_KEYWORD_PIVOT_EN` | Windows | `emitted 1 of 1 … [1 filtered out]` / `records=1 stored=1` (title read back: `Installer for Rakuten Kobo Desktop Application (Windows version) …`) |
| `JPC_ZENN_TOPIC_ARTICLES_PIVOT` | security | `emitted 48 of 48` / `records=48 stored=48` |
| `JPC_TOGETTER_SEARCH_PIVOT` | Sony | `emitted 24 of 24 across 2 page(s) [24 duplicate]` / `records=24 stored=24` (page 2 repeats page 1 when a query has one page; with 地震: `250 of 250 across 10 page(s)`, `stored=251`) |
| `JPC_HATENA_BOOKMARK_TAG_PIVOT` | Sony | `emitted 40 of 40` / `records=40 stored=40` |

Stored == emitted (+ the run's notice records) on all eleven; the notices are
the ten-page ceiling truncation notice and the page-shape notice.

## Rule 4b — real dedupe: the thirteen months whose read-back is short

`audit_registry_emit.py` reports `COLLISION` whenever `stored < emitted`. For
these thirteen rows the shortfall is exactly the number of byte-identical
duplicate lines in the upstream file (`tail -n +2 | sort -u | wc -l` against
`wc -l`), which CLAUDE.md 4b defines as real deduplication, not a discard. The
numbers are on record here; the rows ship.

| row | data rows in file | distinct lines | stored |
| --- | --- | --- | --- |
| `JPC_JPCERT_PHISHURL_202404` | 7267 | 7266 | 7266 |
| `JPC_JPCERT_PHISHURL_202411` | 3415 | 3414 | 3414 |
| `JPC_JPCERT_PHISHURL_202412` | 2686 | 2685 | 2685 |
| `JPC_JPCERT_PHISHURL_202505` | 2572 | 2571 | 2571 |
| `JPC_JPCERT_PHISHURL_202506` | 3718 | 3717 | 3717 |
| `JPC_JPCERT_PHISHURL_202508` | 3035 | 3034 | 3034 |
| `JPC_JPCERT_PHISHURL_202509` | 2783 | 2781 | 2781 |
| `JPC_JPCERT_PHISHURL_202510` | 5818 | 5817 | 5817 |
| `JPC_JPCERT_PHISHURL_202511` | 6158 | 6155 | 6155 |
| `JPC_JPCERT_PHISHURL_202512` | 4448 | 4447 | 4447 |
| `JPC_JPCERT_PHISHURL_202603` | 2786 | 2778 | 2778 |
| `JPC_JPCERT_PHISHURL_202604` | 3071 | 3061 | 3061 |
| `JPC_JPCERT_PHISHURL_202605` | 1743 | 1735 | 1735 |

## Largest results

`JPCERT_PHISHURL_202402` 7,994, `_202403` 6,248, `_202401` 5,772, `_202405` 5,166, `_202507` 5,118, `_202410` 4,729 (the phishing months); `WIKIDATA_TSE_LISTED_JSON` 2,271; the five J-STAGE pivots 1,000 each
(ceiling); `JVN_VU_ADVISORIES_HTML` 385; `OTIT_NEWS_HTML` 322;
`NEC_CYBERSECURITY_BLOG_RSS` 315; `JPCERT_NEWSFLASH_HTML` 263;
`JVN_TRNOTES_RSS` 262; `PEERINGDB_JP_NETWORKS` 248; `PEERINGDB_JP_ORGS` 234;
`KANPO_ISSUE_INDEX_HTML` 214; `JVN_JP_ADVISORIES_HTML` 174 (ja and en);
`KANSAI_AIRPORT_FLIGHTS_HTML` 170; `OPEN2CH_ARMY_SUBJECT` 150.

Smallest: `MYJVN_OVERVIEW_SEVERITY_LOW` 2 (JVN iPedia holds two low-severity
records at the moment), `JPX_NEWS_EN_RSS` 1, `MYJVN_OVERVIEW_KEYWORD_PIVOT_EN` 1
for the probe entity.

Titles were read back out of a scratch database for the Shift_JIS open2ch rows
(`軍事ニュース（速報）＆雑談スレ (43)`, `株式 雑談 (13)`), the MyJVN rows
(`GoogleのGoogle ChromeにおけるTime-of-check Time-of-use (TOCTOU) 競合状態の脆弱性`),
the JVN listings and the JPX feeds — all UTF-8 Japanese, no mojibake.

## Things worth knowing that the tools did not say

* **`lib/hpengine.c` applied the uid-collision guard only on the JSON path — now fixed.**
  When this beat first measured the JPCERT phishing-URL CSVs, `hp_collision_map()`
  was called only from `hp_run_json`; a URL re-confirmed on a later date keys
  onto the URL alone (`id_keys` is first-wins) and every monthly file lost 1–4 %
  (`202401: emitted 5772 stored 5646` = distinct URLs). Reported rather than
  patched; the core session extended the guard to `hp_run_csv` and `hp_run_xml`
  and the 29 rows were re-run: 16 now read back exactly (`5772/5772`). The 13
  still 1–10 short are short by EXACTLY the number of byte-identical duplicate
  lines in the file (`202404`: 7,267 rows, 7,266 distinct lines, stored 7,266;
  `202603`: 2,786/2,778/2,778; `202604`: 3,071/3,061/3,061) — the guard is
  collapsing true duplicates as designed and `audit_registry_emit.py` cannot
  tell that from a discard. Accepted by the coordinator on that evidence and restored.
* **The MyJVN "no results" placeholder is not a record the engine keeps.** Past
  the last page the Japanese API returns one item titled
  `MyJVN 該当する脆弱性対策情報はありません`; the paged severity rows stop on
  that page and it never reached `intel_items` (`SEVERITY_LOW: emitted 2 of 2
  across 2 page(s)`, both titles real), and on the keyword pivots
  `filter_query=1` drops it explicitly (`[1 filtered out]`). The English
  low-severity edition instead re-serves its two records on every page, which
  the engine walked to `page_max` — that row is dropped.
* **J-STAGE ORs digit tokens.** Unquoted, the probe's impossible entity
  `zzqx9nonexistent7q` matches 164,804 articles because `9` and `7` match;
  quoted as a phrase the same entity returns the API's `ERR_001` envelope with
  zero entries and a real phrase is unchanged (`"地震"` in the title: 20,022).
  All six rows send `%22{q}%22`. The registered `JSTAGE_CORPUS_BULK` row's
  description says the search "is not real"; it is, once quoted. `service=2`
  (journal search) returns `ERR_001` for real names too and was rejected.
* **`audit_batch_emit.py` takes the entity from the probe URL as written.** A
  percent-encoded Japanese probe entity is re-encoded by the engine and the
  audit reports `DROPS_EVERYTHING`/`EMPTY_UPSTREAM` for rows that work. Use
  ASCII probe entities for pivots, or read the run line by hand.
* **open2ch hosts are mirrors.** `uni.open2ch.net/newsplus/subject.txt` and
  `hayabusa.open2ch.net/newsplus/subject.txt` are byte-identical, so a board
  registered on one host is the same board on another; the thirteen boards
  here were chosen by NAME against the sibling batches. `uni` throttles at
  about four requests in flight (`429`); serially every board answers.
* **Four hosts refuse the probe's default User-Agent** (jpn.nec.com,
  elecom.co.jp, meti.go.jp ja/en → `403`) and answer any `Mozilla/5.0` UA;
  declared per row. The UA string must not contain `;`.
* **Wikidata SPARQL needs `Accept: application/sparql-results+json`** from
  the probe's client even with `format=json` in the URL (it served an HTML
  page and the probe said `body is not JSON`); the engine's libcurl got JSON
  either way, the header is declared so both agree.
* **PeeringDB rate-limits anonymous clients** after roughly a dozen requests in
  an afternoon (`status=429`); the two population rows measured `234` / `248`
  records under `audit_batch_emit` before the limit was hit.
* **JPCERT newsflash `href_must=20`** also matches six navigation anchors on
  the archive page (the press-release index and five JPCERT/CC Eyes blog links,
  263 emitted vs 257 entries); the row's description states it. `href_must` is
  a substring on the raw href, and every tighter substring lost real entries
  (`01.html` would drop the 39 `…02.html`/`…03.html` same-day issues).
* `kanpou.npb.go.jp` keeps issues for thirty days only; the row is daily so
  the index is captured before issues expire.

## Not verified

* `JPC_PEERINGDB_JP_ORGS` / `JPC_PEERINGDB_JP_NETWORKS`: `stored=` was not
  read back by the registry sweep because the API answered `429` by then;
  `audit_batch_emit` measured `OK 234 of 234` / `OK 248 of 248` earlier, and
  `id_keys=id` (PeeringDB's own primary key) is unique per record by
  construction. Hand re-runs four minutes and about ninety minutes after the sweep still answered `status=429`, so the stored count for these two rows rests on the earlier emit measurement and the key type, not on a 4b read-back; re-run `--run JPC_PEERINGDB_JP_ORGS` once the anonymous window resets to close it.
* `JPC_MYJVN_OVERVIEW_KEYWORD_PIVOT_EN` and `JPC_JPX_NEWS_EN_RSS` emitted one
  record on the probe entity / on fetch; a one-record run cannot show a
  collision defect.
* `JPC_MYJVN_OVERVIEW_SEVERITY_HIGH` measured `80 of 80 across 3 page(s)` in
  the sweep and 1,000 (ceiling) earlier the same day — MyJVN's severity filter
  answered a much smaller set on the later runs. Both are what the API
  returned; the row emits what it is given.
* The paged severity rows can in principle receive the placeholder item on a
  page past the end when the total is an exact multiple of 50; observed
  behaviour is that the engine did not store it, but the exact-multiple case
  was not exercised on the wire.
* `batch_exclusions.py` cannot be re-run meaningfully after the table is
  installed; the 0-collision reading is the pre-install run on the 15,888-id
  registry.
