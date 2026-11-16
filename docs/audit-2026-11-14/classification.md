# Collector classification audit (2026-11-14)

Scope: `native/tools/gen_collectors_def.py` and what it regenerates (`native/core/collectors.def`; the census under `docs/source-pages-revamp/` regenerated identically). No collector `.c`, source id, URL or category was touched. Registry: 85,767 sources, 654 collector keys, 23 groups.

Data used: `./bin/japanosint --collector-census`, `--list-sources` (id -> collector) joined to the `sources` table (name, category, url) of a scratch DB, so every judgement below is from sampled source names, not from key names.

## 1. Measurements (before)

* The committed def equals the script's output on the current census (654 keys, 0 unassigned).
* **(a) Misfiled.** 77 collectors (13,488 sources) have a group different from their dominant category's group; most are deliberate (a beat named for its content, e.g. `europe_env`, `hp3b29a_webcams`). The clearly wrong ones are listed in section 3 (20 collectors, 10,423 sources). Root cause of the largest: the **category fallback** filed ArcGIS layer beats by a 34-56% dominant category (`hp3b61_us` 3,908 and `hp3b60_us` 2,495 -> infrastructure, `hp3b58_arcgis` -> environment, `hp3b53_arcgislayers` -> government, `hp3b46_arcgislayers` -> transport) while 8 sibling layer beats were in Geospatial.
* **(b) Heterogeneous** (dominant category < 60%, >= 10 sources): 78 collectors, 19,221 sources, ~10,000 of which are outside the dominant category. Largest: `hp3b61_us` 45%, `hp3b59_jpgis` 47%, `hp3b60_us` 46%, `hp3b58_arcgis` 56%, `hp3b60_eu` 50%, `jp_webmedia` 49%, `science` 35%, `hp3b53_arcgislayers` 34%, `disaster` 38%.
* **(c) Keyword false matches: none measured.** Matching is token-boundary (`beat == kw`, or ends with `_kw`/`-kw`, or starts with `kw_`), so "corp"/"media"/"gov"/"geo" never fire inside another word (`jpwpmedia` does not match `media`). 277 keys resolve by keyword; every one resolves to the group its name says. The one real keyword problem is semantic: `*_gov` is a token for "government" but `seasia_gov`, `sasia_gov`, `korea_gov` hold mostly press (news 65/72/58%) -> two moved (section 3), `korea_gov` (41 src, 58%) left as heterogeneous.
* **(d) Balance (before):** Government 33.1% of sources; Geospatial 13.2%; Infrastructure 8.8%; Media 8.7%. Smallest: Internal 20 (0.02%), Sanctions 61, Space 208, Telecom 270. Internal/Sanctions/Space/Telecom are small but coherent; no group was merged or invented.
* **(e) File-name labels:** 253 collector keys (52,001 sources) labelled like `eu (batch 62)` / `northam_us_reg (hp2 table)`; of them 203 hp keys (41,599 sources) had no hand label at all.
* **(f) Unmapped categories:** 30 categories on real sources were in no CATEGORY_GROUP rule (`international` 489, `science` 446, `technology` 176, `lifestyle` 59, `routing` 30, `organization` 26, ... plus 1-source country beats such as `tax`, `customs`, `ip-registry`). `hp3b48_igo` (494) reached Government only because 5 of its rows carry category `government`.
* **(g) Sampled 5 sources from each of the 40 largest collectors** (`scratchpad/samples.txt`) plus the suspect ones. Judgements: government-site beats (`frgov_*`, `itgov_*`, `hp3b44_*gov`, `hp3b63_cn`, `hp3b43_jprss`, `us_local`) are right; `hp3b57_jpedu`, `hp3b34_ckanbulk` and the CKAN/Socrata catalogue beats were wrong; `hp3b48_igo` is mixed (IGOs, associations, **game studios and company blogs**: "increpare games", "Factorio Blog").

## 2. Before / after by group

Source % is of 85,767.

