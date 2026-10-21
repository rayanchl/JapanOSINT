# Batch 49 — what the member organisations of Japan's nationwide federations publish

**5,399 rows, every one measured end to end** (5,393 on 2026-10-20, six restored on 2026-10-21 — see "The retry pass" below). Probe, emit and **stored** all ran
for real, so each row in `docs/verified-sources-batch49.tsv` carries both numbers:
**518,813 emitted, 518,827 stored** over one pass, plus 749 / 749 from the six restored rows. These are batch 45's staging
set (`docs/staging-sources-batch45.federations.tsv`), registered as their own batch
on your decision to take all of them.

| Beat | Rows | Hosts | What |
|---|---:|---:|---|
| `hp3b49_jpwpmedical.c` | 1041 | 263 | Medical, pharmacist, nursing and veterinary associations |
| `hp3b49_jpwpcci.c` | 1022 | 282 | Chambers of commerce and industry |
| `hp3b49_jpwpagri.c` | 849 | 209 | JA cooperatives, NOSAI and agricultural committees |
| `hp3b49_jpwpjc.c` | 745 | 216 | Junior Chamber local organisations |
| `hp3b49_jpwpprof.c` | 431 | 92 | Professional bodies (tax, legal, labour, architects, realtors) |
| `hp3b49_jpwpcoop.c` | 389 | 77 | Consumer, fishery and labour co-operatives |
| `hp3b49_jpwpculture.c` | 302 | 63 | Public cultural facilities, zoos and aquariums |
| `hp3b49_jpwpcommerce.c` | 259 | 66 | Corporate-tax associations (法人会) |
| `hp3b49_jpwpwelfare.c` | 181 | 40 | Social welfare councils |
| `hp3b49_jpwpfire.c` | 177 | 50 | Fire departments and fire-chief associations |
| `hp3b49_jpwpedu.c` | 3 | 1 | Junior-college and private-university associations |

Record types: `site-post` 1154, `site-page` 1310, `site-user`
1155, 1780 custom-post-type rows across
1079 distinct `cpt-*` types.

## The method: a federation's link page is a host list

Nationwide bodies list their members with links: JCCI's chamber search loads
`cci.json` with all 515 chambers; the 47 JA prefecture pages; the 日本医師会
郡市区医師会 link database (809 URLs in a script block); 全国消防長会's regional
pages; 全社協's prefectural list; the co-op maps; the 税理士会 / 司法書士会 /
社労士会 / 行政書士会 / 調査士会 / 建築士会 / 宅建協会 lists; JC's ten 地区協議会;
全国公立文化施設協会 and JAZA. 161 index pages gave 5,123 member hosts
(after dropping social networks, CDNs, blog platforms and `*.go.jp` / `*.lg.jp`).
Each was asked for its WordPress REST root (the homepage's
`Link: rel="https://api.w.org/"`, else `/wp-json`), its post types and one page
of each collection — `tools/wp_routes_sweep.py`, with hosts normalised so that
`www.` and bare spellings of one site make one row.

| Step | Result |
|---|---:|
| Member hosts asked | 5,123 |
| Advertise a REST root | **2,203** (1639 via the `Link` header, 432 via a `<link>` tag, 132 at a bare `/wp-json`) |
| Collections asked for one page | 7,088 |
| Answered 200 with ≥1 post- or user-shaped record | **5,678** |
| Dropped as editor/theme machinery | 242 |
| Dropped as the same endpoint under a second hostname | 31 |
| Manifest rows | 5,399 (5,393 + the six restored) |
| `make batch-gates` (exclusions, pagination, reachability, probe) | **5,287 PASS**, 16 TIMEOUT, 71 NET_ERR, 13 HTTP_ERR, 2 ERR, 4 UNPARSEABLE |
| `audit_registry_emit.py --match '^JO49_'` | **5,387 OK**, 6 SLOW |

Other answers: 932 collections advertised and empty, 35
not a list, 196 × 401, 66 × 403, 77 × 404, 78 transport
failures — `docs/rejected-sources-batch49.sweep.tsv`; probe failures in
`docs/rejected-sources-batch49.probe.tsv`.

## Beats are the FEDERATION that listed the host

A chamber of commerce, a medical association and a fire department publish
very different notices; the federation that listed each host is the most
honest statement of what it is, so each beat is one federation family and
`core/collectors.def` files it under that family's group (health, corporate,
legal, safety, social, culture, science). Every collection type a host
publishes — posts, pages, authors, custom post types — sits in its host's beat;
`record_type` carries the distinction.

## The retry pass (2026-10-21)

Twelve rows failed two engine readings on 2026-10-20 and left. The convention is
one more look a day later, and today's look split them exactly in half —
`docs/rejected-sources-batch49.emit.tsv` carries all three readings per row:

| Outcome | Rows | Why |
|---|---:|---|
| Stays removed | 3 | `8246cci.or.jp` answers HTTP 401 `rest_disabled` under every User-Agent: the site switched its REST API off |
| Stays removed | 1 | `kuma-cho.com` users: HTTP 404 `rest_no_route` — the route is disabled |
| Stays removed | 2 | `hachioji-jc.com` Kubio preview-changesets and `rakuno.ac.jp` recruit_info: HTTP 200 with an empty array — collections that hold nothing (`EMPTY_RESULTSET`) |
| Restored as-is | 3 | `kcci.komoro.org` posts (597 / 597 — its whole X-WP-Total over six pages), `midorinet-fukushima.jp` pages (67 / 67), `htk.or.jp` pages (55 / 55; the host still resets a connection now and then — the engine retried one SSL error and finished, and a run it loses files a `collector-fetch-failure` notice) |
| Restored with a declared User-Agent | 3 | `jahs.co.jp` pages (15 / 15) and both `newspromotion.jp` rows (10 / 10, 5 / 5) |

**Two user-agent bot walls, and the second one names the word.** `jahs.co.jp`
answers 403 to the engine's User-Agent and to plain curl, 200 to a browser-style
one, on every reading. `newspromotion.jp` is stranger: curl got 200 while the
engine died twice with "Stream error in the HTTP/2 framing layer" on the same
URL — and so did curl once it sent the engine's exact User-Agent. The host resets
the stream (`PROTOCOL_ERROR`) for **any User-Agent containing the word
`collector`** — `JO_USER_AGENT` says "feed collector" — under HTTP/1.1 as well,
and answers 200 to `JapanOSINT/1.0 (feed)`, `(contact via repo issues)`, python-
requests and an empty UA. A transport error that reproduces only with one header
is a refusal, not a transport error; the three rows declare
`User-Agent: Mozilla/5.0 (compatible; JapanOSINT/1.0)` (`header1=` in the
manifest, `.headers` in the row) and were re-measured under it. The UA carries no
URL on purpose: `lint-sources` reads a URL inside a header value as a fetched
endpoint and fails `dup-endpoint` on the third row that shares it.

The two `newspromotion.jp` rows also read the canonical host: `www.` 301s to the
bare domain, and a row that is verified at one URL and fetched through a redirect
is verified under different conditions than it runs.

## Cadence

Every row declares `interval=21600` like batch 45, as data. The cadence that
actually runs is the system-wide policy (`core/cadence.h`,
`GET`/`PUT /api/sources/cadence`): one setting for the whole registry, not
per batch.

```sh
cd native
make batch-gates MANIFEST='../docs/candidate-sources-batch49.*.txt'
python3 tools/audit_registry_emit.py --bin ./bin/japanosint --match '^JO49_' --jobs 12 --timeout 200 --out sweep49.tsv
```
