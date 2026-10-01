# Batch: 100 High-Leverage World OSINT / SIGINT / Open-Feed Collectors

Added 100 new self-registering `source_def` collectors across 5 files. Every
source is a **real RSS/Atom endpoint** routed through the shared `rss_collect`
toolkit (`lib/rss_atom.h`), so each feed entry lands as a genuine FTS-indexed
intel row keyed by guid/link — no stubs, no fixtures. All are `free_tier=1`,
`type="web_request"`, scheduled (`update_interval_sec>0`), and auto-globbed by
the Makefile (`collectors/sources/*.c`). IDs verified unique against the
existing 605.

These extend the largely JP-domestic collector set with worldwide coverage that
the OSINT entity/alert hooks can pivot on.

| File | Count | Focus |
|------|-------|-------|
| `intel_threat_world.c`     | 28 | Cyber-threat / SIGINT: CERT advisories (CISA, NCSC-UK, CERT-EU), vendor threat research (Talos, Unit 42, Mandiant-adjacent, Securelist, Check Point, ESET, Sophos, Rapid7, Intel 471), incident reporting (BleepingComputer, Krebs, The DFIR Report, The Record) |
| `intel_vuln_world.c`       | 12 | Vulnerability / exploit disclosure: CVEFeed, Exploit-DB, Packet Storm, Full Disclosure & oss-security, ZDI (published + upcoming), VulDB, Tenable TRA, WPScan, Talos disclosures |
| `intel_geoint_conflict.c`  | 20 | Geopolitical / conflict / investigative: Bellingcat, ISW, Long War Journal, OCCRP, ICIJ, CFR, War on the Rocks, The Diplomat, Defense News, Breaking Defense, Naval News, 38 North, Meduza, Kyiv Independent, RFE/RL, SCMP, Maritime Executive |
| `intel_worldnews.c`        | 22 | Global open-source news intelligence — world desks across every region (BBC, Guardian, Al Jazeera, DW, France24, NPR, CNN, NYT, AP, SCMP, Times of India, Straits Times, Korea Herald, Jerusalem Post/ToI, Africanews, M&G, MercoPress, Rio/BA Times, Nikkei Asia) |
| `intel_gov_disaster.c`     | 18 | Real-time gov / disaster / geo-hazard open feeds: GDACS, USGS quake streams (M2.5/M4.5/significant), Smithsonian volcano activity, US NWS alerts, ReliefWeb, State Dept & FCDO travel advisories, FBI, Europol, OFAC actions, UN/NATO, WHO/CDC/ECDC health, NASA Earth Observatory |

**Total: 100 sources.**

## Notes / next steps
- Live geo-hazard feeds (GDACS, USGS, NWS) poll every 10–15 min; advisory/news
  feeds hourly; weekly bulletins (volcano) on longer intervals.
- A handful of endpoints depend on third-party aggregation (e.g. AP via RSSHub);
  if an upstream URL rotates, `rss_collect` returns `-1` and the source surfaces
  as errored in the dashboard rather than emitting bad rows.
- Camera/webcam surveillance already has broad coverage in the repo (13
  `cam_*.c` collectors: insecam, worldcam, webcamtaxi, skylinewebcams, windy,
  youtube-live, etc.); this batch deliberately did **not** add speculative
  camera scrapers. Say the word to extend that set (e.g. traffic-DOT 511 cam
  APIs, additional public webcam networks).