| group | coll. before | src before | % | coll. after | src after | % |
|---|---|---|---|---|---|---|
| grp_government | 137 | 28358 | 33.1% | 128 | 27508 | 32.1% |
| grp_legal | 32 | 1941 | 2.3% | 33 | 2032 | 2.4% |
| grp_corporate | 77 | 5106 | 6.0% | 76 | 5032 | 5.9% |
| grp_procurement | 48 | 187 | 0.2% | 48 | 187 | 0.2% |
| grp_opendata | 66 | 3821 | 4.5% | 75 | 4381 | 5.1% |
| grp_science | 26 | 2537 | 3.0% | 27 | 3697 | 4.3% |
| grp_health | 30 | 3921 | 4.6% | 30 | 3921 | 4.6% |
| grp_environment | 37 | 2982 | 3.5% | 36 | 1398 | 1.6% |
| grp_space | 4 | 208 | 0.2% | 4 | 208 | 0.2% |
| grp_cyber | 21 | 1321 | 1.5% | 20 | 1230 | 1.4% |
| grp_telecom | 9 | 270 | 0.3% | 9 | 270 | 0.3% |
| grp_infrastructure | 29 | 7527 | 8.8% | 26 | 1117 | 1.3% |
| grp_transport | 18 | 757 | 0.9% | 17 | 593 | 0.7% |
| grp_media | 27 | 7492 | 8.7% | 28 | 7573 | 8.8% |
| grp_social | 10 | 926 | 1.1% | 10 | 926 | 1.1% |
| grp_geo | 24 | 11360 | 13.2% | 29 | 19801 | 23.1% |
| grp_safety | 18 | 901 | 1.1% | 18 | 901 | 1.1% |
| grp_sanctions | 4 | 61 | 0.1% | 4 | 61 | 0.1% |
| grp_culture | 11 | 1543 | 1.8% | 11 | 1543 | 1.8% |
| grp_education | 4 | 3893 | 4.5% | 3 | 2733 | 3.2% |
| grp_surveillance | 8 | 313 | 0.4% | 8 | 313 | 0.4% |
| grp_osint | 10 | 322 | 0.4% | 10 | 322 | 0.4% |
| grp_internal | 4 | 20 | 0.0% | 4 | 20 | 0.0% |

Trade-off to know: Geospatial rose to 23.1% and Infrastructure/Environment fell (they had been holding ArcGIS municipal layers by accident). Machine-readable: `groups_after.tsv`.

## 3. Moves made (20 collectors, 10,423 sources = 12.2% of the registry)

| key | sources | old -> new | reason |
|---|---|---|---|
| `hp3b61_us` | 3908 | grp_infrastructure -> grp_geo | municipal/county ArcGIS layers (parcels, wards, hydrants); beat dominant category only 45% and reached infrastructure via category fallback; ArcGIS beats belong with 'GIS servers' (grp_geo description) |
| `hp3b60_us` | 2495 | grp_infrastructure -> grp_geo | same family; 46% dominant, fallback only |
| `hp3b58_arcgis` | 1584 | grp_environment -> grp_geo | EEA air quality, FEMA, state GIS layers; env 56% / geospatial 44%; fallback only |
| `hp3b57_jpedu` | 1160 | grp_education -> grp_science | 1,151/1,160 are *.ac.jp universities/colleges; the script's own group rule keeps universities in Science (itgov_universita, frgov_enseignement, hp3b41_jpuniwp) |
| `hp3b53_arcgislayers` | 290 | grp_government -> grp_geo | ArcGIS layer beat, 34% dominant; fallback only |
| `hp3b46_arcgislayers` | 164 | grp_transport -> grp_geo | ArcGIS layer beat, 37% dominant; fallback only |
| `hp3b23_jpopendata` | 155 | grp_government -> grp_opendata | 155/155 'G-Spatial Information Centre' CKAN organisation listings = open-data catalogue rows |
| `hp3b23_jplocal` | 100 | grp_government -> grp_opendata | 89/100 'open data catalogue (CKAN) - every dataset' rows |
| `hp3b22_breachcat` | 91 | grp_cyber -> grp_legal | 87/91 are GDPRhub data-protection decisions (category legal 99%); 4 are breach databases |
| `hp30tokyo_tokyo` | 90 | grp_government -> grp_opendata | 88/90 Tokyo Metropolitan open-data catalogue / municipal catalogues |
| `hp3b34_ckanbulk` | 87 | grp_government -> grp_opendata | 87/87 'every dataset with its resources' CKAN portals |
| `hp3b28_jpindustry` | 74 | grp_corporate -> grp_government | labour-bureau notices, prefecture statistics pages, subsidies (labor 66%); not companies/markets |
| `seasia_gov` | 64 | grp_government -> grp_media | sampled sources mostly news outlets (news 65%); relabelled |
| `hp3b29a_forgotten` | 51 | grp_government -> grp_opendata | 51/51 CKAN/Socrata dataset-search rows |
| `hp3_portals` | 30 | grp_government -> grp_opendata | 29/30 open-data portal catalogues |
| `sasia_gov` | 25 | grp_government -> grp_media | mostly news outlets (news 72%); relabelled |
| `hp3b34_socratacat` | 24 | grp_government -> grp_opendata | 24/24 Socrata dataset-catalogue search |
| `hp3_gov` | 16 | grp_government -> grp_opendata | GovData, data.gouv.fr, data.europa.eu catalogues |
| `hp3b19_media` | 8 | grp_media -> grp_government | FOI-request portals (Czech/German) + Wikimedia; category government 75% |
| `hp3_infra` | 7 | grp_infrastructure -> grp_opendata | Statistics Norway/Netherlands/Finland databases (not infrastructure) |
Explicit (no group change): `hp3b58_arcgis2/3`, `hp3b59_jpgis`, `hp3b60_eu`, `hp3b61_jp`, `hp3b62_eu`, `hp3b63_euwfs` -> geo and `hp3b63_euods` -> opendata are now stated in OVERRIDES rather than left to the category fallback; `hp3b48_igo` -> government stated explicitly with its caveat.

## 4. Labels

