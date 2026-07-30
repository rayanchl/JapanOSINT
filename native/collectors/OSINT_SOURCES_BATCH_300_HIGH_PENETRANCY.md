# Batch: 300 High-Penetrancy Deep-Record Collectors

Added **300** new registered sources (2202 → **2502**, all ids unique, none lost)
plus one shared engine, `lib/hpengine.{c,h}`, and an offline test driver,
`tests/hpengine_test.c` (`make hptest`).

## What "high penetrancy" means here

The registry was already broad. It was **shallow**: most sources are one HTTP
call deep — a search endpoint that answers *"does this name appear?"* and stops.
Penetrancy is the other axis: how far past the search hit a source reaches into
the underlying record.

The clearest example in the repo: `ripestat_jp.c` used exactly **one** RIPEstat
data call (a country resource list), while the same keyless API answers routing
status, announced prefixes, AS neighbours, BGP update history, whois, abuse
contacts, address-space hierarchy, reverse-DNS delegation, historical whois and
blocklist membership for any resource. Same pattern everywhere else:

| Provider | Was | Now also |
|---|---|---|
| Companies House | `/search/companies` | profile, officers, **PSC (beneficial ownership)**, filing history, charges, insolvency, establishments, officer appointments, disqualified directors, dissolved, advanced search, registers |
| GLEIF | LEI search by name | LEI record, **direct parent, ultimate parent, direct children**, ISIN mapping |
| CourtListener | one opinion search | dockets, **parties & counsel**, RECAP documents, opinions, judges, **judicial financial disclosures** |
| OpenAlex | works + authors search | institutions, funders, sources, raw-affiliation works |
| OpenSanctions | name check | entity (nested), scored match, peps, crime, debarment, wanted, sanctions, **raw statements/provenance**, dataset coverage index |
| SEC | filing index | **XBRL company facts**, reported-revenue concept series |
| GitHub | code search | profile, repos, events, gists, **public SSH keys**, **GPG keys (embedded emails)**, orgs, commits-by-author-email |
| Brønnøysund | — | entity, **roles/officers**, sub-units, name search + role hop |
| Bitcoin/EVM | balance | address txs, UTXO set, token transfers, **internal transactions** |
| USPTO | — | **patent & trademark assignment ledgers** (who sold which IP to whom) |

## Composition

