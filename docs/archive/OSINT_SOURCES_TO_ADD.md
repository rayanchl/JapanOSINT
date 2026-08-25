# OSINT Sources to Add — Full Catalog (any type, any country)

Legend: 🟢 free API · 🟡 free but scrape/needs-signup · 🔴 paid · ⭐ high-impact
Dedup note: this excludes what you already have (OpenCorporates, GLEIF, OpenSanctions,
SEC EDGAR, GDELT, Shodan/Censys/FOFA, VirusTotal, WiGLE, OpenSky/Flightradar,
MarineTraffic, crt.sh, HIBP, Wayback, and the ~500 Japan collectors).

Current state: 517 collectors, ~90% Japan-focused. People-search directory skews
US(47)/JP(32)/IN(13); Europe, CIS, China, Korea, SEA, LATAM, MENA, Africa are thin.

---

## PART 1 — Highest-impact GLOBAL force-multipliers (add these first)

1. ⭐🟢 **OCCRP Aleph** (aleph.occrp.org) — federated search over 250M+ leaked/public
   docs, company registries, court records, sanctions. One API, global. The single
   biggest multiplier for investigative OSINT.
2. ⭐🟡 **ICIJ Offshore Leaks DB** (offshoreleaks.icij.org) — Panama/Paradise/Pandora
   Papers entities, officers, addresses. Downloadable dataset + search.
3. ⭐🟢 **Wikidata / DBpedia SPARQL** — structured entity graph: people, orgs, places,
   relationships, identifiers (VIAF, ORCID, company IDs). Free, links everything.
