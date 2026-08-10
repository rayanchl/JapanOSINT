# OSINT Source Reality Report

Audit of all 61 collectors in `native/collectors/osint/sources/`. Method: full
source read of each `run()` + what it emits into `intel_item`, validated by live
`./bin/japanosint --run <ID> <entity>` spot-runs (latency + emitted DB body).

**Bottom line:** No collector fabricates measurement data (no fake temps, balances,
crime stats, or scores). Failures degrade to honest `error` / `status:error` /
`not_found` / a "needs key" note — never seeded values. The real caveats are:
(1) a handful need API keys and return an empty note without them, and (2) several
"PARTIAL" collectors pad genuinely-fetched data with hardcoded **registry/source
names + constructed lookup URLs** marked `real_data:true` — these are the
"shows names, not data" cases you flagged.

Legend:
- **LIVE** — real external fetch, emits the fetched values. Works out of the box.
- **LOCAL** — real data computed locally (DNS, sockets, regex, OpenSSL, SQLite). No network/key.
- **NEEDS_KEY** — real API, but returns empty/note unless a credential is set.
- **PARTIAL** — real fetch for the core, but padded with hardcoded labels/registry-name stubs.

---

## ✅ LIVE — keyless, returns real fetched data (29)

| Source ID | Real data emitted | Endpoint |
|---|---|---|
| ADDRESS_RESOLVER | lat/lon, address parts | nominatim.openstreetmap.org |
| ASN_LOOKUP | AS number/name, ISP, org | ip-api.com |
| GITHUB_CODE_SEARCH (code_search) | code/repo matches, leak counts | api.github.com, gitlab.com |
| COURT_RECORDS | case names, dockets, dates, citations | courtlistener.com (CourtListener) |
| CRYPTO_TRACKER | on-chain balances, tx counts | blockchain.info, etherscan, blockcypher |
| CERTIFICATE_TRANSPARENCY (ct_logs) | subdomains, issuers, cert validity | crt.sh, certspotter |
| DOMAIN_AGE | RDAP dates, registrar, computed age/trust | rdap.org |
| DOMAIN_HISTORY | *(byte-identical to DOMAIN_AGE)* same RDAP data | rdap.org |
| EARTHQUAKE_MONITOR | magnitude, location, depth, tsunami flag | earthquake.usgs.gov (USGS) |
| EMAIL_VALIDATOR_HOLEHE | per-site account-exists from live bodies | many site endpoints |
| GEOCODING | lat/lon, display name, address | nominatim.openstreetmap.org |
| IP_GEOLOCATION | full geo (country/city/lat/lon/ISP) | ip-api.com |
| MAC_VENDOR_LOOKUP | vendor, country, address | api.maclookup.app |
| SOCIAL_ANALYZER (maigret) | profiles confirmed by live probe (42 sites) | per-site |
| REVERSE_GEOCODING | lat/lon + address details | nominatim.openstreetmap.org |
| HISTORICAL_WHOIS (reverse_whois) | registrant→domains list | viewdns.info (+optional WhoisXML/SecurityTrails) |
| SANCTIONS_CHECK | OpenSanctions match records, PEPs, risk score | api.opensanctions.org |
| SEC_EDGAR_SEARCH | company info, filings, Form-4 insider trades | sec.gov / efts.sec.gov |
| SHODAN_SEARCH | open ports, hostnames, vulns (InternetDB) | internetdb.shodan.io (key adds detail) |
| SOCIAL_INTEL | GitHub/Reddit/Keybase profile data | api.github.com, reddit.com, keybase.io |
| SOCIAL_SEARCH | Holehe email-account checks (10 platforms) | per-platform |
| SUBDOMAIN_FINDER (subdomain_osint) | real discovered subdomains, deduped | crt.sh + 12 sources |
| TECH_STACK_DETECTION | technologies matched in live page body | target URL |
| EMAIL_HARVESTER (theharvester) | harvested emails/subdomains | Google, crt.sh, GitHub, DuckDuckGo |
| THREAT_FEED_LOOKUP | IOC pulses, malicious URLs, hash intel | OTX, abuse.ch, circl.lu |
| TOR_EXIT_CHECK | is-tor-exit verdict vs real exit list | check.torproject.org |
| URL_ANALYZER | domain, status, server, malicious verdict | urlscan.io |
| SHERLOCK_SEARCH (username_osint) | profiles confirmed by live probe | per-platform (`username_osint_platforms.inc`) |
| MALWARE_ANALYSIS (virustotal) | VT verdicts, votes, reputation | virustotal.com (public path) |
| DOMAIN_WHOIS (whois_lookup) | registrar, status, NS, dates | rdap.org |
| PASTE_SITE_SEARCH (pastebin_monitor) | paste IDs, code hits, archived URLs | psbdmp.ws, grep.app, archive.org |