| File | Rows | Focus |
|------|-----:|-------|
| `hp_netintel_deep.c`     | 27 | 13 RIPEstat data calls, BGPView asn→prefixes→peers→upstreams, PeeringDB net/IX/facility, Cert Spotter issuances, Mnemonic passive DNS, Wayback CDX domain index, Common Crawl, urlscan history, OTX passive DNS |
| `hp_people_ident.c`      | 25 | GitHub 8-record deep dive, GitLab/Codeberg, HN, Stack Exchange, Keybase, Gravatar, Bluesky, Mastodon, Lemmy, Telegram preview, Wikidata claims + SPARQL role graph, VIAF, LittleSis relationships, OpenCorporates officers |
| `hp_eu_deep.c`           | 23 | FR DECP/HATVP/RNA, DE OffeneRegister + abgeordnetenwatch, NL KVK profile/establishments + PDOK + rechtspraak, TED search API, EP MEPs, EUR-Lex, CJEU, Zefix, CRO, ARES statutory bodies, RPO, KRS-S, Diavgeia, NAZK declarations, Prozorro, RCBE, Sudreg |
| `hp_americas_deep.c`     | 22 | SEC XBRL, USAspending awards/recipients, SAM.gov, FEC, Senate lobbying, EPA ECHO, openFDA, FCC, FAA registry, NPPES, ProPublica 990s, NY/CA registers, Canada MRAS, Brazil CNPJ ×2 + CEIS, Colombia SECOP, Chile, Mexico DOF |
| `hp_uk_deep.c`           | 22 | 12 Companies House record endpoints, Gazette insolvency/all notices, Charity Commission details/trustees/financials, MP financial interests, Hansard, Land Registry price paid, NHS ODS |
| `hp_research_ip.c`       | 22 | OpenAlex entity graph, Crossref funders/members, ORCID search/employments/works, ROR, S2, Europe PMC, OpenAIRE, NIH RePORTER, NSF, UKRI, ClinicalTrials, PatentsView, USPTO assignment ledgers, Google Patents |
| `hp_apac_deep.c`         | 21 | Korea DART filings/company/major-holders, Taiwan GCIS company + directors, HK CR + open data, China ICP, SG/MY/ID/TH/VN/PH registries, India data.gov.in + ZaubaCorp + Kanoon, AU ABR/ASIC, NZ Companies Office + NZBN |
| `hp_sanctions_deep.c`    | 20 | 8 OpenSanctions dataset/graph endpoints, Interpol red + UN notices, Europol, US CSL, **OFAC SDN + alt-names + addresses (raw)**, Canada SEMA, NAZK, ransomware.live, RansomLook |
| `hp_chain_finance.c`     | 20 | mempool.space ×3, Blockstream, Blockchair, Blockscout ×4, TronGrid, Solana RPC, Ransomwhere, GLEIF ×5, OpenFIGI, FRED, DefiLlama |
| `hp_mena_africa_cis.c`   | 20 | Gulf licence registers, Israel companies dataset, Turkey gazette, ZA CIPC + SAFLII, NG CAC, KE/pan-Africa CKAN, RW RDB, GH RGD, OpenOwnership, RU Rusprofile/List-Org/kad.arbitr, UA Clarity, KZ, GE, AM |
| `hp_maritime_air.c`      | 18 | hexdb aircraft/route/airport, adsb.lol hex/callsign/reg/squawk, airplanes.live, OpenSky metadata, Planespotters, TC/UK registers, OurAirports, aviationstack, Paris MoU, USCG PSIX, GFW vessel identity, sanctioned vessels |
| `hp_nordic_baltic.c`     | 18 | Brønnøysund entity/roles/sub-units/search+hop/bankruptcy, CVR entity/owners/production units, DAWA, allabolag, PoIT, PRH ×2, IS/EE/LV/LT registers, HILMA |
| `hp_courts_legal.c`      | 16 | CourtListener ×6, ECHR HUDOC, CanLII, AustLII, BAILII, Federal Register, GovInfo, WIPO UDRP, IE/NZ courts |
| `hp_exposure_geo.c`      | 14 | HIBP stealer logs/pastes/catalog, Hudson Rock email/domain/username, LeakCheck, ProxyNova, grep.app, OSM changesets/notes, Nominatim extratags, GeoNames hierarchy, NASA CMR |
| `hp_procurement_aid.c`   | 12 | UK Contracts Finder, World Bank procurement/documents, USAspending subawards, IATI, OCHA FTS, SAM opportunities, USAID, Global Fund, Canada CKAN, UNGM, Kohesio |

**240 keyless · 60 credential-gated** · 241 JSON / 55 HTML / 4 CSV · 11 POST
rows · 7 rows do a two-hop (list → full record) fetch.
All are on-demand entity pivots (`update_interval_sec = 0`, `layer = NULL`), so
they cost nothing until a search or an explicit `--run` dispatches them, and
none of them appears in `/api/layers`.

## The engine

`lib/hpengine.{c,h}` — one shared `run()`; a collector is a declarative row
naming a real endpoint, how to build its URL from the pivot entity, and
optionally the follow-up endpoint that turns a list hit into a full record.

- **Token expansion**: `{q}` url-encoded, `{Q}` raw, `{qd}` digits only,
  `{qc}` digits zero-padded to 10 (SEC CIK), `{qh}` host of a URL/email,
  `{qu}` email local part, `{ql}`/`{qU}` case-folded, `{qn}` spaces stripped,
  `{key}`, `{keyb64}` = base64("key:") for HTTP Basic. A row whose template
  needs a token the entity cannot fill is **skipped without a request**.
- **Shape gates** (`want`): domain, IP, email, numeric, hash, ICAO24, ETH, BTC,
  ASN. A domain-only row never burns a request on a person's name.
- **Tolerant JSON shaping**: declared `array_path`, else the densest array of
  objects is discovered automatically; title/id/link/date fall back to
  conventional key lists. An upstream that renames its envelope degrades to
  fewer resolved fields — never to invented ones.
