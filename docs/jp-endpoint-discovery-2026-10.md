# Japanese endpoint discovery, 2026-10-06 — what is there and what is not

A measured record of a sweep across every Japanese organisation this repo could
get a sourced list of. It exists because the expensive part of this work is
finding out which conventions Japanese public-sector and corporate sites
actually implement, and that answer is worth writing down once instead of
re-probing it. **13,872 requests produced 465 live endpoints, all of one
family.**

## The organisation lists (all fetched open data, not recalled)

| Set | Rows | Source |
|---|---:|---|
| Listed companies, with 証券コード | 3,816 | `ymstmsys/japan-domain-list` (CC BY 4.0) — 東証/名証/札証/福証 |
| Financial institutions | 707 | same — 銀行/信金/信組/労金/生保/損保 |
| Local governments | 1,794 | same — 47 prefectures + municipalities |
| Municipalities, with 総務省 団体コード + official URL | 1,741 | `nkawa/Convert-Local-Government-URLs` |
| Prefectures, with lgcode + ISO 3166-2 | 47 | `code4fukui/localgovjp` |
| Universities, with `ac.jp` domains | 572 | `Hipo/university-domains-list`, Japan-filtered |

Deduplicated to 6,720 distinct hosts.

## What was probed, and what answered

Four conventions, chosen because they are specifications rather than per-site
paths, so a hit is a real endpoint and a miss is a real absence.

| Convention | Path | Live | Rate |
|---|---|---:|---:|
| WordPress REST posts | `/wp-json/wp/v2/posts` | **465** | 6.9% |
| CKAN package_search | `/api/3/action/package_search` | 0 | 0% |
| ArcGIS service directory | `/arcgis/rest/services?f=json` | 0 | 0% |
| OAI-PMH Identify | `/oai`, `/dspace-oai/request` | 0 | 0% |

A second sweep tried the open-data portal hostnames that a real domain implies —
`opendata.`, `data.`, `odcs.`, `catalog.` prefixed onto each of the 1,788 local
government domains, 7,152 candidates. **Four answered.** Prefectural and
municipal open data in Japan is not at a predictable hostname; it is on shared
platforms (BODIK, 地方公共団体オープンデータ), which this tree already reads.

### The one family that exists, by organisation type

| Type | Hosts probed | Live WP REST | Rate |
|---|---:|---:|---:|
| Listed companies | 3,816 | 403 | 10.6% |
| Universities | 572 | 28 | 4.9% |
| Financial institutions | 707 | 18 | 2.5% |
| Municipalities + prefectures | 1,788 | 16 | 0.9% |

Corporate Japan runs WordPress; local government mostly does not. That is the
opposite of what an OSINT batch aimed at `.lg.jp` would assume, and it is why
the resulting beat (batch 38) is mostly `co.jp`.

## Things this sweep established that are easy to get wrong

* **Prefecture main hosts expose nothing machine-readable.** All 47 were probed
  for all four conventions and returned nothing. `www.pref.aichi.jp` answers
  `/wp-json/wp/v2/posts` with **403**, not 404 — the API is present and
  deliberately closed. Do not read a 403 as "wrong path".
* **RSS is not a convention here either.** Eight representative gov hosts were
  probed for eight conventional feed paths (`/index.rdf`, `/rss.xml`,
  `/rss/index.rdf`, `/news/index.rdf`, `/feed/`, …). One answered:
  `www.metro.tokyo.lg.jp/rss/index.rdf`, 60 items. Guessing feed paths across
  1,788 hosts is not worth the requests.
* **The obvious national APIs are already in this tree.** Probed live and then
  found already registered: JMA bosai forecast (55 refs in
  `collectors/feed/generated/vsrc13_jp_local_1.c`), BODIK CKAN (5 files incl.
  `hp3b34_ckanbulk.c`), 国会会議録 (`diet_records.c` + 2), MyJVN
  (`my_jvn.c` + 2), WordPress REST itself (37 files, incl. `hp3b25_jpmuni2.c`).
  Of 155 national-API candidates probed, 91 were live and essentially all were
  duplicates. **At 18,643 sources this tree has already harvested the
  reachable Japanese surface**; new Japanese rows now come from narrower
  research, not from sweeps.
* **e-Stat cannot be proof-of-life verified without a key.** `api.e-stat.go.jp`
  answers HTTP **200** with `{"RESULT":{"STATUS":100,"ERROR_MSG":"認証に失敗し
  ました。アプリケーションIDを確認して下さい。"}}` — the HTTP-200 refusal shape
  `probe_hp_batch.py` rejects. A row for it would be registered and unprovable.
* **J-STAGE cannot be enumerated.** `service=2` requires one of
  `material`/`issn`/`cdjournal` (`ERR_011`), so there is no "list all journals"
  call to fan out from.
* **Disabling TLS verification inflates a discovery count by 4.5%.** The sweep
  script ran with `CERT_NONE`; 21 of its 465 hits failed
  `CERTIFICATE_VERIFY_FAILED` or reset the connection under
  `probe_hp_batch.py`, which verifies properly. Those 21 are in
  `docs/rejected-sources-batch38.*.tsv`. A discovery pass that skips
  verification must be re-probed by the real gate before anything is counted.

## Hosts that answered but were already in the tree

Three of the 464 distinct live hosts. The dedupe is a single
`grep -rF` of the host against `native/collectors/`, which is also what
`batch_exclusions.py` does more thoroughly by endpoint — it reported **0
collisions** for the 461 kept rows against all 18,643 registered sources.

## Reproducing or extending this

The scripts are not committed: they are one-shot discovery, and the record of
what they found is this file plus the batch-38 manifests. What matters is the
method — probe a specification, not a guessed path; keep only what answered;
re-probe with verification on; and check the hit against the existing tree
before counting it as new.