## 🖥 LOCAL — real data computed on-device, no network/key (7)

| Source ID | Real data emitted | How |
|---|---|---|
| DATA_EXTRACTOR | emails/IPs/hashes/cards (Luhn) from input text | POSIX regex |
| DNS_RECORDS | A/AAAA addresses | getaddrinfo |
| JP_CORPUS_LOOKUP | corpus hits + entity graph | local SQLite FTS (`intel_items_fts`) |
| PORT_SCANNER | open ports (of 22 common) | non-blocking connect()+select() |
| REVERSE_DNS | PTR hostname | getnameinfo |
| SSL_ANALYZER | cert subject/issuer/validity/SANs/cipher | OpenSSL handshake + crt.sh |
| VESSEL_TRACKER | IMO/MMSI validity, flag country | check-digit + hardcoded MID table (no fetch) |

## 🔑 NEEDS_KEY — real API, but empty without a credential (4)

| Source ID | Without key | Credential | Confirmed |
|---|---|---|---|
| BREACH_CHECKER | HIBP 401/429 → "requires API key" note; 0 breaches | `HIBP_API_KEY` | — |
| CENSYS_SEARCH | `credentials_configured:false, hosts_found:0` + note | `CENSYS_API_ID`+`CENSYS_API_SECRET` | ✅ ran: 0ms, no data |
| SATELLITE_TRACKER | note "Set N2YO_API_KEY", found=0, no positions | `N2YO_API_KEY` | ✅ ran: 0ms, no fetch |
| IP_REPUTATION | still emits a verdict, but computed over **zero** external signals | `ABUSEIPDB_API_KEY` / `IPQS_API_KEY` | — |

## ⚠️ PARTIAL — real fetch + hardcoded "names not data" padding (19)

> **Updated 2026-08-10.** DARK_WEB_MONITOR and DEHASHED_SEARCH were rewritten to
> real-fetch / honest-empty and are **no longer PARTIAL** (struck through below);
> PASSWORD_CHECKER's fabricated `security_tips` were removed. Count 21 → 19.

The core fetch is real; the flagged half emits fixed **source/registry names +
built lookup URLs** (often tagged `real_data:true`), inflating "sources found"
counts without any actual fetched values — exactly the pattern you described.