4. ⭐🟢 **OpenStreetMap Overpass + Nominatim** — POIs, buildings, infra worldwide
   (verify you don't already have Overpass; you have JP OSM). Global geocode/reverse.
5. ⭐🟢 **GeoNames** (geonames.org) — 12M place names, admin hierarchy, alt-names,
   population. Global gazetteer for name→coords anywhere.
6. ⭐🟡 **LittleSis** (littlesis.org) — who-knows-who: power networks, board seats,
   donors, lobbying (US-centric but global elites). Free API.
7. ⭐🟢 **Wikipedia/Wikimedia REST + Wikimedia Commons** — biographies, geotagged
   media, structured infoboxes, any language.
8. ⭐🔴 **Sayari / Kharon / Dun & Bradstreet** — commercial beneficial-ownership +
   supply-chain graphs (the paid tier of what Aleph does free). Enterprise upsell.
9. 🟢 **OpenCorporates full API** (you have partial) — deepen to filings, officers,
   networks, jurisdictions (140+ registries normalized).
10. 🟢 **GLEIF Level-2 relationships** (you have LEI lookup) — parent/child corporate
    ownership tree via LEI-to-LEI, global.
11. 🟢 **Common Crawl / URLScan / Wayback CDX** — historical web presence of any
    domain/entity (you have Wayback; add URLScan search + CDX bulk).
12. ⭐🟢 **OpenSanctions "default" + FollowTheMoney** — you have OpenSanctions; add its
    full entity graph (crime, PEP, wanted, debarment) via the FTM API, not just names.

---

## PART 2 — By OSINT TYPE

### A. Corporate registries & beneficial ownership (per-country, high value)
- 🟢 **UK Companies House API** — officers, filings, PSC (people w/ significant control).
- 🟢 **EU Business Registers (BRIS) / e-Justice** — pan-EU company search portal.
- 🟢 **Germany: Handelsregister / Unternehmensregister / OffeneRegister.de** (🟡 the
  open mirror is scrapeable + bulk).
- 🟢 **France: INPI RNE + data.inpi.fr + Pappers.fr (🟡 free tier)** — SIREN/SIRET,
  officers, beneficial owners (RBE), financials.
- 🟢 **Netherlands: KVK API** · **Belgium: KBO/BCE** · **Luxembourg: LBR/RCS**.
- 🟢 **Nordics: Brønnøysund (NO), CVR (DK, fully open API), Bolagsverket (SE),
  YTJ/PRH (FI)** — Scandinavia has the best open company data in the world.
- 🟡 **Italy: Registro Imprese / Telemaco** · **Spain: Registro Mercantil / BORME**.
- 🟢 **Switzerland: Zefix** · **Austria: Firmenbuch**.
- 🟢 **India: MCA21 / Zauba (🟡)** · **Singapore: ACRA BizFile (🔴)** ·
  **Hong Kong: ICRIS/CR** · **Australia: ASIC** · **NZ: Companies Office (🟢 open API)**.
- 🟡 **Russia: EGRUL/Rusprofile/List-Org/Kartoteka** · **Ukraine: YouControl/Opendatabot/
  Clarity Project** · **Kazakhstan: adata.kz**.
- 🟢 **Canada: Corporations Canada + provincial (OpenCorporates covers)**.
- 🟢 **Brazil: Receita CNPJ (open) / brasil.io** · **Mexico: RPC/SAT** ·
  **Argentina: cuitonline** · other LATAM via OpenCorporates.
- 🟢 **US: SEC EDGAR (have) + state SoS (Delaware, CA, NY), OpenCorporates, FTC, USASpending**.

### B. Sanctions, PEP, watchlists, courts, legal
- 🟢 **OFAC SDN + Consolidated (US Treasury)** · **EU Consolidated Sanctions** ·
  **UK OFSI** · **UN Security Council** · **Interpol Red Notices API**.
- 🟢 **World Bank Debarred Firms** · **EU Early Detection (EDES)** · **ADB/AfDB debarment**.
- 🟢 **CourtListener/RECAP (US federal)** (you have court_records) · add **PACER (🔴)**,
  **UK BAILII / Judiciary**, **EU Curia (CJEU)**, **Canada CanLII**, **Australia AustLII**,
  **India Kanoon (🟡)**.
- 🟢 **US: PACER, state court portals, county recorder deeds, UCC filings**.
- 🟡 **OpenSanctions "peps" dataset** — PEP lists by country (you have the name check;
  add the structured PEP graph).

### C. Domain / IP / infrastructure / certificate / ASN
- 🟢 **RDAP (IANA/ICANN)** — modern WHOIS for any TLD (structured JSON).
- 🟢 **Certificate Transparency: Censys CT, Certspotter, Facebook CT, Google CT** (you
  have crt.sh; add mirrors for resilience).
- 🟢 **BGP: RIPEstat, bgp.tools, RouteViews, Team Cymru IP-to-ASN** (verify overlap).
- 🟢 **Passive DNS: Mnemonic PDNS (🟢 free), CIRCL PDNS (🟡), DNSDB (🔴 Farsight)**.
- 🟢 **Rapid7 Open Data / Project Sonar** — internet-wide scans (bulk, free for research).
- 🟢 **URLScan.io search API** · **Netcraft** · **BuiltWith (🔴)/Wappalyzer** for tech stack.
- 🟢 **IPinfo / ip-api / MaxMind GeoLite** (you have some) · **AbuseIPDB, GreyNoise (have)**.
- 🟢 **Onyphe, Netlas, ZoomEye, Quake/360, Fofa (have several)** — internet scan engines.
- 🟢 **crt.sh + Subdomain: Amass sources, SecurityTrails (🔴), chaos (have), Sublist3r feeds**.

### D. Breach / credential / leak / paste
- 🔴 **DeHashed, Snusbase, LeakCheck, IntelX, Hudson Rock, Have I Been Pwned (have most)**
  — these are your paid-key cyber tier; keep as BYOK.
- 🟢 **HIBP "breaches" catalog (metadata, free) + Pwned Passwords k-anonymity API**.
- 🟢 **Dehashed alternatives free-ish: LeakPeek, ProxyNova combolist search (🟡),
  BreachForums mirrors (🟡, volatile)**.
- 🟡 **Paste monitoring: Pastebin scraping, psbdmp, GitHub/GitLab secret search (have),
  Doxbin mirrors (have)**.

### E. Email / phone / username / people
- 🟢 **Username: WhatsMyName (fork of Sherlock, 600+ sites, JSON-driven), Maigret (have),
  Sherlock**.
- 🟢 **Email: Hunter.io (have), EmailRep, Have I Been Pwned, Gravatar (have),
  holehe (have), Mosint feeds**.
- 🟡 **Phone: NumVerify (🟡), Truecaller (🔴/unofficial), OpenCNAM, Twilio Lookup (🔴),
  libphonenumber carrier lookup (🟢 local)**.
- 🟡 **People aggregators by country (extend your _person_links directory):**
  - **DE:** Das Örtliche, 11880, DasTelefonbuch. **FR:** PagesBlanches, 118712.
  - **UK:** 192.com, ukphonebook. **IT:** PagineBianche. **ES:** PaginasBlancas.
  - **RU:** nomer.io, spravkaru. **BR:** telelistas, tel.search.
  - **KR:** people search + naver. **MENA:** dalili, Yellow Pages Gulf.
  - **Nordics you partly have (Ratsit/Hitta/Eniro SE); add NO (1881, gulesider),
    DK (krak, degulesider), FI (fonecta).**
  - **NL:** telefoonboek. **PL:** pf.pl. **Global:** IDCrawl, Pipl (🔴), Spokeo (🔴).

### F. Social media & messaging
- 🟢 **Reddit API (have some), Mastodon/Fediverse (have), Bluesky Jetstream (have),
  Lemmy, Nostr relays**.
- 🟡 **Telegram: TGStat, telemetr.io, channel search (have partial), tgcrawl**.
- 🟡 **X/Twitter: Nitter mirrors (have), snscrape, community-notes dump, API (🔴)**.
- 🟡 **TikTok, Instagram, Facebook — location/hashtag (have geo variants); official
  APIs 🔴/restricted**. **YouTube Data API (🟡)**, **Twitch (have)**.
- 🟢 **VK, OK.ru (Russia)** · **Weibo, Douyin, Zhihu (China, 🟡)** · **Naver/KakaoTalk
  open (Korea)** · **LINE Open Chat (JP/TW/TH)** · **Discord public (have)**.
- 🟢 **4chan/8kun/other imageboards (have 5ch), Kiwifarms mirrors (🟡)**.

### G. News, media, broadcast, RSS
- 🟢 **GDELT (have) + GDELT DOC 2.0 + Media Cloud (mediacloud.org, free)**.
- 🟢 **Common Crawl News, Google News RSS, Bing News, NewsAPI (🟡), currents,
  EventRegistry/NewsAPI.ai (🟡)**.
- 🟢 **RSS by country/outlet (you have JP outlets):** add BBC, Reuters, AP, AFP, DW,
  Rট, TASS, Xinhua, Al Jazeera, Le Monde, Spiegel, El País, per-region wire feeds.
- 🟡 **Broadcast/streaming: radio-browser.info (🟢 global radio stations), TVGarden,
  YouTube live geo (have)**.

### H. Aviation, maritime, transport (global, you're JP-strong)
- 🟢 **ADS-B: adsb.lol, adsb.fi, OpenSky (have), ADSBExchange (🟡)** — global aircraft.
- 🟢 **Aircraft registries: FAA (US), EASA, G-INFO (UK), per-country tail-number DBs**.
- 🟢 **Maritime: AISHub, aisstream.io (🟢 free WS), MarineTraffic (have), VesselFinder,
  Equasis (🟡 ownership), IMO GISIS, Paris MOU port inspections**.
- 🟢 **Rail/transit: Transitland, OpenRailwayMap, per-country GTFS feeds (you have JP)**.
- 🟢 **Flight schedules: OpenFlights, aviationstack (🟡)**. **Airport data: OurAirports (🟢)**.

### I. Satellite & imagery
- 🟢 **Sentinel Hub / Copernicus Data Space (🟢), NASA FIRMS fires (have), NASA GIBS,
  Landsat (have), Planet (🔴), Umbra/Capella SAR open data (🟢)**.
- 🟢 **Sentinel-2/1 via AWS Open Data, EO Browser, SentinelHub statistical API**.
- 🟢 **Fire/thermal: NASA FIRMS (have), VIIRS. Nightlights: VIIRS DNB, Black Marble**.
- 🟡 **Ship/vehicle detection: Skywatch, SkyFi (🔴). Historical imagery: Google Earth
  Engine (🟢 research), USGS EarthExplorer**.

### J. Crypto & blockchain
- 🟢 **Blockstream/mempool.space (BTC), Etherscan family (have?), Blockchair (multi-chain,
  🟡), Bitquery (🟡)**. **Address labels: Arkham (🟡), OFAC crypto SDN list (🟢),
  Chainalysis (🔴), Elliptic (🔴)**.
- 🟢 **ENS/unstoppable domains resolution, DeBank/Zapper portfolios (🟡)**.

### K. Financial markets, filings, economic
- 🟢 **SEC EDGAR (have), UK Companies House filings, ESMA/national regulators,
  OpenFIGI, Yahoo/Stooq quotes, FRED (US macro), World Bank, IMF, Eurostat, OECD,
  UN Comtrade (trade flows), ImportGenius/Panjiva (🔴 bills of lading)**.
- 🟢 **Lobbying/donations: US OpenSecrets/FEC (have FEC), EU Transparency Register,
  UK Electoral Commission, LobbyControl (DE)**.

### L. Academic, patents, research, IP
- 🟢 **OpenAlex (250M papers, 🟢 the big one), Crossref, Semantic Scholar, CORE,
  arXiv, PubMed, ORCID, ROR (institutions)**.
- 🟢 **Patents: Google Patents / USPTO PatentsView, EPO OPS (🟡), WIPO PATENTSCOPE,
  Espacenet, Lens.org (🟢)** — you have JP JPO; add global.
- 🟢 **Grants: NIH RePORTER, CORDIS (EU), UKRI, NSF** (you have JP KAKEN).

### M. Government transparency / procurement / lobbying / aid
- 🟢 **OpenSpending, TheyWorkForYou (UK), ProPublica APIs (US: Congress, Nonprofit
  Explorer 990s), USASpending, TED (EU tenders), UK Contracts Finder, OpenTender.eu,
  World Bank/UN procurement, IATI aid data, FTS OCHA**.
- 🟢 **Parliaments: EveryPolitician (archived), Wikidata, national open-data portals
  (data.gov, data.gov.uk, data.europa.eu, etc.)**.

### N. Images / faces / reverse image / geolocation
- 🟡 **Reverse image: Yandex (best for faces), Google Lens, Bing, TinEye (🟡),
  PimEyes (🔴 faces)**.
- 🟢 **EXIF/metadata: local exiftool. Geolocation helpers: SunCalc, shadow calc,
  Mapillary/KartaView (street-level imagery, 🟢), Flickr geo (have)**.

### O. Dark web / paste / underground (BYOK / volatile)
- 🔴/🟡 **IntelX (have), Ahmia (🟢 onion search), Tor2web mirrors, Dread mirrors,
  ransomware leak-site trackers (ransomwatch, ransomlook.io 🟢), breach forums (🟡)**.
- 🟢 **Ransomware.live / ransomlook.io API** — victim orgs by ransomware group (free,
  high signal for corporate threat OSINT).

---

## PART 3 — By COUNTRY / REGION (national registries & sources to add)

### Europe (biggest gap after JP)
- **EU-wide:** BRIS company registers, e-Justice, TED procurement, data.europa.eu,
  EU sanctions, ESMA, Curia, Transparency Register, EUR-Lex.
- **UK:** Companies House (⭐), 192.com, Land Registry (🔴), FCA register, Charity
  Commission, BAILII, TheyWorkForYou, Electoral Commission, gov.uk data.
- **Germany:** OffeneRegister/Handelsregister, Bundesanzeiger, Transparenzregister,
  Northdata (🟡), das Örtliche, Abgeordnetenwatch.
- **France:** INPI/Pappers/société.com, BODACC, JORF/Legifrance, HATVP (declarations),
  PagesJaunes.
- **Nordics (best open data):** CVR (DK ⭐ fully open), Brønnøysund (NO), Bolagsverket +
  Ratsit (SE), PRH/YTJ (FI). Person: 1881/gulesider (NO), krak (DK), fonecta (FI).
- **NL/BE/LU/CH/AT/IE/IT/ES/PT/PL/CZ:** KVK, KBO, LBR, Zefix, Firmenbuch, CRO (IE),
  Registro Imprese, BORME/Registro Mercantil, Publicações (PT), KRS (PL), justice.cz.

### Russia / CIS
- **Registries:** EGRUL/nalog.ru, Rusprofile, List-Org, Kartoteka, Kontur.Focus (🔴).
- **People/vehicle:** nomer.io, avinfo (cars), FSSP (bailiffs/debts), sudact (courts),
  GIBDD fines, VK/OK social. **Ukraine:** YouControl, Opendatabot, Clarity Project,
  Prozorro (procurement ⭐), NAZK declarations, Myrotvorets (🟡). **BY/KZ/GE/AM/AZ**
  national registries.

### China / Hong Kong / Taiwan
- **China:** Qichacha/Tianyancha/Aiqicha (🟡 company), Wikitongue, Weibo/Zhihu/Douyin,
  MIIT ICP/beian domain registry, China Judgements Online (裁判文书网), SIPO patents.
- **HK:** Companies Registry ICRIS, Land Registry. **TW:** GCIS company, judicial yuan.

### Korea
- **DART (⭐ Korean EDINET — corporate filings, free API), NICE/KED (🔴 credit),
  Supreme Court registry, Naver/Kakao, KISA whois, data.go.kr**.

### South & Southeast Asia
- **India:** MCA21, Zauba, GST portal, ECI (elections), Kanoon, DRT, data.gov.in.
- **SG:** ACRA, OneMap, data.gov.sg. **MY:** SSM. **ID:** AHU, OSS.
- **TH:** DBD company, **PH:** SEC-PH, **VN:** national business registration portal.

### Middle East / Gulf / Turkey / Israel
- **Registries:** UAE (DED/ADGM/DIFC), Saudi (MC/Qiwa), Qatar MOCI, Bahrain SIJILAT,
  Turkey Ticaret Sicil/MERSIS, Israel Rasham HaChavarot + Nevo (courts, 🔴).
- **People/dir:** dalili (UAE), yellowpages Gulf.

### Africa
- **SA:** CIPC (companies), deeds office, saflii (courts), IEC. **NG:** CAC. **KE:** BRS,
  eCitizen. **GH, EG (GAFI), MA, TN, RW (open registry ⭐).** Pan-Africa: OpenOwnership,
  Africa data portals.

### Latin America
- **BR:** Receita CNPJ (⭐ open), brasil.io, portal transparência, JusBrasil (courts 🟡),
  TSE (elections). **MX:** RPC, SAT, INE. **AR:** cuitonline, BORA. **CL:** SII, poder
  judicial. **CO:** RUES, **PE:** SUNARP/SUNAT. Most via OpenCorporates too.

### North America / Oceania
- **US:** state SoS registries, county deeds/UCC, USASpending, FEC (have), ProPublica
  990s, PACER, DEA/FDA/FCC/FAA registries, voter files (🟡 by state), OpenSecrets.
- **CA:** Corporations Canada + provincial, SEDAR+ (filings), CanLII.
- **AU:** ASIC, AustLII, ABR (business register 🟢), AEC. **NZ:** Companies Office (🟢),
  NZLII, data.govt.nz.

---

## PART 4 — PRIORITIZATION (add in this order)

Tier 1 — global multipliers, mostly free, huge coverage jump (do first):
  OCCRP Aleph ⭐, Wikidata SPARQL ⭐, GeoNames, OpenAlex, RDAP, OpenStreetMap Overpass
  (if missing), radio-browser, adsb.lol/aisstream, ICIJ Offshore Leaks, ransomlook.live,
  WhatsMyName username, Media Cloud, Sentinel/Copernicus.

Tier 2 — top national registries (best open data, direct investigative value):
  UK Companies House ⭐, Nordic registries (CVR/Brønnøysund) ⭐, France Pappers/INPI,
  Germany OffeneRegister, Korea DART ⭐, Brazil CNPJ ⭐, Ukraine YouControl/Prozorro,
  India MCA/ECI, NZ/AU ABR (open).

Tier 3 — breadth of people/phone directories per country (extend _person_links.inc):
  DE/FR/UK/IT/ES/RU/BR/KR/Gulf/Nordics phone books + IDCrawl/WhatsMyName.

Tier 4 — paid BYOK enterprise tier (monetization upsell, not core):
  Sayari, DNSDB/Farsight, PACER, Pipl/Spokeo, Chainalysis/Elliptic, Planet/SkyFi,
  ImportGenius/Panjiva, PimEyes.

Notes:
- Many national registries are 🟡 scrape or need a free signup; wrap them exactly like
  the JP scrape collectors (real fetch or honest-empty, per the no-fabrication rule).
- Non-Latin sources (RU/CN/KR/AR) need charset + tokenizer handling like the JP MeCab path.
- Country registries are best surfaced as entity-pivot (on-demand) collectors, not
  scheduled feeds — same pattern as your PERSON_SEARCH / COMPANY_SEARCH.
