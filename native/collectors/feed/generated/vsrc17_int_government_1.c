/* Verified-live int_government sources (15), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "_vjson_idkeys.inc"   /* VJSON_IDKEYS: the UNHCR rows below (2026-11-07) */

VJSON(unesco_uis_geounits, "unesco-uis-geounits", "UNESCO UIS — geographic units", "UNESCO UIS — geographic units",
  "int_government", "government",
  "https://api.uis.unesco.org/api/public/definitions/geounits",
  "",
  "en", "[\"int\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "462 UIS geographic units with ISO3 id, name and type (NATIONAL/REGIONAL) — the geography codelist for UNESCO education statistics. The sibling indicators endpoint is already in the tree; the actual data endpoint is NOT returned because its records carry no name-ish or id-ish field.");

/* 2026-11-07: the year was frozen at 2024 (yearFrom=yearTo=2024), so from the day 2025 was
 * published this row re-read a closed year. UNHCR publishes year X in mid X+1: measured
 * 2026-11-07, 2024 and 2025 answer records and 2026 answers maxPages 0. {date-30m:%Y}..
 * {date-18m:%Y} reads the two most recent published years (2024..2025 today, 2025..2026 from
 * July 2027), within 13 pages of 1000. Keyed on the record's own dimensions so two years
 * cannot collapse onto each other (no field here is in VJSON's id list, so the fallback
 * keyed on the title): 6,009 records in 2025, 6,009 distinct whole and 6,009 under the key (year+coo_id+coa_id alone is 3,967). */
VJSON_IDKEYS(unhcr_asylum_applications, "unhcr-asylum-applications", "UNHCR — asylum applications", "UNHCR — asylum applications",
  "int_government", "government",
  "https://api.unhcr.org/population/v1/asylum-applications/?limit=1000&per_page=1000&page=1&yearFrom={date-30m:%Y}&yearTo={date-18m:%Y}&coo_all=true&coa_all=true",
  "items",
  "en", "[\"int\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "New asylum applications lodged, by origin and asylum country, with procedure type, application type, decision level and protection category.",
  "year+coo_id+coa_id+procedure_type+app_type+dec_level+app_pc");

/* 2026-11-07: the year was frozen at 2024 (yearFrom=yearTo=2024), so from the day 2025 was
 * published this row re-read a closed year. UNHCR publishes year X in mid X+1: measured
 * 2026-11-07, 2024 and 2025 answer records and 2026 answers maxPages 0. {date-30m:%Y}..
 * {date-18m:%Y} reads the two most recent published years (2024..2025 today, 2025..2026 from
 * July 2027), within 13 pages of 1000. Keyed on the record's own dimensions so two years
 * cannot collapse onto each other (no field here is in VJSON's id list, so the fallback
 * keyed on the title): 5,485 in 2025, 5,485 distinct whole and under the key (year+coo_id+coa_id alone is 3,989). */
VJSON_IDKEYS(unhcr_asylum_decisions, "unhcr-asylum-decisions", "UNHCR — asylum decisions", "UNHCR — asylum decisions",
  "int_government", "government",
  "https://api.unhcr.org/population/v1/asylum-decisions/?limit=1000&per_page=1000&page=1&yearFrom={date-30m:%Y}&yearTo={date-18m:%Y}&coo_all=true&coa_all=true",
  "items",
  "en", "[\"int\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Asylum decision outcomes per origin/asylum pair: recognised, other outcome, rejected, closed, total, plus procedure type, decision level and protection category. 2.16 million decisions in 2024 alone.",
  "year+coo_id+coa_id+procedure_type+dec_level+dec_pc");

/* 2026-11-07: the year was frozen at 2024 (yearFrom=yearTo=2024), so from the day 2025 was
 * published this row re-read a closed year. UNHCR publishes year X in mid X+1: measured
 * 2026-11-07, 2024 and 2025 answer records and 2026 answers maxPages 0. {date-30m:%Y}..
 * {date-18m:%Y} reads the two most recent published years (2024..2025 today, 2025..2026 from
 * July 2027), within 13 pages of 1000. Keyed on the record's own dimensions so two years
 * cannot collapse onto each other (no field here is in VJSON's id list, so the fallback
 * keyed on the title): 180 in 2025, 180 under the key. */
VJSON_IDKEYS(unhcr_demographics, "unhcr-demographics", "UNHCR — demographics of displaced populations", "UNHCR — demographics of displaced populations",
  "int_government", "government",
  "https://api.unhcr.org/population/v1/demographics/?limit=1000&per_page=1000&page=1&yearFrom={date-30m:%Y}&yearTo={date-18m:%Y}&coa_all=true",
  "items",
  "en", "[\"int\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Age/sex disaggregation of displaced populations by asylum country: female and male counts in bands 0-4, 5-11, 12-17, 18-59, 60+, other and totals.",
  "year+coo_id+coa_id");

/* 2026-11-07: the year was frozen at 2024 (yearFrom=yearTo=2024), so from the day 2025 was
 * published this row re-read a closed year. UNHCR publishes year X in mid X+1: measured
 * 2026-11-07, 2024 and 2025 answer records and 2026 answers maxPages 0. {date-30m:%Y}..
 * {date-18m:%Y} reads the two most recent published years (2024..2025 today, 2025..2026 from
 * July 2027), within 13 pages of 1000. Keyed on the record's own dimensions so two years
 * cannot collapse onto each other (no field here is in VJSON's id list, so the fallback
 * keyed on the title): 6,290 in 2025, 6,290 under the key. */
VJSON_IDKEYS(unhcr_population, "unhcr-population", "UNHCR — refugee population by origin and asylum country", "UNHCR — refugee population by origin and asylum country",
  "int_government", "government",
  "https://api.unhcr.org/population/v1/population/?limit=1000&per_page=1000&page=1&yearFrom={date-30m:%Y}&yearTo={date-18m:%Y}&coo_all=true&coa_all=true",
  "items",
  "en", "[\"int\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Origin-country x asylum-country pairs with refugees, asylum_seekers, returned_refugees, IDPs, returned_idps, stateless, others of concern and host-community counts. coo_all/coa_all are REQUIRED — without them every row collapses to '-' and the parser drops the lot.",
  "year+coo_id+coa_id");

/* 2026-11-07: the year was frozen at 2024 (yearFrom=yearTo=2024), so from the day 2025 was
 * published this row re-read a closed year. UNHCR publishes year X in mid X+1: measured
 * 2026-11-07, 2024 and 2025 answer records and 2026 answers maxPages 0. {date-30m:%Y}..
 * {date-18m:%Y} reads the two most recent published years (2024..2025 today, 2025..2026 from
 * July 2027), within 13 pages of 1000. Keyed on the record's own dimensions so two years
 * cannot collapse onto each other (no field here is in VJSON's id list, so the fallback
 * keyed on the title): 846 in 2025, 846 under the key. */
VJSON_IDKEYS(unhcr_solutions, "unhcr-solutions", "UNHCR — durable solutions", "UNHCR — durable solutions",
  "int_government", "government",
  "https://api.unhcr.org/population/v1/solutions/?limit=1000&per_page=1000&page=1&yearFrom={date-30m:%Y}&yearTo={date-18m:%Y}&coo_all=true&coa_all=true",
  "items",
  "en", "[\"int\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Returned refugees, resettlement departures, naturalisations and returned IDPs by origin and asylum country — who actually left the caseload and how.",
  "year+coo_id+coa_id");

/* 2026-11-07: the year was frozen at 2024 (yearFrom=yearTo=2024), so from the day 2025 was
 * published this row re-read a closed year. UNHCR publishes year X in mid X+1: measured
 * 2026-11-07, 2024 and 2025 answer records and 2026 answers maxPages 0. {date-30m:%Y}..
 * {date-18m:%Y} reads the two most recent published years (2024..2025 today, 2025..2026 from
 * July 2027), within 13 pages of 1000. Keyed on the record's own dimensions so two years
 * cannot collapse onto each other (no field here is in VJSON's id list, so the fallback
 * keyed on the title): 5 in 2025, 5 under the key. */
VJSON_IDKEYS(unhcr_unrwa, "unhcr-unrwa", "UNHCR — UNRWA registered Palestine refugees", "UNHCR — UNRWA registered Palestine refugees",
  "int_government", "government",
  "https://api.unhcr.org/population/v1/unrwa/?limit=1000&per_page=1000&page=1&yearFrom={date-30m:%Y}&yearTo={date-18m:%Y}&coa_all=true",
  "items",
  "en", "[\"int\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "UNRWA-registered Palestine refugee totals by field of operation (State of Palestine, Jordan, Lebanon, Syria).",
  "year+coo_id+coa_id");

VJSON(unicef_sdmx_dataflows, "unicef-sdmx-dataflows", "UNICEF — SDMX dataflow catalogue", "UNICEF — SDMX dataflow catalogue",
  "int_government", "government",
  "https://sdmx.data.unicef.org/ws/public/sdmxapi/rest/dataflow/UNICEF?format=sdmx-json",
  "data.dataflows",
  "en", "[\"int\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "42 UNICEF statistical dataflows (CAUSE_OF_DEATH, immunisation, child protection…) with id, human name, multilingual names, version, agency and the URN of the data structure behind each — the map of what UNICEF publishes and how to query it.");

VJSON(unsd_sdg_geoarea_list, "unsd-sdg-geoarea-list", "UN Statistics — SDG geo areas", "UN Statistics — SDG geo areas",
  "int_government", "government",
  "https://unstats.un.org/sdgapi/v1/sdg/GeoArea/List",
  "",
  "en", "[\"int\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "460 UN M49 geographic areas (countries, regions, aggregates) with geoAreaCode and geoAreaName — the join key for every SDG observation.");

VJSON(unsd_sdg_series_data, "unsd-sdg-series-data", "UN Statistics — SDG observations (extreme poverty series)", "UN Statistics — SDG observations (extreme poverty series)",
  "int_government", "government",
  "https://unstats.un.org/sdgapi/v1/sdg/Series/Data?seriesCode=SI_POV_DAY1&pageSize=500",
  "data",
  "en", "[\"int\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Actual SDG observations: geoAreaCode/geoAreaName, time period, value, upper and lower bound, base period, the named source institution, footnotes and the full attribute/dimension breakdown. 11,407 rows for this one series; swap seriesCode for any of the 713. Array is at data — the response also carries attributes and dimensions arrays that would fool the auto-detector.");

VJSON(unsd_sdg_series_list, "unsd-sdg-series-list", "UN Statistics — SDG data series", "UN Statistics — SDG data series",
  "int_government", "government",
  "https://unstats.un.org/sdgapi/v1/sdg/Series/List",
  "",
  "en", "[\"int\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "713 SDG data series with series code (DC_ODA_BDVDL), the goals/targets/indicators each maps to, release version and full description — the code list you need to query Series/Data.");

VJSON(unsd_sdg_target_list, "unsd-sdg-target-list", "UN Statistics — SDG targets", "UN Statistics — SDG targets",
  "int_government", "government",
  "https://unstats.un.org/sdgapi/v1/sdg/Target/List",
  "",
  "en", "[\"int\",\"government\",\"batch17\",\"high-penetrancy\"]", 86400,
  "All 169 SDG targets with goal, code (1.1), full target text, description and the nested indicator list belonging to each target.");