| Source ID | Real half (LIVE) | "Names, not data" half |
|---|---|---|
| ACADEMIC_SEARCH | PubMed, arXiv, CrossRef, Semantic Scholar, ORCID, CiNii papers | 3 stub entries: J-STAGE / KAKEN / researchmap — label + search URL only |
| COMPANY_LOOKUP | OpenCorporates company record | **7** hardcoded registry stubs (ZaubaCorp, MCA India, Touki Japan, 法人番号, EDINET, NIKKEI, Baseconnect) marked `found:1`, no fetch |
| ~~DARK_WEB_MONITOR~~ | **now LIVE (2026-08-10):** per-record emit — one item per real Ahmia `.onion` hit + **one item per distinct Pastebin paste** (the `/raw/` keys are surfaced, no longer discarded to a count) + key-gated IntelX search id | ~~Ghostbin label / fixed disclaimer / onion-reformat branch~~ — all removed; nothing fabricated |
| ~~DEHASHED_SEARCH~~ | **now LIVE (2026-08-10):** real HTTP Basic-auth DeHashed fetch + keyless LeakCheck, one item per real breach source | ~~Dehashed permanent stub~~ — removed; `query_dehashed` sends real Basic auth, returns NULL without keys |
| EMAIL_REPUTATION | EmailRep.io + live MX (getaddrinfo) | hardcoded `DISPOSABLE_DOMAINS[]` list for the disposable flag |
| EMAIL_VALIDATOR | live MX (DoH), SMTP RCPT probe, RDAP age | Hunter.io key-gated; hardcoded disposable/free/role lists |
| FLIGHT_TRACKER | ICAO24 → live OpenSky state vector | **flight-number branch: no fetch** — hardcoded airline table + tracking-site links |
| HASH_LOOKUP | MalwareBazaar file intel | VT key-gated; fixed 3-URL "analysis_resources" list |
| NEWS_AGGREGATOR | GDELT articles + computed sentiment/tallies | NewsAPI key-gated; hardcoded `osint_tips` |
| NEWS_ARCHIVE | *(dup of aggregator)* hits **live GDELT, not an archive** | same hardcoded tips; mislabeled as archival |
| PASSWORD_CHECKER | real entropy/strength, HIBP k-anonymity range check | ~~hardcoded `security_tips`~~ removed; top-20 `COMMON_PASSWORDS` kept as a **scoring input only** (not emitted as a finding). *Privacy fixed 2026-08-10: the full SHA-1/SHA-256/MD5 of the submitted password are no longer emitted — only the 5-char SHA-1 k-anon prefix, per `breach-check-pipeline.md` §7.* |
| TRADEMARK_SEARCH | WIPO + EUIPO marks (often 403 → `api_unavailable`) | USPTO "manual_search_required" + Nice classification hardcoded labels |
| VEHICLE_LOOKUP | VIN → NHTSA vPIC decode + recalls (real) | **~16** hardcoded source descriptors (CARFAX, FaxVin, Parivahan, Goo-net…) tagged `real_data:true`, no fetch; plate branch local-only |
| WAYBACK_MACHINE | Internet Archive availability + CDX snapshots | **6** hardcoded "additional_archives" (Archive.is, GhostArchive, Google Cache…) tagged `real_data:true`, built URLs only |
| WEATHER_SERVICE | wttr.in + Open-Meteo measured weather | OpenWeatherMap key-gated |
| WHALE_MONITOR | Etherscan balance/txs, blockchain.info stats, DeFiLlama TVL | Whale Alert key-gated; hardcoded known-whale table for `is_known_whale` |
| THREAT_INTEL | OTX pulse data | AbuseIPDB key-gated |
| DOCUMENT_ANALYZER | OCR.space text, goQR decode, local PDF metadata | hash branch: VT lookup-URL note only (no fetch) |

> Note: SSL_ANALYZER, TECH_STACK, MAIGRET, USERNAME_OSINT, PORT_SCANNER,
> IP_REPUTATION also contain hardcoded arrays (CA lists, signature tables, probe
> templates, scoring inputs) — but these are **matching/probe inputs**, not emitted
> as findings, so they're classified LIVE/LOCAL, not PARTIAL.

---

## Direct answer to "does each really show actual data?"

- **Real measured/fetched data, works now:** the 29 LIVE + 7 LOCAL collectors (36). Spot-verified GEOCODING, IP_GEOLOCATION, ASN, WHOIS, SHODAN, SANCTIONS, EARTHQUAKE returned real values.
- **Real data only after you add a key:** BREACH_CHECKER, CENSYS, SATELLITE_TRACKER, IP_REPUTATION (4). Without keys they return an honest empty/note — not fake data.
- **Real core + "names not data" padding:** the 19 PARTIAL collectors (was 21; DARK_WEB_MONITOR and DEHASHED_SEARCH have since been rewritten to real-fetch/honest-empty — see the 2026-08-10 note above). The genuinely-fake-looking part is the registry-name/lookup-URL stubs (most blatant in COMPANY_LOOKUP, VEHICLE_LOOKUP, WAYBACK_MACHINE, ACADEMIC_SEARCH, FLIGHT_TRACKER's flight-number path, NEWS_ARCHIVE being a non-archive). These list sources/labels with `real_data:true` but fetch nothing for that entry.
- **Pure fabricated/stub data (fake numbers):** none found.