- **Full-record properties**: every scalar in the record is flattened into
  `properties` with dotted keys (bounded: 160 keys, depth 4, 12 array members),
  so the emitted item is the record, not a headline.
- **Second hop**: `detail_url` + `detail_key` fetch the record behind a list hit
  and merge it under `detail.*`, capped at `detail_max` per run.

### Honesty properties (enforced, and tested)

Same rule as `SOURCE_REALITY_REPORT.md`: **real fetch or honest empty.** The
engine has no fixture path, no fallback table and no synthesized row — the only
values that reach the sink are bytes that came back over the wire in that run.

- missing credential → 0 rows, logged note, **no request issued**
- entity shape mismatch / unfillable token → 0 rows, no request
- 404 / 4xx → 0 rows, `rc = 0` (honest empty, not an error)
- 5xx or transport failure → `rc = -1`, so the source shows as **errored** in the
  Source Dashboard and the existing anomaly detector opens a case for it
- JS-only or anti-bot HTML rows (Gulf/Russian/SE-Asian registries, several courts)
  legitimately return zero from a datacenter IP. That is expected and documented
  per row — those rows never substitute a "found in registry" placeholder.

## Verification performed

- `make` clean (`-Wall -Wextra`), zero new warnings from the 15 new files or the
  engine.
- **2502 sources registered, 2502 unique ids, exactly +300 vs the 2202 baseline,
  zero pre-existing ids lost** (`--list-sources` diff).
- Fresh-DB boot: `[db] seeded sources from registry (2502 new rows)`,
  `[selftest] PASS`, integrity ok — all 300 carry name/type/category/description
  into the `sources` table and are dispatchable by id.
- `make hptest` — 37 assertions over the real engine and one real shipped row
  (`UK_CH_PSC`): token expansion, nested/array flattening, link templates,
  remote-key composition, array auto-discovery, detail-hop merge and call
  counting, headerless-CSV columns, HTML href filtering and dedupe, Basic-auth
  header construction, credential gating, shape gating, POST bodies, and the
  404-vs-5xx return contract. Clean under `-fsanitize=address,undefined` with
  LeakSanitizer.
- Spot `--run` through the real scheduler/sink path for JSON, CSV, keyed and
  ICAO24 rows: URLs build correctly against the documented endpoints
  (e.g. `RIPE_AS_OVERVIEW` + `AS13335` → `stat.ripe.net/data/as-overview/
  data.json?resource=AS13335`; `BR_MINHARECEITA_CNPJ` extracts the digits;
  `ADSBLOL_HEX` + `4ca1fd` → `api.adsb.lol/v2/hex/4ca1fd`), and blocked fetches
  degrade to errored/honest-empty with no crash.

### Not verified here (and why)

**Live reachability was not tested for any row.** This sandbox's egress is
policy-restricted: every host tested (including `api.github.com`, which answers
only repo-scoped paths) is refused at the proxy with a 403 CONNECT. Endpoint
paths come from each provider's published API surface; expect a minority to need
small adjustments in a real network, exactly as with the earlier 1058-row world
batch. The dashboard's errored-source list plus the existing anomaly detector
make those trivial to spot, and `core/url_override.c` can repoint a source's URL
at runtime without a recompile.

## Also in this change (pre-existing build breaks)

`main` did not link before this batch; three small fixes were required to be able
to build and verify anything:

- `core/searchapi.{c,h}` — `searchapi_analyze()` took 3 args while `httpd.c`
  called it with 4 and had a 429 `too_many_searches` branch that could never
  fire. Added the `status_out` parameter and the concurrency cap it implied
  (`JO_SEARCH_MAX_CONCURRENT`, default 4) so an unbounded thread-per-search is no
  longer a trivial DoS.
- `core/httpclient.{c,h}` — `http_client_global_init()` was called by
  `camera_stills.c` but never defined; factored the existing once-only
  `curl_global_init` out of `http_client_new()`.
- `core/db.{c,h}` — `db_attach()` was called by the evidence GC thread but never
  defined; added it (second connection to an already-migrated DB, no schema
  apply, so a background worker owns its own transactions).