* 211 collector labels changed (41,968 sources). 203 hp keys that had only a file-name label now have a human label written from a sample of their sources (`LABELS_AUDIT` in the script; the `(batch N)` suffix is kept as provenance). Examples: `hp3b62_eu` "eu (batch 62)" -> "Europe — municipal & regional ArcGIS layers (batch 62)"; `hp3b44_usgov` "usgov (batch 44)" -> "United States — local governments: cities, counties, townships, tribes, districts (batch 44)"; `hp2_northam_us_reg` -> "United States — federal regulators: CMS, NTSB, SEC (hp2)"; `net_infra` "net infra" -> "Network infrastructure (BGP, DNS, exchanges)".
* No hp key falls back to a raw label any more, and the generator now prints `WARNING: hp key X has no human label` on stderr if a future beat does (non-fatal, so the build still gates only on unassigned).
* Japanese labels were not added for the new labels (None, as for many existing ones).
* CATEGORY_GROUP gained 36 entries for categories no rule mapped (fallback coverage; no assignment changed by them). Still unmapped: `organization` (26 sources, all in override-assigned collectors) and the empty category (264).

## 5. Remaining problems (not fixable at collector level)

Recommended per-row `.collector` splits, like `tools/split_beats_by_body_type.py` did (counts from the census):
1. **ArcGIS layer beats** `hp3b61_us` 3,908, `hp3b60_us` 2,495, `hp3b59_jpgis` 2,543, `hp3b62_eu` 4,535, `hp3b60_eu` 1,264, `hp3b58_arcgis*` 2,947, `hp3b61_jp` 761, `hp3b53/46_arcgislayers` 454 (~19k): one collector mixes parcels, wards, hydrants, flood zones, schools. They now sit honestly in Geospatial, but a split by each layer's own category (already stored per source: infrastructure/safety/environment/education...) would let Infrastructure, Safety and Environment show them. Dominant share is 34-56%.
2. **`hp3b48_igo` (494)**: IGOs and associations mixed with game studios and company blogs; needs a split by body type (IGO/association vs company/blog) — currently Government.
3. **Government is still 32.1%** and is genuinely government sites (US/UK/CN/JP/FR/IT municipalities); the only further reduction is a split by body type within `hp3b44_usgov` (2,072: cities, townships, tribes, transit districts, schools), `hp3b47_wdgov` (1,559: ministries, agencies, research funders, universities), `government` (1,505: gov.uk bodies).
4. **Mixed regional/theme bags** (heterogeneous, 87 collectors of any size, 78 with >= 10 sources / 19,221 sources): `science` (396), `academic` (371), `disaster` (253), `infrastructure` (196), `health` (192), `jp_local` (182), `jp_prefectural` (185), `hp3b19_eurasia/latam/usstate/mena/apac/africa` (~790), `korea_gov` (41), `us_state` (195), `jp_webmedia` (544: media 219 / technology 167 / lifestyle 59, all filed Media).
5. **`news` (1,003)** and `hp3b57_jpcult` hold some company/school sites; `hp3b45_jpwporg`/`hp3b54_jpwporg` (39) are associations filed Legal by an earlier decision (category `organization` maps nowhere).
6. **Judgement calls to confirm with the owner**: (i) universities in Science vs Education (`hp3b57_jpedu`, 1,160, followed the script's documented line; if Education should hold universities, 4 collectors move: this one, `itgov_universita` 150, `frgov_enseignement` 125, `hp3b41_jpuniwp`/`hp3b42_jpcptuni`); (ii) `hp3b25_jpgeospatial` (88, G-Spatial CKAN, kept Geospatial) vs the moved `hp3b23_jpopendata` (same publisher, now Open data); (iii) `earthobs` (129, STAC catalogues, dominant `geodata`) sits in Environment.

## 6. Verification (run from /home/user/JapanOSINT/native, JO_DB=scratchpad/cls.db)

* `python3 tools/gen_collectors_def.py <census>` -> `# 654 collectors, 23 groups, 0 unassigned`, exit 0, no WARNING lines.
* `make collectors-def` -> regenerated `core/collectors.def` (446-line diff: 20 group changes, 211 labels); census 2026-11-14 identical.
* `make` -> `built bin/japanosint` (def compiled in).
* `make collectors-def-check` -> `collectors-def-check: every registered collector has a def line` (0 unassigned).
* `tests/unit/test_sourceapi.c` (only unit test that reads groups/collectors.def: asserts 0 unassigned collectors, every group non-empty, `grp_government` answers as a collector, collectors partition the registry) built and run through a temp copy of `tests/unit/run.sh` restricted to that test (deleted afterwards): `test_sourceapi: all passed`, plus run.sh's two python self-tests passed. The full `make unit` (~8 min) was NOT run: no other test references groups/the def (grep of tests/).
* Static checks on the script: no duplicate keys in OVERRIDES/LABELS/CATEGORY_GROUP (ast walk).
* Not committed; working tree: `native/tools/gen_collectors_def.py`, `native/core/collectors.def`. (`native/.tmp` was already untracked before this work.)
