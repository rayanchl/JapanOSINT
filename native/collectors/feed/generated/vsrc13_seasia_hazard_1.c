/* Verified-live seasia_hazard sources (28), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* sas-bmkg-adm-11 RETIRED 2026-09-19. cuaca.bmkg.go.id answers HTTP 403 to
 * every non-browser client — verified from the engine and from a plain probe
 * with an honest client string. The only thing that would make it fetch is a
 * browser User-Agent, and this tree does not impersonate browsers (see the
 * UA_OVERRIDE note in core/httpclient.c: those twelve entries are honest
 * product/contact strings). A row that can never return a record is the
 * silent-nothing house rule 1 names, so it is removed rather than left to
 * fail forever. The id stays listed in collectors/existing_ids.txt so it is
 * not recycled. Its sibling adm-12/13/14/16/18/33 rows are unaffected and
 * still fetch. */

VJSON(sas_bmkg_adm_12, "sas-bmkg-adm-12", "BMKG — weather forecast for Sumatera Utara", "BMKG — Sumatera Utaraの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=12",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for Sumatera Utara administrative areas.");

VJSON(sas_bmkg_adm_13, "sas-bmkg-adm-13", "BMKG — weather forecast for Sumatera Barat", "BMKG — Sumatera Baratの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=13",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for Sumatera Barat administrative areas.");

VJSON(sas_bmkg_adm_14, "sas-bmkg-adm-14", "BMKG — weather forecast for Riau", "BMKG — Riauの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=14",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for Riau administrative areas.");

VJSON(sas_bmkg_adm_16, "sas-bmkg-adm-16", "BMKG — weather forecast for Sumatera Selatan", "BMKG — Sumatera Selatanの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=16",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for Sumatera Selatan administrative areas.");

VJSON(sas_bmkg_adm_18, "sas-bmkg-adm-18", "BMKG — weather forecast for Lampung", "BMKG — Lampungの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=18",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for Lampung administrative areas.");

VJSON(sas_bmkg_adm_33, "sas-bmkg-adm-33", "BMKG — weather forecast for Jawa Tengah", "BMKG — Jawa Tengahの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=33",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for Jawa Tengah administrative areas.");

VJSON(sas_bmkg_adm_34, "sas-bmkg-adm-34", "BMKG — weather forecast for DI Yogyakarta", "BMKG — DI Yogyakartaの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=34",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for DI Yogyakarta administrative areas.");

VJSON(sas_bmkg_adm_35, "sas-bmkg-adm-35", "BMKG — weather forecast for Jawa Timur", "BMKG — Jawa Timurの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=35",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for Jawa Timur administrative areas.");

VJSON(sas_bmkg_adm_36, "sas-bmkg-adm-36", "BMKG — weather forecast for Banten", "BMKG — Bantenの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=36",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for Banten administrative areas.");

VJSON(sas_bmkg_adm_51, "sas-bmkg-adm-51", "BMKG — weather forecast for Bali", "BMKG — Baliの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=51",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for Bali administrative areas.");

VJSON(sas_bmkg_adm_53, "sas-bmkg-adm-53", "BMKG — weather forecast for Nusa Tenggara Timur", "BMKG — Nusa Tenggara Timurの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=53",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for Nusa Tenggara Timur administrative areas.");

VJSON(sas_bmkg_adm_73, "sas-bmkg-adm-73", "BMKG — weather forecast for Sulawesi Selatan", "BMKG — Sulawesi Selatanの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=73",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for Sulawesi Selatan administrative areas.");

VJSON(sas_bmkg_adm_94, "sas-bmkg-adm-94", "BMKG — weather forecast for Papua", "BMKG — Papuaの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=94",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for Papua administrative areas.");

VJSON(sas_bmkg_cuaca_31, "sas-bmkg-cuaca-31", "BMKG — weather forecast for DKI Jakarta", "BMKG — DKI Jakartaの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=31",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for DKI Jakarta administrative areas.");

VJSON(sas_bmkg_cuaca_32, "sas-bmkg-cuaca-32", "BMKG — weather forecast for Jawa Barat", "BMKG — Jawa Baratの気象予報",
  "seasia_hazard", "hazard",
  "https://cuaca.bmkg.go.id/api/df/v1/forecast/adm?adm1=32",
  "data",
  "id", "[\"idn\",\"weather\",\"forecast\"]", 10800,
  "Indonesian meteorological agency short-range weather forecast for Jawa Barat administrative areas.");

VJSON(sas_ckan_mm_disaster, "sas-ckan-mm-disaster", "Open Development Myanmar — disaster datasets", "Open Development Myanmar — disaster datasets",
  "seasia_hazard", "hazard",
  "https://data.opendevelopmentmyanmar.net/api/3/action/package_search?q=disaster&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"mmr\",\"disaster\",\"hazard\"]", 86400,
  "Myanmar cyclone, flood and earthquake hazard and impact datasets.");

VJSON(sas_hdx_idn, "sas-hdx-idn", "OCHA HDX — Indonesia humanitarian datasets", "OCHA HDX — Indonesia humanitarian datasets",
  "seasia_hazard", "hazard",
  "https://data.humdata.org/api/3/action/package_search?fq=groups:idn&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"idn\",\"humanitarian\",\"opendata\"]", 43200,
  "Humanitarian Data Exchange datasets scoped to Indonesia: population, displacement, hazard and 3W data.");

VJSON(sas_hdx_khm, "sas-hdx-khm", "OCHA HDX — Cambodia humanitarian datasets", "OCHA HDX — Cambodia humanitarian datasets",
  "seasia_hazard", "hazard",
  "https://data.humdata.org/api/3/action/package_search?fq=groups:khm&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"khm\",\"humanitarian\",\"opendata\"]", 43200,
  "Humanitarian Data Exchange datasets scoped to Cambodia: population, displacement, hazard and 3W data.");

VJSON(sas_hdx_lao, "sas-hdx-lao", "OCHA HDX — Laos humanitarian datasets", "OCHA HDX — Laos humanitarian datasets",
  "seasia_hazard", "hazard",
  "https://data.humdata.org/api/3/action/package_search?fq=groups:lao&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"lao\",\"humanitarian\",\"opendata\"]", 43200,
  "Humanitarian Data Exchange datasets scoped to Laos: population, displacement, hazard and 3W data.");

VJSON(sas_hdx_mmr, "sas-hdx-mmr", "OCHA HDX — Myanmar humanitarian datasets", "OCHA HDX — Myanmar humanitarian datasets",
  "seasia_hazard", "hazard",
  "https://data.humdata.org/api/3/action/package_search?fq=groups:mmr&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"mmr\",\"humanitarian\",\"opendata\"]", 43200,
  "Humanitarian Data Exchange datasets scoped to Myanmar: population, displacement, hazard and 3W data.");

VJSON(sas_hdx_phl, "sas-hdx-phl", "OCHA HDX — Philippines humanitarian datasets", "OCHA HDX — Philippines humanitarian datasets",
  "seasia_hazard", "hazard",
  "https://data.humdata.org/api/3/action/package_search?fq=groups:phl&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"phl\",\"humanitarian\",\"opendata\"]", 43200,
  "Humanitarian Data Exchange datasets scoped to Philippines: population, displacement, hazard and 3W data.");

VJSON(sas_hdx_tls, "sas-hdx-tls", "OCHA HDX — Timor-Leste humanitarian datasets", "OCHA HDX — Timor-Leste humanitarian datasets",
  "seasia_hazard", "hazard",
  "https://data.humdata.org/api/3/action/package_search?fq=groups:tls&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"tls\",\"humanitarian\",\"opendata\"]", 43200,
  "Humanitarian Data Exchange datasets scoped to Timor-Leste: population, displacement, hazard and 3W data.");

VJSON(sas_hdx_vnm, "sas-hdx-vnm", "OCHA HDX — Viet Nam humanitarian datasets", "OCHA HDX — Viet Nam humanitarian datasets",
  "seasia_hazard", "hazard",
  "https://data.humdata.org/api/3/action/package_search?fq=groups:vnm&rows=100&sort=id%20asc",
  "result.results",
  "en", "[\"vnm\",\"humanitarian\",\"opendata\"]", 43200,
  "Humanitarian Data Exchange datasets scoped to Viet Nam: population, displacement, hazard and 3W data.");

VRSS(sas_org_ahacentre, "sas-org-ahacentre", "ASEAN AHA Centre — disaster updates", "ASEAN AHA Centre — disaster updates",
  "seasia_hazard", "hazard",
  "https://ahacentre.org/feed/",
  "en", "[\"asean\",\"disaster\",\"humanitarian\"]", 3600,
  "ASEAN disaster management centre situation updates and flash reports for member states.");

VRSS(sas_org_mrc, "sas-org-mrc", "Mekong River Commission — news", "Mekong River Commission — news",
  "seasia_hazard", "environment",
  "https://www.mrcmekong.org/feed/",
  "en", "[\"mekong\",\"water\",\"transboundary\"]", 3600,
  "Mekong River Commission announcements on river flow, drought, dams and basin cooperation.");

VRSS(sas_tmd_eq_inside, "sas-tmd-eq-inside", "Thai Meteorological Department — domestic earthquakes", "Thai Meteorological Department — domestic earthquakes",
  "seasia_hazard", "hazard",
  "https://earthquake.tmd.go.th/feed/rss_inside.xml",
  "th", "[\"tha\",\"earthquake\",\"seismic\"]", 1800,
  "Earthquakes located inside Thailand by the Thai Meteorological Department.");

VRSS(sas_tmd_eq_rss, "sas-tmd-eq-rss", "Thai Meteorological Department — earthquake reports", "Thai Meteorological Department — earthquake reports",
  "seasia_hazard", "hazard",
  "https://earthquake.tmd.go.th/feed/rss_tmd.xml",
  "th", "[\"tha\",\"earthquake\",\"seismic\"]", 1800,
  "Earthquakes located by the Thai Meteorological Department seismic network, regional and global.");
