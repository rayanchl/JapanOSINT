/* collectors/sources/us_feeds_batch50.c
 * Batch 50 — 915 United States government / institutional RSS-Atom feeds.
 *
 * Every row was fetched live on 2026-10-20: HTTP 200, an <rss>/<feed>/<rdf> root,
 * and at least one <item>/<entry>. URLs already registered anywhere in the tree
 * were dropped before probing.
 *
 * 841 of these were found by AUTODISCOVERY: the official homepage (from
 * Wikidata P856, or a host already in the tree) was fetched and its own
 * <link rel="alternate" type="application/rss+xml"> declarations were read —
 * the site was asked what it serves rather than probed at guessed paths.
 * Closed gov.uk bodies (gov.uk organisations API govuk_status=closed) poll weekly.
 * Per-row probe counts: docs/verified-sources-batch50.tsv. */
#include "_source_macros.inc"

/* ── crime ───────────────────────────────────────────────────────────── */

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o_9, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o-9",
     "Fed Register DOJ", "Fed Register DOJ",
     "crime", "safety",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-justice",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register DOJ\",\"safety\",\"batch:50\"]",
     3600,
     "Fed Register DOJ — safety feed (200 entries at probe)")

/* ── defense ─────────────────────────────────────────────────────────── */

RSSX(us_b50_af_mil_desktopmodules_articlecs_rss_ashx_contenttype_1_site_1_max_10, "us-b50-af-mil-desktopmodules-articlecs-rss-ashx-contenttype-1-site-1-max-10",
     "US Air Force News", "US Air Force News",
     "defense", "defense",
     "https://www.af.mil/DesktopModules/ArticleCS/RSS.ashx?ContentType=1&Site=1&max=10",
     "en", "[\"gov\",\"country:us\",\"publisher:US Air Force News\",\"defense\",\"batch:50\"]",
     3600,
     "US Air Force News — defense feed (10 entries at probe)")

RSSX(us_b50_defense_gov_desktopmodules_articlecs_rss_ashx_contenttype_1_site_945_category_0, "us-b50-defense-gov-desktopmodules-articlecs-rss-ashx-contenttype-1-site-945-category-0",
     "DOD All News", "DOD All News",
     "defense", "defense",
     "https://www.defense.gov/DesktopModules/ArticleCS/RSS.ashx?ContentType=1&Site=945&Category=0&max=20",
     "en", "[\"gov\",\"country:us\",\"publisher:DOD All News\",\"defense\",\"batch:50\"]",
     3600,
     "DOD All News — defense feed (20 entries at probe)")

RSSX(us_b50_defense_gov_desktopmodules_articlecs_rss_ashx_contenttype_400_site_945_max_10, "us-b50-defense-gov-desktopmodules-articlecs-rss-ashx-contenttype-400-site-945-max-10",
     "DOD Contracts", "DOD Contracts",
     "defense", "defense",
     "https://www.defense.gov/DesktopModules/ArticleCS/RSS.ashx?ContentType=400&Site=945&max=10",
     "en", "[\"gov\",\"country:us\",\"publisher:DOD Contracts\",\"defense\",\"batch:50\"]",
     3600,
     "DOD Contracts — defense feed (10 entries at probe)")

RSSX(us_b50_defense_gov_desktopmodules_articlecs_rss_ashx_max_10_contenttype_1_site_1, "us-b50-defense-gov-desktopmodules-articlecs-rss-ashx-max-10-contenttype-1-site-1",
     "US DoD News", "US DoD News",
     "defense", "defense",
     "https://www.defense.gov/DesktopModules/ArticleCS/RSS.ashx?max=10&ContentType=1&Site=1",
     "en", "[\"gov\",\"country:us\",\"publisher:US DoD News\",\"defense\",\"batch:50\"]",
     3600,
     "US DoD News — defense feed (10 entries at probe)")

RSSX(us_b50_dla_mil_desktopmodules_articlecs_rss_ashx_contenttype_1_site_760_max_10, "us-b50-dla-mil-desktopmodules-articlecs-rss-ashx-contenttype-1-site-760-max-10",
     "DLA News", "DLA News",
     "defense", "defense",
     "https://www.dla.mil/DesktopModules/ArticleCS/RSS.ashx?ContentType=1&Site=760&max=10",
     "en", "[\"gov\",\"country:us\",\"publisher:DLA News\",\"defense\",\"batch:50\"]",
     3600,
     "DLA News — defense feed (2 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o_3, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o-3",
     "Fed Register DOD", "Fed Register DOD",
     "defense", "defense",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-defense",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register DOD\",\"defense\",\"batch:50\"]",
     3600,
     "Fed Register DOD — defense feed (200 entries at probe)")

RSSX(us_b50_marines_mil_desktopmodules_articlecs_rss_ashx_contenttype_1_site_1_max_10, "us-b50-marines-mil-desktopmodules-articlecs-rss-ashx-contenttype-1-site-1-max-10",
     "US Marines News", "US Marines News",
     "defense", "defense",
     "https://www.marines.mil/DesktopModules/ArticleCS/RSS.ashx?ContentType=1&Site=1&max=10",
     "en", "[\"gov\",\"country:us\",\"publisher:US Marines News\",\"defense\",\"batch:50\"]",
     3600,
     "US Marines News — defense feed (10 entries at probe)")

RSSX(us_b50_pacom_mil_desktopmodules_articlecs_rss_ashx_contenttype_1_site_636_max_10, "us-b50-pacom-mil-desktopmodules-articlecs-rss-ashx-contenttype-1-site-636-max-10",
     "INDOPACOM News", "INDOPACOM News",
     "defense", "defense",
     "https://www.pacom.mil/DesktopModules/ArticleCS/RSS.ashx?ContentType=1&Site=636&max=10",
     "en", "[\"gov\",\"country:us\",\"publisher:INDOPACOM News\",\"defense\",\"batch:50\"]",
     3600,
     "INDOPACOM News — defense feed (9 entries at probe)")

RSSX(us_b50_spaceforce_mil_desktopmodules_articlecs_rss_ashx_contenttype_1_site_1_max_10, "us-b50-spaceforce-mil-desktopmodules-articlecs-rss-ashx-contenttype-1-site-1-max-10",
     "US Space Force News", "US Space Force News",
     "defense", "defense",
     "https://www.spaceforce.mil/DesktopModules/ArticleCS/RSS.ashx?ContentType=1&Site=1&max=10",
     "en", "[\"gov\",\"country:us\",\"publisher:US Space Force News\",\"defense\",\"batch:50\"]",
     3600,
     "US Space Force News — defense feed (10 entries at probe)")

RSSX(us_b50_spaceforce_mil_desktopmodules_articlecs_rss_ashx_contenttype_1_site_1060_max_10, "us-b50-spaceforce-mil-desktopmodules-articlecs-rss-ashx-contenttype-1-site-1060-max-10",
     "US Space Force News", "US Space Force News",
     "defense", "defense",
     "https://www.spaceforce.mil/DesktopModules/ArticleCS/RSS.ashx?ContentType=1&Site=1060&max=10",
     "en", "[\"gov\",\"country:us\",\"publisher:US Space Force News\",\"defense\",\"batch:50\"]",
     3600,
     "US Space Force News — defense feed (10 entries at probe)")

/* ── disaster ────────────────────────────────────────────────────────── */

RSSX(us_b50_earthquake_usgs_gov_earthquakes_v1_0_summary_1_0_hour, "us-b50-earthquake-usgs-gov-earthquakes-v1-0-summary-1-0-hour",
     "USGS M1.0+ Earthquakes Hourly", "USGS M1.0+ Earthquakes Hourly",
     "disaster", "disaster",
     "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/1.0_hour.atom",
     "en", "[\"gov\",\"country:us\",\"publisher:USGS M1.0+ Earthquakes Hourly\",\"disaster\",\"batch:50\"]",
     3600,
     "USGS M1.0+ Earthquakes Hourly — disaster feed (7 entries at probe)")

RSSX(us_b50_earthquake_usgs_gov_earthquakes_v1_0_summary_2_5_day, "us-b50-earthquake-usgs-gov-earthquakes-v1-0-summary-2-5-day",
     "USGS M2.5 Daily", "USGS M2.5毎日",
     "disaster", "disaster",
     "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/2.5_day.atom",
     "en", "[\"gov\",\"country:us\",\"publisher:USGS M2.5 Daily\",\"disaster\",\"batch:50\"]",
     3600,
     "USGS M2.5 Daily — disaster feed (49 entries at probe)")

RSSX(us_b50_earthquake_usgs_gov_earthquakes_v1_0_summary_4_5_week, "us-b50-earthquake-usgs-gov-earthquakes-v1-0-summary-4-5-week",
     "USGS M4.5 Weekly", "USGS M4.5週間",
     "disaster", "disaster",
     "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/4.5_week.atom",
     "en", "[\"gov\",\"country:us\",\"publisher:USGS M4.5 Weekly\",\"disaster\",\"batch:50\"]",
     3600,
     "USGS M4.5 Weekly — disaster feed (108 entries at probe)")

RSSX(us_b50_earthquake_usgs_gov_earthquakes_v1_0_summary_all_day, "us-b50-earthquake-usgs-gov-earthquakes-v1-0-summary-all-day",
     "USGS Earthquakes Daily", "USGS 地震 毎日",
     "disaster", "disaster",
     "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/all_day.atom",
     "en", "[\"gov\",\"country:us\",\"publisher:USGS Earthquakes Daily\",\"disaster\",\"batch:50\"]",
     3600,
     "USGS Earthquakes Daily — disaster feed (207 entries at probe)")

RSSX(us_b50_earthquake_usgs_gov_earthquakes_v1_0_summary_all_hour, "us-b50-earthquake-usgs-gov-earthquakes-v1-0-summary-all-hour",
     "USGS Earthquakes Hourly", "USGS 地震 毎時",
     "disaster", "disaster",
     "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/all_hour.atom",
     "en", "[\"gov\",\"country:us\",\"publisher:USGS Earthquakes Hourly\",\"disaster\",\"batch:50\"]",
     3600,
     "USGS Earthquakes Hourly — disaster feed (7 entries at probe)")

RSSX(us_b50_earthquake_usgs_gov_earthquakes_v1_0_summary_significant_month, "us-b50-earthquake-usgs-gov-earthquakes-v1-0-summary-significant-month",
     "USGS Significant Earthquakes", "USGS 重大地震",
     "disaster", "disaster",
     "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/significant_month.atom",
     "en", "[\"gov\",\"country:us\",\"publisher:USGS Significant Earthquakes\",\"disaster\",\"batch:50\"]",
     3600,
     "USGS Significant Earthquakes — disaster feed (15 entries at probe)")

/* ── finreg ──────────────────────────────────────────────────────────── */

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_commodity_fu, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-commodity-fu",
     "Fed Register CFTC", "Fed Register CFTC",
     "finreg", "finance",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=commodity-futures-trading-commission",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register CFTC\",\"finance\",\"batch:50\"]",
     3600,
     "Fed Register CFTC — finance feed (7 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_consumer_fin, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-consumer-fin",
     "Fed Register CFPB", "Fed Register CFPB",
     "finreg", "finance",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=consumer-financial-protection-bureau",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register CFPB\",\"finance\",\"batch:50\"]",
     3600,
     "Fed Register CFPB — finance feed (2 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o_13, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o-13",
     "Fed Register Treasury", "Fed Register Treasury",
     "finreg", "finance",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-the-treasury",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register Treasury\",\"finance\",\"batch:50\"]",
     3600,
     "Fed Register Treasury — finance feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_internal_rev, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-internal-rev",
     "Fed Register IRS", "Fed Register IRS",
     "finreg", "finance",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=internal-revenue-service",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register IRS\",\"finance\",\"batch:50\"]",
     3600,
     "Fed Register IRS — finance feed (32 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_securities_a, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-securities-a",
     "Fed Register SEC", "Fed Register SEC",
     "finreg", "finance",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=securities-and-exchange-commission",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register SEC\",\"finance\",\"batch:50\"]",
     3600,
     "Fed Register SEC — finance feed (200 entries at probe)")

/* ── us_environment ──────────────────────────────────────────────────── */

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_environmenta, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-environmenta",
     "Fed Register EPA", "Fed Register EPA",
     "us_environment", "environment",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=environmental-protection-agency",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register EPA\",\"environment\",\"batch:50\"]",
     3600,
     "Fed Register EPA — environment feed (63 entries at probe)")

/* ── us_government ───────────────────────────────────────────────────── */

RSSX(us_b50_boardofed_idaho_gov_board_of_education_all_committees, "us-b50-boardofed-idaho-gov-board-of-education-all-committees",
     "Higher Education Research Council — Idaho State Board of Education » Committees Feed", "Higher Education Research Council — Idaho State Board of Education » Committees Feed",
     "us_government", "government",
     "https://boardofed.idaho.gov/board-of-education/all-committees/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Higher Education Research Council — Idaho State Board of Education » Committees Feed\",\"government\",\"batch:50\"]",
     3600,
     "Higher Education Research Council — Idaho State Board of Education » Committees Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_census_gov_economic_indicators_indicator, "us-b50-census-gov-economic-indicators-indicator",
     "Census Indicators", "Census Indicators",
     "us_government", "government",
     "https://www.census.gov/economic-indicators/indicator.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Census Indicators\",\"government\",\"batch:50\"]",
     3600,
     "Census Indicators — government agency feed (18 entries at probe)")

RSSX(us_b50_chronicdata_cdc_gov_catalog, "us-b50-chronicdata-cdc-gov-catalog",
     "Data — Subscribe to Datasets", "Data — Subscribe to Datasets",
     "us_government", "government",
     "https://chronicdata.cdc.gov/catalog.rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Data — Subscribe to Datasets\",\"government\",\"batch:50\"]",
     3600,
     "Data — Subscribe to Datasets — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 40 entries at probe")

RSSX(us_b50_congress_gov_presented_to_president, "us-b50-congress-gov-presented-to-president",
     "Congress Bills to President", "Congress Bills to President",
     "us_government", "government",
     "https://www.congress.gov/rss/presented-to-president.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Congress Bills to President\",\"government\",\"batch:50\"]",
     3600,
     "Congress Bills to President — government agency feed (12 entries at probe)")

RSSX(us_b50_councilofthesouthernmountains_com_blog_feed, "us-b50-councilofthesouthernmountains-com-blog-feed",
     "Council of the Southern Mountains — CouncilSouthernMtn - RSS", "Council of the Southern Mountains — CouncilSouthernMtn - RSS",
     "us_government", "government",
     "https://www.councilofthesouthernmountains.com/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Council of the Southern Mountains — CouncilSouthernMtn - RSS\",\"government\",\"batch:50\"]",
     3600,
     "Council of the Southern Mountains — CouncilSouthernMtn - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_dars_virginia_gov, "us-b50-dars-virginia-gov",
     "Virginia Department for Aging and Rehabilitative Services", "Virginia Department for Aging and Rehabilitative Services",
     "us_government", "government",
     "https://dars.virginia.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Virginia Department for Aging and Rehabilitative Services\",\"government\",\"batch:50\"]",
     3600,
     "Virginia Department for Aging and Rehabilitative Services — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_datacatalog_cookcountyil_gov_catalog, "us-b50-datacatalog-cookcountyil-gov-catalog",
     "Cook County Open Data — Subscribe to Datasets", "Cook County Open Data — Subscribe to Datasets",
     "us_government", "government",
     "https://datacatalog.cookcountyil.gov/catalog.rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Cook County Open Data — Subscribe to Datasets\",\"government\",\"batch:50\"]",
     3600,
     "Cook County Open Data — Subscribe to Datasets — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 40 entries at probe")

RSSX(us_b50_defense_gov_desktopmodules_articlecs_rss_ashx_contenttype_1_site_945_max_10, "us-b50-defense-gov-desktopmodules-articlecs-rss-ashx-contenttype-1-site-945-max-10",
     "DOD News", "DOD News",
     "us_government", "government",
     "https://www.defense.gov/DesktopModules/ArticleCS/RSS.ashx?ContentType=1&Site=945&max=10",
     "en", "[\"gov\",\"country:us\",\"publisher:DOD News\",\"government\",\"batch:50\"]",
     3600,
     "DOD News — government agency feed (10 entries at probe)")

RSSX(us_b50_dma_wi_gov, "us-b50-dma-wi-gov",
     "Wisconsin Department of Military Affairs", "Wisconsin Department of Military Affairs",
     "us_government", "government",
     "https://dma.wi.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Wisconsin Department of Military Affairs\",\"government\",\"batch:50\"]",
     3600,
     "Wisconsin Department of Military Affairs — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_dps_nm_gov, "us-b50-dps-nm-gov",
     "New Mexico Department of Public Safety", "New Mexico Department of Public Safety",
     "us_government", "government",
     "https://www.dps.nm.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:New Mexico Department of Public Safety\",\"government\",\"batch:50\"]",
     3600,
     "New Mexico Department of Public Safety — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_energy_gov, "us-b50-energy-gov",
     "DOE News", "DOE News",
     "us_government", "government",
     "https://www.energy.gov/rss.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:DOE News\",\"government\",\"batch:50\"]",
     3600,
     "DOE News — government agency feed (10 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents, "us-b50-federalregister-gov-api-v1-documents",
     "Federal Register All Documents", "Federal Register All Documents",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Federal Register All Documents\",\"government\",\"batch:50\"]",
     3600,
     "Federal Register All Documents — government agency feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o",
     "Fed Register USDA", "Fed Register USDA",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-agriculture",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register USDA\",\"government\",\"batch:50\"]",
     3600,
     "Fed Register USDA — government agency feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o_10, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o-10",
     "Fed Register DOL", "Fed Register DOL",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-labor",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register DOL\",\"government\",\"batch:50\"]",
     3600,
     "Fed Register DOL — government agency feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o_11, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o-11",
     "Fed Register State", "Fed Register State",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-state",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register State\",\"government\",\"batch:50\"]",
     3600,
     "Fed Register State — government agency feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o_12, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o-12",
     "Fed Register Interior", "Fed Register Interior",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-the-interior",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register Interior\",\"government\",\"batch:50\"]",
     3600,
     "Fed Register Interior — government agency feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o_14, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o-14",
     "Fed Register DOT", "Fed Register DOT",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-transportation",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register DOT\",\"government\",\"batch:50\"]",
     3600,
     "Fed Register DOT — government agency feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o_2, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o-2",
     "Fed Register Commerce", "Fed Register Commerce",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-commerce",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register Commerce\",\"government\",\"batch:50\"]",
     3600,
     "Fed Register Commerce — government agency feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o_4, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o-4",
     "Fed Register Education", "Fed Register Education",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-education",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register Education\",\"government\",\"batch:50\"]",
     3600,
     "Fed Register Education — government agency feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o_5, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o-5",
     "Fed Register DOE", "Fed Register DOE",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-energy",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register DOE\",\"government\",\"batch:50\"]",
     3600,
     "Fed Register DOE — government agency feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o_7, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o-7",
     "Fed Register DHS", "Fed Register DHS",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-homeland-security",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register DHS\",\"government\",\"batch:50\"]",
     3600,
     "Fed Register DHS — government agency feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o_8, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o-8",
     "Fed Register HUD", "Fed Register HUD",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-housing-and-urban-development",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register HUD\",\"government\",\"batch:50\"]",
     3600,
     "Fed Register HUD — government agency feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_federal_comm, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-federal-comm",
     "Fed Register FCC", "Fed Register FCC",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=federal-communications-commission",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register FCC\",\"government\",\"batch:50\"]",
     3600,
     "Fed Register FCC — government agency feed (49 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_federal_trad, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-federal-trad",
     "Fed Register FTC", "Fed Register FTC",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=federal-trade-commission",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register FTC\",\"government\",\"batch:50\"]",
     3600,
     "Fed Register FTC — government agency feed (7 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_nuclear_regu, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-nuclear-regu",
     "Fed Register NRC", "Fed Register NRC",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=nuclear-regulatory-commission",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register NRC\",\"government\",\"batch:50\"]",
     3600,
     "Fed Register NRC — government agency feed (38 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_small_busine, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-small-busine",
     "Fed Register SBA", "Fed Register SBA",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=small-business-administration",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register SBA\",\"government\",\"batch:50\"]",
     3600,
     "Fed Register SBA — government agency feed (43 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5btype_5d_notice, "us-b50-federalregister-gov-api-v1-documents-conditions-5btype-5d-notice",
     "Federal Register Notices", "Federal Register Notices",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Btype%5D=NOTICE",
     "en", "[\"gov\",\"country:us\",\"publisher:Federal Register Notices\",\"government\",\"batch:50\"]",
     3600,
     "Federal Register Notices — government agency feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5btype_5d_presdocu, "us-b50-federalregister-gov-api-v1-documents-conditions-5btype-5d-presdocu",
     "Federal Register Presidential Documents", "Federal Register Presidential Documents",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Btype%5D=PRESDOCU",
     "en", "[\"gov\",\"country:us\",\"publisher:Federal Register Presidential Documents\",\"government\",\"batch:50\"]",
     3600,
     "Federal Register Presidential Documents — government agency feed (23 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5btype_5d_prorule, "us-b50-federalregister-gov-api-v1-documents-conditions-5btype-5d-prorule",
     "Federal Register Proposed Rules", "Federal Register Proposed Rules",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Btype%5D=PRORULE",
     "en", "[\"gov\",\"country:us\",\"publisher:Federal Register Proposed Rules\",\"government\",\"batch:50\"]",
     3600,
     "Federal Register Proposed Rules — government agency feed (167 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5btype_5d_rule, "us-b50-federalregister-gov-api-v1-documents-conditions-5btype-5d-rule",
     "Federal Register Rules", "Federal Register Rules",
     "us_government", "government",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Btype%5D=RULE",
     "en", "[\"gov\",\"country:us\",\"publisher:Federal Register Rules\",\"government\",\"batch:50\"]",
     3600,
     "Federal Register Rules — government agency feed (200 entries at probe)")

RSSX(us_b50_frcog_org, "us-b50-frcog-org",
     "Franklin Regional Council of Governments", "Franklin Regional Council of Governments",
     "us_government", "government",
     "https://frcog.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Franklin Regional Council of Governments\",\"government\",\"batch:50\"]",
     3600,
     "Franklin Regional Council of Governments — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_gcianj_com, "us-b50-gcianj-com",
     "Gloucester County Improvement Authority", "Gloucester County Improvement Authority",
     "us_government", "government",
     "https://www.gcianj.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Gloucester County Improvement Authority\",\"government\",\"batch:50\"]",
     3600,
     "Gloucester County Improvement Authority — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 2 entries at probe")

RSSX(us_b50_gogbt_com, "us-b50-gogbt-com",
     "Greater Bridgeport Transit Authority", "Greater Bridgeport Transit Authority",
     "us_government", "government",
     "https://gogbt.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Greater Bridgeport Transit Authority\",\"government\",\"batch:50\"]",
     3600,
     "Greater Bridgeport Transit Authority — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_govinfo_gov_hjournal, "us-b50-govinfo-gov-hjournal",
     "GovInfo House Journal", "GovInfo House Journal",
     "us_government", "government",
     "https://www.govinfo.gov/rss/hjournal.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:GovInfo House Journal\",\"government\",\"batch:50\"]",
     3600,
     "GovInfo House Journal — government agency feed (8 entries at probe)")

RSSX(us_b50_hcia_org, "us-b50-hcia-org",
     "Hudson County Improvement Authority", "Hudson County Improvement Authority",
     "us_government", "government",
     "https://hcia.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hudson County Improvement Authority\",\"government\",\"batch:50\"]",
     3600,
     "Hudson County Improvement Authority — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_health_data_ny_gov_catalog, "us-b50-health-data-ny-gov-catalog",
     "State of New York — Subscribe to Datasets", "State of New York — Subscribe to Datasets",
     "us_government", "government",
     "https://health.data.ny.gov/catalog.rss",
     "en", "[\"gov\",\"country:us\",\"publisher:State of New York — Subscribe to Datasets\",\"government\",\"batch:50\"]",
     3600,
     "State of New York — Subscribe to Datasets — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 40 entries at probe")

RSSX(us_b50_healthdata_gov_catalog, "us-b50-healthdata-gov-catalog",
     "HealthData.gov — Subscribe to Datasets", "HealthData.gov — Subscribe to Datasets",
     "us_government", "government",
     "https://healthdata.gov/catalog.rss",
     "en", "[\"gov\",\"country:us\",\"publisher:HealthData.gov — Subscribe to Datasets\",\"government\",\"batch:50\"]",
     3600,
     "HealthData.gov — Subscribe to Datasets — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 40 entries at probe")

RSSX(us_b50_hicscdata_hawaii_gov_catalog, "us-b50-hicscdata-hawaii-gov-catalog",
     "Catalog — Subscribe to Datasets", "Catalog — Subscribe to Datasets",
     "us_government", "government",
     "https://hicscdata.hawaii.gov/catalog.rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Catalog — Subscribe to Datasets\",\"government\",\"batch:50\"]",
     3600,
     "Catalog — Subscribe to Datasets — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 40 entries at probe")

RSSX(us_b50_houstonsports_org, "us-b50-houstonsports-org",
     "Houston Sports Authority", "Houston Sports Authority",
     "us_government", "government",
     "https://houstonsports.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Houston Sports Authority\",\"government\",\"batch:50\"]",
     3600,
     "Houston Sports Authority — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_mbta_com_news, "us-b50-mbta-com-news",
     "Massachusetts Bay Transportation Authority Police — MBTA.com Latest News", "Massachusetts Bay Transportation Authority Police — MBTA.com Latest News",
     "us_government", "government",
     "https://www.mbta.com:443/news/rss.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Massachusetts Bay Transportation Authority Police — MBTA.com Latest News\",\"government\",\"batch:50\"]",
     3600,
     "Massachusetts Bay Transportation Authority Police — MBTA.com Latest News — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_mobilehousing_org_format_feed_type_atom, "us-b50-mobilehousing-org-format-feed-type-atom",
     "Mobile Housing Board — Atom 1.0", "Mobile Housing Board — Atom 1.0",
     "us_government", "government",
     "https://mobilehousing.org/?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Mobile Housing Board — Atom 1.0\",\"government\",\"batch:50\"]",
     3600,
     "Mobile Housing Board — Atom 1.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 7 entries at probe")

RSSX(us_b50_mobilehousing_org_format_feed_type_rss, "us-b50-mobilehousing-org-format-feed-type-rss",
     "Mobile Housing Board", "Mobile Housing Board",
     "us_government", "government",
     "https://mobilehousing.org/?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Mobile Housing Board\",\"government\",\"batch:50\"]",
     3600,
     "Mobile Housing Board — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 7 entries at probe")

RSSX(us_b50_nasa_gov_dyn_breaking_news, "us-b50-nasa-gov-dyn-breaking-news",
     "NASA Breaking News", "NASA Breaking News",
     "us_government", "government",
     "https://www.nasa.gov/rss/dyn/breaking_news.rss",
     "en", "[\"gov\",\"country:us\",\"publisher:NASA Breaking News\",\"government\",\"batch:50\"]",
     3600,
     "NASA Breaking News — government agency feed (10 entries at probe)")

RSSX(us_b50_ndcorncouncil_org, "us-b50-ndcorncouncil-org",
     "North Dakota Corn Utilization Council", "North Dakota Corn Utilization Council",
     "us_government", "government",
     "https://ndcorncouncil.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:North Dakota Corn Utilization Council\",\"government\",\"batch:50\"]",
     3600,
     "North Dakota Corn Utilization Council — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_news_va_gov, "us-b50-news-va-gov",
     "VA News", "VA News",
     "us_government", "government",
     "https://news.va.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:VA News\",\"government\",\"batch:50\"]",
     3600,
     "VA News — government agency feed (30 entries at probe)")

RSSX(us_b50_nhc_noaa_gov_index_at_sp, "us-b50-nhc-noaa-gov-index-at-sp",
     "Central Pacific Hurricane Center — NHC NHC Atlantic Basin Tropical Cyclones RSS (Español) RSS F", "Central Pacific Hurricane Center — NHC NHC Atlantic Basin Tropical Cyclones RSS (Español) RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/index-at-sp.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC NHC Atlantic Basin Tropical Cyclones RSS (Español) RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC NHC Atlantic Basin Tropical Cyclones RSS (Español) RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 2 entries at probe")

RSSX(us_b50_nhc_noaa_gov_index_ep_sp, "us-b50-nhc-noaa-gov-index-ep-sp",
     "Central Pacific Hurricane Center — NHC NHC Eastern Pacific Basin Tropical Cyclones RSS (Español", "Central Pacific Hurricane Center — NHC NHC Eastern Pacific Basin Tropical Cyclones RSS (Español",
     "us_government", "government",
     "https://www.nhc.noaa.gov/index-ep-sp.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC NHC Eastern Pacific Basin Tropical Cyclones RSS (Español\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC NHC Eastern Pacific Basin Tropical Cyclones RSS (Español — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 2 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwsat1, "us-b50-nhc-noaa-gov-pwsat1",
     "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 1 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 1 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSAT1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 1 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 1 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwsat2, "us-b50-nhc-noaa-gov-pwsat2",
     "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 2 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 2 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSAT2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 2 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 2 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwsat3, "us-b50-nhc-noaa-gov-pwsat3",
     "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 3 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 3 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSAT3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 3 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 3 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwsat4, "us-b50-nhc-noaa-gov-pwsat4",
     "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 4 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 4 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSAT4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 4 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 4 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwsat5, "us-b50-nhc-noaa-gov-pwsat5",
     "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 5 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 5 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSAT5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 5 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Wind Speed Probabilities Wallet 5 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwscp1, "us-b50-nhc-noaa-gov-pwscp1",
     "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 1 RSS F", "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 1 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSCP1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 1 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 1 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwscp2, "us-b50-nhc-noaa-gov-pwscp2",
     "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 2 RSS F", "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 2 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSCP2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 2 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 2 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwscp3, "us-b50-nhc-noaa-gov-pwscp3",
     "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 3 RSS F", "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 3 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSCP3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 3 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 3 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwscp4, "us-b50-nhc-noaa-gov-pwscp4",
     "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 4 RSS F", "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 4 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSCP4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 4 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 4 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwscp5, "us-b50-nhc-noaa-gov-pwscp5",
     "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 5 RSS F", "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 5 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSCP5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 5 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific Wind Speed Probabilities Wallet 5 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwsep1, "us-b50-nhc-noaa-gov-pwsep1",
     "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 1 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 1 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSEP1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 1 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 1 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwsep2, "us-b50-nhc-noaa-gov-pwsep2",
     "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 2 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 2 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSEP2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 2 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 2 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwsep3, "us-b50-nhc-noaa-gov-pwsep3",
     "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 3 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 3 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSEP3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 3 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 3 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwsep4, "us-b50-nhc-noaa-gov-pwsep4",
     "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 4 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 4 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSEP4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 4 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 4 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_pwsep5, "us-b50-nhc-noaa-gov-pwsep5",
     "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 5 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 5 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/PWSEP5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 5 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Wind Speed Probabilities Wallet 5 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tasat1, "us-b50-nhc-noaa-gov-tasat1",
     "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 1 RSS Feed", "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 1 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TASAT1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 1 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 1 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tasat2, "us-b50-nhc-noaa-gov-tasat2",
     "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 2 RSS Feed", "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 2 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TASAT2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 2 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 2 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tasat3, "us-b50-nhc-noaa-gov-tasat3",
     "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 3 RSS Feed", "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 3 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TASAT3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 3 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 3 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tasat4, "us-b50-nhc-noaa-gov-tasat4",
     "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 4 RSS Feed", "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 4 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TASAT4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 4 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 4 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tasat5, "us-b50-nhc-noaa-gov-tasat5",
     "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 5 RSS Feed", "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 5 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TASAT5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 5 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Aviso Público de Atlántico para cartera 5 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tasep1, "us-b50-nhc-noaa-gov-tasep1",
     "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 1 RSS Fe", "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 1 RSS Fe",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TASEP1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 1 RSS Fe\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 1 RSS Fe — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tasep2, "us-b50-nhc-noaa-gov-tasep2",
     "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 2 RSS Fe", "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 2 RSS Fe",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TASEP2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 2 RSS Fe\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 2 RSS Fe — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tasep3, "us-b50-nhc-noaa-gov-tasep3",
     "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 3 RSS Fe", "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 3 RSS Fe",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TASEP3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 3 RSS Fe\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 3 RSS Fe — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tasep4, "us-b50-nhc-noaa-gov-tasep4",
     "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 4 RSS Fe", "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 4 RSS Fe",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TASEP4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 4 RSS Fe\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 4 RSS Fe — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tasep5, "us-b50-nhc-noaa-gov-tasep5",
     "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 5 RSS Fe", "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 5 RSS Fe",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TASEP5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 5 RSS Fe\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Aviso Público de Pacífico Oriental para cartera 5 RSS Fe — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcant1, "us-b50-nhc-noaa-gov-tcant1",
     "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 1 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 1 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCANT1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 1 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 1 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcant2, "us-b50-nhc-noaa-gov-tcant2",
     "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 2 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 2 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCANT2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 2 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 2 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcant3, "us-b50-nhc-noaa-gov-tcant3",
     "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 3 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 3 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCANT3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 3 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 3 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcant4, "us-b50-nhc-noaa-gov-tcant4",
     "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 4 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 4 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCANT4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 4 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 4 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcant5, "us-b50-nhc-noaa-gov-tcant5",
     "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 5 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 5 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCANT5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 5 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic ICAO (Aviation) Advisory Wallet 5 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcapa1, "us-b50-nhc-noaa-gov-tcapa1",
     "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 1 RSS F", "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 1 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCAPA1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 1 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 1 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcapa2, "us-b50-nhc-noaa-gov-tcapa2",
     "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 2 RSS F", "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 2 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCAPA2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 2 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 2 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcapa3, "us-b50-nhc-noaa-gov-tcapa3",
     "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 3 RSS F", "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 3 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCAPA3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 3 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 3 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcapa4, "us-b50-nhc-noaa-gov-tcapa4",
     "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 4 RSS F", "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 4 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCAPA4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 4 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 4 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcapa5, "us-b50-nhc-noaa-gov-tcapa5",
     "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 5 RSS F", "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 5 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCAPA5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 5 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific ICAO (Aviation) Advisory Wallet 5 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcapz1, "us-b50-nhc-noaa-gov-tcapz1",
     "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 1 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 1 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCAPZ1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 1 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 1 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcapz2, "us-b50-nhc-noaa-gov-tcapz2",
     "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 2 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 2 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCAPZ2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 2 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 2 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcapz3, "us-b50-nhc-noaa-gov-tcapz3",
     "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 3 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 3 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCAPZ3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 3 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 3 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcapz4, "us-b50-nhc-noaa-gov-tcapz4",
     "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 4 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 4 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCAPZ4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 4 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 4 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcapz5, "us-b50-nhc-noaa-gov-tcapz5",
     "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 5 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 5 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCAPZ5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 5 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific ICAO (Aviation) Advisory Wallet 5 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcdat4, "us-b50-nhc-noaa-gov-tcdat4",
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Discussion Wallet 4 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Discussion Wallet 4 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCDAT4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Discussion Wallet 4 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Discussion Wallet 4 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcdat5, "us-b50-nhc-noaa-gov-tcdat5",
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Discussion Wallet 5 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Discussion Wallet 5 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCDAT5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Discussion Wallet 5 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Discussion Wallet 5 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcdcp1, "us-b50-nhc-noaa-gov-tcdcp1",
     "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 1 RS", "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 1 RS",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCDCP1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 1 RS\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 1 RS — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcdcp2, "us-b50-nhc-noaa-gov-tcdcp2",
     "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 2 RS", "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 2 RS",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCDCP2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 2 RS\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 2 RS — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcdcp3, "us-b50-nhc-noaa-gov-tcdcp3",
     "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 3 RS", "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 3 RS",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCDCP3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 3 RS\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 3 RS — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcdcp4, "us-b50-nhc-noaa-gov-tcdcp4",
     "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 4 RS", "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 4 RS",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCDCP4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 4 RS\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 4 RS — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcdcp5, "us-b50-nhc-noaa-gov-tcdcp5",
     "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 5 RS", "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 5 RS",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCDCP5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 5 RS\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Discussion Wallet 5 RS — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcdep1, "us-b50-nhc-noaa-gov-tcdep1",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 1 RSS Fe", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 1 RSS Fe",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCDEP1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 1 RSS Fe\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 1 RSS Fe — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcdep2, "us-b50-nhc-noaa-gov-tcdep2",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 2 RSS Fe", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 2 RSS Fe",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCDEP2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 2 RSS Fe\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 2 RSS Fe — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcdep3, "us-b50-nhc-noaa-gov-tcdep3",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 3 RSS Fe", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 3 RSS Fe",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCDEP3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 3 RSS Fe\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 3 RSS Fe — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcdep4, "us-b50-nhc-noaa-gov-tcdep4",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 4 RSS Fe", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 4 RSS Fe",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCDEP4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 4 RSS Fe\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 4 RSS Fe — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcdep5, "us-b50-nhc-noaa-gov-tcdep5",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 5 RSS Fe", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 5 RSS Fe",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCDEP5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 5 RSS Fe\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Discussion Wallet 5 RSS Fe — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcuat1, "us-b50-nhc-noaa-gov-tcuat1",
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 1 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 1 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCUAT1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 1 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 1 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcuat2, "us-b50-nhc-noaa-gov-tcuat2",
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 2 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 2 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCUAT2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 2 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 2 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcuat3, "us-b50-nhc-noaa-gov-tcuat3",
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 3 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 3 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCUAT3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 3 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 3 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcuat4, "us-b50-nhc-noaa-gov-tcuat4",
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 4 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 4 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCUAT4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 4 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 4 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcuat5, "us-b50-nhc-noaa-gov-tcuat5",
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 5 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 5 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCUAT5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 5 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Update Wallet 5 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcucp2, "us-b50-nhc-noaa-gov-tcucp2",
     "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Update Wallet 2 RSS Fe", "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Update Wallet 2 RSS Fe",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCUCP2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Update Wallet 2 RSS Fe\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Update Wallet 2 RSS Fe — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcucp4, "us-b50-nhc-noaa-gov-tcucp4",
     "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Update Wallet 4 RSS Fe", "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Update Wallet 4 RSS Fe",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCUCP4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Update Wallet 4 RSS Fe\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — CPHC Central Pacific Tropical Cyclone Update Wallet 4 RSS Fe — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcuep1, "us-b50-nhc-noaa-gov-tcuep1",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 1 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 1 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCUEP1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 1 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 1 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcuep2, "us-b50-nhc-noaa-gov-tcuep2",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 2 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 2 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCUEP2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 2 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 2 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcuep3, "us-b50-nhc-noaa-gov-tcuep3",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 3 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 3 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCUEP3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 3 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 3 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcuep4, "us-b50-nhc-noaa-gov-tcuep4",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 4 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 4 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCUEP4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 4 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 4 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcuep5, "us-b50-nhc-noaa-gov-tcuep5",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 5 RSS Feed", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 5 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCUEP5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 5 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Update Wallet 5 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcvat1, "us-b50-nhc-noaa-gov-tcvat1",
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 1 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 1 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCVAT1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 1 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 1 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcvat2, "us-b50-nhc-noaa-gov-tcvat2",
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 2 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 2 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCVAT2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 2 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 2 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcvat3, "us-b50-nhc-noaa-gov-tcvat3",
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 3 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 3 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCVAT3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 3 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 3 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcvat4, "us-b50-nhc-noaa-gov-tcvat4",
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 4 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 4 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCVAT4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 4 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 4 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcvat5, "us-b50-nhc-noaa-gov-tcvat5",
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 5 RSS Feed", "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 5 RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCVAT5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 5 RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Atlantic Tropical Cyclone Breakpoints Wallet 5 RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcvep1, "us-b50-nhc-noaa-gov-tcvep1",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 1 RSS F", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 1 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCVEP1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 1 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 1 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcvep2, "us-b50-nhc-noaa-gov-tcvep2",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 2 RSS F", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 2 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCVEP2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 2 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 2 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcvep3, "us-b50-nhc-noaa-gov-tcvep3",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 3 RSS F", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 3 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCVEP3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 3 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 3 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcvep4, "us-b50-nhc-noaa-gov-tcvep4",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 4 RSS F", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 4 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCVEP4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 4 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 4 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tcvep5, "us-b50-nhc-noaa-gov-tcvep5",
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 5 RSS F", "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 5 RSS F",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TCVEP5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 5 RSS F\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC East Pacific Tropical Cyclone Breakpoints Wallet 5 RSS F — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tdsat1, "us-b50-nhc-noaa-gov-tdsat1",
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 1 RSS Fee", "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 1 RSS Fee",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TDSAT1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 1 RSS Fee\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 1 RSS Fee — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tdsat2, "us-b50-nhc-noaa-gov-tdsat2",
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 2 RSS Fee", "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 2 RSS Fee",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TDSAT2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 2 RSS Fee\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 2 RSS Fee — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tdsat3, "us-b50-nhc-noaa-gov-tdsat3",
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 3 RSS Fee", "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 3 RSS Fee",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TDSAT3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 3 RSS Fee\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 3 RSS Fee — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tdsat4, "us-b50-nhc-noaa-gov-tdsat4",
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 4 RSS Fee", "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 4 RSS Fee",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TDSAT4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 4 RSS Fee\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 4 RSS Fee — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tdsat5, "us-b50-nhc-noaa-gov-tdsat5",
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 5 RSS Fee", "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 5 RSS Fee",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TDSAT5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 5 RSS Fee\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Atlántico para cartera 5 RSS Fee — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tdsep1, "us-b50-nhc-noaa-gov-tdsep1",
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 1", "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 1",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TDSEP1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 1\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 1 — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tdsep2, "us-b50-nhc-noaa-gov-tdsep2",
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 2", "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 2",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TDSEP2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 2\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 2 — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tdsep3, "us-b50-nhc-noaa-gov-tdsep3",
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 3", "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 3",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TDSEP3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 3\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 3 — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tdsep4, "us-b50-nhc-noaa-gov-tdsep4",
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 4", "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 4",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TDSEP4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 4\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 4 — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tdsep5, "us-b50-nhc-noaa-gov-tdsep5",
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 5", "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 5",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TDSEP5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 5\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Pronóstico Discusión de Pacífico Oriental para cartera 5 — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tusat1, "us-b50-nhc-noaa-gov-tusat1",
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 1", "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 1",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TUSAT1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 1\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 1 — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tusat2, "us-b50-nhc-noaa-gov-tusat2",
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 2", "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 2",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TUSAT2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 2\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 2 — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tusat3, "us-b50-nhc-noaa-gov-tusat3",
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 3", "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 3",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TUSAT3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 3\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 3 — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tusat4, "us-b50-nhc-noaa-gov-tusat4",
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 4", "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 4",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TUSAT4.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 4\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 4 — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tusat5, "us-b50-nhc-noaa-gov-tusat5",
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 5", "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 5",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TUSAT5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 5\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Atlántico para cartera 5 — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tusep1, "us-b50-nhc-noaa-gov-tusep1",
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c", "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TUSEP1.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tusep2, "us-b50-nhc-noaa-gov-tusep2",
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c", "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TUSEP2.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tusep3, "us-b50-nhc-noaa-gov-tusep3",
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c", "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TUSEP3.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_tusep5, "us-b50-nhc-noaa-gov-tusep5",
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c", "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TUSEP5.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC Declaracion de Actualización de Pacífico Oriental para c — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_twosat, "us-b50-nhc-noaa-gov-twosat",
     "Central Pacific Hurricane Center — NHC La Perspectiva de la Atlántico RSS Feed", "Central Pacific Hurricane Center — NHC La Perspectiva de la Atlántico RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TWOSAT.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC La Perspectiva de la Atlántico RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC La Perspectiva de la Atlántico RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_nhc_noaa_gov_twosep, "us-b50-nhc-noaa-gov-twosep",
     "Central Pacific Hurricane Center — NHC La Perspectiva de Pacífico Oriental RSS Feed", "Central Pacific Hurricane Center — NHC La Perspectiva de Pacífico Oriental RSS Feed",
     "us_government", "government",
     "https://www.nhc.noaa.gov/xml/TWOSEP.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Pacific Hurricane Center — NHC La Perspectiva de Pacífico Oriental RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Central Pacific Hurricane Center — NHC La Perspectiva de Pacífico Oriental RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 1 entries at probe")

RSSX(us_b50_njn_net, "us-b50-njn-net",
     "New Jersey Network", "New Jersey Network",
     "us_government", "government",
     "https://njn.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:New Jersey Network\",\"government\",\"batch:50\"]",
     3600,
     "New Jersey Network — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_njoag_gov, "us-b50-njoag-gov",
     "New Jersey Division of Gaming Enforcement", "New Jersey Division of Gaming Enforcement",
     "us_government", "government",
     "https://www.njoag.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:New Jersey Division of Gaming Enforcement\",\"government\",\"batch:50\"]",
     3600,
     "New Jersey Division of Gaming Enforcement — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_ocaatlanta_com, "us-b50-ocaatlanta-com",
     "Atlanta Office of Cultural Affairs", "Atlanta Office of Cultural Affairs",
     "us_government", "government",
     "https://ocaatlanta.com/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Atlanta Office of Cultural Affairs\",\"government\",\"batch:50\"]",
     3600,
     "Atlanta Office of Cultural Affairs — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_opendata_fcc_gov_catalog, "us-b50-opendata-fcc-gov-catalog",
     "Catalog — Subscribe to Datasets", "Catalog — Subscribe to Datasets",
     "us_government", "government",
     "https://opendata.fcc.gov/catalog.rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Catalog — Subscribe to Datasets\",\"government\",\"batch:50\"]",
     3600,
     "Catalog — Subscribe to Datasets — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 40 entries at probe")

RSSX(us_b50_opendata_maryland_gov_catalog, "us-b50-opendata-maryland-gov-catalog",
     "Open Data — Subscribe to Datasets", "Open Data — Subscribe to Datasets",
     "us_government", "government",
     "https://opendata.maryland.gov/catalog.rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Open Data — Subscribe to Datasets\",\"government\",\"batch:50\"]",
     3600,
     "Open Data — Subscribe to Datasets — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 40 entries at probe")

RSSX(us_b50_sanjosecleanenergy_org, "us-b50-sanjosecleanenergy-org",
     "San Jose Clean Energy", "San Jose Clean Energy",
     "us_government", "government",
     "https://sanjosecleanenergy.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:San Jose Clean Energy\",\"government\",\"batch:50\"]",
     3600,
     "San Jose Clean Energy — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_twosidesna_org, "us-b50-twosidesna-org",
     "Paper and Packaging Board", "Paper and Packaging Board",
     "us_government", "government",
     "https://twosidesna.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Paper and Packaging Board\",\"government\",\"batch:50\"]",
     3600,
     "Paper and Packaging Board — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_veterans_nv_gov, "us-b50-veterans-nv-gov",
     "Nevada Department of Veterans Services", "Nevada Department of Veterans Services",
     "us_government", "government",
     "https://veterans.nv.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Nevada Department of Veterans Services\",\"government\",\"batch:50\"]",
     3600,
     "Nevada Department of Veterans Services — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_wem_wi_gov, "us-b50-wem-wi-gov",
     "Wisconsin Emergency Management", "Wisconsin Emergency Management",
     "us_government", "government",
     "https://wem.wi.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Wisconsin Emergency Management\",\"government\",\"batch:50\"]",
     3600,
     "Wisconsin Emergency Management — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 10 entries at probe")

RSSX(us_b50_whitehouse_gov_presidential_actions, "us-b50-whitehouse-gov-presidential-actions",
     "White House Presidential Actions", "White House Presidential Actions",
     "us_government", "government",
     "https://www.whitehouse.gov/presidential-actions/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:White House Presidential Actions\",\"government\",\"batch:50\"]",
     3600,
     "White House Presidential Actions — government agency feed (30 entries at probe)")

RSSX(us_b50_zoomstatus_com_history, "us-b50-zoomstatus-com-history",
     "Zoom Status — Zoom Status History - Atom Feed", "Zoom Status — Zoom Status History - Atom Feed",
     "us_government", "government",
     "https://www.zoomstatus.com/history.atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Zoom Status — Zoom Status History - Atom Feed\",\"government\",\"batch:50\"]",
     3600,
     "Zoom Status — Zoom Status History - Atom Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 25 entries at probe")

RSSX(us_b50_zoomstatus_com_history_2, "us-b50-zoomstatus-com-history-2",
     "Zoom Status — Zoom Status History - RSS Feed", "Zoom Status — Zoom Status History - RSS Feed",
     "us_government", "government",
     "https://www.zoomstatus.com/history.rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Zoom Status — Zoom Status History - RSS Feed\",\"government\",\"batch:50\"]",
     3600,
     "Zoom Status — Zoom Status History - RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this government agency; 25 entries at probe")

/* ── us_health ───────────────────────────────────────────────────────── */

RSSX(us_b50_fda_gov_about_fda_contact_fda_stay_informed_rss_feeds_drugs, "us-b50-fda-gov-about-fda-contact-fda-stay-informed-rss-feeds-drugs",
     "FDA Drug News", "FDA Drug News",
     "us_health", "health",
     "https://www.fda.gov/about-fda/contact-fda/stay-informed/rss-feeds/drugs/rss.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:FDA Drug News\",\"health\",\"batch:50\"]",
     3600,
     "FDA Drug News — health feed (20 entries at probe)")

RSSX(us_b50_fda_gov_about_fda_contact_fda_stay_informed_rss_feeds_food_safety_recalls, "us-b50-fda-gov-about-fda-contact-fda-stay-informed-rss-feeds-food-safety-recalls",
     "FDA Food Safety Recalls", "FDA Food Safety Recalls",
     "us_health", "health",
     "https://www.fda.gov/about-fda/contact-fda/stay-informed/rss-feeds/food-safety-recalls/rss.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:FDA Food Safety Recalls\",\"health\",\"batch:50\"]",
     3600,
     "FDA Food Safety Recalls — health feed (20 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o_15, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o-15",
     "Fed Register VA", "Fed Register VA",
     "us_health", "health",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-veterans-affairs",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register VA\",\"health\",\"batch:50\"]",
     3600,
     "Fed Register VA — health feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_department_o_6, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-department-o-6",
     "Fed Register HHS", "Fed Register HHS",
     "us_health", "health",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=department-of-health-and-human-services",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register HHS\",\"health\",\"batch:50\"]",
     3600,
     "Fed Register HHS — health feed (200 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_food_and_dru, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-food-and-dru",
     "Fed Register FDA", "Fed Register FDA",
     "us_health", "health",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=food-and-drug-administration",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register FDA\",\"health\",\"batch:50\"]",
     3600,
     "Fed Register FDA — health feed (64 entries at probe)")

RSSX(us_b50_samhsa_gov_newsroom_press_announcements, "us-b50-samhsa-gov-newsroom-press-announcements",
     "SAMHSA Press", "SAMHSA Press",
     "us_health", "health",
     "https://www.samhsa.gov/newsroom/press-announcements/rss",
     "en", "[\"gov\",\"country:us\",\"publisher:SAMHSA Press\",\"health\",\"batch:50\"]",
     3600,
     "SAMHSA Press — health feed (50 entries at probe)")

/* ── us_local ────────────────────────────────────────────────────────── */

RSSX(us_b50_ackleyiowa_net_blog_feed, "us-b50-ackleyiowa-net-blog-feed",
     "Ackley — City of Ackley, Iowa - RSS", "Ackley — City of Ackley, Iowa - RSS",
     "us_local", "government",
     "https://www.ackleyiowa.net/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Ackley — City of Ackley, Iowa - RSS\",\"government\",\"batch:50\"]",
     10800,
     "Ackley — City of Ackley, Iowa - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 20 entries at probe")

RSSX(us_b50_alamotexas_org, "us-b50-alamotexas-org",
     "Alamo", "Alamo",
     "us_local", "government",
     "https://alamotexas.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Alamo\",\"government\",\"batch:50\"]",
     10800,
     "Alamo — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_altoniowa_us, "us-b50-altoniowa-us",
     "Alton", "Alton",
     "us_local", "government",
     "https://altoniowa.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Alton\",\"government\",\"batch:50\"]",
     10800,
     "Alton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_amherstohio_org, "us-b50-amherstohio-org",
     "Amherst", "Amherst",
     "us_local", "government",
     "https://amherstohio.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Amherst\",\"government\",\"batch:50\"]",
     10800,
     "Amherst — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_andalusiaal_gov, "us-b50-andalusiaal-gov",
     "Andalusia", "Andalusia",
     "us_local", "government",
     "https://andalusiaal.gov/feed/rss/",
     "en", "[\"gov\",\"country:us\",\"publisher:Andalusia\",\"government\",\"batch:50\"]",
     10800,
     "Andalusia — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_arbyrdmissouri_com_rss2, "us-b50-arbyrdmissouri-com-rss2",
     "Arbyrd", "Arbyrd",
     "us_local", "government",
     "https://www.arbyrdmissouri.com/feed/rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Arbyrd\",\"government\",\"batch:50\"]",
     10800,
     "Arbyrd — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 8 entries at probe")

RSSX(us_b50_arnoldmo_org, "us-b50-arnoldmo-org",
     "Arnold", "Arnold",
     "us_local", "government",
     "https://www.arnoldmo.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Arnold\",\"government\",\"batch:50\"]",
     10800,
     "Arnold — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_atkinsonne_com, "us-b50-atkinsonne-com",
     "Atkinson", "Atkinson",
     "us_local", "government",
     "https://atkinsonne.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Atkinson\",\"government\",\"batch:50\"]",
     10800,
     "Atkinson — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 8 entries at probe")

RSSX(us_b50_auburn_ne_gov, "us-b50-auburn-ne-gov",
     "Auburn", "Auburn",
     "us_local", "government",
     "https://auburn.ne.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Auburn\",\"government\",\"batch:50\"]",
     10800,
     "Auburn — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_auburndalefl_com, "us-b50-auburndalefl-com",
     "Auburndale", "Auburndale",
     "us_local", "government",
     "https://auburndalefl.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Auburndale\",\"government\",\"batch:50\"]",
     10800,
     "Auburndale — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_auburnhills_org, "us-b50-auburnhills-org",
     "Auburn Hills", "Auburn Hills",
     "us_local", "government",
     "https://www.auburnhills.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Auburn Hills\",\"government\",\"batch:50\"]",
     10800,
     "Auburn Hills — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_aureliaia_com, "us-b50-aureliaia-com",
     "Aurelia", "Aurelia",
     "us_local", "government",
     "https://aureliaia.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Aurelia\",\"government\",\"batch:50\"]",
     10800,
     "Aurelia — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_beachnear_me, "us-b50-beachnear-me",
     "Greenfield", "Greenfield",
     "us_local", "government",
     "https://beachnear.me/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Greenfield\",\"government\",\"batch:50\"]",
     10800,
     "Greenfield — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_beattyville_org, "us-b50-beattyville-org",
     "Beattyville", "Beattyville",
     "us_local", "government",
     "https://beattyville.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Beattyville\",\"government\",\"batch:50\"]",
     10800,
     "Beattyville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_bellevueky_org, "us-b50-bellevueky-org",
     "Bellevue", "Bellevue",
     "us_local", "government",
     "https://bellevueky.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Bellevue\",\"government\",\"batch:50\"]",
     10800,
     "Bellevue — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_benhamky_org, "us-b50-benhamky-org",
     "Benham", "Benham",
     "us_local", "government",
     "https://www.benhamky.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Benham\",\"government\",\"batch:50\"]",
     10800,
     "Benham — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_bernein_com, "us-b50-bernein-com",
     "Berne", "Berne",
     "us_local", "government",
     "https://bernein.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Berne\",\"government\",\"batch:50\"]",
     10800,
     "Berne — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_bessemercity_com, "us-b50-bessemercity-com",
     "Bessemer City", "Bessemer City",
     "us_local", "government",
     "https://bessemercity.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Bessemer City\",\"government\",\"batch:50\"]",
     10800,
     "Bessemer City — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_bethanymo_com, "us-b50-bethanymo-com",
     "Bethany", "Bethany",
     "us_local", "government",
     "https://bethanymo.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Bethany\",\"government\",\"batch:50\"]",
     10800,
     "Bethany — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_beverlyhillstexas_net, "us-b50-beverlyhillstexas-net",
     "Beverly Hills", "Beverly Hills",
     "us_local", "government",
     "https://beverlyhillstexas.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Beverly Hills\",\"government\",\"batch:50\"]",
     10800,
     "Beverly Hills — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_billingsmo_com_feed_rss2, "us-b50-billingsmo-com-feed-rss2",
     "Billings", "Billings",
     "us_local", "government",
     "https://billingsmo.com/?feed=rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Billings\",\"government\",\"batch:50\"]",
     10800,
     "Billings — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_bishoptx_com, "us-b50-bishoptx-com",
     "Bishop", "Bishop",
     "us_local", "government",
     "https://www.bishoptx.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Bishop\",\"government\",\"batch:50\"]",
     10800,
     "Bishop — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_bloomington_in_gov, "us-b50-bloomington-in-gov",
     "Bloomington", "Bloomington",
     "us_local", "government",
     "https://bloomington.in.gov/rss.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Bloomington\",\"government\",\"batch:50\"]",
     10800,
     "Bloomington — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_bluelake_ca_gov, "us-b50-bluelake-ca-gov",
     "Blue Lake", "Blue Lake",
     "us_local", "government",
     "https://bluelake.ca.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Blue Lake\",\"government\",\"batch:50\"]",
     10800,
     "Blue Lake — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 24 entries at probe")

RSSX(us_b50_bostonga_com, "us-b50-bostonga-com",
     "Boston", "Boston",
     "us_local", "government",
     "https://bostonga.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Boston\",\"government\",\"batch:50\"]",
     10800,
     "Boston — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_bowbellsnd_com_feed_rss2, "us-b50-bowbellsnd-com-feed-rss2",
     "Bowbells", "Bowbells",
     "us_local", "government",
     "https://bowbellsnd.com/?feed=rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Bowbells\",\"government\",\"batch:50\"]",
     10800,
     "Bowbells — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_bowdon_net, "us-b50-bowdon-net",
     "Bowdon", "Bowdon",
     "us_local", "government",
     "https://www.bowdon.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Bowdon\",\"government\",\"batch:50\"]",
     10800,
     "Bowdon — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_bradleyark_com, "us-b50-bradleyark-com",
     "Bradley", "Bradley",
     "us_local", "government",
     "https://bradleyark.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Bradley\",\"government\",\"batch:50\"]",
     10800,
     "Bradley — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_brazil_in_gov, "us-b50-brazil-in-gov",
     "Brazil", "Brazil",
     "us_local", "government",
     "http://brazil.in.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Brazil\",\"government\",\"batch:50\"]",
     10800,
     "Brazil — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_breese_org, "us-b50-breese-org",
     "Breese", "Breese",
     "us_local", "government",
     "https://breese.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Breese\",\"government\",\"batch:50\"]",
     10800,
     "Breese — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 12 entries at probe")

RSSX(us_b50_bridgeportwashington_net, "us-b50-bridgeportwashington-net",
     "Bridgeport", "Bridgeport",
     "us_local", "government",
     "https://bridgeportwashington.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Bridgeport\",\"government\",\"batch:50\"]",
     10800,
     "Bridgeport — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_brokenbowareachamber_com, "us-b50-brokenbowareachamber-com",
     "Broken Bow", "Broken Bow",
     "us_local", "government",
     "https://brokenbowareachamber.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Broken Bow\",\"government\",\"batch:50\"]",
     10800,
     "Broken Bow — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_brooklynpark_org, "us-b50-brooklynpark-org",
     "Brooklyn Park", "Brooklyn Park",
     "us_local", "government",
     "https://www.brooklynpark.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Brooklyn Park\",\"government\",\"batch:50\"]",
     10800,
     "Brooklyn Park — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 9 entries at probe")

RSSX(us_b50_browncountyil_org, "us-b50-browncountyil-org",
     "Mount Sterling", "Mount Sterling",
     "us_local", "government",
     "https://browncountyil.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Mount Sterling\",\"government\",\"batch:50\"]",
     10800,
     "Mount Sterling — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_brownsborovillage_org, "us-b50-brownsborovillage-org",
     "Brownsboro Village", "Brownsboro Village",
     "us_local", "government",
     "https://brownsborovillage.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Brownsboro Village\",\"government\",\"batch:50\"]",
     10800,
     "Brownsboro Village — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_brunswick_oh_us, "us-b50-brunswick-oh-us",
     "Brunswick", "Brunswick",
     "us_local", "government",
     "https://www.brunswick.oh.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Brunswick\",\"government\",\"batch:50\"]",
     10800,
     "Brunswick — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_cachegov_com, "us-b50-cachegov-com",
     "Cache", "Cache",
     "us_local", "government",
     "https://www.cachegov.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Cache\",\"government\",\"batch:50\"]",
     10800,
     "Cache — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_calipatria_com, "us-b50-calipatria-com",
     "Calipatria", "Calipatria",
     "us_local", "government",
     "https://www.calipatria.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Calipatria\",\"government\",\"batch:50\"]",
     10800,
     "Calipatria — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_campbellsville_us, "us-b50-campbellsville-us",
     "Campbellsville", "Campbellsville",
     "us_local", "government",
     "https://campbellsville.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Campbellsville\",\"government\",\"batch:50\"]",
     10800,
     "Campbellsville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_canton_georgia_com, "us-b50-canton-georgia-com",
     "Canton", "Canton",
     "us_local", "government",
     "https://www.canton-georgia.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Canton\",\"government\",\"batch:50\"]",
     10800,
     "Canton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_canton_mississippi_com, "us-b50-canton-mississippi-com",
     "Canton", "Canton",
     "us_local", "government",
     "https://www.canton-mississippi.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Canton\",\"government\",\"batch:50\"]",
     10800,
     "Canton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cantrilia_com, "us-b50-cantrilia-com",
     "Cantril", "Cantril",
     "us_local", "government",
     "https://cantrilia.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Cantril\",\"government\",\"batch:50\"]",
     10800,
     "Cantril — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_carencro_org, "us-b50-carencro-org",
     "Carencro", "Carencro",
     "us_local", "government",
     "https://carencro.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Carencro\",\"government\",\"batch:50\"]",
     10800,
     "Carencro — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cariboumaine_org, "us-b50-cariboumaine-org",
     "City of Caribou, Maine", "City of Caribou, Maine",
     "us_local", "government",
     "https://www.cariboumaine.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:City of Caribou, Maine\",\"government\",\"batch:50\"]",
     10800,
     "City of Caribou, Maine — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_cascadeid_us, "us-b50-cascadeid-us",
     "Cascade", "Cascade",
     "us_local", "government",
     "https://cascadeid.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Cascade\",\"government\",\"batch:50\"]",
     10800,
     "Cascade — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_casevillecity_com, "us-b50-casevillecity-com",
     "Caseville", "Caseville",
     "us_local", "government",
     "https://casevillecity.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Caseville\",\"government\",\"batch:50\"]",
     10800,
     "Caseville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cawh_org, "us-b50-cawh-org",
     "Airway Heights", "Airway Heights",
     "us_local", "government",
     "https://cawh.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Airway Heights\",\"government\",\"batch:50\"]",
     10800,
     "Airway Heights — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cawkercitykansas_com_blog_feed, "us-b50-cawkercitykansas-com-blog-feed",
     "Cawker City — Cawker City, Kansas - RSS", "Cawker City — Cawker City, Kansas - RSS",
     "us_local", "government",
     "https://www.cawkercitykansas.com/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Cawker City — Cawker City, Kansas - RSS\",\"government\",\"batch:50\"]",
     10800,
     "Cawker City — Cawker City, Kansas - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 20 entries at probe")

RSSX(us_b50_cedar_rapids_org, "us-b50-cedar-rapids-org",
     "Cedar Rapids", "Cedar Rapids",
     "us_local", "government",
     "http://www.cedar-rapids.org/rss.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Cedar Rapids\",\"government\",\"batch:50\"]",
     10800,
     "Cedar Rapids — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 25 entries at probe")

RSSX(us_b50_cedar_rapids_org_blog, "us-b50-cedar-rapids-org-blog",
     "Cedar Rapids", "Cedar Rapids",
     "us_local", "government",
     "http://www.cedar-rapids.org/blog.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Cedar Rapids\",\"government\",\"batch:50\"]",
     10800,
     "Cedar Rapids — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 74 entries at probe")

RSSX(us_b50_celinaohio_org, "us-b50-celinaohio-org",
     "Celina", "Celina",
     "us_local", "government",
     "https://celinaohio.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Celina\",\"government\",\"batch:50\"]",
     10800,
     "Celina — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_centernd_net, "us-b50-centernd-net",
     "Center", "Center",
     "us_local", "government",
     "https://centernd.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Center\",\"government\",\"batch:50\"]",
     10800,
     "Center — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_centervillega_org, "us-b50-centervillega-org",
     "Centerville", "Centerville",
     "us_local", "government",
     "https://www.centervillega.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Centerville\",\"government\",\"batch:50\"]",
     10800,
     "Centerville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_chinookmontana_com, "us-b50-chinookmontana-com",
     "Chinook", "Chinook",
     "us_local", "government",
     "https://chinookmontana.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Chinook\",\"government\",\"batch:50\"]",
     10800,
     "Chinook — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_ci_austin_mn_us, "us-b50-ci-austin-mn-us",
     "Austin", "Austin",
     "us_local", "government",
     "https://www.ci.austin.mn.us/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Austin\",\"government\",\"batch:50\"]",
     10800,
     "Austin — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_ci_ferndale_ca_us, "us-b50-ci-ferndale-ca-us",
     "Ferndale", "Ferndale",
     "us_local", "government",
     "https://ci.ferndale.ca.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Ferndale\",\"government\",\"batch:50\"]",
     10800,
     "Ferndale — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_ci_foley_mn_us, "us-b50-ci-foley-mn-us",
     "Foley", "Foley",
     "us_local", "government",
     "https://ci.foley.mn.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Foley\",\"government\",\"batch:50\"]",
     10800,
     "Foley — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_ci_monticello_ia_us, "us-b50-ci-monticello-ia-us",
     "Monticello", "Monticello",
     "us_local", "government",
     "https://www.ci.monticello.ia.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Monticello\",\"government\",\"batch:50\"]",
     10800,
     "Monticello — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_ci_moraine_oh_us, "us-b50-ci-moraine-oh-us",
     "Moraine", "Moraine",
     "us_local", "government",
     "https://ci.moraine.oh.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Moraine\",\"government\",\"batch:50\"]",
     10800,
     "Moraine — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_ci_morris_mn_us, "us-b50-ci-morris-mn-us",
     "Morris", "Morris",
     "us_local", "government",
     "https://www.ci.morris.mn.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Morris\",\"government\",\"batch:50\"]",
     10800,
     "Morris — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_ci_newtonfalls_oh_us, "us-b50-ci-newtonfalls-oh-us",
     "Newton Falls", "Newton Falls",
     "us_local", "government",
     "https://ci.newtonfalls.oh.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Newton Falls\",\"government\",\"batch:50\"]",
     10800,
     "Newton Falls — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofabbeville_net, "us-b50-cityofabbeville-net",
     "Abbeville", "Abbeville",
     "us_local", "government",
     "https://cityofabbeville.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Abbeville\",\"government\",\"batch:50\"]",
     10800,
     "Abbeville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofabernathy_org, "us-b50-cityofabernathy-org",
     "Abernathy", "Abernathy",
     "us_local", "government",
     "https://cityofabernathy.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Abernathy\",\"government\",\"batch:50\"]",
     10800,
     "Abernathy — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofainsworth_com, "us-b50-cityofainsworth-com",
     "Ainsworth", "Ainsworth",
     "us_local", "government",
     "https://cityofainsworth.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Ainsworth\",\"government\",\"batch:50\"]",
     10800,
     "Ainsworth — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_cityofalbertlea_org, "us-b50-cityofalbertlea-org",
     "Albert Lea", "Albert Lea",
     "us_local", "government",
     "https://www.cityofalbertlea.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Albert Lea\",\"government\",\"batch:50\"]",
     10800,
     "Albert Lea — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofalexandriala_com, "us-b50-cityofalexandriala-com",
     "Alexandria", "Alexandria",
     "us_local", "government",
     "https://www.cityofalexandriala.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Alexandria\",\"government\",\"batch:50\"]",
     10800,
     "Alexandria — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofandersonsc_com, "us-b50-cityofandersonsc-com",
     "Anderson", "Anderson",
     "us_local", "government",
     "https://www.cityofandersonsc.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Anderson\",\"government\",\"batch:50\"]",
     10800,
     "Anderson — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofangoon_org, "us-b50-cityofangoon-org",
     "Angoon", "Angoon",
     "us_local", "government",
     "https://cityofangoon.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Angoon\",\"government\",\"batch:50\"]",
     10800,
     "Angoon — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_cityofanna_org, "us-b50-cityofanna-org",
     "Anna", "Anna",
     "us_local", "government",
     "https://cityofanna.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Anna\",\"government\",\"batch:50\"]",
     10800,
     "Anna — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofansonia_com_qcontent_newsfeed_ashx_chanid_520, "us-b50-cityofansonia-com-qcontent-newsfeed-ashx-chanid-520",
     "Ansonia", "Ansonia",
     "us_local", "government",
     "http://www.cityofansonia.com/qcontent/NewsFeed.ashx?ChanID=520",
     "en", "[\"gov\",\"country:us\",\"publisher:Ansonia\",\"government\",\"batch:50\"]",
     10800,
     "Ansonia — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_cityofashburn_net, "us-b50-cityofashburn-net",
     "Ashburn", "Ashburn",
     "us_local", "government",
     "https://cityofashburn.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Ashburn\",\"government\",\"batch:50\"]",
     10800,
     "Ashburn — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofatchison_com, "us-b50-cityofatchison-com",
     "Atchison", "Atchison",
     "us_local", "government",
     "https://cityofatchison.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Atchison\",\"government\",\"batch:50\"]",
     10800,
     "Atchison — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofaustin_in_gov, "us-b50-cityofaustin-in-gov",
     "Austin", "Austin",
     "us_local", "government",
     "https://cityofaustin.in.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Austin\",\"government\",\"batch:50\"]",
     10800,
     "Austin — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofbaldwin_org_blog_feed, "us-b50-cityofbaldwin-org-blog-feed",
     "Baldwin — City of Baldwin - RSS", "Baldwin — City of Baldwin - RSS",
     "us_local", "government",
     "https://www.cityofbaldwin.org/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Baldwin — City of Baldwin - RSS\",\"government\",\"batch:50\"]",
     10800,
     "Baldwin — City of Baldwin - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 20 entries at probe")

RSSX(us_b50_cityofbarbourville_com, "us-b50-cityofbarbourville-com",
     "Barbourville", "Barbourville",
     "us_local", "government",
     "https://cityofbarbourville.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Barbourville\",\"government\",\"batch:50\"]",
     10800,
     "Barbourville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_cityofbartlesville_org, "us-b50-cityofbartlesville-org",
     "Bartlesville", "Bartlesville",
     "us_local", "government",
     "https://www.cityofbartlesville.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Bartlesville\",\"government\",\"batch:50\"]",
     10800,
     "Bartlesville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofbastrop_com, "us-b50-cityofbastrop-com",
     "Bastrop", "Bastrop",
     "us_local", "government",
     "https://www.cityofbastrop.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Bastrop\",\"government\",\"batch:50\"]",
     10800,
     "Bastrop — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofbelpre_com, "us-b50-cityofbelpre-com",
     "Belpre", "Belpre",
     "us_local", "government",
     "https://cityofbelpre.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Belpre\",\"government\",\"batch:50\"]",
     10800,
     "Belpre — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_cityofbennington_com, "us-b50-cityofbennington-com",
     "Bennington", "Bennington",
     "us_local", "government",
     "https://www.cityofbennington.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Bennington\",\"government\",\"batch:50\"]",
     10800,
     "Bennington — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_cityofblakely_net, "us-b50-cityofblakely-net",
     "Blakely", "Blakely",
     "us_local", "government",
     "https://cityofblakely.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Blakely\",\"government\",\"batch:50\"]",
     10800,
     "Blakely — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofbrookpark_com, "us-b50-cityofbrookpark-com",
     "Brook Park", "Brook Park",
     "us_local", "government",
     "https://www.cityofbrookpark.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Brook Park\",\"government\",\"batch:50\"]",
     10800,
     "Brook Park — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofbryan_com, "us-b50-cityofbryan-com",
     "Bryan", "Bryan",
     "us_local", "government",
     "https://www.cityofbryan.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Bryan\",\"government\",\"batch:50\"]",
     10800,
     "Bryan — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofcalhoun_ga_com, "us-b50-cityofcalhoun-ga-com",
     "Calhoun", "Calhoun",
     "us_local", "government",
     "https://www.cityofcalhoun-ga.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Calhoun\",\"government\",\"batch:50\"]",
     10800,
     "Calhoun — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofcaliente_com, "us-b50-cityofcaliente-com",
     "Caliente", "Caliente",
     "us_local", "government",
     "https://www.cityofcaliente.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Caliente\",\"government\",\"batch:50\"]",
     10800,
     "Caliente — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofcalifornia_net, "us-b50-cityofcalifornia-net",
     "California", "California",
     "us_local", "government",
     "https://cityofcalifornia.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:California\",\"government\",\"batch:50\"]",
     10800,
     "California — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_cityofcherryville_com_blog_feed, "us-b50-cityofcherryville-com-blog-feed",
     "Cherryville — City of Cherryville - RSS", "Cherryville — City of Cherryville - RSS",
     "us_local", "government",
     "https://www.cityofcherryville.com/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Cherryville — City of Cherryville - RSS\",\"government\",\"batch:50\"]",
     10800,
     "Cherryville — City of Cherryville - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_cityofchickasaw_org, "us-b50-cityofchickasaw-org",
     "Chickasaw", "Chickasaw",
     "us_local", "government",
     "https://cityofchickasaw.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Chickasaw\",\"government\",\"batch:50\"]",
     10800,
     "Chickasaw — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofchubbuck_us, "us-b50-cityofchubbuck-us",
     "Chubbuck", "Chubbuck",
     "us_local", "government",
     "https://cityofchubbuck.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Chubbuck\",\"government\",\"batch:50\"]",
     10800,
     "Chubbuck — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofcollins_com, "us-b50-cityofcollins-com",
     "Collins", "Collins",
     "us_local", "government",
     "https://cityofcollins.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Collins\",\"government\",\"batch:50\"]",
     10800,
     "Collins — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofcollinwood_org_index_format_feed_type_atom, "us-b50-cityofcollinwood-org-index-format-feed-type-atom",
     "Collinwood — Atom 1.0", "Collinwood — Atom 1.0",
     "us_local", "government",
     "https://www.cityofcollinwood.org/index.php?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Collinwood — Atom 1.0\",\"government\",\"batch:50\"]",
     10800,
     "Collinwood — Atom 1.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofcollinwood_org_index_format_feed_type_rss, "us-b50-cityofcollinwood-org-index-format-feed-type-rss",
     "Collinwood", "Collinwood",
     "us_local", "government",
     "https://www.cityofcollinwood.org/index.php?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Collinwood\",\"government\",\"batch:50\"]",
     10800,
     "Collinwood — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofcordova_net, "us-b50-cityofcordova-net",
     "Cordova", "Cordova",
     "us_local", "government",
     "https://www.cityofcordova.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Cordova\",\"government\",\"batch:50\"]",
     10800,
     "Cordova — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofcranetexas_com, "us-b50-cityofcranetexas-com",
     "Crane", "Crane",
     "us_local", "government",
     "https://cityofcranetexas.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Crane\",\"government\",\"batch:50\"]",
     10800,
     "Crane — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofcrisfield_com, "us-b50-cityofcrisfield-com",
     "Crisfield", "Crisfield",
     "us_local", "government",
     "https://cityofcrisfield.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Crisfield\",\"government\",\"batch:50\"]",
     10800,
     "Crisfield — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofcrystalsprings_com, "us-b50-cityofcrystalsprings-com",
     "Crystal Springs", "Crystal Springs",
     "us_local", "government",
     "http://cityofcrystalsprings.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Crystal Springs\",\"government\",\"batch:50\"]",
     10800,
     "Crystal Springs — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_cityofdarienga_com, "us-b50-cityofdarienga-com",
     "Darien", "Darien",
     "us_local", "government",
     "https://cityofdarienga.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Darien\",\"government\",\"batch:50\"]",
     10800,
     "Darien — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_cityofdarlington_com, "us-b50-cityofdarlington-com",
     "Darlington", "Darlington",
     "us_local", "government",
     "https://www.cityofdarlington.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Darlington\",\"government\",\"batch:50\"]",
     10800,
     "Darlington — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_camdencounty_com, "us-b50-camdencounty-com",
     "Camden County, NJ", "Camden County, NJ",
     "us_local", "government",
     "https://www.camdencounty.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Camden County, NJ\",\"government\",\"batch:50\"]",
     10800,
     "Camden County, NJ — county government news feed (commissioners, services, roadworks); 10 entries at probe 2026-10-21")

RSSX(us_b50_cityofdeerwood_gov_format_feed_type_atom, "us-b50-cityofdeerwood-gov-format-feed-type-atom",
     "Deerwood — Home", "Deerwood — Home",
     "us_local", "government",
     "https://cityofdeerwood.gov/?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Deerwood — Home\",\"government\",\"batch:50\"]",
     10800,
     "Deerwood — Home — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_cityofdeerwood_gov_format_feed_type_rss, "us-b50-cityofdeerwood-gov-format-feed-type-rss",
     "Deerwood — Home", "Deerwood — Home",
     "us_local", "government",
     "https://cityofdeerwood.gov/?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Deerwood — Home\",\"government\",\"batch:50\"]",
     10800,
     "Deerwood — Home — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_cityofdickson_com_rss2, "us-b50-cityofdickson-com-rss2",
     "Dickson", "Dickson",
     "us_local", "government",
     "https://www.cityofdickson.com/feed/rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Dickson\",\"government\",\"batch:50\"]",
     10800,
     "Dickson — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 8 entries at probe")

RSSX(us_b50_cityofdora_com, "us-b50-cityofdora-com",
     "Dora", "Dora",
     "us_local", "government",
     "https://cityofdora.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Dora\",\"government\",\"batch:50\"]",
     10800,
     "Dora — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofedmundson_com_rss2, "us-b50-cityofedmundson-com-rss2",
     "Edmundson", "Edmundson",
     "us_local", "government",
     "https://www.cityofedmundson.com/feed/rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Edmundson\",\"government\",\"batch:50\"]",
     10800,
     "Edmundson — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 11 entries at probe")

RSSX(us_b50_cityofelcenizo_com, "us-b50-cityofelcenizo-com",
     "El Cenizo", "El Cenizo",
     "us_local", "government",
     "https://cityofelcenizo.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:El Cenizo\",\"government\",\"batch:50\"]",
     10800,
     "El Cenizo — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofeldridgeia_org, "us-b50-cityofeldridgeia-org",
     "Eldridge", "Eldridge",
     "us_local", "government",
     "https://cityofeldridgeia.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Eldridge\",\"government\",\"batch:50\"]",
     10800,
     "Eldridge — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofelephantbutte_com, "us-b50-cityofelephantbutte-com",
     "Elephant Butte", "Elephant Butte",
     "us_local", "government",
     "https://cityofelephantbutte.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Elephant Butte\",\"government\",\"batch:50\"]",
     10800,
     "Elephant Butte — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofellinwoodks_com, "us-b50-cityofellinwoodks-com",
     "Ellinwood", "Ellinwood",
     "us_local", "government",
     "https://www.cityofellinwoodks.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Ellinwood\",\"government\",\"batch:50\"]",
     10800,
     "Ellinwood — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_cityofelyria_org, "us-b50-cityofelyria-org",
     "Elyria", "Elyria",
     "us_local", "government",
     "https://www.cityofelyria.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Elyria\",\"government\",\"batch:50\"]",
     10800,
     "Elyria — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofemmett_org_blog_feed, "us-b50-cityofemmett-org-blog-feed",
     "Emmett — city-of-emmett - RSS", "Emmett — city-of-emmett - RSS",
     "us_local", "government",
     "https://www.cityofemmett.org/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Emmett — city-of-emmett - RSS\",\"government\",\"batch:50\"]",
     10800,
     "Emmett — city-of-emmett - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 20 entries at probe")

RSSX(us_b50_cityofenterprise_net, "us-b50-cityofenterprise-net",
     "Fairfield", "Fairfield",
     "us_local", "government",
     "https://cityofenterprise.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Fairfield\",\"government\",\"batch:50\"]",
     10800,
     "Fairfield — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_cityofesmo_com, "us-b50-cityofesmo-com",
     "Excelsior Springs", "Excelsior Springs",
     "us_local", "government",
     "https://cityofesmo.com/index.php/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Excelsior Springs\",\"government\",\"batch:50\"]",
     10800,
     "Excelsior Springs — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofeufaulaok_com, "us-b50-cityofeufaulaok-com",
     "Eufaula", "Eufaula",
     "us_local", "government",
     "https://www.cityofeufaulaok.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Eufaula\",\"government\",\"batch:50\"]",
     10800,
     "Eufaula — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_cityofeverett_com, "us-b50-cityofeverett-com",
     "Everett", "Everett",
     "us_local", "government",
     "https://cityofeverett.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Everett\",\"government\",\"batch:50\"]",
     10800,
     "Everett — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofflint_com, "us-b50-cityofflint-com",
     "Flint", "Flint",
     "us_local", "government",
     "https://www.cityofflint.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Flint\",\"government\",\"batch:50\"]",
     10800,
     "Flint — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityoffortmeade_com, "us-b50-cityoffortmeade-com",
     "Fort Meade — www.cityoffortmeade.com » ฟีด", "Fort Meade — www.cityoffortmeade.com » ฟีด",
     "us_local", "government",
     "https://www.cityoffortmeade.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Fort Meade — www.cityoffortmeade.com » ฟีด\",\"government\",\"batch:50\"]",
     10800,
     "Fort Meade — www.cityoffortmeade.com » ฟีด — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityoffranklintx_com, "us-b50-cityoffranklintx-com",
     "Franklin", "Franklin",
     "us_local", "government",
     "https://www.cityoffranklintx.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Franklin\",\"government\",\"batch:50\"]",
     10800,
     "Franklin — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_cityoffrostproof_com, "us-b50-cityoffrostproof-com",
     "Frostproof", "Frostproof",
     "us_local", "government",
     "https://cityoffrostproof.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Frostproof\",\"government\",\"batch:50\"]",
     10800,
     "Frostproof — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofganado_com_cms_feed_rss2, "us-b50-cityofganado-com-cms-feed-rss2",
     "Ganado", "Ganado",
     "us_local", "government",
     "http://cityofganado.com/cms/?feed=rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Ganado\",\"government\",\"batch:50\"]",
     10800,
     "Ganado — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_cityofgenesee_com, "us-b50-cityofgenesee-com",
     "Genesee", "Genesee",
     "us_local", "government",
     "https://cityofgenesee.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Genesee\",\"government\",\"batch:50\"]",
     10800,
     "Genesee — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_cityofgeneseo_com, "us-b50-cityofgeneseo-com",
     "Geneseo", "Geneseo",
     "us_local", "government",
     "https://www.cityofgeneseo.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Geneseo\",\"government\",\"batch:50\"]",
     10800,
     "Geneseo — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofglencoe_org, "us-b50-cityofglencoe-org",
     "Glencoe", "Glencoe",
     "us_local", "government",
     "https://cityofglencoe.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Glencoe\",\"government\",\"batch:50\"]",
     10800,
     "Glencoe — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofgoldbar_us, "us-b50-cityofgoldbar-us",
     "Gold Bar", "Gold Bar",
     "us_local", "government",
     "https://cityofgoldbar.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Gold Bar\",\"government\",\"batch:50\"]",
     10800,
     "Gold Bar — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_cityofgraham_com, "us-b50-cityofgraham-com",
     "Graham", "Graham",
     "us_local", "government",
     "https://www.cityofgraham.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Graham\",\"government\",\"batch:50\"]",
     10800,
     "Graham — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofgw_org, "us-b50-cityofgw-org",
     "George West", "George West",
     "us_local", "government",
     "https://cityofgw.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:George West\",\"government\",\"batch:50\"]",
     10800,
     "George West — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofhelena_org, "us-b50-cityofhelena-org",
     "Helena — City of Helena, AL - Official Website | Make Helena Home | H", "Helena — City of Helena, AL - Official Website | Make Helena Home | H",
     "us_local", "government",
     "https://www.cityofhelena.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Helena — City of Helena, AL - Official Website | Make Helena Home | H\",\"government\",\"batch:50\"]",
     10800,
     "Helena — City of Helena, AL - Official Website | Make Helena Home | H — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofhooks_org, "us-b50-cityofhooks-org",
     "Hooks", "Hooks",
     "us_local", "government",
     "https://cityofhooks.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hooks\",\"government\",\"batch:50\"]",
     10800,
     "Hooks — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofhoopeston_com, "us-b50-cityofhoopeston-com",
     "Hoopeston", "Hoopeston",
     "us_local", "government",
     "https://cityofhoopeston.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hoopeston\",\"government\",\"batch:50\"]",
     10800,
     "Hoopeston — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofhoughton_com, "us-b50-cityofhoughton-com",
     "Houghton", "Houghton",
     "us_local", "government",
     "https://www.cityofhoughton.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Houghton\",\"government\",\"batch:50\"]",
     10800,
     "Houghton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofhubbard_oh_gov, "us-b50-cityofhubbard-oh-gov",
     "Hubbard", "Hubbard",
     "us_local", "government",
     "https://cityofhubbard-oh.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hubbard\",\"government\",\"batch:50\"]",
     10800,
     "Hubbard — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofirwinton_org, "us-b50-cityofirwinton-org",
     "Irwinton", "Irwinton",
     "us_local", "government",
     "https://www.cityofirwinton.org/index.php/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Irwinton\",\"government\",\"batch:50\"]",
     10800,
     "Irwinton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofjeff_net, "us-b50-cityofjeff-net",
     "Jeffersonville", "Jeffersonville",
     "us_local", "government",
     "https://cityofjeff.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Jeffersonville\",\"government\",\"batch:50\"]",
     10800,
     "Jeffersonville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofjeffersonville_org, "us-b50-cityofjeffersonville-org",
     "Jeffersonville", "Jeffersonville",
     "us_local", "government",
     "https://cityofjeffersonville.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Jeffersonville\",\"government\",\"batch:50\"]",
     10800,
     "Jeffersonville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofkake_com, "us-b50-cityofkake-com",
     "Kake — City of Kake &raquo; Feed", "Kake — City of Kake &raquo; Feed",
     "us_local", "government",
     "https://www.cityofkake.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Kake — City of Kake &raquo; Feed\",\"government\",\"batch:50\"]",
     10800,
     "Kake — City of Kake &raquo; Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofkellogg_org, "us-b50-cityofkellogg-org",
     "Kellogg", "Kellogg",
     "us_local", "government",
     "https://www.cityofkellogg.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Kellogg\",\"government\",\"batch:50\"]",
     10800,
     "Kellogg — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_cityofkeokuk_org, "us-b50-cityofkeokuk-org",
     "Keokuk", "Keokuk",
     "us_local", "government",
     "http://cityofkeokuk.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Keokuk\",\"government\",\"batch:50\"]",
     10800,
     "Keokuk — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofkingstonga_org_rss2, "us-b50-cityofkingstonga-org-rss2",
     "Kingston", "Kingston",
     "us_local", "government",
     "https://www.cityofkingstonga.org/feed/rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Kingston\",\"government\",\"batch:50\"]",
     10800,
     "Kingston — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_cityofkingsville_com, "us-b50-cityofkingsville-com",
     "Kingsville", "Kingsville",
     "us_local", "government",
     "https://www.cityofkingsville.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Kingsville\",\"government\",\"batch:50\"]",
     10800,
     "Kingsville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofkrum_com, "us-b50-cityofkrum-com",
     "Krum", "Krum",
     "us_local", "government",
     "https://cityofkrum.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Krum\",\"government\",\"batch:50\"]",
     10800,
     "Krum — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_cityoflaferia_com, "us-b50-cityoflaferia-com",
     "La Feria", "La Feria",
     "us_local", "government",
     "https://www.cityoflaferia.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:La Feria\",\"government\",\"batch:50\"]",
     10800,
     "La Feria — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityoflakewood_us, "us-b50-cityoflakewood-us",
     "Lakewood", "Lakewood",
     "us_local", "government",
     "https://cityoflakewood.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lakewood\",\"government\",\"batch:50\"]",
     10800,
     "Lakewood — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityoflastrup_com, "us-b50-cityoflastrup-com",
     "Lastrup", "Lastrup",
     "us_local", "government",
     "https://cityoflastrup.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lastrup\",\"government\",\"batch:50\"]",
     10800,
     "Lastrup — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_cityoflavilla_org, "us-b50-cityoflavilla-org",
     "La Villa", "La Villa",
     "us_local", "government",
     "https://cityoflavilla.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:La Villa\",\"government\",\"batch:50\"]",
     10800,
     "La Villa — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityoflexington_com, "us-b50-cityoflexington-com",
     "Lexington", "Lexington",
     "us_local", "government",
     "https://www.cityoflexington.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lexington\",\"government\",\"batch:50\"]",
     10800,
     "Lexington — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityoflittleflock_com, "us-b50-cityoflittleflock-com",
     "Little Flock", "Little Flock",
     "us_local", "government",
     "https://cityoflittleflock.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Little Flock\",\"government\",\"batch:50\"]",
     10800,
     "Little Flock — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_cityoflonetree_com, "us-b50-cityoflonetree-com",
     "Lone Tree", "Lone Tree",
     "us_local", "government",
     "https://cityoflonetree.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lone Tree\",\"government\",\"batch:50\"]",
     10800,
     "Lone Tree — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityoflucedale_com, "us-b50-cityoflucedale-com",
     "Lucedale", "Lucedale",
     "us_local", "government",
     "https://cityoflucedale.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lucedale\",\"government\",\"batch:50\"]",
     10800,
     "Lucedale — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_cityoflumberton_com, "us-b50-cityoflumberton-com",
     "Lumberton", "Lumberton",
     "us_local", "government",
     "https://cityoflumberton.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lumberton\",\"government\",\"batch:50\"]",
     10800,
     "Lumberton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_cityofmacomb_com, "us-b50-cityofmacomb-com",
     "Macomb", "Macomb",
     "us_local", "government",
     "https://cityofmacomb.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Macomb\",\"government\",\"batch:50\"]",
     10800,
     "Macomb — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofmadisonil_com, "us-b50-cityofmadisonil-com",
     "Madison", "Madison",
     "us_local", "government",
     "https://cityofmadisonil.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Madison\",\"government\",\"batch:50\"]",
     10800,
     "Madison — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_cityofmarion_in_gov_news_events, "us-b50-cityofmarion-in-gov-news-events",
     "Marion — City of Marion - All News & Events", "Marion — City of Marion - All News & Events",
     "us_local", "government",
     "https://cityofmarion.in.gov/news-events/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Marion — City of Marion - All News & Events\",\"government\",\"batch:50\"]",
     10800,
     "Marion — City of Marion - All News & Events — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofmarseilles_com, "us-b50-cityofmarseilles-com",
     "Marseilles", "Marseilles",
     "us_local", "government",
     "https://www.cityofmarseilles.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Marseilles\",\"government\",\"batch:50\"]",
     10800,
     "Marseilles — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofmauldin_org, "us-b50-cityofmauldin-org",
     "Mauldin", "Mauldin",
     "us_local", "government",
     "https://cityofmauldin.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Mauldin\",\"government\",\"batch:50\"]",
     10800,
     "Mauldin — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofmayfield_org, "us-b50-cityofmayfield-org",
     "Mayfield", "Mayfield",
     "us_local", "government",
     "https://cityofmayfield.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Mayfield\",\"government\",\"batch:50\"]",
     10800,
     "Mayfield — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_cityofmccaysvillega_gov, "us-b50-cityofmccaysvillega-gov",
     "McCaysville", "McCaysville",
     "us_local", "government",
     "https://cityofmccaysvillega.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:McCaysville\",\"government\",\"batch:50\"]",
     10800,
     "McCaysville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofmccrory_com, "us-b50-cityofmccrory-com",
     "McCrory", "McCrory",
     "us_local", "government",
     "https://cityofmccrory.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:McCrory\",\"government\",\"batch:50\"]",
     10800,
     "McCrory — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofmelba_org, "us-b50-cityofmelba-org",
     "Melba", "Melba",
     "us_local", "government",
     "https://cityofmelba.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Melba\",\"government\",\"batch:50\"]",
     10800,
     "Melba — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofmentor_com, "us-b50-cityofmentor-com",
     "Mentor", "Mentor",
     "us_local", "government",
     "http://cityofmentor.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Mentor\",\"government\",\"batch:50\"]",
     10800,
     "Mentor — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofmidfield_com_feed_rss2, "us-b50-cityofmidfield-com-feed-rss2",
     "Midfield", "Midfield",
     "us_local", "government",
     "http://www.cityofmidfield.com/?feed=rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Midfield\",\"government\",\"batch:50\"]",
     10800,
     "Midfield — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofmillbrook_org, "us-b50-cityofmillbrook-org",
     "Millbrook", "Millbrook",
     "us_local", "government",
     "https://cityofmillbrook.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Millbrook\",\"government\",\"batch:50\"]",
     10800,
     "Millbrook — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofmorton_com, "us-b50-cityofmorton-com",
     "Morton", "Morton",
     "us_local", "government",
     "https://cityofmorton.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Morton\",\"government\",\"batch:50\"]",
     10800,
     "Morton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_cityofmtmorris_org, "us-b50-cityofmtmorris-org",
     "Mount Morris", "Mount Morris",
     "us_local", "government",
     "https://cityofmtmorris.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Mount Morris\",\"government\",\"batch:50\"]",
     10800,
     "Mount Morris — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_cityofnashvillega_net, "us-b50-cityofnashvillega-net",
     "Nashville", "Nashville",
     "us_local", "government",
     "http://cityofnashvillega.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Nashville\",\"government\",\"batch:50\"]",
     10800,
     "Nashville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_cityofnenana_com_blog_feed, "us-b50-cityofnenana-com-blog-feed",
     "Nenana — City of Nenana - RSS", "Nenana — City of Nenana - RSS",
     "us_local", "government",
     "https://www.cityofnenana.com/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Nenana — City of Nenana - RSS\",\"government\",\"batch:50\"]",
     10800,
     "Nenana — City of Nenana - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 20 entries at probe")

RSSX(us_b50_cityofnewalbany_com, "us-b50-cityofnewalbany-com",
     "New Albany", "New Albany",
     "us_local", "government",
     "https://cityofnewalbany.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:New Albany\",\"government\",\"batch:50\"]",
     10800,
     "New Albany — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofnewcastle_net, "us-b50-cityofnewcastle-net",
     "New Castle", "New Castle",
     "us_local", "government",
     "https://cityofnewcastle.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:New Castle\",\"government\",\"batch:50\"]",
     10800,
     "New Castle — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofnewcumberland_net_rss2, "us-b50-cityofnewcumberland-net-rss2",
     "New Cumberland", "New Cumberland",
     "us_local", "government",
     "https://www.cityofnewcumberland.net/feed/rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:New Cumberland\",\"government\",\"batch:50\"]",
     10800,
     "New Cumberland — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_cityofnewhome_com, "us-b50-cityofnewhome-com",
     "New Home", "New Home",
     "us_local", "government",
     "https://cityofnewhome.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:New Home\",\"government\",\"batch:50\"]",
     10800,
     "New Home — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_cityofnewiberia_com_blog_feed, "us-b50-cityofnewiberia-com-blog-feed",
     "New Iberia — Cityofni - RSS", "New Iberia — Cityofni - RSS",
     "us_local", "government",
     "https://www.cityofnewiberia.com/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:New Iberia — Cityofni - RSS\",\"government\",\"batch:50\"]",
     10800,
     "New Iberia — Cityofni - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 20 entries at probe")

RSSX(us_b50_cityofnewman_gov, "us-b50-cityofnewman-gov",
     "Newman", "Newman",
     "us_local", "government",
     "https://cityofnewman.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Newman\",\"government\",\"batch:50\"]",
     10800,
     "Newman — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofnitro_org, "us-b50-cityofnitro-org",
     "Nitro", "Nitro",
     "us_local", "government",
     "https://cityofnitro.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Nitro\",\"government\",\"batch:50\"]",
     10800,
     "Nitro — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofnoble_org, "us-b50-cityofnoble-org",
     "Noble", "Noble",
     "us_local", "government",
     "https://cityofnoble.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Noble\",\"government\",\"batch:50\"]",
     10800,
     "Noble — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofoberlin_com, "us-b50-cityofoberlin-com",
     "Oberlin", "Oberlin",
     "us_local", "government",
     "https://cityofoberlin.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Oberlin\",\"government\",\"batch:50\"]",
     10800,
     "Oberlin — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofolivet_org, "us-b50-cityofolivet-org",
     "Olivet", "Olivet",
     "us_local", "government",
     "https://cityofolivet.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Olivet\",\"government\",\"batch:50\"]",
     10800,
     "Olivet — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_cityofopelousas_com, "us-b50-cityofopelousas-com",
     "Opelousas", "Opelousas",
     "us_local", "government",
     "https://www.cityofopelousas.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Opelousas\",\"government\",\"batch:50\"]",
     10800,
     "Opelousas — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofopp_com, "us-b50-cityofopp-com",
     "Opp", "Opp",
     "us_local", "government",
     "https://www.cityofopp.com/index.php/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Opp\",\"government\",\"batch:50\"]",
     10800,
     "Opp — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_cityoforangecove_com, "us-b50-cityoforangecove-com",
     "Orange Cove", "Orange Cove",
     "us_local", "government",
     "https://cityoforangecove.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Orange Cove\",\"government\",\"batch:50\"]",
     10800,
     "Orange Cove — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_cityoforofino_org, "us-b50-cityoforofino-org",
     "Orofino", "Orofino",
     "us_local", "government",
     "https://cityoforofino.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Orofino\",\"government\",\"batch:50\"]",
     10800,
     "Orofino — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofpage_org, "us-b50-cityofpage-org",
     "Page", "Page",
     "us_local", "government",
     "https://cityofpage.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Page\",\"government\",\"batch:50\"]",
     10800,
     "Page — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofpearl_com, "us-b50-cityofpearl-com",
     "Pearl", "Pearl",
     "us_local", "government",
     "https://www.cityofpearl.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Pearl\",\"government\",\"batch:50\"]",
     10800,
     "Pearl — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofpickens_com, "us-b50-cityofpickens-com",
     "Pickens", "Pickens",
     "us_local", "government",
     "https://cityofpickens.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Pickens\",\"government\",\"batch:50\"]",
     10800,
     "Pickens — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofpocomoke_com, "us-b50-cityofpocomoke-com",
     "Pocomoke City — Pocomoke City, Maryland RSS", "Pocomoke City — Pocomoke City, Maryland RSS",
     "us_local", "government",
     "https://www.cityofpocomoke.com/rss.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Pocomoke City — Pocomoke City, Maryland RSS\",\"government\",\"batch:50\"]",
     10800,
     "Pocomoke City — Pocomoke City, Maryland RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_cityofponderay_org, "us-b50-cityofponderay-org",
     "Ponderay — City of Ponderay RSS Feed", "Ponderay — City of Ponderay RSS Feed",
     "us_local", "government",
     "https://cityofponderay.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Ponderay — City of Ponderay RSS Feed\",\"government\",\"batch:50\"]",
     10800,
     "Ponderay — City of Ponderay RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_cityofportaransas_org, "us-b50-cityofportaransas-org",
     "Port Aransas", "Port Aransas",
     "us_local", "government",
     "https://cityofportaransas.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Port Aransas\",\"government\",\"batch:50\"]",
     10800,
     "Port Aransas — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofredbay_org_rss2, "us-b50-cityofredbay-org-rss2",
     "Red Bay", "Red Bay",
     "us_local", "government",
     "https://www.cityofredbay.org/feed/rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Red Bay\",\"government\",\"batch:50\"]",
     10800,
     "Red Bay — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_cityofredbud_org, "us-b50-cityofredbud-org",
     "Red Bud", "Red Bud",
     "us_local", "government",
     "https://cityofredbud.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Red Bud\",\"government\",\"batch:50\"]",
     10800,
     "Red Bud — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_cityofrichmondmo_org, "us-b50-cityofrichmondmo-org",
     "Richmond", "Richmond",
     "us_local", "government",
     "https://cityofrichmondmo.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Richmond\",\"government\",\"batch:50\"]",
     10800,
     "Richmond — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofrincon_com, "us-b50-cityofrincon-com",
     "Rincon", "Rincon",
     "us_local", "government",
     "https://www.cityofrincon.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Rincon\",\"government\",\"batch:50\"]",
     10800,
     "Rincon — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofroberta_com, "us-b50-cityofroberta-com",
     "Roberta", "Roberta",
     "us_local", "government",
     "https://www.cityofroberta.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Roberta\",\"government\",\"batch:50\"]",
     10800,
     "Roberta — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_cityofrobinson_com, "us-b50-cityofrobinson-com",
     "Robinson", "Robinson",
     "us_local", "government",
     "https://cityofrobinson.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Robinson\",\"government\",\"batch:50\"]",
     10800,
     "Robinson — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_cityofronan_org, "us-b50-cityofronan-org",
     "Ronan", "Ronan",
     "us_local", "government",
     "https://cityofronan.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Ronan\",\"government\",\"batch:50\"]",
     10800,
     "Ronan — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofrushville_in_gov, "us-b50-cityofrushville-in-gov",
     "Rushville", "Rushville",
     "us_local", "government",
     "https://www.cityofrushville.in.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Rushville\",\"government\",\"batch:50\"]",
     10800,
     "Rushville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofsalemin_com_index_format_feed_type_atom, "us-b50-cityofsalemin-com-index-format-feed-type-atom",
     "Salem — Atom 1.0", "Salem — Atom 1.0",
     "us_local", "government",
     "https://www.cityofsalemin.com/index.php?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Salem — Atom 1.0\",\"government\",\"batch:50\"]",
     10800,
     "Salem — Atom 1.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_cityofsalemin_com_index_format_feed_type_rss, "us-b50-cityofsalemin-com-index-format-feed-type-rss",
     "Salem", "Salem",
     "us_local", "government",
     "https://www.cityofsalemin.com/index.php?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Salem\",\"government\",\"batch:50\"]",
     10800,
     "Salem — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_cityofsalemohio_org, "us-b50-cityofsalemohio-org",
     "Salem", "Salem",
     "us_local", "government",
     "https://cityofsalemohio.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Salem\",\"government\",\"batch:50\"]",
     10800,
     "Salem — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofscottsboro_com, "us-b50-cityofscottsboro-com",
     "Scottsboro", "Scottsboro",
     "us_local", "government",
     "https://cityofscottsboro.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Scottsboro\",\"government\",\"batch:50\"]",
     10800,
     "Scottsboro — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofsomerset_com, "us-b50-cityofsomerset-com",
     "Somerset", "Somerset",
     "us_local", "government",
     "https://www.cityofsomerset.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Somerset\",\"government\",\"batch:50\"]",
     10800,
     "Somerset — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_cityofsouthfield_com, "us-b50-cityofsouthfield-com",
     "Southfield", "Southfield",
     "us_local", "government",
     "https://www.cityofsouthfield.com/rss.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Southfield\",\"government\",\"batch:50\"]",
     10800,
     "Southfield — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofspiritlake_org, "us-b50-cityofspiritlake-org",
     "Spirit Lake", "Spirit Lake",
     "us_local", "government",
     "https://www.cityofspiritlake.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Spirit Lake\",\"government\",\"batch:50\"]",
     10800,
     "Spirit Lake — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofstafford_com, "us-b50-cityofstafford-com",
     "Stafford", "Stafford",
     "us_local", "government",
     "https://cityofstafford.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Stafford\",\"government\",\"batch:50\"]",
     10800,
     "Stafford — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_cityofstorycity_org, "us-b50-cityofstorycity-org",
     "Story City", "Story City",
     "us_local", "government",
     "https://cityofstorycity.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Story City\",\"government\",\"batch:50\"]",
     10800,
     "Story City — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_cityofsugarhill_com, "us-b50-cityofsugarhill-com",
     "Sugar Hill", "Sugar Hill",
     "us_local", "government",
     "https://cityofsugarhill.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Sugar Hill\",\"government\",\"batch:50\"]",
     10800,
     "Sugar Hill — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofsumas_com, "us-b50-cityofsumas-com",
     "Sumas", "Sumas",
     "us_local", "government",
     "https://www.cityofsumas.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Sumas\",\"government\",\"batch:50\"]",
     10800,
     "Sumas — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofswainsboro_org, "us-b50-cityofswainsboro-org",
     "Swainsboro", "Swainsboro",
     "us_local", "government",
     "https://cityofswainsboro.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Swainsboro\",\"government\",\"batch:50\"]",
     10800,
     "Swainsboro — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofswanvalley_com, "us-b50-cityofswanvalley-com",
     "Swan Valley", "Swan Valley",
     "us_local", "government",
     "https://www.cityofswanvalley.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Swan Valley\",\"government\",\"batch:50\"]",
     10800,
     "Swan Valley — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofunionky_org, "us-b50-cityofunionky-org",
     "Union", "Union",
     "us_local", "government",
     "https://cityofunionky.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Union\",\"government\",\"batch:50\"]",
     10800,
     "Union — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofvancouver_us, "us-b50-cityofvancouver-us",
     "Vancouver", "Vancouver",
     "us_local", "government",
     "https://www.cityofvancouver.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Vancouver\",\"government\",\"batch:50\"]",
     10800,
     "Vancouver — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofvinton_org, "us-b50-cityofvinton-org",
     "Vinton", "Vinton",
     "us_local", "government",
     "https://www.cityofvinton.org/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Vinton\",\"government\",\"batch:50\"]",
     10800,
     "Vinton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofwaelder_org, "us-b50-cityofwaelder-org",
     "Waelder", "Waelder",
     "us_local", "government",
     "https://www.cityofwaelder.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Waelder\",\"government\",\"batch:50\"]",
     10800,
     "Waelder — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_cityofwalcott_com, "us-b50-cityofwalcott-com",
     "Walcott", "Walcott",
     "us_local", "government",
     "https://cityofwalcott.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Walcott\",\"government\",\"batch:50\"]",
     10800,
     "Walcott — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofwarwv_com, "us-b50-cityofwarwv-com",
     "War", "War",
     "us_local", "government",
     "https://cityofwarwv.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:War\",\"government\",\"batch:50\"]",
     10800,
     "War — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofwaterford_org, "us-b50-cityofwaterford-org",
     "Waterford", "Waterford",
     "us_local", "government",
     "https://cityofwaterford.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Waterford\",\"government\",\"batch:50\"]",
     10800,
     "Waterford — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_cityofwaukon_com, "us-b50-cityofwaukon-com",
     "Waukon", "Waukon",
     "us_local", "government",
     "https://www.cityofwaukon.com/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Waukon\",\"government\",\"batch:50\"]",
     10800,
     "Waukon — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofwaynesboro_org_format_feed_type_atom, "us-b50-cityofwaynesboro-org-format-feed-type-atom",
     "Waynesboro — Home", "Waynesboro — Home",
     "us_local", "government",
     "https://www.cityofwaynesboro.org/?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Waynesboro — Home\",\"government\",\"batch:50\"]",
     10800,
     "Waynesboro — Home — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_cityofwaynesboro_org_format_feed_type_rss, "us-b50-cityofwaynesboro-org-format-feed-type-rss",
     "Waynesboro — Home", "Waynesboro — Home",
     "us_local", "government",
     "https://www.cityofwaynesboro.org/?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Waynesboro — Home\",\"government\",\"batch:50\"]",
     10800,
     "Waynesboro — Home — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_cityofweatherford_com, "us-b50-cityofweatherford-com",
     "Weatherford", "Weatherford",
     "us_local", "government",
     "https://cityofweatherford.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Weatherford\",\"government\",\"batch:50\"]",
     10800,
     "Weatherford — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 8 entries at probe")

RSSX(us_b50_cityofwellston_org, "us-b50-cityofwellston-org",
     "Wellston", "Wellston",
     "us_local", "government",
     "https://cityofwellston.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Wellston\",\"government\",\"batch:50\"]",
     10800,
     "Wellston — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_cityofwellsvilleks_org, "us-b50-cityofwellsvilleks-org",
     "Wellsville", "Wellsville",
     "us_local", "government",
     "https://www.cityofwellsvilleks.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Wellsville\",\"government\",\"batch:50\"]",
     10800,
     "Wellsville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_cityofwenona_org, "us-b50-cityofwenona-org",
     "Wenona", "Wenona",
     "us_local", "government",
     "https://cityofwenona.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Wenona\",\"government\",\"batch:50\"]",
     10800,
     "Wenona — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofwest_com, "us-b50-cityofwest-com",
     "West", "West",
     "us_local", "government",
     "https://www.cityofwest.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:West\",\"government\",\"batch:50\"]",
     10800,
     "West — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofwestmorland_net, "us-b50-cityofwestmorland-net",
     "Westmorland", "Westmorland",
     "us_local", "government",
     "https://www.cityofwestmorland.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Westmorland\",\"government\",\"batch:50\"]",
     10800,
     "Westmorland — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofwheatland_org, "us-b50-cityofwheatland-org",
     "Wheatland", "Wheatland",
     "us_local", "government",
     "https://cityofwheatland.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Wheatland\",\"government\",\"batch:50\"]",
     10800,
     "Wheatland — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_cityofwinthrop_com, "us-b50-cityofwinthrop-com",
     "Winthrop", "Winthrop",
     "us_local", "government",
     "https://www.cityofwinthrop.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Winthrop\",\"government\",\"batch:50\"]",
     10800,
     "Winthrop — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofwolverton_com, "us-b50-cityofwolverton-com",
     "Wolverton", "Wolverton",
     "us_local", "government",
     "https://cityofwolverton.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Wolverton\",\"government\",\"batch:50\"]",
     10800,
     "Wolverton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cityofwoodlake_com, "us-b50-cityofwoodlake-com",
     "Woodlake", "Woodlake",
     "us_local", "government",
     "https://cityofwoodlake.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Woodlake\",\"government\",\"batch:50\"]",
     10800,
     "Woodlake — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofwrens_com, "us-b50-cityofwrens-com",
     "Wrens", "Wrens",
     "us_local", "government",
     "https://cityofwrens.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Wrens\",\"government\",\"batch:50\"]",
     10800,
     "Wrens — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_cityofzion_com, "us-b50-cityofzion-com",
     "Zion", "Zion",
     "us_local", "government",
     "https://www.cityofzion.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Zion\",\"government\",\"batch:50\"]",
     10800,
     "Zion — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_clearlakeiowa_com, "us-b50-clearlakeiowa-com",
     "Clear Lake", "Clear Lake",
     "us_local", "government",
     "https://clearlakeiowa.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Clear Lake\",\"government\",\"batch:50\"]",
     10800,
     "Clear Lake — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_clintonmo_com, "us-b50-clintonmo-com",
     "Clinton", "Clinton",
     "us_local", "government",
     "https://clintonmo.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Clinton\",\"government\",\"batch:50\"]",
     10800,
     "Clinton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_clintonms_org, "us-b50-clintonms-org",
     "Clinton", "Clinton",
     "us_local", "government",
     "https://clintonms.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Clinton\",\"government\",\"batch:50\"]",
     10800,
     "Clinton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cloverport_com, "us-b50-cloverport-com",
     "Cloverport", "Cloverport",
     "us_local", "government",
     "http://www.cloverport.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Cloverport\",\"government\",\"batch:50\"]",
     10800,
     "Cloverport — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_cob_org, "us-b50-cob-org",
     "Bellingham", "Bellingham",
     "us_local", "government",
     "https://cob.org/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Bellingham\",\"government\",\"batch:50\"]",
     10800,
     "Bellingham — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_colo_iowa_org, "us-b50-colo-iowa-org",
     "Colo", "Colo",
     "us_local", "government",
     "https://www.colo-iowa.org/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Colo\",\"government\",\"batch:50\"]",
     10800,
     "Colo — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_colognemn_com, "us-b50-colognemn-com",
     "Cologne — Cologne, Minnesota | A great place to Live, Work and Grow! »", "Cologne — Cologne, Minnesota | A great place to Live, Work and Grow! »",
     "us_local", "government",
     "http://www.colognemn.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Cologne — Cologne, Minnesota | A great place to Live, Work and Grow! »\",\"government\",\"batch:50\"]",
     10800,
     "Cologne — Cologne, Minnesota | A great place to Live, Work and Grow! » — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_columbiacity_net, "us-b50-columbiacity-net",
     "Columbia City", "Columbia City",
     "us_local", "government",
     "https://columbiacity.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Columbia City\",\"government\",\"batch:50\"]",
     10800,
     "Columbia City — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_columbus_in_gov, "us-b50-columbus-in-gov",
     "Columbus", "Columbus",
     "us_local", "government",
     "https://www.columbus.in.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Columbus\",\"government\",\"batch:50\"]",
     10800,
     "Columbus — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_conwayarkansas_org, "us-b50-conwayarkansas-org",
     "Conway", "Conway",
     "us_local", "government",
     "https://conwayarkansas.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Conway\",\"government\",\"batch:50\"]",
     10800,
     "Conway — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cornelia_city, "us-b50-cornelia-city",
     "Cornelia", "Cornelia",
     "us_local", "government",
     "https://cornelia.city/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Cornelia\",\"government\",\"batch:50\"]",
     10800,
     "Cornelia — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_countryclubhills_org, "us-b50-countryclubhills-org",
     "Country Club Hills", "Country Club Hills",
     "us_local", "government",
     "https://countryclubhills.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Country Club Hills\",\"government\",\"batch:50\"]",
     10800,
     "Country Club Hills — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_crowley_la_com, "us-b50-crowley-la-com",
     "Crowley", "Crowley",
     "us_local", "government",
     "https://crowley-la.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Crowley\",\"government\",\"batch:50\"]",
     10800,
     "Crowley — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_cubamomurals_com, "us-b50-cubamomurals-com",
     "Cuba", "Cuba",
     "us_local", "government",
     "https://cubamomurals.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Cuba\",\"government\",\"batch:50\"]",
     10800,
     "Cuba — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_dadevillechamber_com_rss2, "us-b50-dadevillechamber-com-rss2",
     "Dadeville", "Dadeville",
     "us_local", "government",
     "https://www.dadevillechamber.com/feed/rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Dadeville\",\"government\",\"batch:50\"]",
     10800,
     "Dadeville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_deerfieldkansas_com, "us-b50-deerfieldkansas-com",
     "Deerfield", "Deerfield",
     "us_local", "government",
     "https://deerfieldkansas.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Deerfield\",\"government\",\"batch:50\"]",
     10800,
     "Deerfield — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_denisonia_com_feed_rss2, "us-b50-denisonia-com-feed-rss2",
     "Denison", "Denison",
     "us_local", "government",
     "https://denisonia.com/?feed=rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Denison\",\"government\",\"batch:50\"]",
     10800,
     "Denison — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_desoto_ia_org_feed_rss2, "us-b50-desoto-ia-org-feed-rss2",
     "De Soto", "De Soto",
     "us_local", "government",
     "https://desoto-ia.org/?feed=rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:De Soto\",\"government\",\"batch:50\"]",
     10800,
     "De Soto — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_dillardgeorgia_com_wp_content_plugins_nextgen_gallery_src_legacy_media_rss, "us-b50-dillardgeorgia-com-wp-content-plugins-nextgen-gallery-src-legacy-media-rss",
     "Dillard — NextGEN Gallery RSS Feed", "Dillard — NextGEN Gallery RSS Feed",
     "us_local", "government",
     "https://www.dillardgeorgia.com/wp-content/plugins/nextgen-gallery/src/Legacy/xml/media-rss.php",
     "en", "[\"gov\",\"country:us\",\"publisher:Dillard — NextGEN Gallery RSS Feed\",\"government\",\"batch:50\"]",
     10800,
     "Dillard — NextGEN Gallery RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 196 entries at probe")

RSSX(us_b50_dinuba_org_format_feed_type_atom, "us-b50-dinuba-org-format-feed-type-atom",
     "Dinuba — Dinuba - \"Together, a better community.\"", "Dinuba — Dinuba - \"Together, a better community.\"",
     "us_local", "government",
     "https://www.dinuba.org/?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Dinuba — Dinuba - Together, a better community.\",\"government\",\"batch:50\"]",
     10800,
     "Dinuba — Dinuba - \"Together, a better community.\" — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 9 entries at probe")

RSSX(us_b50_dinuba_org_format_feed_type_rss, "us-b50-dinuba-org-format-feed-type-rss",
     "Dinuba — Dinuba - \"Together, a better community.\"", "Dinuba — Dinuba - \"Together, a better community.\"",
     "us_local", "government",
     "https://www.dinuba.org/?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Dinuba — Dinuba - Together, a better community.\",\"government\",\"batch:50\"]",
     10800,
     "Dinuba — Dinuba - \"Together, a better community.\" — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 9 entries at probe")

RSSX(us_b50_discoverlogan_com, "us-b50-discoverlogan-com",
     "Logan", "Logan",
     "us_local", "government",
     "https://discoverlogan.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Logan\",\"government\",\"batch:50\"]",
     10800,
     "Logan — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_discoverthomasville_com, "us-b50-discoverthomasville-com",
     "Thomasville", "Thomasville",
     "us_local", "government",
     "https://discoverthomasville.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Thomasville\",\"government\",\"batch:50\"]",
     10800,
     "Thomasville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_districtheights_org, "us-b50-districtheights-org",
     "District Heights", "District Heights",
     "us_local", "government",
     "https://districtheights.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:District Heights\",\"government\",\"batch:50\"]",
     10800,
     "District Heights — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_dorrisca_us, "us-b50-dorrisca-us",
     "Dorris", "Dorris",
     "us_local", "government",
     "https://www.dorrisca.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Dorris\",\"government\",\"batch:50\"]",
     10800,
     "Dorris — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_duenwegmo_com, "us-b50-duenwegmo-com",
     "Duenweg", "Duenweg",
     "us_local", "government",
     "https://duenwegmo.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Duenweg\",\"government\",\"batch:50\"]",
     10800,
     "Duenweg — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_dwightks_net, "us-b50-dwightks-net",
     "Dwight", "Dwight",
     "us_local", "government",
     "http://dwightks.net/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Dwight\",\"government\",\"batch:50\"]",
     10800,
     "Dwight — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_eaglegroveiowa_org, "us-b50-eaglegroveiowa-org",
     "Eagle Grove", "Eagle Grove",
     "us_local", "government",
     "https://eaglegroveiowa.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Eagle Grove\",\"government\",\"batch:50\"]",
     10800,
     "Eagle Grove — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_earlhamiowa_org, "us-b50-earlhamiowa-org",
     "Earlham", "Earlham",
     "us_local", "government",
     "https://earlhamiowa.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Earlham\",\"government\",\"batch:50\"]",
     10800,
     "Earlham — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_easthelenamt_us_coeh, "us-b50-easthelenamt-us-coeh",
     "East Helena", "East Helena",
     "us_local", "government",
     "https://easthelenamt.us/coeh/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:East Helena\",\"government\",\"batch:50\"]",
     10800,
     "East Helena — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_eastlakeohio_com, "us-b50-eastlakeohio-com",
     "Eastlake", "Eastlake",
     "us_local", "government",
     "https://eastlakeohio.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Eastlake\",\"government\",\"batch:50\"]",
     10800,
     "Eastlake — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_edgertonks_org, "us-b50-edgertonks-org",
     "Edgerton", "Edgerton",
     "us_local", "government",
     "https://edgertonks.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Edgerton\",\"government\",\"batch:50\"]",
     10800,
     "Edgerton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_edinburgnd_com, "us-b50-edinburgnd-com",
     "Edinburg", "Edinburg",
     "us_local", "government",
     "https://edinburgnd.com/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Edinburg\",\"government\",\"batch:50\"]",
     10800,
     "Edinburg — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_elginmn_com, "us-b50-elginmn-com",
     "Elgin", "Elgin",
     "us_local", "government",
     "https://www.elginmn.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Elgin\",\"government\",\"batch:50\"]",
     10800,
     "Elgin — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_elmwoodil_com_blog_feed, "us-b50-elmwoodil-com-blog-feed",
     "Elmwood — City of Elmwood, IL - RSS", "Elmwood — City of Elmwood, IL - RSS",
     "us_local", "government",
     "https://www.elmwoodil.com/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Elmwood — City of Elmwood, IL - RSS\",\"government\",\"batch:50\"]",
     10800,
     "Elmwood — City of Elmwood, IL - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 20 entries at probe")

RSSX(us_b50_enjoyrisingsun_com, "us-b50-enjoyrisingsun-com",
     "Rising Sun", "Rising Sun",
     "us_local", "government",
     "https://enjoyrisingsun.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Rising Sun\",\"government\",\"batch:50\"]",
     10800,
     "Rising Sun — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_ennis_texas_com, "us-b50-ennis-texas-com",
     "Ennis", "Ennis",
     "us_local", "government",
     "https://www.ennis-texas.com/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Ennis\",\"government\",\"batch:50\"]",
     10800,
     "Ennis — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_eskridgeks_org_home, "us-b50-eskridgeks-org-home",
     "Eskridge", "Eskridge",
     "us_local", "government",
     "http://eskridgeks.org/home/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Eskridge\",\"government\",\"batch:50\"]",
     10800,
     "Eskridge — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_experiencecamdensc_com, "us-b50-experiencecamdensc-com",
     "Camden", "Camden",
     "us_local", "government",
     "https://experiencecamdensc.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Camden\",\"government\",\"batch:50\"]",
     10800,
     "Camden — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_explorenewyorkmills_com, "us-b50-explorenewyorkmills-com",
     "New York Mills", "New York Mills",
     "us_local", "government",
     "https://www.explorenewyorkmills.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:New York Mills\",\"government\",\"batch:50\"]",
     10800,
     "New York Mills — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_fairmont_org, "us-b50-fairmont-org",
     "Fairmont", "Fairmont",
     "us_local", "government",
     "https://fairmont.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Fairmont\",\"government\",\"batch:50\"]",
     10800,
     "Fairmont — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_fanningspringsflorida_org, "us-b50-fanningspringsflorida-org",
     "Fanning Springs", "Fanning Springs",
     "us_local", "government",
     "https://fanningspringsflorida.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Fanning Springs\",\"government\",\"batch:50\"]",
     10800,
     "Fanning Springs — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_fdl_wi_gov, "us-b50-fdl-wi-gov",
     "Fond du Lac", "Fond du Lac",
     "us_local", "government",
     "https://www.fdl.wi.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Fond du Lac\",\"government\",\"batch:50\"]",
     10800,
     "Fond du Lac — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_fernanvillage_org_feed_rss2, "us-b50-fernanvillage-org-feed-rss2",
     "Fernan Lake Village", "Fernan Lake Village",
     "us_local", "government",
     "http://www.fernanvillage.org/?feed=rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Fernan Lake Village\",\"government\",\"batch:50\"]",
     10800,
     "Fernan Lake Village — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_fertileiowa_us, "us-b50-fertileiowa-us",
     "Fertile", "Fertile",
     "us_local", "government",
     "https://fertileiowa.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Fertile\",\"government\",\"batch:50\"]",
     10800,
     "Fertile — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_firebaugh_org, "us-b50-firebaugh-org",
     "Firebaugh", "Firebaugh",
     "us_local", "government",
     "https://firebaugh.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Firebaugh\",\"government\",\"batch:50\"]",
     10800,
     "Firebaugh — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_floresvilletx_gov, "us-b50-floresvilletx-gov",
     "Floresville", "Floresville",
     "us_local", "government",
     "https://www.floresvilletx.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Floresville\",\"government\",\"batch:50\"]",
     10800,
     "Floresville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_folkston_com, "us-b50-folkston-com",
     "Folkston", "Folkston",
     "us_local", "government",
     "https://folkston.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Folkston\",\"government\",\"batch:50\"]",
     10800,
     "Folkston — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_forkswashington_org, "us-b50-forkswashington-org",
     "Forks", "Forks",
     "us_local", "government",
     "https://forkswashington.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Forks\",\"government\",\"batch:50\"]",
     10800,
     "Forks — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_fortbenton_com, "us-b50-fortbenton-com",
     "Fort Benton", "Fort Benton",
     "us_local", "government",
     "https://fortbenton.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Fort Benton\",\"government\",\"batch:50\"]",
     10800,
     "Fort Benton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_fortogov_com, "us-b50-fortogov-com",
     "Fort Oglethorpe", "Fort Oglethorpe",
     "us_local", "government",
     "https://fortogov.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Fort Oglethorpe\",\"government\",\"batch:50\"]",
     10800,
     "Fort Oglethorpe — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 9 entries at probe")

RSSX(us_b50_fortpayne_org, "us-b50-fortpayne-org",
     "Fort Payne", "Fort Payne",
     "us_local", "government",
     "https://fortpayne.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Fort Payne\",\"government\",\"batch:50\"]",
     10800,
     "Fort Payne — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_fortvalleyga_org, "us-b50-fortvalleyga-org",
     "Fort Valley", "Fort Valley",
     "us_local", "government",
     "https://www.fortvalleyga.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Fort Valley\",\"government\",\"batch:50\"]",
     10800,
     "Fort Valley — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_fullerton_ne_com, "us-b50-fullerton-ne-com",
     "Fullerton", "Fullerton",
     "us_local", "government",
     "https://fullerton-ne.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Fullerton\",\"government\",\"batch:50\"]",
     10800,
     "Fullerton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_gibsoncityillinois_com, "us-b50-gibsoncityillinois-com",
     "Gibson City", "Gibson City",
     "us_local", "government",
     "https://gibsoncityillinois.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Gibson City\",\"government\",\"batch:50\"]",
     10800,
     "Gibson City — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_glasgowepb_com, "us-b50-glasgowepb-com",
     "Glasgow", "Glasgow",
     "us_local", "government",
     "https://www.glasgowepb.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Glasgow\",\"government\",\"batch:50\"]",
     10800,
     "Glasgow — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_glencoemn_org, "us-b50-glencoemn-org",
     "Glencoe", "Glencoe",
     "us_local", "government",
     "https://www.glencoemn.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Glencoe\",\"government\",\"batch:50\"]",
     10800,
     "Glencoe — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_gohammond_com, "us-b50-gohammond-com",
     "Hammond", "Hammond",
     "us_local", "government",
     "https://www.gohammond.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hammond\",\"government\",\"batch:50\"]",
     10800,
     "Hammond — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_gonzalesla_com, "us-b50-gonzalesla-com",
     "Gonzales", "Gonzales",
     "us_local", "government",
     "https://gonzalesla.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Gonzales\",\"government\",\"batch:50\"]",
     10800,
     "Gonzales — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_goodmanmo_net, "us-b50-goodmanmo-net",
     "Goodman", "Goodman",
     "us_local", "government",
     "http://www.goodmanmo.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Goodman\",\"government\",\"batch:50\"]",
     10800,
     "Goodman — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_goshen_in_gov, "us-b50-goshen-in-gov",
     "Goshen", "Goshen",
     "us_local", "government",
     "https://goshen.in.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Goshen\",\"government\",\"batch:50\"]",
     10800,
     "Goshen — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 12 entries at probe")

RSSX(us_b50_grandhaven_org_pub_news_city_news, "us-b50-grandhaven-org-pub-news-city-news",
     "Grand Haven — City News", "Grand Haven — City News",
     "us_local", "government",
     "https://grandhaven.org/pub/news/feeds/city-news",
     "en", "[\"gov\",\"country:us\",\"publisher:Grand Haven — City News\",\"government\",\"batch:50\"]",
     10800,
     "Grand Haven — City News — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_grandmeadow_org, "us-b50-grandmeadow-org",
     "Grand Meadow", "Grand Meadow",
     "us_local", "government",
     "https://grandmeadow.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Grand Meadow\",\"government\",\"batch:50\"]",
     10800,
     "Grand Meadow — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_grangeville_us, "us-b50-grangeville-us",
     "Grangeville", "Grangeville",
     "us_local", "government",
     "https://grangeville.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Grangeville\",\"government\",\"batch:50\"]",
     10800,
     "Grangeville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_grayga_us, "us-b50-grayga-us",
     "Gray", "Gray",
     "us_local", "government",
     "http://grayga.us/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Gray\",\"government\",\"batch:50\"]",
     10800,
     "Gray — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_graymoordevondale_com, "us-b50-graymoordevondale-com",
     "Graymoor-Devondale", "Graymoor-Devondale",
     "us_local", "government",
     "https://graymoordevondale.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Graymoor-Devondale\",\"government\",\"batch:50\"]",
     10800,
     "Graymoor-Devondale — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_greaterstlinc_com, "us-b50-greaterstlinc-com",
     "St. Louis", "St. Louis",
     "us_local", "government",
     "https://greaterstlinc.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:St. Louis\",\"government\",\"batch:50\"]",
     10800,
     "St. Louis — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_greenfieldiowa_com_rss2, "us-b50-greenfieldiowa-com-rss2",
     "Greenfield", "Greenfield",
     "us_local", "government",
     "https://www.greenfieldiowa.com/feed/rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Greenfield\",\"government\",\"batch:50\"]",
     10800,
     "Greenfield — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_greersferry_com, "us-b50-greersferry-com",
     "Greers Ferry", "Greers Ferry",
     "us_local", "government",
     "https://www.greersferry.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Greers Ferry\",\"government\",\"batch:50\"]",
     10800,
     "Greers Ferry — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_gretnala_com_events, "us-b50-gretnala-com-events",
     "Gretna — City Of Gretna » Events Feed", "Gretna — City Of Gretna » Events Feed",
     "us_local", "government",
     "https://www.gretnala.com/events/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Gretna — City Of Gretna » Events Feed\",\"government\",\"batch:50\"]",
     10800,
     "Gretna — City Of Gretna » Events Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_guntersvilleal_org, "us-b50-guntersvilleal-org",
     "Guntersville", "Guntersville",
     "us_local", "government",
     "https://guntersvilleal.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Guntersville\",\"government\",\"batch:50\"]",
     10800,
     "Guntersville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_guthrieky_com, "us-b50-guthrieky-com",
     "Guthrie", "Guthrie",
     "us_local", "government",
     "https://guthrieky.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Guthrie\",\"government\",\"batch:50\"]",
     10800,
     "Guthrie — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_haileycityhall_org, "us-b50-haileycityhall-org",
     "Hailey", "Hailey",
     "us_local", "government",
     "https://haileycityhall.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hailey\",\"government\",\"batch:50\"]",
     10800,
     "Hailey — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_hallockmn_org, "us-b50-hallockmn-org",
     "Hallock", "Hallock",
     "us_local", "government",
     "https://hallockmn.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hallock\",\"government\",\"batch:50\"]",
     10800,
     "Hallock — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_halstad_com, "us-b50-halstad-com",
     "Halstad", "Halstad",
     "us_local", "government",
     "https://halstad.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Halstad\",\"government\",\"batch:50\"]",
     10800,
     "Halstad — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_hamburgark_com, "us-b50-hamburgark-com",
     "Hamburg", "Hamburg",
     "us_local", "government",
     "https://hamburgark.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hamburg\",\"government\",\"batch:50\"]",
     10800,
     "Hamburg — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_hattiesburgms_com, "us-b50-hattiesburgms-com",
     "Hattiesburg", "Hattiesburg",
     "us_local", "government",
     "https://www.hattiesburgms.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hattiesburg\",\"government\",\"batch:50\"]",
     10800,
     "Hattiesburg — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_hcv_org, "us-b50-hcv-org",
     "Hill Country Village", "Hill Country Village",
     "us_local", "government",
     "https://hcv.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hill Country Village\",\"government\",\"batch:50\"]",
     10800,
     "Hill Country Village — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_heathtx_com, "us-b50-heathtx-com",
     "Heath", "Heath",
     "us_local", "government",
     "https://www.heathtx.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Heath\",\"government\",\"batch:50\"]",
     10800,
     "Heath — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_hermantownmn_com, "us-b50-hermantownmn-com",
     "Hermantown", "Hermantown",
     "us_local", "government",
     "https://hermantownmn.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hermantown\",\"government\",\"batch:50\"]",
     10800,
     "Hermantown — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_hickoryhillsil_org, "us-b50-hickoryhillsil-org",
     "Hickory Hills", "Hickory Hills",
     "us_local", "government",
     "https://hickoryhillsil.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hickory Hills\",\"government\",\"batch:50\"]",
     10800,
     "Hickory Hills — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_hico_tx_com, "us-b50-hico-tx-com",
     "Hico", "Hico",
     "us_local", "government",
     "https://www.hico-tx.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hico\",\"government\",\"batch:50\"]",
     10800,
     "Hico — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_hillsboroillinois_net, "us-b50-hillsboroillinois-net",
     "Hillsboro", "Hillsboro",
     "us_local", "government",
     "https://hillsboroillinois.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hillsboro\",\"government\",\"batch:50\"]",
     10800,
     "Hillsboro — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_hintoniowa_com, "us-b50-hintoniowa-com",
     "Hinton", "Hinton",
     "us_local", "government",
     "https://hintoniowa.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hinton\",\"government\",\"batch:50\"]",
     10800,
     "Hinton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_horizoncity_org, "us-b50-horizoncity-org",
     "Horizon City", "Horizon City",
     "us_local", "government",
     "https://www.horizoncity.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Horizon City\",\"government\",\"batch:50\"]",
     10800,
     "Horizon City — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_hurstbourne_org, "us-b50-hurstbourne-org",
     "Hurstbourne", "Hurstbourne",
     "us_local", "government",
     "https://www.hurstbourne.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hurstbourne\",\"government\",\"batch:50\"]",
     10800,
     "Hurstbourne — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_idagroveia_com, "us-b50-idagroveia-com",
     "Ida Grove", "Ida Grove",
     "us_local", "government",
     "https://idagroveia.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Ida Grove\",\"government\",\"batch:50\"]",
     10800,
     "Ida Grove — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 20 entries at probe")

RSSX(us_b50_idaloutx_com, "us-b50-idaloutx-com",
     "Idalou", "Idalou",
     "us_local", "government",
     "https://idaloutx.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Idalou\",\"government\",\"batch:50\"]",
     10800,
     "Idalou — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_indian_rocks_beach_com, "us-b50-indian-rocks-beach-com",
     "Indian Rocks Beach", "Indian Rocks Beach",
     "us_local", "government",
     "https://www.indian-rocks-beach.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Indian Rocks Beach\",\"government\",\"batch:50\"]",
     10800,
     "Indian Rocks Beach — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_inkomcity_org, "us-b50-inkomcity-org",
     "Inkom", "Inkom",
     "us_local", "government",
     "https://inkomcity.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Inkom\",\"government\",\"batch:50\"]",
     10800,
     "Inkom — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_irontonohio_org, "us-b50-irontonohio-org",
     "Ironton", "Ironton",
     "us_local", "government",
     "https://irontonohio.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Ironton\",\"government\",\"batch:50\"]",
     10800,
     "Ironton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_ishpemingcity_org, "us-b50-ishpemingcity-org",
     "Ishpeming", "Ishpeming",
     "us_local", "government",
     "https://ishpemingcity.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Ishpeming\",\"government\",\"batch:50\"]",
     10800,
     "Ishpeming — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_jasperflonline_com, "us-b50-jasperflonline-com",
     "Jasper", "Jasper",
     "us_local", "government",
     "https://www.jasperflonline.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Jasper\",\"government\",\"batch:50\"]",
     10800,
     "Jasper — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_jeffersoniowa_com, "us-b50-jeffersoniowa-com",
     "Jefferson", "Jefferson",
     "us_local", "government",
     "http://jeffersoniowa.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Jefferson\",\"government\",\"batch:50\"]",
     10800,
     "Jefferson — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_jeffersontexas_us, "us-b50-jeffersontexas-us",
     "Jefferson", "Jefferson",
     "us_local", "government",
     "https://www.jeffersontexas.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Jefferson\",\"government\",\"batch:50\"]",
     10800,
     "Jefferson — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_jetmorekansas_com, "us-b50-jetmorekansas-com",
     "Jetmore", "Jetmore",
     "us_local", "government",
     "http://www.jetmorekansas.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Jetmore\",\"government\",\"batch:50\"]",
     10800,
     "Jetmore — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_jonesboroindiana_net, "us-b50-jonesboroindiana-net",
     "Jonesboro", "Jonesboro",
     "us_local", "government",
     "https://www.jonesboroindiana.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Jonesboro\",\"government\",\"batch:50\"]",
     10800,
     "Jonesboro — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_kanopoliskansas_com, "us-b50-kanopoliskansas-com",
     "Kanopolis", "Kanopolis",
     "us_local", "government",
     "https://kanopoliskansas.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Kanopolis\",\"government\",\"batch:50\"]",
     10800,
     "Kanopolis — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_kemmerer_org, "us-b50-kemmerer-org",
     "Kemmerer", "Kemmerer",
     "us_local", "government",
     "https://www.kemmerer.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Kemmerer\",\"government\",\"batch:50\"]",
     10800,
     "Kemmerer — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_kensingtonmn_com, "us-b50-kensingtonmn-com",
     "Kensington", "Kensington",
     "us_local", "government",
     "https://kensingtonmn.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Kensington\",\"government\",\"batch:50\"]",
     10800,
     "Kensington — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 8 entries at probe")

RSSX(us_b50_keycolonybeach_net, "us-b50-keycolonybeach-net",
     "Key Colony Beach", "Key Colony Beach",
     "us_local", "government",
     "https://keycolonybeach.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Key Colony Beach\",\"government\",\"batch:50\"]",
     10800,
     "Key Colony Beach — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_kingfisher_org, "us-b50-kingfisher-org",
     "Kingfisher", "Kingfisher",
     "us_local", "government",
     "https://www.kingfisher.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Kingfisher\",\"government\",\"batch:50\"]",
     10800,
     "Kingfisher — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_kingstree_org, "us-b50-kingstree-org",
     "Kingstree — Town of Kingstree - Official Town Site - Kingstree, SC » Fee", "Kingstree — Town of Kingstree - Official Town Site - Kingstree, SC » Fee",
     "us_local", "government",
     "https://kingstree.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Kingstree — Town of Kingstree - Official Town Site - Kingstree, SC » Fee\",\"government\",\"batch:50\"]",
     10800,
     "Kingstree — Town of Kingstree - Official Town Site - Kingstree, SC » Fee — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_kirbytx_org, "us-b50-kirbytx-org",
     "Kirby", "Kirby",
     "us_local", "government",
     "https://www.kirbytx.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Kirby\",\"government\",\"batch:50\"]",
     10800,
     "Kirby — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 8 entries at probe")

RSSX(us_b50_kirtlandohio_com, "us-b50-kirtlandohio-com",
     "Kirtland", "Kirtland",
     "us_local", "government",
     "https://kirtlandohio.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Kirtland\",\"government\",\"batch:50\"]",
     10800,
     "Kirtland — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_kooskia_com, "us-b50-kooskia-com",
     "Kooskia", "Kooskia",
     "us_local", "government",
     "https://www.kooskia.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Kooskia\",\"government\",\"batch:50\"]",
     10800,
     "Kooskia — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_kville_org, "us-b50-kville-org",
     "Knoxville", "Knoxville",
     "us_local", "government",
     "https://kville.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Knoxville\",\"government\",\"batch:50\"]",
     10800,
     "Knoxville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_laconcity_com, "us-b50-laconcity-com",
     "Lacon", "Lacon",
     "us_local", "government",
     "https://laconcity.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lacon\",\"government\",\"batch:50\"]",
     10800,
     "Lacon — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_laingsburg_us, "us-b50-laingsburg-us",
     "Laingsburg", "Laingsburg",
     "us_local", "government",
     "https://www.laingsburg.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Laingsburg\",\"government\",\"batch:50\"]",
     10800,
     "Laingsburg — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_lakeview_ia_com, "us-b50-lakeview-ia-com",
     "Lake View", "Lake View",
     "us_local", "government",
     "http://lakeview-ia.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lake View\",\"government\",\"batch:50\"]",
     10800,
     "Lake View — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_lamourend_com, "us-b50-lamourend-com",
     "LaMoure", "LaMoure",
     "us_local", "government",
     "https://www.lamourend.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:LaMoure\",\"government\",\"batch:50\"]",
     10800,
     "LaMoure — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_langdonplace_com, "us-b50-langdonplace-com",
     "Langdon Place", "Langdon Place",
     "us_local", "government",
     "https://langdonplace.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Langdon Place\",\"government\",\"batch:50\"]",
     10800,
     "Langdon Place — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_lapuente_org, "us-b50-lapuente-org",
     "La Puente", "La Puente",
     "us_local", "government",
     "https://lapuente.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:La Puente\",\"government\",\"batch:50\"]",
     10800,
     "La Puente — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_laurelne_com, "us-b50-laurelne-com",
     "Laurel", "Laurel",
     "us_local", "government",
     "https://laurelne.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Laurel\",\"government\",\"batch:50\"]",
     10800,
     "Laurel — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 8 entries at probe")

RSSX(us_b50_lawrenceburgky_org, "us-b50-lawrenceburgky-org",
     "Lawrenceburg", "Lawrenceburg",
     "us_local", "government",
     "https://lawrenceburgky.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lawrenceburg\",\"government\",\"batch:50\"]",
     10800,
     "Lawrenceburg — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_leadmethere_org, "us-b50-leadmethere-org",
     "Lead", "Lead",
     "us_local", "government",
     "https://www.leadmethere.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lead\",\"government\",\"batch:50\"]",
     10800,
     "Lead — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_leadville_com, "us-b50-leadville-com",
     "Leadville", "Leadville",
     "us_local", "government",
     "https://leadville.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Leadville\",\"government\",\"batch:50\"]",
     10800,
     "Leadville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 12 entries at probe")

RSSX(us_b50_leboks_org, "us-b50-leboks-org",
     "Lebo — Lebo, Kansas", "Lebo — Lebo, Kansas",
     "us_local", "government",
     "https://leboks.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lebo — Lebo, Kansas\",\"government\",\"batch:50\"]",
     10800,
     "Lebo — Lebo, Kansas — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_lecompton_org, "us-b50-lecompton-org",
     "Lecompton", "Lecompton",
     "us_local", "government",
     "https://lecompton.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lecompton\",\"government\",\"batch:50\"]",
     10800,
     "Lecompton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_lewistownillinois_org, "us-b50-lewistownillinois-org",
     "Lewistown", "Lewistown",
     "us_local", "government",
     "https://lewistownillinois.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lewistown\",\"government\",\"batch:50\"]",
     10800,
     "Lewistown — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_lincolntonga_org, "us-b50-lincolntonga-org",
     "Lincolnton", "Lincolnton",
     "us_local", "government",
     "https://lincolntonga.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lincolnton\",\"government\",\"batch:50\"]",
     10800,
     "Lincolnton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_lindenmi_us_rss2, "us-b50-lindenmi-us-rss2",
     "Linden", "Linden",
     "us_local", "government",
     "https://www.lindenmi.us/feed/rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Linden\",\"government\",\"batch:50\"]",
     10800,
     "Linden — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_lintonnd_org_feed_rss2, "us-b50-lintonnd-org-feed-rss2",
     "Linton", "Linton",
     "us_local", "government",
     "https://lintonnd.org/?feed=rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Linton\",\"government\",\"batch:50\"]",
     10800,
     "Linton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_locustnc_com, "us-b50-locustnc-com",
     "Locust", "Locust",
     "us_local", "government",
     "https://locustnc.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Locust\",\"government\",\"batch:50\"]",
     10800,
     "Locust — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_lomitacity_com, "us-b50-lomitacity-com",
     "Lomita", "Lomita",
     "us_local", "government",
     "https://lomitacity.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lomita\",\"government\",\"batch:50\"]",
     10800,
     "Lomita — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_louisvillenebraska_com_index_format_feed_type_atom, "us-b50-louisvillenebraska-com-index-format-feed-type-atom",
     "Louisville — Welcome to Louisville, Nebraska", "Louisville — Welcome to Louisville, Nebraska",
     "us_local", "government",
     "https://www.louisvillenebraska.com/index.php?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Louisville — Welcome to Louisville, Nebraska\",\"government\",\"batch:50\"]",
     10800,
     "Louisville — Welcome to Louisville, Nebraska — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_louisvillenebraska_com_index_format_feed_type_rss, "us-b50-louisvillenebraska-com-index-format-feed-type-rss",
     "Louisville — Welcome to Louisville, Nebraska", "Louisville — Welcome to Louisville, Nebraska",
     "us_local", "government",
     "https://www.louisvillenebraska.com/index.php?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Louisville — Welcome to Louisville, Nebraska\",\"government\",\"batch:50\"]",
     10800,
     "Louisville — Welcome to Louisville, Nebraska — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_loupcity_com, "us-b50-loupcity-com",
     "Loup City — Loup City Field Notes", "Loup City — Loup City Field Notes",
     "us_local", "government",
     "https://loupcity.com/rss.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Loup City — Loup City Field Notes\",\"government\",\"batch:50\"]",
     10800,
     "Loup City — Loup City Field Notes — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_lovington_org, "us-b50-lovington-org",
     "Lovington", "Lovington",
     "us_local", "government",
     "https://www.lovington.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lovington\",\"government\",\"batch:50\"]",
     10800,
     "Lovington — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_lumbertonms_info, "us-b50-lumbertonms-info",
     "Lumberton", "Lumberton",
     "us_local", "government",
     "https://lumbertonms.info/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lumberton\",\"government\",\"batch:50\"]",
     10800,
     "Lumberton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_luverne_org, "us-b50-luverne-org",
     "Luverne", "Luverne",
     "us_local", "government",
     "http://luverne.org/feed/rss/",
     "en", "[\"gov\",\"country:us\",\"publisher:Luverne\",\"government\",\"batch:50\"]",
     10800,
     "Luverne — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_lynchburgtn_com_blog_feed, "us-b50-lynchburgtn-com-blog-feed",
     "Lynchburg — Lynchburg Chamber - RSS", "Lynchburg — Lynchburg Chamber - RSS",
     "us_local", "government",
     "https://www.lynchburgtn.com/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Lynchburg — Lynchburg Chamber - RSS\",\"government\",\"batch:50\"]",
     10800,
     "Lynchburg — Lynchburg Chamber - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_madeliamn_com, "us-b50-madeliamn-com",
     "Madelia", "Madelia",
     "us_local", "government",
     "https://www.madeliamn.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Madelia\",\"government\",\"batch:50\"]",
     10800,
     "Madelia — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_main_d1o7e556q4duxw_amplifyapp_com_blog, "us-b50-main-d1o7e556q4duxw-amplifyapp-com-blog",
     "Clay", "Clay",
     "us_local", "government",
     "https://main.d1o7e556q4duxw.amplifyapp.com/blog/rss.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Clay\",\"government\",\"batch:50\"]",
     10800,
     "Clay — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 14 entries at probe")

RSSX(us_b50_mayvilleportland_com, "us-b50-mayvilleportland-com",
     "Mayville", "Mayville",
     "us_local", "government",
     "https://www.mayvilleportland.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Mayville\",\"government\",\"batch:50\"]",
     10800,
     "Mayville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_mcphersonchamber_org, "us-b50-mcphersonchamber-org",
     "McPherson", "McPherson",
     "us_local", "government",
     "https://www.mcphersonchamber.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:McPherson\",\"government\",\"batch:50\"]",
     10800,
     "McPherson — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_medical_lake_org, "us-b50-medical-lake-org",
     "Medical Lake", "Medical Lake",
     "us_local", "government",
     "https://medical-lake.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Medical Lake\",\"government\",\"batch:50\"]",
     10800,
     "Medical Lake — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_medora_com, "us-b50-medora-com",
     "Medora", "Medora",
     "us_local", "government",
     "https://medora.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Medora\",\"government\",\"batch:50\"]",
     10800,
     "Medora — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_melcherdallasiowa_com, "us-b50-melcherdallasiowa-com",
     "Melcher-Dallas — melcherdallasiowa.com RSS", "Melcher-Dallas — melcherdallasiowa.com RSS",
     "us_local", "government",
     "https://melcherdallasiowa.com/feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Melcher-Dallas — melcherdallasiowa.com RSS\",\"government\",\"batch:50\"]",
     10800,
     "Melcher-Dallas — melcherdallasiowa.com RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_melvernks_com, "us-b50-melvernks-com",
     "Melvern", "Melvern",
     "us_local", "government",
     "https://melvernks.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Melvern\",\"government\",\"batch:50\"]",
     10800,
     "Melvern — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_menahga_com_rss2, "us-b50-menahga-com-rss2",
     "Menahga", "Menahga",
     "us_local", "government",
     "https://www.menahga.com/feed/rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Menahga\",\"government\",\"batch:50\"]",
     10800,
     "Menahga — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 11 entries at probe")

RSSX(us_b50_mendoncity_org, "us-b50-mendoncity-org",
     "Mendon", "Mendon",
     "us_local", "government",
     "https://mendoncity.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Mendon\",\"government\",\"batch:50\"]",
     10800,
     "Mendon — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_middleburgheights_com, "us-b50-middleburgheights-com",
     "Middleburg Heights", "Middleburg Heights",
     "us_local", "government",
     "https://middleburgheights.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Middleburg Heights\",\"government\",\"batch:50\"]",
     10800,
     "Middleburg Heights — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_milescitychamber_com, "us-b50-milescitychamber-com",
     "Miles City", "Miles City",
     "us_local", "government",
     "https://milescitychamber.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Miles City\",\"government\",\"batch:50\"]",
     10800,
     "Miles City — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_milford_ia_us, "us-b50-milford-ia-us",
     "Milford", "Milford",
     "us_local", "government",
     "https://milford.ia.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Milford\",\"government\",\"batch:50\"]",
     10800,
     "Milford — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_millcreekut_org, "us-b50-millcreekut-org",
     "Millcreek", "Millcreek",
     "us_local", "government",
     "https://millcreekut.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Millcreek\",\"government\",\"batch:50\"]",
     10800,
     "Millcreek — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_milledgevillega_us, "us-b50-milledgevillega-us",
     "Milledgeville", "Milledgeville",
     "us_local", "government",
     "https://www.milledgevillega.us/index.php/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Milledgeville\",\"government\",\"batch:50\"]",
     10800,
     "Milledgeville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_millersd_org, "us-b50-millersd-org",
     "Miller", "Miller",
     "us_local", "government",
     "https://millersd.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Miller\",\"government\",\"batch:50\"]",
     10800,
     "Miller — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_mishawaka_in_gov, "us-b50-mishawaka-in-gov",
     "Mishawaka", "Mishawaka",
     "us_local", "government",
     "https://mishawaka.in.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Mishawaka\",\"government\",\"batch:50\"]",
     10800,
     "Mishawaka — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_missionks_org, "us-b50-missionks-org",
     "Mission", "Mission",
     "us_local", "government",
     "https://www.missionks.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Mission\",\"government\",\"batch:50\"]",
     10800,
     "Mission — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_mitchellville_org, "us-b50-mitchellville-org",
     "Mitchellville", "Mitchellville",
     "us_local", "government",
     "https://www.mitchellville.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Mitchellville\",\"government\",\"batch:50\"]",
     10800,
     "Mitchellville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_monroela_us, "us-b50-monroela-us",
     "Monroe", "Monroe",
     "us_local", "government",
     "https://monroela.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Monroe\",\"government\",\"batch:50\"]",
     10800,
     "Monroe — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_montpelier_id_gov, "us-b50-montpelier-id-gov",
     "Montpelier", "Montpelier",
     "us_local", "government",
     "https://montpelier.id.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Montpelier\",\"government\",\"batch:50\"]",
     10800,
     "Montpelier — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_moorehaven_org, "us-b50-moorehaven-org",
     "Moore Haven", "Moore Haven",
     "us_local", "government",
     "https://moorehaven.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Moore Haven\",\"government\",\"batch:50\"]",
     10800,
     "Moore Haven — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_moraviaiowa_com, "us-b50-moraviaiowa-com",
     "Moravia", "Moravia",
     "us_local", "government",
     "https://moraviaiowa.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Moravia\",\"government\",\"batch:50\"]",
     10800,
     "Moravia — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_moundcity_org, "us-b50-moundcity-org",
     "Mound City", "Mound City",
     "us_local", "government",
     "https://www.moundcity.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Mound City\",\"government\",\"batch:50\"]",
     10800,
     "Mound City — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_mpcity_net, "us-b50-mpcity-net",
     "Mount Pleasant", "Mount Pleasant",
     "us_local", "government",
     "https://mpcity.net/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Mount Pleasant\",\"government\",\"batch:50\"]",
     10800,
     "Mount Pleasant — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_mundaytexas_com, "us-b50-mundaytexas-com",
     "Munday", "Munday",
     "us_local", "government",
     "https://www.mundaytexas.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Munday\",\"government\",\"batch:50\"]",
     10800,
     "Munday — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_muskegonheights_us, "us-b50-muskegonheights-us",
     "Muskegon Heights", "Muskegon Heights",
     "us_local", "government",
     "https://muskegonheights.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Muskegon Heights\",\"government\",\"batch:50\"]",
     10800,
     "Muskegon Heights — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_mycarrabelle_com, "us-b50-mycarrabelle-com",
     "Carrabelle", "Carrabelle",
     "us_local", "government",
     "https://www.mycarrabelle.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Carrabelle\",\"government\",\"batch:50\"]",
     10800,
     "Carrabelle — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_mygulfport_us, "us-b50-mygulfport-us",
     "Gulfport", "Gulfport",
     "us_local", "government",
     "https://mygulfport.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Gulfport\",\"government\",\"batch:50\"]",
     10800,
     "Gulfport — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_mypigeonforge_com, "us-b50-mypigeonforge-com",
     "Pigeon Forge", "Pigeon Forge",
     "us_local", "government",
     "https://www.mypigeonforge.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Pigeon Forge\",\"government\",\"batch:50\"]",
     10800,
     "Pigeon Forge — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_myslidell_com, "us-b50-myslidell-com",
     "Slidell", "Slidell",
     "us_local", "government",
     "https://myslidell.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Slidell\",\"government\",\"batch:50\"]",
     10800,
     "Slidell — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_neodesha_org, "us-b50-neodesha-org",
     "Neodesha", "Neodesha",
     "us_local", "government",
     "http://neodesha.org/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Neodesha\",\"government\",\"batch:50\"]",
     10800,
     "Neodesha — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_neoga_org, "us-b50-neoga-org",
     "Neoga", "Neoga",
     "us_local", "government",
     "https://neoga.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Neoga\",\"government\",\"batch:50\"]",
     10800,
     "Neoga — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_neopolitics_org, "us-b50-neopolitics-org",
     "Belle Plaine", "Belle Plaine",
     "us_local", "government",
     "https://www.neopolitics.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Belle Plaine\",\"government\",\"batch:50\"]",
     10800,
     "Belle Plaine — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_newbostontx_org_new_boston_chamber_of_commerce, "us-b50-newbostontx-org-new-boston-chamber-of-commerce",
     "New Boston", "New Boston",
     "us_local", "government",
     "https://www.newbostontx.org/new-boston-chamber-of-commerce.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:New Boston\",\"government\",\"batch:50\"]",
     10800,
     "New Boston — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_newcastleky_com_rss2, "us-b50-newcastleky-com-rss2",
     "New Castle", "New Castle",
     "us_local", "government",
     "https://www.newcastleky.com/feed/rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:New Castle\",\"government\",\"batch:50\"]",
     10800,
     "New Castle — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_newport_wa_org, "us-b50-newport-wa-org",
     "Newport", "Newport",
     "us_local", "government",
     "https://newport-wa.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Newport\",\"government\",\"batch:50\"]",
     10800,
     "Newport — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_newportvermont_org, "us-b50-newportvermont-org",
     "Newport", "Newport",
     "us_local", "government",
     "https://www.newportvermont.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Newport\",\"government\",\"batch:50\"]",
     10800,
     "Newport — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_newstrawn_org, "us-b50-newstrawn-org",
     "New Strawn", "New Strawn",
     "us_local", "government",
     "https://newstrawn.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:New Strawn\",\"government\",\"batch:50\"]",
     10800,
     "New Strawn — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_nicholasville_org, "us-b50-nicholasville-org",
     "Nicholasville", "Nicholasville",
     "us_local", "government",
     "https://nicholasville.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Nicholasville\",\"government\",\"batch:50\"]",
     10800,
     "Nicholasville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_nicholson_ga_com_feed_rss2, "us-b50-nicholson-ga-com-feed-rss2",
     "Nicholson", "Nicholson",
     "us_local", "government",
     "https://nicholson-ga.com/?feed=rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Nicholson\",\"government\",\"batch:50\"]",
     10800,
     "Nicholson — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_oakdalegov_com_blog_feed, "us-b50-oakdalegov-com-blog-feed",
     "Oakdale — city-of-oakdale-ca - RSS", "Oakdale — city-of-oakdale-ca - RSS",
     "us_local", "government",
     "https://www.oakdalegov.com/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Oakdale — city-of-oakdale-ca - RSS\",\"government\",\"batch:50\"]",
     10800,
     "Oakdale — city-of-oakdale-ca - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_oaklandparkfl_org, "us-b50-oaklandparkfl-org",
     "Oakland Park", "Oakland Park",
     "us_local", "government",
     "https://www.oaklandparkfl.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Oakland Park\",\"government\",\"batch:50\"]",
     10800,
     "Oakland Park — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_okemahok_gov, "us-b50-okemahok-gov",
     "Okemah", "Okemah",
     "us_local", "government",
     "https://okemahok.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Okemah\",\"government\",\"batch:50\"]",
     10800,
     "Okemah — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_onamia_com_q_taxonomy_term_4, "us-b50-onamia-com-q-taxonomy-term-4",
     "Onamia — RSS - Onamia MN", "Onamia — RSS - Onamia MN",
     "us_local", "government",
     "http://www.onamia.com/?q=taxonomy/term/4/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Onamia — RSS - Onamia MN\",\"government\",\"batch:50\"]",
     10800,
     "Onamia — RSS - Onamia MN — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_orangecityiowa_com, "us-b50-orangecityiowa-com",
     "Orange City", "Orange City",
     "us_local", "government",
     "https://orangecityiowa.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Orange City\",\"government\",\"batch:50\"]",
     10800,
     "Orange City — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_oroville_wa_com, "us-b50-oroville-wa-com",
     "Oroville", "Oroville",
     "us_local", "government",
     "https://oroville-wa.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Oroville\",\"government\",\"batch:50\"]",
     10800,
     "Oroville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_oswegokansas_com_index_format_feed_type_atom, "us-b50-oswegokansas-com-index-format-feed-type-atom",
     "Oswego — Home", "Oswego — Home",
     "us_local", "government",
     "https://www.oswegokansas.com/index.php?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Oswego — Home\",\"government\",\"batch:50\"]",
     10800,
     "Oswego — Home — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_oswegokansas_com_index_format_feed_type_rss, "us-b50-oswegokansas-com-index-format-feed-type-rss",
     "Oswego — Home", "Oswego — Home",
     "us_local", "government",
     "https://www.oswegokansas.com/index.php?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Oswego — Home\",\"government\",\"batch:50\"]",
     10800,
     "Oswego — Home — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_ozawkie_org, "us-b50-ozawkie-org",
     "Ozawkie", "Ozawkie",
     "us_local", "government",
     "https://ozawkie.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Ozawkie\",\"government\",\"batch:50\"]",
     10800,
     "Ozawkie — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_pagopago_com, "us-b50-pagopago-com",
     "Pago Pago", "Pago Pago",
     "us_local", "government",
     "https://pagopago.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Pago Pago\",\"government\",\"batch:50\"]",
     10800,
     "Pago Pago — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 9 entries at probe")

RSSX(us_b50_parchment_org, "us-b50-parchment-org",
     "Parchment", "Parchment",
     "us_local", "government",
     "https://parchment.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Parchment\",\"government\",\"batch:50\"]",
     10800,
     "Parchment — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_parisillinois_org, "us-b50-parisillinois-org",
     "Paris", "Paris",
     "us_local", "government",
     "https://www.parisillinois.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Paris\",\"government\",\"batch:50\"]",
     10800,
     "Paris — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_parkersprairie_net, "us-b50-parkersprairie-net",
     "Parkers Prairie", "Parkers Prairie",
     "us_local", "government",
     "https://www.parkersprairie.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Parkers Prairie\",\"government\",\"batch:50\"]",
     10800,
     "Parkers Prairie — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_perry_mi_us, "us-b50-perry-mi-us",
     "Perry", "Perry",
     "us_local", "government",
     "https://perry.mi.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Perry\",\"government\",\"batch:50\"]",
     10800,
     "Perry — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_pilotmound_org, "us-b50-pilotmound-org",
     "Pilot Mound", "Pilot Mound",
     "us_local", "government",
     "https://pilotmound.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Pilot Mound\",\"government\",\"batch:50\"]",
     10800,
     "Pilot Mound — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_plattsburgmo_com, "us-b50-plattsburgmo-com",
     "Plattsburg — Life and tourism in Plattsburg, Kansas City, Missouri and mo", "Plattsburg — Life and tourism in Plattsburg, Kansas City, Missouri and mo",
     "us_local", "government",
     "http://plattsburgmo.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Plattsburg — Life and tourism in Plattsburg, Kansas City, Missouri and mo\",\"government\",\"batch:50\"]",
     10800,
     "Plattsburg — Life and tourism in Plattsburg, Kansas City, Missouri and mo — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_plattsmouth_org_index_format_feed_type_atom, "us-b50-plattsmouth-org-index-format-feed-type-atom",
     "Plattsmouth — City of Plattsmouth, Nebraska", "Plattsmouth — City of Plattsmouth, Nebraska",
     "us_local", "government",
     "https://www.plattsmouth.org/index.php?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Plattsmouth — City of Plattsmouth, Nebraska\",\"government\",\"batch:50\"]",
     10800,
     "Plattsmouth — City of Plattsmouth, Nebraska — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_plattsmouth_org_index_format_feed_type_rss, "us-b50-plattsmouth-org-index-format-feed-type-rss",
     "Plattsmouth — City of Plattsmouth, Nebraska", "Plattsmouth — City of Plattsmouth, Nebraska",
     "us_local", "government",
     "https://www.plattsmouth.org/index.php?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Plattsmouth — City of Plattsmouth, Nebraska\",\"government\",\"batch:50\"]",
     10800,
     "Plattsmouth — City of Plattsmouth, Nebraska — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_pointarena_ca_gov, "us-b50-pointarena-ca-gov",
     "Point Arena", "Point Arena",
     "us_local", "government",
     "https://pointarena.ca.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Point Arena\",\"government\",\"batch:50\"]",
     10800,
     "Point Arena — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_pottervillemi_org, "us-b50-pottervillemi-org",
     "Potterville", "Potterville",
     "us_local", "government",
     "https://pottervillemi.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Potterville\",\"government\",\"batch:50\"]",
     10800,
     "Potterville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_presidiotx_us_feed_rss2, "us-b50-presidiotx-us-feed-rss2",
     "Presidio", "Presidio",
     "us_local", "government",
     "https://presidiotx.us/?feed=rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Presidio\",\"government\",\"batch:50\"]",
     10800,
     "Presidio — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_prestonsburgky_org, "us-b50-prestonsburgky-org",
     "Prestonsburg", "Prestonsburg",
     "us_local", "government",
     "https://prestonsburgky.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Prestonsburg\",\"government\",\"batch:50\"]",
     10800,
     "Prestonsburg — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_ramonaks_com_index_format_feed_type_atom, "us-b50-ramonaks-com-index-format-feed-type-atom",
     "Ramona — Atom 1.0", "Ramona — Atom 1.0",
     "us_local", "government",
     "https://www.RamonaKS.com/index.php?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Ramona — Atom 1.0\",\"government\",\"batch:50\"]",
     10800,
     "Ramona — Atom 1.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_ramonaks_com_index_format_feed_type_rss, "us-b50-ramonaks-com-index-format-feed-type-rss",
     "Ramona", "Ramona",
     "us_local", "government",
     "https://www.RamonaKS.com/index.php?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Ramona\",\"government\",\"batch:50\"]",
     10800,
     "Ramona — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_randolphks_com, "us-b50-randolphks-com",
     "Randolph", "Randolph",
     "us_local", "government",
     "https://randolphks.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Randolph\",\"government\",\"batch:50\"]",
     10800,
     "Randolph — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_raymondms_com, "us-b50-raymondms-com",
     "Raymond", "Raymond",
     "us_local", "government",
     "https://raymondms.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Raymond\",\"government\",\"batch:50\"]",
     10800,
     "Raymond — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_raymondvilletx_us, "us-b50-raymondvilletx-us",
     "Raymondville", "Raymondville",
     "us_local", "government",
     "https://raymondvilletx.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Raymondville\",\"government\",\"batch:50\"]",
     10800,
     "Raymondville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_redoakiowa_com, "us-b50-redoakiowa-com",
     "Red Oak", "Red Oak",
     "us_local", "government",
     "https://www.redoakiowa.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Red Oak\",\"government\",\"batch:50\"]",
     10800,
     "Red Oak — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_reedley_ca_gov, "us-b50-reedley-ca-gov",
     "Reedley", "Reedley",
     "us_local", "government",
     "https://reedley.ca.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Reedley\",\"government\",\"batch:50\"]",
     10800,
     "Reedley — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_richlandmo_info, "us-b50-richlandmo-info",
     "Richland", "Richland",
     "us_local", "government",
     "http://richlandmo.info/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Richland\",\"government\",\"batch:50\"]",
     10800,
     "Richland — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_ridgelandms_org, "us-b50-ridgelandms-org",
     "Ridgeland", "Ridgeland",
     "us_local", "government",
     "https://www.ridgelandms.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Ridgeland\",\"government\",\"batch:50\"]",
     10800,
     "Ridgeland — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_riodellcity_com, "us-b50-riodellcity-com",
     "Rio Dell", "Rio Dell",
     "us_local", "government",
     "https://www.riodellcity.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Rio Dell\",\"government\",\"batch:50\"]",
     10800,
     "Rio Dell — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_riverheights_gov, "us-b50-riverheights-gov",
     "River Heights", "River Heights",
     "us_local", "government",
     "https://riverheights.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:River Heights\",\"government\",\"batch:50\"]",
     10800,
     "River Heights — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_roanokealabama_org, "us-b50-roanokealabama-org",
     "Roanoke", "Roanoke",
     "us_local", "government",
     "https://roanokealabama.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Roanoke\",\"government\",\"batch:50\"]",
     10800,
     "Roanoke — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_roanokerapidsnc_gov_format_feed_type_atom, "us-b50-roanokerapidsnc-gov-format-feed-type-atom",
     "Roanoke Rapids — City of Roanoke Rapids, NC Official Website", "Roanoke Rapids — City of Roanoke Rapids, NC Official Website",
     "us_local", "government",
     "https://roanokerapidsnc.gov/?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Roanoke Rapids — City of Roanoke Rapids, NC Official Website\",\"government\",\"batch:50\"]",
     10800,
     "Roanoke Rapids — City of Roanoke Rapids, NC Official Website — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_roanokerapidsnc_gov_format_feed_type_rss, "us-b50-roanokerapidsnc-gov-format-feed-type-rss",
     "Roanoke Rapids — City of Roanoke Rapids, NC Official Website", "Roanoke Rapids — City of Roanoke Rapids, NC Official Website",
     "us_local", "government",
     "https://roanokerapidsnc.gov/?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Roanoke Rapids — City of Roanoke Rapids, NC Official Website\",\"government\",\"batch:50\"]",
     10800,
     "Roanoke Rapids — City of Roanoke Rapids, NC Official Website — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_robinsonks_wordpress_com, "us-b50-robinsonks-wordpress-com",
     "Robinson", "Robinson",
     "us_local", "government",
     "https://robinsonks.wordpress.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Robinson\",\"government\",\"batch:50\"]",
     10800,
     "Robinson — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_rockrapids_com, "us-b50-rockrapids-com",
     "Rock Rapids", "Rock Rapids",
     "us_local", "government",
     "https://rockrapids.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Rock Rapids\",\"government\",\"batch:50\"]",
     10800,
     "Rock Rapids — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_rockyfordcolo_com, "us-b50-rockyfordcolo-com",
     "Rocky Ford", "Rocky Ford",
     "us_local", "government",
     "https://www.rockyfordcolo.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Rocky Ford\",\"government\",\"batch:50\"]",
     10800,
     "Rocky Ford — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_sansabatexas_com, "us-b50-sansabatexas-com",
     "San Saba", "San Saba",
     "us_local", "government",
     "https://sansabatexas.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:San Saba\",\"government\",\"batch:50\"]",
     10800,
     "San Saba — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_sedalia_com, "us-b50-sedalia-com",
     "Sedalia", "Sedalia",
     "us_local", "government",
     "https://www.sedalia.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Sedalia\",\"government\",\"batch:50\"]",
     10800,
     "Sedalia — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_seneca_kansas_us, "us-b50-seneca-kansas-us",
     "Seneca", "Seneca",
     "us_local", "government",
     "https://www.seneca-kansas.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Seneca\",\"government\",\"batch:50\"]",
     10800,
     "Seneca — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_seymourin_org_index_format_feed_type_atom, "us-b50-seymourin-org-index-format-feed-type-atom",
     "Seymour — Atom 1.0", "Seymour — Atom 1.0",
     "us_local", "government",
     "https://www.seymourin.org/index.php?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Seymour — Atom 1.0\",\"government\",\"batch:50\"]",
     10800,
     "Seymour — Atom 1.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_seymourin_org_index_format_feed_type_rss, "us-b50-seymourin-org-index-format-feed-type-rss",
     "Seymour", "Seymour",
     "us_local", "government",
     "https://www.seymourin.org/index.php?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Seymour\",\"government\",\"batch:50\"]",
     10800,
     "Seymour — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_shelbycity_oh_gov, "us-b50-shelbycity-oh-gov",
     "Shelby", "Shelby",
     "us_local", "government",
     "https://shelbycity.oh.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Shelby\",\"government\",\"batch:50\"]",
     10800,
     "Shelby — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_shelbyvilleillinois_net, "us-b50-shelbyvilleillinois-net",
     "Shelbyville", "Shelbyville",
     "us_local", "government",
     "https://shelbyvilleillinois.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Shelbyville\",\"government\",\"batch:50\"]",
     10800,
     "Shelbyville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_smithvilletn_com, "us-b50-smithvilletn-com",
     "Smithville", "Smithville",
     "us_local", "government",
     "https://www.smithvilletn.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Smithville\",\"government\",\"batch:50\"]",
     10800,
     "Smithville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_spenceriowacity_com, "us-b50-spenceriowacity-com",
     "Spencer", "Spencer",
     "us_local", "government",
     "https://spenceriowacity.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Spencer\",\"government\",\"batch:50\"]",
     10800,
     "Spencer — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_springfieldmn_org, "us-b50-springfieldmn-org",
     "Springfield", "Springfield",
     "us_local", "government",
     "https://springfieldmn.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Springfield\",\"government\",\"batch:50\"]",
     10800,
     "Springfield — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 12 entries at probe")

RSSX(us_b50_springfieldnebraska_com_index_format_feed_type_atom, "us-b50-springfieldnebraska-com-index-format-feed-type-atom",
     "Springfield — Springfield, Nebraska Official Chartered Website of the City", "Springfield — Springfield, Nebraska Official Chartered Website of the City",
     "us_local", "government",
     "https://www.springfieldnebraska.com/index.php?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Springfield — Springfield, Nebraska Official Chartered Website of the City\",\"government\",\"batch:50\"]",
     10800,
     "Springfield — Springfield, Nebraska Official Chartered Website of the City — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_springfieldnebraska_com_index_format_feed_type_rss, "us-b50-springfieldnebraska-com-index-format-feed-type-rss",
     "Springfield — Springfield, Nebraska Official Chartered Website of the City", "Springfield — Springfield, Nebraska Official Chartered Website of the City",
     "us_local", "government",
     "https://www.springfieldnebraska.com/index.php?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Springfield — Springfield, Nebraska Official Chartered Website of the City\",\"government\",\"batch:50\"]",
     10800,
     "Springfield — Springfield, Nebraska Official Chartered Website of the City — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_stalbanswv_com, "us-b50-stalbanswv-com",
     "St. Albans", "St. Albans",
     "us_local", "government",
     "https://stalbanswv.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:St. Albans\",\"government\",\"batch:50\"]",
     10800,
     "St. Albans — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_starbuckmn_org, "us-b50-starbuckmn-org",
     "Starbuck", "Starbuck",
     "us_local", "government",
     "https://starbuckmn.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Starbuck\",\"government\",\"batch:50\"]",
     10800,
     "Starbuck — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_steelville_com, "us-b50-steelville-com",
     "Steelville", "Steelville",
     "us_local", "government",
     "https://steelville.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Steelville\",\"government\",\"batch:50\"]",
     10800,
     "Steelville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_stfranciskansas_com, "us-b50-stfranciskansas-com",
     "St. Francis", "St. Francis",
     "us_local", "government",
     "https://stfranciskansas.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:St. Francis\",\"government\",\"batch:50\"]",
     10800,
     "St. Francis — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_stignace_com, "us-b50-stignace-com",
     "St. Ignace", "St. Ignace",
     "us_local", "government",
     "https://stignace.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:St. Ignace\",\"government\",\"batch:50\"]",
     10800,
     "St. Ignace — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_stromsburgnebraska_com, "us-b50-stromsburgnebraska-com",
     "Stromsburg — City of Stromsburg Nebraska RSS Feed", "Stromsburg — City of Stromsburg Nebraska RSS Feed",
     "us_local", "government",
     "https://stromsburgnebraska.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Stromsburg — City of Stromsburg Nebraska RSS Feed\",\"government\",\"batch:50\"]",
     10800,
     "Stromsburg — City of Stromsburg Nebraska RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_stuartia_com, "us-b50-stuartia-com",
     "Stuart", "Stuart",
     "us_local", "government",
     "https://stuartia.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Stuart\",\"government\",\"batch:50\"]",
     10800,
     "Stuart — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_summerset_us, "us-b50-summerset-us",
     "Summerset", "Summerset",
     "us_local", "government",
     "https://summerset.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Summerset\",\"government\",\"batch:50\"]",
     10800,
     "Summerset — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_terrell_hills_com_format_feed_type_atom, "us-b50-terrell-hills-com-format-feed-type-atom",
     "Terrell Hills — City of Terrell Hills - Home", "Terrell Hills — City of Terrell Hills - Home",
     "us_local", "government",
     "https://www.terrell-hills.com/?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Terrell Hills — City of Terrell Hills - Home\",\"government\",\"batch:50\"]",
     10800,
     "Terrell Hills — City of Terrell Hills - Home — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_terrell_hills_com_format_feed_type_rss, "us-b50-terrell-hills-com-format-feed-type-rss",
     "Terrell Hills — City of Terrell Hills - Home", "Terrell Hills — City of Terrell Hills - Home",
     "us_local", "government",
     "https://www.terrell-hills.com/?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Terrell Hills — City of Terrell Hills - Home\",\"government\",\"batch:50\"]",
     10800,
     "Terrell Hills — City of Terrell Hills - Home — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 3 entries at probe")

RSSX(us_b50_thayerks_com, "us-b50-thayerks-com",
     "Thayer", "Thayer",
     "us_local", "government",
     "https://www.thayerks.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Thayer\",\"government\",\"batch:50\"]",
     10800,
     "Thayer — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_thecityofmcgehee_com, "us-b50-thecityofmcgehee-com",
     "McGehee", "McGehee",
     "us_local", "government",
     "https://www.thecityofmcgehee.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:McGehee\",\"government\",\"batch:50\"]",
     10800,
     "McGehee — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_thecityofwagoner_org, "us-b50-thecityofwagoner-org",
     "Wagoner", "Wagoner",
     "us_local", "government",
     "https://thecityofwagoner.org/index.php/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Wagoner\",\"government\",\"batch:50\"]",
     10800,
     "Wagoner — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_tontitown_com, "us-b50-tontitown-com",
     "Tontitown", "Tontitown",
     "us_local", "government",
     "https://www.tontitown.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Tontitown\",\"government\",\"batch:50\"]",
     10800,
     "Tontitown — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_townofdanbury_org, "us-b50-townofdanbury-org",
     "Danbury", "Danbury",
     "us_local", "government",
     "https://townofdanbury.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Danbury\",\"government\",\"batch:50\"]",
     10800,
     "Danbury — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_townsendmt_com, "us-b50-townsendmt-com",
     "Townsend", "Townsend",
     "us_local", "government",
     "https://townsendmt.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Townsend\",\"government\",\"batch:50\"]",
     10800,
     "Townsend — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_travelersrestsc_com, "us-b50-travelersrestsc-com",
     "Travelers Rest", "Travelers Rest",
     "us_local", "government",
     "https://travelersrestsc.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Travelers Rest\",\"government\",\"batch:50\"]",
     10800,
     "Travelers Rest — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_trotwood_org, "us-b50-trotwood-org",
     "Trotwood", "Trotwood",
     "us_local", "government",
     "https://trotwood.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Trotwood\",\"government\",\"batch:50\"]",
     10800,
     "Trotwood — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_troykansas_com, "us-b50-troykansas-com",
     "Troy", "Troy",
     "us_local", "government",
     "https://troykansas.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Troy\",\"government\",\"batch:50\"]",
     10800,
     "Troy — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_underwoodia_com, "us-b50-underwoodia-com",
     "Underwood", "Underwood",
     "us_local", "government",
     "https://underwoodia.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Underwood\",\"government\",\"batch:50\"]",
     10800,
     "Underwood — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_utahlinks_org, "us-b50-utahlinks-org",
     "Hyde Park", "Hyde Park",
     "us_local", "government",
     "https://utahlinks.org/index.php/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hyde Park\",\"government\",\"batch:50\"]",
     10800,
     "Hyde Park — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_valleycity_us, "us-b50-valleycity-us",
     "Valley City", "Valley City",
     "us_local", "government",
     "http://www.valleycity.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Valley City\",\"government\",\"batch:50\"]",
     10800,
     "Valley City — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_victorcolorado_com, "us-b50-victorcolorado-com",
     "Victor", "Victor",
     "us_local", "government",
     "https://victorcolorado.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Victor\",\"government\",\"batch:50\"]",
     10800,
     "Victor — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_villahillsky_org, "us-b50-villahillsky-org",
     "Villa Hills", "Villa Hills",
     "us_local", "government",
     "https://villahillsky.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Villa Hills\",\"government\",\"batch:50\"]",
     10800,
     "Villa Hills — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_virginiaillinois_net, "us-b50-virginiaillinois-net",
     "Virginia", "Virginia",
     "us_local", "government",
     "http://virginiaillinois.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Virginia\",\"government\",\"batch:50\"]",
     10800,
     "Virginia — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 5 entries at probe")

RSSX(us_b50_visitkingsland_com, "us-b50-visitkingsland-com",
     "Kingsland", "Kingsland",
     "us_local", "government",
     "https://visitkingsland.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Kingsland\",\"government\",\"batch:50\"]",
     10800,
     "Kingsland — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_visitmorton_com, "us-b50-visitmorton-com",
     "Morton", "Morton",
     "us_local", "government",
     "https://visitmorton.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Morton\",\"government\",\"batch:50\"]",
     10800,
     "Morton — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_visitwarroad_com, "us-b50-visitwarroad-com",
     "Warroad", "Warroad",
     "us_local", "government",
     "https://visitwarroad.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Warroad\",\"government\",\"batch:50\"]",
     10800,
     "Warroad — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_visityazoo_org, "us-b50-visityazoo-org",
     "Yazoo City", "Yazoo City",
     "us_local", "government",
     "https://visityazoo.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Yazoo City\",\"government\",\"batch:50\"]",
     10800,
     "Yazoo City — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_wabasha_org, "us-b50-wabasha-org",
     "Wabasha", "Wabasha",
     "us_local", "government",
     "https://www.wabasha.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Wabasha\",\"government\",\"batch:50\"]",
     10800,
     "Wabasha — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_waldo_fl_com, "us-b50-waldo-fl-com",
     "Waldo", "Waldo",
     "us_local", "government",
     "https://waldo-fl.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Waldo\",\"government\",\"batch:50\"]",
     10800,
     "Waldo — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_warrenminnesota_com, "us-b50-warrenminnesota-com",
     "Warren", "Warren",
     "us_local", "government",
     "https://warrenminnesota.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Warren\",\"government\",\"batch:50\"]",
     10800,
     "Warren — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 6 entries at probe")

RSSX(us_b50_watertowntn_com, "us-b50-watertowntn-com",
     "Watertown — Watertown, Tennessee RSS Feed", "Watertown — Watertown, Tennessee RSS Feed",
     "us_local", "government",
     "https://watertowntn.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Watertown — Watertown, Tennessee RSS Feed\",\"government\",\"batch:50\"]",
     10800,
     "Watertown — Watertown, Tennessee RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_waterville_org, "us-b50-waterville-org",
     "Waterville", "Waterville",
     "us_local", "government",
     "https://waterville.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Waterville\",\"government\",\"batch:50\"]",
     10800,
     "Waterville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 4 entries at probe")

RSSX(us_b50_weepingwaternebraska_com_nl_index_format_feed_type_atom, "us-b50-weepingwaternebraska-com-nl-index-format-feed-type-atom",
     "Weeping Water — Atom 1.0", "Weeping Water — Atom 1.0",
     "us_local", "government",
     "https://www.weepingwaternebraska.com/nl/index.php?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Weeping Water — Atom 1.0\",\"government\",\"batch:50\"]",
     10800,
     "Weeping Water — Atom 1.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_weepingwaternebraska_com_nl_index_format_feed_type_rss, "us-b50-weepingwaternebraska-com-nl-index-format-feed-type-rss",
     "Weeping Water", "Weeping Water",
     "us_local", "government",
     "https://www.weepingwaternebraska.com/nl/index.php?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Weeping Water\",\"government\",\"batch:50\"]",
     10800,
     "Weeping Water — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_weippe_com, "us-b50-weippe-com",
     "Weippe", "Weippe",
     "us_local", "government",
     "https://www.weippe.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Weippe\",\"government\",\"batch:50\"]",
     10800,
     "Weippe — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_wellsvillecity_com, "us-b50-wellsvillecity-com",
     "Wellsville", "Wellsville",
     "us_local", "government",
     "https://www.wellsvillecity.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Wellsville\",\"government\",\"batch:50\"]",
     10800,
     "Wellsville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_westbranchiowa_org, "us-b50-westbranchiowa-org",
     "West Branch", "West Branch",
     "us_local", "government",
     "https://westbranchiowa.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:West Branch\",\"government\",\"batch:50\"]",
     10800,
     "West Branch — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 15 entries at probe")

RSSX(us_b50_whitecitycc_org, "us-b50-whitecitycc-org",
     "White City", "White City",
     "us_local", "government",
     "https://whitecitycc.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:White City\",\"government\",\"batch:50\"]",
     10800,
     "White City — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_whitecitycc_org_events, "us-b50-whitecitycc-org-events",
     "White City — White City Community Council » Events Feed", "White City — White City Community Council » Events Feed",
     "us_local", "government",
     "https://whitecitycc.org/events/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:White City — White City Community Council » Events Feed\",\"government\",\"batch:50\"]",
     10800,
     "White City — White City Community Council » Events Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_whitevillecity_com, "us-b50-whitevillecity-com",
     "Whiteville", "Whiteville",
     "us_local", "government",
     "https://whitevillecity.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Whiteville\",\"government\",\"batch:50\"]",
     10800,
     "Whiteville — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_williamsburgks_us_blog_feed, "us-b50-williamsburgks-us-blog-feed",
     "Williamsburg — Williamsburg - RSS", "Williamsburg — Williamsburg - RSS",
     "us_local", "government",
     "https://www.williamsburgks.us/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Williamsburg — Williamsburg - RSS\",\"government\",\"batch:50\"]",
     10800,
     "Williamsburg — Williamsburg - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 2 entries at probe")

RSSX(us_b50_williamson_ga_us, "us-b50-williamson-ga-us",
     "Williamson", "Williamson",
     "us_local", "government",
     "https://williamson-ga.us/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Williamson\",\"government\",\"batch:50\"]",
     10800,
     "Williamson — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 8 entries at probe")

RSSX(us_b50_willoughbyohio_com, "us-b50-willoughbyohio-com",
     "Willoughby", "Willoughby",
     "us_local", "government",
     "https://willoughbyohio.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Willoughby\",\"government\",\"batch:50\"]",
     10800,
     "Willoughby — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_winfieldcity_org_en_format_feed_type_atom, "us-b50-winfieldcity-org-en-format-feed-type-atom",
     "Winfield — Atom 1.0", "Winfield — Atom 1.0",
     "us_local", "government",
     "https://www.winfieldcity.org/en/?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Winfield — Atom 1.0\",\"government\",\"batch:50\"]",
     10800,
     "Winfield — Atom 1.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_winfieldcity_org_en_format_feed_type_rss, "us-b50-winfieldcity-org-en-format-feed-type-rss",
     "Winfield", "Winfield",
     "us_local", "government",
     "https://www.winfieldcity.org/en/?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Winfield\",\"government\",\"batch:50\"]",
     10800,
     "Winfield — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 7 entries at probe")

RSSX(us_b50_wolfecitytx_org, "us-b50-wolfecitytx-org",
     "Wolfe City", "Wolfe City",
     "us_local", "government",
     "https://wolfecitytx.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Wolfe City\",\"government\",\"batch:50\"]",
     10800,
     "Wolfe City — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

RSSX(us_b50_wymorebluesprings_com, "us-b50-wymorebluesprings-com",
     "Blue Springs", "Blue Springs",
     "us_local", "government",
     "https://www.wymorebluesprings.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Blue Springs\",\"government\",\"batch:50\"]",
     10800,
     "Blue Springs — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 1 entries at probe")

RSSX(us_b50_wynnearkansas_com, "us-b50-wynnearkansas-com",
     "Wynne", "Wynne",
     "us_local", "government",
     "https://wynnearkansas.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Wynne\",\"government\",\"batch:50\"]",
     10800,
     "Wynne — RSS/Atom feed declared by the official website (Wikidata P856) of this city government; 10 entries at probe")

/* ── us_opendata ─────────────────────────────────────────────────────── */

RSSX(us_b50_census_gov_content_census_en_newsroom_press_releases, "us-b50-census-gov-content-census-en-newsroom-press-releases",
     "Census Bureau Press Releases", "Census Bureau Press Releases",
     "us_opendata", "opendata",
     "https://www.census.gov/content/census/en/newsroom/press-releases.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Census Bureau Press Releases\",\"opendata\",\"batch:50\"]",
     3600,
     "Census Bureau Press Releases — opendata feed (10 entries at probe)")

/* ── us_research ─────────────────────────────────────────────────────── */

RSSX(us_b50_academyart_edu, "us-b50-academyart-edu",
     "Academy of Art University", "Academy of Art University",
     "us_research", "science",
     "https://www.academyart.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Academy of Art University\",\"science\",\"batch:50\"]",
     10800,
     "Academy of Art University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_aci_edu, "us-b50-aci-edu",
     "American Career Institute", "American Career Institute",
     "us_research", "science",
     "https://aci.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:American Career Institute\",\"science\",\"batch:50\"]",
     10800,
     "American Career Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_adams_edu, "us-b50-adams-edu",
     "Adams State University", "Adams State University",
     "us_research", "science",
     "https://www.adams.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Adams State University\",\"science\",\"batch:50\"]",
     10800,
     "Adams State University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_agsci_colostate_edu, "us-b50-agsci-colostate-edu",
     "Colorado State University College of Agricultural Sciences", "Colorado State University College of Agricultural Sciences",
     "us_research", "science",
     "https://agsci.colostate.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Colorado State University College of Agricultural Sciences\",\"science\",\"batch:50\"]",
     10800,
     "Colorado State University College of Agricultural Sciences — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_ahs_illinois_edu, "us-b50-ahs-illinois-edu",
     "UIUC College of Applied Health Sciences", "UIUC College of Applied Health Sciences",
     "us_research", "science",
     "https://www.ahs.illinois.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:UIUC College of Applied Health Sciences\",\"science\",\"batch:50\"]",
     10800,
     "UIUC College of Applied Health Sciences — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_albizu_edu, "us-b50-albizu-edu",
     "Carlos Albizu University", "Carlos Albizu University",
     "us_research", "science",
     "https://www.albizu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Carlos Albizu University\",\"science\",\"batch:50\"]",
     10800,
     "Carlos Albizu University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_alcorn_edu, "us-b50-alcorn-edu",
     "Alcorn State University", "Alcorn State University",
     "us_research", "science",
     "https://www.alcorn.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Alcorn State University\",\"science\",\"batch:50\"]",
     10800,
     "Alcorn State University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_alliant_edu_schools_cspp, "us-b50-alliant-edu-schools-cspp",
     "California School of Professional Psychology — Alliant University » California School of Professional Psych", "California School of Professional Psychology — Alliant University » California School of Professional Psych",
     "us_research", "science",
     "https://www.alliant.edu/schools/cspp/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:California School of Professional Psychology — Alliant University » California School of Professional Psych\",\"science\",\"batch:50\"]",
     10800,
     "California School of Professional Psychology — Alliant University » California School of Professional Psych — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 12 entries at probe")

RSSX(us_b50_alu_edu, "us-b50-alu-edu",
     "Abraham Lincoln University", "Abraham Lincoln University",
     "us_research", "science",
     "https://alu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Abraham Lincoln University\",\"science\",\"batch:50\"]",
     10800,
     "Abraham Lincoln University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 12 entries at probe")

RSSX(us_b50_amesall_rutgers_edu_format_feed_type_atom, "us-b50-amesall-rutgers-edu-format-feed-type-atom",
     "Rutgers University Department of African, Middle Eastern, and South Asian Languages and Literatures — Atom 1.0", "Rutgers University Department of African, Middle Eastern, and South Asian Languages and Literatures — Atom 1.0",
     "us_research", "science",
     "https://www.amesall.rutgers.edu/?format=feed&type=atom",
     "en", "[\"gov\",\"country:us\",\"publisher:Rutgers University Department of African, Middle Eastern, and South Asian Languages and Literatures — Atom 1.0\",\"science\",\"batch:50\"]",
     10800,
     "Rutgers University Department of African, Middle Eastern, and South Asian Languages and Literatures — Atom 1.0 — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(us_b50_amesall_rutgers_edu_format_feed_type_rss, "us-b50-amesall-rutgers-edu-format-feed-type-rss",
     "Rutgers University Department of African, Middle Eastern, and South Asian Languages and Literatures", "Rutgers University Department of African, Middle Eastern, and South Asian Languages and Literatures",
     "us_research", "science",
     "https://www.amesall.rutgers.edu/?format=feed&type=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Rutgers University Department of African, Middle Eastern, and South Asian Languages and Literatures\",\"science\",\"batch:50\"]",
     10800,
     "Rutgers University Department of African, Middle Eastern, and South Asian Languages and Literatures — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(us_b50_an_edu, "us-b50-an-edu",
     "American National University", "American National University",
     "us_research", "science",
     "https://an.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:American National University\",\"science\",\"batch:50\"]",
     10800,
     "American National University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_an_edu_rsslatest, "us-b50-an-edu-rsslatest",
     "American National University", "American National University",
     "us_research", "science",
     "https://an.edu/rsslatest.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:American National University\",\"science\",\"batch:50\"]",
     10800,
     "American National University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 50 entries at probe")

RSSX(us_b50_anderson_edu, "us-b50-anderson-edu",
     "Anderson University", "Anderson University",
     "us_research", "science",
     "https://anderson.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Anderson University\",\"science\",\"batch:50\"]",
     10800,
     "Anderson University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 16 entries at probe")

RSSX(us_b50_arch_tamu_edu, "us-b50-arch-tamu-edu",
     "Texas A&M College of Architecture", "Texas A&M College of Architecture",
     "us_research", "science",
     "https://www.arch.tamu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Texas A&M College of Architecture\",\"science\",\"batch:50\"]",
     10800,
     "Texas A&M College of Architecture — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_ash_harvard_edu, "us-b50-ash-harvard-edu",
     "Ash Center for Democratic Governance and Innovation", "Ash Center for Democratic Governance and Innovation",
     "us_research", "science",
     "https://ash.harvard.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Ash Center for Democratic Governance and Innovation\",\"science\",\"batch:50\"]",
     10800,
     "Ash Center for Democratic Governance and Innovation — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 4 entries at probe")

RSSX(us_b50_atlanticu_edu, "us-b50-atlanticu-edu",
     "Atlantic University College", "Atlantic University College",
     "us_research", "science",
     "https://atlanticu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Atlantic University College\",\"science\",\"batch:50\"]",
     10800,
     "Atlantic University College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_atlanticuniv_edu, "us-b50-atlanticuniv-edu",
     "Atlantic University", "Atlantic University",
     "us_research", "science",
     "https://atlanticuniv.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Atlantic University\",\"science\",\"batch:50\"]",
     10800,
     "Atlantic University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 4 entries at probe")

RSSX(us_b50_aucenter_edu, "us-b50-aucenter-edu",
     "Atlanta University Center", "Atlanta University Center",
     "us_research", "science",
     "https://aucenter.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Atlanta University Center\",\"science\",\"batch:50\"]",
     10800,
     "Atlanta University Center — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_austincc_edu, "us-b50-austincc-edu",
     "Austin Community College District", "Austin Community College District",
     "us_research", "science",
     "https://www.austincc.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Austin Community College District\",\"science\",\"batch:50\"]",
     10800,
     "Austin Community College District — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 2 entries at probe")

RSSX(us_b50_autodealerinstitute_com_rss2, "us-b50-autodealerinstitute-com-rss2",
     "Automotive Dealership Institute", "Automotive Dealership Institute",
     "us_research", "science",
     "https://www.autodealerinstitute.com/feed/rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Automotive Dealership Institute\",\"science\",\"batch:50\"]",
     10800,
     "Automotive Dealership Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 15 entries at probe")

RSSX(us_b50_averett_edu, "us-b50-averett-edu",
     "Averett University", "Averett University",
     "us_research", "science",
     "https://www.averett.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Averett University\",\"science\",\"batch:50\"]",
     10800,
     "Averett University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_ben_edu, "us-b50-ben-edu",
     "Benedictine University", "Benedictine University",
     "us_research", "science",
     "https://ben.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Benedictine University\",\"science\",\"batch:50\"]",
     10800,
     "Benedictine University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_bethanygu_edu, "us-b50-bethanygu-edu",
     "Bethany Global University", "Bethany Global University",
     "us_research", "science",
     "https://bethanygu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Bethany Global University\",\"science\",\"batch:50\"]",
     10800,
     "Bethany Global University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_betheluniversity_edu, "us-b50-betheluniversity-edu",
     "Bethel University", "Bethel University",
     "us_research", "science",
     "https://betheluniversity.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Bethel University\",\"science\",\"batch:50\"]",
     10800,
     "Bethel University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_bouve_northeastern_edu, "us-b50-bouve-northeastern-edu",
     "Northeastern University School of Pharmacy", "Northeastern University School of Pharmacy",
     "us_research", "science",
     "https://bouve.northeastern.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Northeastern University School of Pharmacy\",\"science\",\"batch:50\"]",
     10800,
     "Northeastern University School of Pharmacy — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_brandcenter_vcu_edu, "us-b50-brandcenter-vcu-edu",
     "VCU Brandcenter", "VCU Brandcenter",
     "us_research", "science",
     "https://brandcenter.vcu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:VCU Brandcenter\",\"science\",\"batch:50\"]",
     10800,
     "VCU Brandcenter — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 5 entries at probe")

RSSX(us_b50_brenau_edu, "us-b50-brenau-edu",
     "Brenau University", "Brenau University",
     "us_research", "science",
     "https://www.brenau.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Brenau University\",\"science\",\"batch:50\"]",
     10800,
     "Brenau University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 15 entries at probe")

RSSX(us_b50_breyerstate_com, "us-b50-breyerstate-com",
     "Breyer State University", "Breyer State University",
     "us_research", "science",
     "https://breyerstate.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Breyer State University\",\"science\",\"batch:50\"]",
     10800,
     "Breyer State University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_brockport_edu_live_profiles_header_world_class_20graduate_20_26amp_3b_20undergra, "us-b50-brockport-edu-live-profiles-header-world-class-20graduate-20-26amp-3b-20undergra",
     "State University of New York at Brockport — Profiles: World-class Graduate & Undergraduate Education, Di", "State University of New York at Brockport — Profiles: World-class Graduate & Undergraduate Education, Di",
     "us_research", "science",
     "https://www.brockport.edu/live/rss/profiles/header/World-class%20graduate%20%26amp%3B%20undergraduate%20education%2C%20distinguished%20faculty%2C%20and%20modern%20facilities%2C%20at%20one%20of%20the%20lowest%20tuition%20rates%20in%20the%20nation./widget_template/program-search/format_widget/%3Cdiv%20class%3D%22search-section%20js-search-dd%20program-search%22%3E%0A%20%20%20%20%3Cdiv%20class%3D%22container%22%3E%0A%20%20%20%20%20%20%3Cdiv%20class%3D%22search__title%22%3E%0A%20%20%20%20%20%20%20%20%3Cp%20class%3D%22h3%22%3E%0A%20%20%20%20%20%20%20%20%20%20%7Bheader_clean%7D%0A%20%20%20%20%20%20%20%20%3Cp%3E%0A%20%20%20%20%20%20%3Cdiv%3E%3C%21--%20.search__title%20--%3E%0A%20%20%20%20%20%20%3Cdiv%20class%3D%22search__body%22%3E%0A%20%20%20%20%20%20%20%20%3Cdiv%20class%3D%22search%20search--big%20txt-color-green%20bg-color-yellow%22%3E%0A%20%20%20%20%20%20%20%20%20%20%3C%21--%20animate%20animate-%20-fade-up%20js-animate%20--%3E%0A%20%20%20%20%20%20%20%20%20%20%3Cform%20action%3D%22%22%20method%3D%22get%22%20autocomplete%3D%22off%22%3E%0A%20%20%20%20%20%20%20%20%20%20%20%20%3Clabel%20for%3D%22program-search%22%20class%3D%22%22%3E%3Cspan%20class%3D%22sr-only%22%3ESearch%20programs%3Cspan%3E%3Cem%20class%3D%22ico-search%22%3E%3Cem%3E%3Clabel%3E%20%3Cinput%20type%3D%22text%22%20placeholder%3D%22Search%20programs%22%20name%3D%22program-search%22%20id%3D%22program-search%22%20value%3D%22%22%20class%3D%22search__field%20lw_qa_filter%22%20data-qa-max-results-per-category%3D%2220%22%20data-qa-more-results-msg%3D%22All%20programs%20%C2%BB%22%20data-qa-more-results-url%3D%22academicsmajors-minors%3Fq%3D%22%20data-qa-too-many%3D%22%22%20data-qa-target%3D%22%23program-list%20a%22%20data-qa-none-found%3D%22%22%20%3E%20%3Cbutton%20type%3D%22submit%22%20class%3D%22search__btn%20sr-only-focusable%20mb-3%22%3ESearch%3Cbutton%3E%0A%20%20%20%20%20%20%20%20%20%20%3Cform%3E%0A%20%20%20%20%20%20%20%20%3Cdiv%3E%0A%20%20%20%20%20%20%3Cdiv%3E%3C%21--%20.search__body%20--%3E%0A%20%20%20%20%20%20%3Cwidget%20type%3D%22profiles%22%20inherit%3D%22filter%22%3E%0A%09%09%09%3Carg%20id%3D%22widget_template%22%3Eprogram-search-hints%3Carg%3E%0A%09%09%3Cwidget%3E%0A%20%20%20%20%20%20%3Cdiv%20id%3D%22program-list%22%20class%3D%22lw_hidden%22%3E%0A%20%20%20%20%20%20%20%20%7Bwidget%7D%0A%20%20%20%20%20%20%3Cdiv%3E%0A%20%20%20%20%3Cdiv%3E%3C%21--%20.container%20--%3E%0A%20%20%3Cdiv%3E/type/Public%20Program%20List/header_block/h3/exclude_group/Brockport%20Testing%20Group/exclude_group/General%20Education%20-%20new/exclude_group/School%20of%20Business%20%26amp%3B%20Management%20Video%20Wall",
     "en", "[\"gov\",\"country:us\",\"publisher:State University of New York at Brockport — Profiles: World-class Graduate & Undergraduate Education, Di\",\"science\",\"batch:50\"]",
     10800,
     "State University of New York at Brockport — Profiles: World-class Graduate & Undergraduate Education, Di — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 211 entries at probe")

RSSX(us_b50_byuh_edu, "us-b50-byuh-edu",
     "Brigham Young University–Hawaii", "Brigham Young University–Hawaii",
     "us_research", "science",
     "https://www.byuh.edu/index.rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Brigham Young University–Hawaii\",\"science\",\"batch:50\"]",
     10800,
     "Brigham Young University–Hawaii — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 3 entries at probe")

RSSX(us_b50_byui_edu, "us-b50-byui-edu",
     "Brigham Young University–Idaho — BYU-Idaho", "Brigham Young University–Idaho — BYU-Idaho",
     "us_research", "science",
     "https://www.byui.edu/index.rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Brigham Young University–Idaho — BYU-Idaho\",\"science\",\"batch:50\"]",
     10800,
     "Brigham Young University–Idaho — BYU-Idaho — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 3 entries at probe")

RSSX(us_b50_cairn_edu, "us-b50-cairn-edu",
     "Cairn University", "Cairn University",
     "us_research", "science",
     "https://cairn.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Cairn University\",\"science\",\"batch:50\"]",
     10800,
     "Cairn University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_caluniversity_edu, "us-b50-caluniversity-edu",
     "California Intercontinental University", "California Intercontinental University",
     "us_research", "science",
     "https://caluniversity.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:California Intercontinental University\",\"science\",\"batch:50\"]",
     10800,
     "California Intercontinental University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_calvary_edu, "us-b50-calvary-edu",
     "Calvary University", "Calvary University",
     "us_research", "science",
     "https://www.calvary.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Calvary University\",\"science\",\"batch:50\"]",
     10800,
     "Calvary University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_caribbean_edu, "us-b50-caribbean-edu",
     "Caribbean University", "Caribbean University",
     "us_research", "science",
     "https://caribbean.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Caribbean University\",\"science\",\"batch:50\"]",
     10800,
     "Caribbean University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_cbu_edu_podcast, "us-b50-cbu-edu-podcast",
     "Christian Brothers University — Podcast RSS feed", "Christian Brothers University — Podcast RSS feed",
     "us_research", "science",
     "https://www.cbu.edu/feed/podcast",
     "en", "[\"gov\",\"country:us\",\"publisher:Christian Brothers University — Podcast RSS feed\",\"science\",\"batch:50\"]",
     10800,
     "Christian Brothers University — Podcast RSS feed — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_cdkc_edu, "us-b50-cdkc-edu",
     "Chief Dull Knife College — Chief Dull Knife College Feed", "Chief Dull Knife College — Chief Dull Knife College Feed",
     "us_research", "science",
     "https://www.cdkc.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Chief Dull Knife College — Chief Dull Knife College Feed\",\"science\",\"batch:50\"]",
     10800,
     "Chief Dull Knife College — Chief Dull Knife College Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_centralgatech_edu, "us-b50-centralgatech-edu",
     "Central Georgia Technical College", "Central Georgia Technical College",
     "us_research", "science",
     "https://www.centralgatech.edu/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:Central Georgia Technical College\",\"science\",\"batch:50\"]",
     10800,
     "Central Georgia Technical College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_chekhov_net, "us-b50-chekhov-net",
     "National Michael Chekhov Association", "National Michael Chekhov Association",
     "us_research", "science",
     "https://chekhov.net/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:National Michael Chekhov Association\",\"science\",\"batch:50\"]",
     10800,
     "National Michael Chekhov Association — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 2 entries at probe")

RSSX(us_b50_cheyney_edu, "us-b50-cheyney-edu",
     "Cheyney University of Pennsylvania", "Cheyney University of Pennsylvania",
     "us_research", "science",
     "https://cheyney.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Cheyney University of Pennsylvania\",\"science\",\"batch:50\"]",
     10800,
     "Cheyney University of Pennsylvania — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_cityu_edu, "us-b50-cityu-edu",
     "City University of Seattle", "City University of Seattle",
     "us_research", "science",
     "https://cityu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:City University of Seattle\",\"science\",\"batch:50\"]",
     10800,
     "City University of Seattle — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 9 entries at probe")

RSSX(us_b50_cityvision_edu, "us-b50-cityvision-edu",
     "City Vision University", "City Vision University",
     "us_research", "science",
     "https://www.cityvision.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:City Vision University\",\"science\",\"batch:50\"]",
     10800,
     "City Vision University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_ciu_edu, "us-b50-ciu-edu",
     "Columbia International University", "Columbia International University",
     "us_research", "science",
     "https://ciu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Columbia International University\",\"science\",\"batch:50\"]",
     10800,
     "Columbia International University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_claremontlincoln_edu, "us-b50-claremontlincoln-edu",
     "Claremont Lincoln University", "Claremont Lincoln University",
     "us_research", "science",
     "https://claremontlincoln.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Claremont Lincoln University\",\"science\",\"batch:50\"]",
     10800,
     "Claremont Lincoln University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_cleary_edu, "us-b50-cleary-edu",
     "Cleary University", "Cleary University",
     "us_research", "science",
     "https://www.cleary.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Cleary University\",\"science\",\"batch:50\"]",
     10800,
     "Cleary University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 12 entries at probe")

RSSX(us_b50_coker_edu, "us-b50-coker-edu",
     "Coker University", "Coker University",
     "us_research", "science",
     "https://www.coker.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Coker University\",\"science\",\"batch:50\"]",
     10800,
     "Coker University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 4 entries at probe")

RSSX(us_b50_csuglobal_edu, "us-b50-csuglobal-edu",
     "CSU–Global Campus", "CSU–Global Campus",
     "us_research", "science",
     "https://csuglobal.edu/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:CSU–Global Campus\",\"science\",\"batch:50\"]",
     10800,
     "CSU–Global Campus — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_dallas_edu, "us-b50-dallas-edu",
     "Dallas Christian College", "Dallas Christian College",
     "us_research", "science",
     "https://www.dallas.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Dallas Christian College\",\"science\",\"batch:50\"]",
     10800,
     "Dallas Christian College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_dentistry_unc_edu, "us-b50-dentistry-unc-edu",
     "University of North Carolina at Chapel Hill School of Dentistry", "University of North Carolina at Chapel Hill School of Dentistry",
     "us_research", "science",
     "https://dentistry.unc.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of North Carolina at Chapel Hill School of Dentistry\",\"science\",\"batch:50\"]",
     10800,
     "University of North Carolina at Chapel Hill School of Dentistry — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_divinemercy_edu, "us-b50-divinemercy-edu",
     "Institute for the Psychological Sciences", "Institute for the Psychological Sciences",
     "us_research", "science",
     "https://divinemercy.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Institute for the Psychological Sciences\",\"science\",\"batch:50\"]",
     10800,
     "Institute for the Psychological Sciences — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_divinewordseminary_blogspot_com_posts_default, "us-b50-divinewordseminary-blogspot-com-posts-default",
     "Divine Word Seminary — Divine Word Seminary-Tagaytay City - Atom", "Divine Word Seminary — Divine Word Seminary-Tagaytay City - Atom",
     "us_research", "science",
     "http://divinewordseminary.blogspot.com/feeds/posts/default",
     "en", "[\"gov\",\"country:us\",\"publisher:Divine Word Seminary — Divine Word Seminary-Tagaytay City - Atom\",\"science\",\"batch:50\"]",
     10800,
     "Divine Word Seminary — Divine Word Seminary-Tagaytay City - Atom — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 25 entries at probe")

RSSX(us_b50_divinewordseminary_blogspot_com_posts_default_alt_rss, "us-b50-divinewordseminary-blogspot-com-posts-default-alt-rss",
     "Divine Word Seminary — Divine Word Seminary-Tagaytay City - RSS", "Divine Word Seminary — Divine Word Seminary-Tagaytay City - RSS",
     "us_research", "science",
     "http://divinewordseminary.blogspot.com/feeds/posts/default?alt=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Divine Word Seminary — Divine Word Seminary-Tagaytay City - RSS\",\"science\",\"batch:50\"]",
     10800,
     "Divine Word Seminary — Divine Word Seminary-Tagaytay City - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 25 entries at probe")

RSSX(us_b50_dornsife_usc_edu_poir, "us-b50-dornsife-usc-edu-poir",
     "University of Southern California School of International Relations — Department of Political Science and International Relations", "University of Southern California School of International Relations — Department of Political Science and International Relations",
     "us_research", "science",
     "https://dornsife.usc.edu/poir/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Southern California School of International Relations — Department of Political Science and International Relations\",\"science\",\"batch:50\"]",
     10800,
     "University of Southern California School of International Relations — Department of Political Science and International Relations — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_dou_us_feed_rss2, "us-b50-dou-us-feed-rss2",
     "Dhammakaya Open University — » ฟีด", "Dhammakaya Open University — » ฟีด",
     "us_research", "science",
     "http://www.dou.us/?feed=rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:Dhammakaya Open University — » ฟีด\",\"science\",\"batch:50\"]",
     10800,
     "Dhammakaya Open University — » ฟีด — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(us_b50_drew_edu, "us-b50-drew-edu",
     "Drew University", "Drew University",
     "us_research", "science",
     "https://drew.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Drew University\",\"science\",\"batch:50\"]",
     10800,
     "Drew University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_drury_edu, "us-b50-drury-edu",
     "Drury University", "Drury University",
     "us_research", "science",
     "https://www.drury.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Drury University\",\"science\",\"batch:50\"]",
     10800,
     "Drury University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_dwu_edu_call_blogs, "us-b50-dwu-edu-call-blogs",
     "Dakota Wesleyan University — www.dwu.edu - Main Channel", "Dakota Wesleyan University — www.dwu.edu - Main Channel",
     "us_research", "science",
     "https://www.dwu.edu/call/blogs/rss/",
     "en", "[\"gov\",\"country:us\",\"publisher:Dakota Wesleyan University — www.dwu.edu - Main Channel\",\"science\",\"batch:50\"]",
     10800,
     "Dakota Wesleyan University — www.dwu.edu - Main Channel — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_dwu_edu_call_blogs_category_18, "us-b50-dwu-edu-call-blogs-category-18",
     "Dakota Wesleyan University — www.dwu.edu - News", "Dakota Wesleyan University — www.dwu.edu - News",
     "us_research", "science",
     "https://www.dwu.edu/call/blogs/rss/category/18",
     "en", "[\"gov\",\"country:us\",\"publisher:Dakota Wesleyan University — www.dwu.edu - News\",\"science\",\"batch:50\"]",
     10800,
     "Dakota Wesleyan University — www.dwu.edu - News — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_eap_edu, "us-b50-eap-edu",
     "School of Plastic Arts of Puerto Rico", "School of Plastic Arts of Puerto Rico",
     "us_research", "science",
     "https://eap.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:School of Plastic Arts of Puerto Rico\",\"science\",\"batch:50\"]",
     10800,
     "School of Plastic Arts of Puerto Rico — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_ecsu_edu, "us-b50-ecsu-edu",
     "Elizabeth City State University", "Elizabeth City State University",
     "us_research", "science",
     "https://ecsu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Elizabeth City State University\",\"science\",\"batch:50\"]",
     10800,
     "Elizabeth City State University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_ecua_edu, "us-b50-ecua-edu",
     "Evangel Christian University of America", "Evangel Christian University of America",
     "us_research", "science",
     "https://ecua.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Evangel Christian University of America\",\"science\",\"batch:50\"]",
     10800,
     "Evangel Christian University of America — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 5 entries at probe")

RSSX(us_b50_eidhi_com, "us-b50-eidhi-com",
     "EIDHI International University", "EIDHI International University",
     "us_research", "science",
     "https://www.eidhi.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:EIDHI International University\",\"science\",\"batch:50\"]",
     10800,
     "EIDHI International University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_evangel_edu, "us-b50-evangel-edu",
     "Evangel University", "Evangel University",
     "us_research", "science",
     "https://www.evangel.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Evangel University\",\"science\",\"batch:50\"]",
     10800,
     "Evangel University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_events_mtu_edu_widget_view_schools_michigantech_days_30_num_50_format_rss, "us-b50-events-mtu-edu-widget-view-schools-michigantech-days-30-num-50-format-rss",
     "Michigan Technological University — Michigan Tech Events", "Michigan Technological University — Michigan Tech Events",
     "us_research", "science",
     "https://events.mtu.edu/widget/view?schools=michigantech&days=30&num=50&format=rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Michigan Technological University — Michigan Tech Events\",\"science\",\"batch:50\"]",
     10800,
     "Michigan Technological University — Michigan Tech Events — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 50 entries at probe")

RSSX(us_b50_faulkner_edu, "us-b50-faulkner-edu",
     "Faulkner University", "Faulkner University",
     "us_research", "science",
     "https://www.faulkner.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Faulkner University\",\"science\",\"batch:50\"]",
     10800,
     "Faulkner University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_national_aer, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-national-aer",
     "Fed Register NASA", "Fed Register NASA",
     "us_research", "science",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=national-aeronautics-and-space-administration",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register NASA\",\"science\",\"batch:50\"]",
     10800,
     "Fed Register NASA — science feed (12 entries at probe)")

RSSX(us_b50_federalregister_gov_api_v1_documents_conditions_5bagencies_5d_5b_5d_national_sci, "us-b50-federalregister-gov-api-v1-documents-conditions-5bagencies-5d-5b-5d-national-sci",
     "Fed Register NSF", "Fed Register NSF",
     "us_research", "science",
     "https://www.federalregister.gov/api/v1/documents.rss?conditions%5Bagencies%5D%5B%5D=national-science-foundation",
     "en", "[\"gov\",\"country:us\",\"publisher:Fed Register NSF\",\"science\",\"batch:50\"]",
     10800,
     "Fed Register NSF — science feed (7 entries at probe)")

RSSX(us_b50_feeds_feedburner_com_charteroak, "us-b50-feeds-feedburner-com-charteroak",
     "Charter Oak State College — Charter Oak State College News", "Charter Oak State College — Charter Oak State College News",
     "us_research", "science",
     "https://feeds.feedburner.com/charteroak",
     "en", "[\"gov\",\"country:us\",\"publisher:Charter Oak State College — Charter Oak State College News\",\"science\",\"batch:50\"]",
     10800,
     "Charter Oak State College — Charter Oak State College News — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 4 entries at probe")

RSSX(us_b50_fielding_edu, "us-b50-fielding-edu",
     "Fielding Graduate University", "Fielding Graduate University",
     "us_research", "science",
     "https://www.fielding.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Fielding Graduate University\",\"science\",\"batch:50\"]",
     10800,
     "Fielding Graduate University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_floridapoly_edu, "us-b50-floridapoly-edu",
     "Florida Polytechnic University", "Florida Polytechnic University",
     "us_research", "science",
     "https://floridapoly.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Florida Polytechnic University\",\"science\",\"batch:50\"]",
     10800,
     "Florida Polytechnic University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 999 entries at probe")

RSSX(us_b50_fmarion_edu, "us-b50-fmarion-edu",
     "Francis Marion University", "Francis Marion University",
     "us_research", "science",
     "https://www.fmarion.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Francis Marion University\",\"science\",\"batch:50\"]",
     10800,
     "Francis Marion University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_fnu_edu, "us-b50-fnu-edu",
     "Florida National University", "Florida National University",
     "us_research", "science",
     "https://www.fnu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Florida National University\",\"science\",\"batch:50\"]",
     10800,
     "Florida National University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_fontbonne_edu, "us-b50-fontbonne-edu",
     "Fontbonne University", "Fontbonne University",
     "us_research", "science",
     "https://fontbonne.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Fontbonne University\",\"science\",\"batch:50\"]",
     10800,
     "Fontbonne University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_franciscan_edu, "us-b50-franciscan-edu",
     "Franciscan University of Steubenville", "Franciscan University of Steubenville",
     "us_research", "science",
     "https://franciscan.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Franciscan University of Steubenville\",\"science\",\"batch:50\"]",
     10800,
     "Franciscan University of Steubenville — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_friends_edu, "us-b50-friends-edu",
     "Friends University", "Friends University",
     "us_research", "science",
     "https://www.friends.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Friends University\",\"science\",\"batch:50\"]",
     10800,
     "Friends University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 12 entries at probe")

RSSX(us_b50_fuller_edu, "us-b50-fuller-edu",
     "Fuller Theological Seminary", "Fuller Theological Seminary",
     "us_research", "science",
     "https://fuller.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Fuller Theological Seminary\",\"science\",\"batch:50\"]",
     10800,
     "Fuller Theological Seminary — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_gardner_webb_edu, "us-b50-gardner-webb-edu",
     "Gardner–Webb University", "Gardner–Webb University",
     "us_research", "science",
     "https://gardner-webb.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Gardner–Webb University\",\"science\",\"batch:50\"]",
     10800,
     "Gardner–Webb University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_geneseo_edu, "us-b50-geneseo-edu",
     "State University of New York at Geneseo", "State University of New York at Geneseo",
     "us_research", "science",
     "https://www.geneseo.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:State University of New York at Geneseo\",\"science\",\"batch:50\"]",
     10800,
     "State University of New York at Geneseo — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_glcc_edu, "us-b50-glcc-edu",
     "Great Lakes Christian College", "Great Lakes Christian College",
     "us_research", "science",
     "https://glcc.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Great Lakes Christian College\",\"science\",\"batch:50\"]",
     10800,
     "Great Lakes Christian College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_gradschool_duke_edu_blog, "us-b50-gradschool-duke-edu-blog",
     "Graduate School of Duke University — Blog RSS Feed", "Graduate School of Duke University — Blog RSS Feed",
     "us_research", "science",
     "https://gradschool.duke.edu/blog/rss.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Graduate School of Duke University — Blog RSS Feed\",\"science\",\"batch:50\"]",
     10800,
     "Graduate School of Duke University — Blog RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 462 entries at probe")

RSSX(us_b50_gradschool_duke_edu_events, "us-b50-gradschool-duke-edu-events",
     "Graduate School of Duke University — Events XML Feed", "Graduate School of Duke University — Events XML Feed",
     "us_research", "science",
     "https://gradschool.duke.edu/events/index.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Graduate School of Duke University — Events XML Feed\",\"science\",\"batch:50\"]",
     10800,
     "Graduate School of Duke University — Events XML Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 40 entries at probe")

RSSX(us_b50_gradschool_duke_edu_news, "us-b50-gradschool-duke-edu-news",
     "Graduate School of Duke University — News RSS Feed", "Graduate School of Duke University — News RSS Feed",
     "us_research", "science",
     "https://gradschool.duke.edu/news/rss.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Graduate School of Duke University — News RSS Feed\",\"science\",\"batch:50\"]",
     10800,
     "Graduate School of Duke University — News RSS Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 501 entries at probe")

RSSX(us_b50_graham_uchicago_edu, "us-b50-graham-uchicago-edu",
     "Graham School of Continuing Liberal and Professional Studies", "Graham School of Continuing Liberal and Professional Studies",
     "us_research", "science",
     "https://graham.uchicago.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Graham School of Continuing Liberal and Professional Studies\",\"science\",\"batch:50\"]",
     10800,
     "Graham School of Continuing Liberal and Professional Studies — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 12 entries at probe")

RSSX(us_b50_griggs_edu, "us-b50-griggs-edu",
     "Griggs University", "Griggs University",
     "us_research", "science",
     "https://griggs.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Griggs University\",\"science\",\"batch:50\"]",
     10800,
     "Griggs University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 5 entries at probe")

RSSX(us_b50_gw_edu_feed_rss2, "us-b50-gw-edu-feed-rss2",
     "George Wythe University", "George Wythe University",
     "us_research", "science",
     "https://gw.edu/?feed=rss2",
     "en", "[\"gov\",\"country:us\",\"publisher:George Wythe University\",\"science\",\"batch:50\"]",
     10800,
     "George Wythe University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(us_b50_haskell_edu, "us-b50-haskell-edu",
     "Haskell Indian Nations University", "Haskell Indian Nations University",
     "us_research", "science",
     "https://haskell.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Haskell Indian Nations University\",\"science\",\"batch:50\"]",
     10800,
     "Haskell Indian Nations University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_hcu_edu, "us-b50-hcu-edu",
     "Heritage Christian University", "Heritage Christian University",
     "us_research", "science",
     "https://www.hcu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Heritage Christian University\",\"science\",\"batch:50\"]",
     10800,
     "Heritage Christian University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_heritage_edu, "us-b50-heritage-edu",
     "Heritage University", "Heritage University",
     "us_research", "science",
     "https://heritage.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Heritage University\",\"science\",\"batch:50\"]",
     10800,
     "Heritage University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_highlands_edu, "us-b50-highlands-edu",
     "Georgia Highlands College", "Georgia Highlands College",
     "us_research", "science",
     "https://www.highlands.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Georgia Highlands College\",\"science\",\"batch:50\"]",
     10800,
     "Georgia Highlands College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_hlg_edu, "us-b50-hlg-edu",
     "Hannibal-LaGrange University", "Hannibal-LaGrange University",
     "us_research", "science",
     "https://www.hlg.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hannibal-LaGrange University\",\"science\",\"batch:50\"]",
     10800,
     "Hannibal-LaGrange University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_home_hamptonu_edu, "us-b50-home-hamptonu-edu",
     "Hampton University", "Hampton University",
     "us_research", "science",
     "https://home.hamptonu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hampton University\",\"science\",\"batch:50\"]",
     10800,
     "Hampton University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 12 entries at probe")

RSSX(us_b50_hs_iastate_edu, "us-b50-hs-iastate-edu",
     "Iowa State University College of Health and Human Sciences", "Iowa State University College of Health and Human Sciences",
     "us_research", "science",
     "https://hs.iastate.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Iowa State University College of Health and Human Sciences\",\"science\",\"batch:50\"]",
     10800,
     "Iowa State University College of Health and Human Sciences — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_hylesanderson_edu, "us-b50-hylesanderson-edu",
     "Hyles–Anderson College", "Hyles–Anderson College",
     "us_research", "science",
     "https://hylesanderson.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Hyles–Anderson College\",\"science\",\"batch:50\"]",
     10800,
     "Hyles–Anderson College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 2 entries at probe")

RSSX(us_b50_ico_edu_blog_feed, "us-b50-ico-edu-blog-feed",
     "Illinois College of Optometry — ICO - RSS", "Illinois College of Optometry — ICO - RSS",
     "us_research", "science",
     "https://www.ico.edu/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Illinois College of Optometry — ICO - RSS\",\"science\",\"batch:50\"]",
     10800,
     "Illinois College of Optometry — ICO - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 20 entries at probe")

RSSX(us_b50_icsw_edu, "us-b50-icsw-edu",
     "The Institute for Clinical Social Work", "The Institute for Clinical Social Work",
     "us_research", "science",
     "https://icsw.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:The Institute for Clinical Social Work\",\"science\",\"batch:50\"]",
     10800,
     "The Institute for Clinical Social Work — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 5 entries at probe")

RSSX(us_b50_ieccolleges_com_blog_feed, "us-b50-ieccolleges-com-blog-feed",
     "International Education Corporation — IEC Colleges - RSS", "International Education Corporation — IEC Colleges - RSS",
     "us_research", "science",
     "https://www.ieccolleges.com/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:International Education Corporation — IEC Colleges - RSS\",\"science\",\"batch:50\"]",
     10800,
     "International Education Corporation — IEC Colleges - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 20 entries at probe")

RSSX(us_b50_immaculata_edu, "us-b50-immaculata-edu",
     "Immaculata University", "Immaculata University",
     "us_research", "science",
     "https://www.immaculata.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Immaculata University\",\"science\",\"batch:50\"]",
     10800,
     "Immaculata University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_investorsunited_com, "us-b50-investorsunited-com",
     "Investors United", "Investors United",
     "us_research", "science",
     "https://investorsunited.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Investors United\",\"science\",\"batch:50\"]",
     10800,
     "Investors United — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 3 entries at probe")

RSSX(us_b50_ischool_cci_fsu_edu, "us-b50-ischool-cci-fsu-edu",
     "Florida State University School of Library & Information Studies", "Florida State University School of Library & Information Studies",
     "us_research", "science",
     "https://ischool.cci.fsu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Florida State University School of Library & Information Studies\",\"science\",\"batch:50\"]",
     10800,
     "Florida State University School of Library & Information Studies — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_itu_edu, "us-b50-itu-edu",
     "International Technological University", "International Technological University",
     "us_research", "science",
     "https://itu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:International Technological University\",\"science\",\"batch:50\"]",
     10800,
     "International Technological University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_jarvis_edu, "us-b50-jarvis-edu",
     "Jarvis Christian University", "Jarvis Christian University",
     "us_research", "science",
     "https://www.jarvis.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Jarvis Christian University\",\"science\",\"batch:50\"]",
     10800,
     "Jarvis Christian University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_jessup_edu, "us-b50-jessup-edu",
     "William Jessup University", "William Jessup University",
     "us_research", "science",
     "https://jessup.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:William Jessup University\",\"science\",\"batch:50\"]",
     10800,
     "William Jessup University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_johnpaulii_edu, "us-b50-johnpaulii-edu",
     "John Paul II Institute — Pontifical John Paul II Institute for Studies on Marriage &", "John Paul II Institute — Pontifical John Paul II Institute for Studies on Marriage &",
     "us_research", "science",
     "https://www.johnpaulii.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:John Paul II Institute — Pontifical John Paul II Institute for Studies on Marriage &\",\"science\",\"batch:50\"]",
     10800,
     "John Paul II Institute — Pontifical John Paul II Institute for Studies on Marriage & — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_jsums_edu, "us-b50-jsums-edu",
     "Jackson State University", "Jackson State University",
     "us_research", "science",
     "https://www.jsums.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Jackson State University\",\"science\",\"batch:50\"]",
     10800,
     "Jackson State University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 8 entries at probe")

RSSX(us_b50_knowledge_luskin_ucla_edu, "us-b50-knowledge-luskin-ucla-edu",
     "UCLA Center for Neighborhood Knowledge", "UCLA Center for Neighborhood Knowledge",
     "us_research", "science",
     "https://knowledge.luskin.ucla.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:UCLA Center for Neighborhood Knowledge\",\"science\",\"batch:50\"]",
     10800,
     "UCLA Center for Neighborhood Knowledge — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_kwc_edu, "us-b50-kwc-edu",
     "Kentucky Wesleyan College", "Kentucky Wesleyan College",
     "us_research", "science",
     "https://kwc.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Kentucky Wesleyan College\",\"science\",\"batch:50\"]",
     10800,
     "Kentucky Wesleyan College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_larecordingschool_com, "us-b50-larecordingschool-com",
     "Los Angeles Recording School", "Los Angeles Recording School",
     "us_research", "science",
     "https://www.larecordingschool.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Los Angeles Recording School\",\"science\",\"batch:50\"]",
     10800,
     "Los Angeles Recording School — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_lawrence_edu, "us-b50-lawrence-edu",
     "Lawrence University", "Lawrence University",
     "us_research", "science",
     "https://www.lawrence.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lawrence University\",\"science\",\"batch:50\"]",
     10800,
     "Lawrence University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_lewis_ucla_edu, "us-b50-lewis-ucla-edu",
     "UCLA Lewis Center for Regional Policy Studies", "UCLA Lewis Center for Regional Policy Studies",
     "us_research", "science",
     "https://lewis.ucla.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:UCLA Lewis Center for Regional Policy Studies\",\"science\",\"batch:50\"]",
     10800,
     "UCLA Lewis Center for Regional Policy Studies — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_lifepacific_edu, "us-b50-lifepacific-edu",
     "Life Pacific University", "Life Pacific University",
     "us_research", "science",
     "https://lifepacific.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Life Pacific University\",\"science\",\"batch:50\"]",
     10800,
     "Life Pacific University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 4 entries at probe")

RSSX(us_b50_lssu_edu, "us-b50-lssu-edu",
     "Lake Superior State University", "Lake Superior State University",
     "us_research", "science",
     "https://www.lssu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lake Superior State University\",\"science\",\"batch:50\"]",
     10800,
     "Lake Superior State University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_ltu_edu, "us-b50-ltu-edu",
     "Lawrence Technological University", "Lawrence Technological University",
     "us_research", "science",
     "https://ltu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Lawrence Technological University\",\"science\",\"batch:50\"]",
     10800,
     "Lawrence Technological University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_luskin_ucla_edu, "us-b50-luskin-ucla-edu",
     "UCLA Luskin School of Public Affairs", "UCLA Luskin School of Public Affairs",
     "us_research", "science",
     "https://luskin.ucla.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:UCLA Luskin School of Public Affairs\",\"science\",\"batch:50\"]",
     10800,
     "UCLA Luskin School of Public Affairs — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_manchester_edu, "us-b50-manchester-edu",
     "Manchester University", "Manchester University",
     "us_research", "science",
     "https://www.manchester.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Manchester University\",\"science\",\"batch:50\"]",
     10800,
     "Manchester University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 15 entries at probe")

RSSX(us_b50_martin_edu_blog_feed, "us-b50-martin-edu-blog-feed",
     "Martin University — Martin University - RSS", "Martin University — Martin University - RSS",
     "us_research", "science",
     "https://www.martin.edu/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Martin University — Martin University - RSS\",\"science\",\"batch:50\"]",
     10800,
     "Martin University — Martin University - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 20 entries at probe")

RSSX(us_b50_medicine_cuny_edu, "us-b50-medicine-cuny-edu",
     "Sophie Davis School of Biomedical Education", "Sophie Davis School of Biomedical Education",
     "us_research", "science",
     "https://medicine.cuny.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Sophie Davis School of Biomedical Education\",\"science\",\"batch:50\"]",
     10800,
     "Sophie Davis School of Biomedical Education — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_michigantechhuskies_com, "us-b50-michigantechhuskies-com",
     "Michigan Technological University — Michigan Tech Athletics News", "Michigan Technological University — Michigan Tech Athletics News",
     "us_research", "science",
     "https://michigantechhuskies.com/rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Michigan Technological University — Michigan Tech Athletics News\",\"science\",\"batch:50\"]",
     10800,
     "Michigan Technological University — Michigan Tech Athletics News — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_mildred_elley_edu, "us-b50-mildred-elley-edu",
     "Mildred Elley", "Mildred Elley",
     "us_research", "science",
     "https://www.mildred-elley.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Mildred Elley\",\"science\",\"batch:50\"]",
     10800,
     "Mildred Elley — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_missouriwestern_edu, "us-b50-missouriwestern-edu",
     "Missouri Western State University", "Missouri Western State University",
     "us_research", "science",
     "https://www.missouriwestern.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Missouri Western State University\",\"science\",\"batch:50\"]",
     10800,
     "Missouri Western State University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(us_b50_mkecc_edu, "us-b50-mkecc-edu",
     "Milwaukee Career College", "Milwaukee Career College",
     "us_research", "science",
     "https://mkecc.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Milwaukee Career College\",\"science\",\"batch:50\"]",
     10800,
     "Milwaukee Career College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 6 entries at probe")

RSSX(us_b50_mobap_edu, "us-b50-mobap-edu",
     "Missouri Baptist University", "Missouri Baptist University",
     "us_research", "science",
     "https://www.mobap.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Missouri Baptist University\",\"science\",\"batch:50\"]",
     10800,
     "Missouri Baptist University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_montevallo_edu, "us-b50-montevallo-edu",
     "University of Montevallo", "University of Montevallo",
     "us_research", "science",
     "https://www.montevallo.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Montevallo\",\"science\",\"batch:50\"]",
     10800,
     "University of Montevallo — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_mountmadonna_org, "us-b50-mountmadonna-org",
     "Mount Madonna Institute", "Mount Madonna Institute",
     "us_research", "science",
     "https://mountmadonna.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Mount Madonna Institute\",\"science\",\"batch:50\"]",
     10800,
     "Mount Madonna Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_mtsa_edu, "us-b50-mtsa-edu",
     "Middle Tennessee School of Anesthesia", "Middle Tennessee School of Anesthesia",
     "us_research", "science",
     "https://www.mtsa.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Middle Tennessee School of Anesthesia\",\"science\",\"batch:50\"]",
     10800,
     "Middle Tennessee School of Anesthesia — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_mtu_edu_mtu_resources_php_news_site_news_magazine, "us-b50-mtu-edu-mtu-resources-php-news-site-news-magazine",
     "Michigan Technological University — Michigan Tech News", "Michigan Technological University — Michigan Tech News",
     "us_research", "science",
     "https://www.mtu.edu/mtu_resources/php/news/rss/?site=news,magazine",
     "en", "[\"gov\",\"country:us\",\"publisher:Michigan Technological University — Michigan Tech News\",\"science\",\"batch:50\"]",
     10800,
     "Michigan Technological University — Michigan Tech News — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_mustedu_com, "us-b50-mustedu-com",
     "MUST University", "MUST University",
     "us_research", "science",
     "https://mustedu.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:MUST University\",\"science\",\"batch:50\"]",
     10800,
     "MUST University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_mvnu_edu, "us-b50-mvnu-edu",
     "Mount Vernon Nazarene University", "Mount Vernon Nazarene University",
     "us_research", "science",
     "https://mvnu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Mount Vernon Nazarene University\",\"science\",\"batch:50\"]",
     10800,
     "Mount Vernon Nazarene University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_na_edu, "us-b50-na-edu",
     "North American University", "North American University",
     "us_research", "science",
     "https://www.na.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:North American University\",\"science\",\"batch:50\"]",
     10800,
     "North American University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_nasa_gov_aeronautics, "us-b50-nasa-gov-aeronautics",
     "NASA Aeronautics", "NASA Aeronautics",
     "us_research", "science",
     "https://www.nasa.gov/aeronautics/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:NASA Aeronautics\",\"science\",\"batch:50\"]",
     10800,
     "NASA Aeronautics — science feed (10 entries at probe)")

RSSX(us_b50_nasa_gov_earth, "us-b50-nasa-gov-earth",
     "NASA Earth Science", "NASA Earth Science",
     "us_research", "science",
     "https://www.nasa.gov/earth/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:NASA Earth Science\",\"science\",\"batch:50\"]",
     10800,
     "NASA Earth Science — science feed (10 entries at probe)")

RSSX(us_b50_nasa_gov_humans_in_space, "us-b50-nasa-gov-humans-in-space",
     "NASA Humans in Space", "NASA Humans in Space",
     "us_research", "science",
     "https://www.nasa.gov/humans-in-space/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:NASA Humans in Space\",\"science\",\"batch:50\"]",
     10800,
     "NASA Humans in Space — science feed (10 entries at probe)")

RSSX(us_b50_nasa_gov_iotd_feed, "us-b50-nasa-gov-iotd-feed",
     "NASA Image of the Day", "NASA Image of the Day",
     "us_research", "science",
     "https://www.nasa.gov/feeds/iotd-feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:NASA Image of the Day\",\"science\",\"batch:50\"]",
     10800,
     "NASA Image of the Day — science feed (60 entries at probe)")

RSSX(us_b50_nasa_gov_learning_resources, "us-b50-nasa-gov-learning-resources",
     "NASA Learning", "NASA Learning",
     "us_research", "science",
     "https://www.nasa.gov/learning-resources/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:NASA Learning\",\"science\",\"batch:50\"]",
     10800,
     "NASA Learning — science feed (10 entries at probe)")

RSSX(us_b50_nasa_gov_missions, "us-b50-nasa-gov-missions",
     "NASA Missions", "NASA Missions",
     "us_research", "science",
     "https://www.nasa.gov/missions/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:NASA Missions\",\"science\",\"batch:50\"]",
     10800,
     "NASA Missions — science feed (10 entries at probe)")

RSSX(us_b50_nasa_gov_solar_system, "us-b50-nasa-gov-solar-system",
     "NASA Solar System", "NASA Solar System",
     "us_research", "science",
     "https://www.nasa.gov/solar-system/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:NASA Solar System\",\"science\",\"batch:50\"]",
     10800,
     "NASA Solar System — science feed (10 entries at probe)")

RSSX(us_b50_nasa_gov_technology, "us-b50-nasa-gov-technology",
     "NASA Technology", "NASA Technology",
     "us_research", "science",
     "https://www.nasa.gov/technology/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:NASA Technology\",\"science\",\"batch:50\"]",
     10800,
     "NASA Technology — science feed (10 entries at probe)")

RSSX(us_b50_nasa_gov_universe, "us-b50-nasa-gov-universe",
     "NASA Universe", "NASA Universe",
     "us_research", "science",
     "https://www.nasa.gov/universe/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:NASA Universe\",\"science\",\"batch:50\"]",
     10800,
     "NASA Universe — science feed (10 entries at probe)")

RSSX(us_b50_nationalaglawcenter_org, "us-b50-nationalaglawcenter-org",
     "National Agricultural Law Center", "National Agricultural Law Center",
     "us_research", "science",
     "https://nationalaglawcenter.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:National Agricultural Law Center\",\"science\",\"batch:50\"]",
     10800,
     "National Agricultural Law Center — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_natsci_uprrp_edu, "us-b50-natsci-uprrp-edu",
     "UPRRP College of Natural Sciences", "UPRRP College of Natural Sciences",
     "us_research", "science",
     "https://natsci.uprrp.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:UPRRP College of Natural Sciences\",\"science\",\"batch:50\"]",
     10800,
     "UPRRP College of Natural Sciences — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_ncmissouri_edu, "us-b50-ncmissouri-edu",
     "North Central Missouri College", "North Central Missouri College",
     "us_research", "science",
     "https://www.ncmissouri.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:North Central Missouri College\",\"science\",\"batch:50\"]",
     10800,
     "North Central Missouri College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_ndm_edu, "us-b50-ndm-edu",
     "Notre Dame of Maryland University", "Notre Dame of Maryland University",
     "us_research", "science",
     "https://ndm.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Notre Dame of Maryland University\",\"science\",\"batch:50\"]",
     10800,
     "Notre Dame of Maryland University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_nicholls_edu, "us-b50-nicholls-edu",
     "Nicholls State University", "Nicholls State University",
     "us_research", "science",
     "https://www.nicholls.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Nicholls State University\",\"science\",\"batch:50\"]",
     10800,
     "Nicholls State University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 8 entries at probe")

RSSX(us_b50_nist_gov_news_events_news, "us-b50-nist-gov-news-events-news",
     "NIST News", "NIST News",
     "us_research", "science",
     "https://www.nist.gov/news-events/news/rss.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:NIST News\",\"science\",\"batch:50\"]",
     10800,
     "NIST News — science feed (40 entries at probe)")

RSSX(us_b50_nmhu_edu, "us-b50-nmhu-edu",
     "New Mexico Highlands University", "New Mexico Highlands University",
     "us_research", "science",
     "https://www.nmhu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:New Mexico Highlands University\",\"science\",\"batch:50\"]",
     10800,
     "New Mexico Highlands University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_nnu_edu, "us-b50-nnu-edu",
     "Northwest Nazarene University", "Northwest Nazarene University",
     "us_research", "science",
     "https://nnu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Northwest Nazarene University\",\"science\",\"batch:50\"]",
     10800,
     "Northwest Nazarene University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_nsf_gov_rss_www_funding, "us-b50-nsf-gov-rss-www-funding",
     "NSF Funding Opportunities", "NSF Funding Opportunities",
     "us_research", "science",
     "https://www.nsf.gov/rss/rss_www_funding.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:NSF Funding Opportunities\",\"science\",\"batch:50\"]",
     10800,
     "NSF Funding Opportunities — science feed (20 entries at probe)")

RSSX(us_b50_nuhs_edu, "us-b50-nuhs-edu",
     "National University of Health Sciences", "National University of Health Sciences",
     "us_research", "science",
     "https://www.nuhs.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:National University of Health Sciences\",\"science\",\"batch:50\"]",
     10800,
     "National University of Health Sciences — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_nwhealth_edu, "us-b50-nwhealth-edu",
     "Northwestern Health Sciences University", "Northwestern Health Sciences University",
     "us_research", "science",
     "https://www.nwhealth.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Northwestern Health Sciences University\",\"science\",\"batch:50\"]",
     10800,
     "Northwestern Health Sciences University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_nwosu_edu, "us-b50-nwosu-edu",
     "Northwestern Oklahoma State University", "Northwestern Oklahoma State University",
     "us_research", "science",
     "https://www.nwosu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Northwestern Oklahoma State University\",\"science\",\"batch:50\"]",
     10800,
     "Northwestern Oklahoma State University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_oftc_edu, "us-b50-oftc-edu",
     "Oconee Fall Line Technical College", "Oconee Fall Line Technical College",
     "us_research", "science",
     "https://oftc.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Oconee Fall Line Technical College\",\"science\",\"batch:50\"]",
     10800,
     "Oconee Fall Line Technical College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_oglethorpe_edu, "us-b50-oglethorpe-edu",
     "Oglethorpe University", "Oglethorpe University",
     "us_research", "science",
     "https://oglethorpe.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Oglethorpe University\",\"science\",\"batch:50\"]",
     10800,
     "Oglethorpe University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_okwu_edu, "us-b50-okwu-edu",
     "Oklahoma Wesleyan University", "Oklahoma Wesleyan University",
     "us_research", "science",
     "https://www.okwu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Oklahoma Wesleyan University\",\"science\",\"batch:50\"]",
     10800,
     "Oklahoma Wesleyan University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_olivet_edu, "us-b50-olivet-edu",
     "Olivet Nazarene University", "Olivet Nazarene University",
     "us_research", "science",
     "https://www.olivet.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Olivet Nazarene University\",\"science\",\"batch:50\"]",
     10800,
     "Olivet Nazarene University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_orthodoxinstitute_org, "us-b50-orthodoxinstitute-org",
     "Patriarch Athenagoras Orthodox Institute", "Patriarch Athenagoras Orthodox Institute",
     "us_research", "science",
     "https://orthodoxinstitute.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Patriarch Athenagoras Orthodox Institute\",\"science\",\"batch:50\"]",
     10800,
     "Patriarch Athenagoras Orthodox Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 3 entries at probe")

RSSX(us_b50_paulquinn_edu, "us-b50-paulquinn-edu",
     "Paul Quinn College", "Paul Quinn College",
     "us_research", "science",
     "https://paulquinn.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Paul Quinn College\",\"science\",\"batch:50\"]",
     10800,
     "Paul Quinn College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 6 entries at probe")

RSSX(us_b50_peace_edu, "us-b50-peace-edu",
     "William Peace University", "William Peace University",
     "us_research", "science",
     "https://peace.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:William Peace University\",\"science\",\"batch:50\"]",
     10800,
     "William Peace University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_pensacolastate_edu, "us-b50-pensacolastate-edu",
     "Pensacola State College", "Pensacola State College",
     "us_research", "science",
     "https://pensacolastate.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Pensacola State College\",\"science\",\"batch:50\"]",
     10800,
     "Pensacola State College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_pims_edu, "us-b50-pims-edu",
     "Pittsburgh Institute of Mortuary Science", "Pittsburgh Institute of Mortuary Science",
     "us_research", "science",
     "https://pims.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Pittsburgh Institute of Mortuary Science\",\"science\",\"batch:50\"]",
     10800,
     "Pittsburgh Institute of Mortuary Science — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_plazacollege_edu, "us-b50-plazacollege-edu",
     "Plaza College", "Plaza College",
     "us_research", "science",
     "https://plazacollege.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Plaza College\",\"science\",\"batch:50\"]",
     10800,
     "Plaza College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_post_edu, "us-b50-post-edu",
     "Post University", "Post University",
     "us_research", "science",
     "https://post.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Post University\",\"science\",\"batch:50\"]",
     10800,
     "Post University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_potomac_edu, "us-b50-potomac-edu",
     "University of the Potomac", "University of the Potomac",
     "us_research", "science",
     "https://potomac.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of the Potomac\",\"science\",\"batch:50\"]",
     10800,
     "University of the Potomac — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_pvamu_edu, "us-b50-pvamu-edu",
     "Prairie View A&M University", "Prairie View A&M University",
     "us_research", "science",
     "https://www.pvamu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Prairie View A&M University\",\"science\",\"batch:50\"]",
     10800,
     "Prairie View A&M University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_queens_edu, "us-b50-queens-edu",
     "Queens University of Charlotte", "Queens University of Charlotte",
     "us_research", "science",
     "https://www.queens.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Queens University of Charlotte\",\"science\",\"batch:50\"]",
     10800,
     "Queens University of Charlotte — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_quincy_edu, "us-b50-quincy-edu",
     "Quincy University", "Quincy University",
     "us_research", "science",
     "https://www.quincy.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Quincy University\",\"science\",\"batch:50\"]",
     10800,
     "Quincy University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_rcu_edu, "us-b50-rcu-edu",
     "Rochester Christian University", "Rochester Christian University",
     "us_research", "science",
     "https://rcu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Rochester Christian University\",\"science\",\"batch:50\"]",
     10800,
     "Rochester Christian University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_redeemeruniversityead_com, "us-b50-redeemeruniversityead-com",
     "Redeemer University — Feed para Redeermer University »", "Redeemer University — Feed para Redeermer University »",
     "us_research", "science",
     "https://redeemeruniversityead.com/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Redeemer University — Feed para Redeermer University »\",\"science\",\"batch:50\"]",
     10800,
     "Redeemer University — Feed para Redeermer University » — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_regent_edu, "us-b50-regent-edu",
     "Regent University", "Regent University",
     "us_research", "science",
     "https://www.regent.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Regent University\",\"science\",\"batch:50\"]",
     10800,
     "Regent University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_research_ibm_com, "us-b50-research-ibm-com",
     "IBM Almaden Research Center", "IBM Almaden Research Center",
     "us_research", "science",
     "https://research.ibm.com/rss",
     "en", "[\"gov\",\"country:us\",\"publisher:IBM Almaden Research Center\",\"science\",\"batch:50\"]",
     10800,
     "IBM Almaden Research Center — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 20 entries at probe")

RSSX(us_b50_saintpeters_edu, "us-b50-saintpeters-edu",
     "Saint Peter's University", "Saint Peter's University",
     "us_research", "science",
     "https://www.saintpeters.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Saint Peter's University\",\"science\",\"batch:50\"]",
     10800,
     "Saint Peter's University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 25 entries at probe")

RSSX(us_b50_salemu_edu, "us-b50-salemu-edu",
     "Salem University", "Salem University",
     "us_research", "science",
     "https://www.salemu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Salem University\",\"science\",\"batch:50\"]",
     10800,
     "Salem University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_savannahstate_edu, "us-b50-savannahstate-edu",
     "Savannah State University", "Savannah State University",
     "us_research", "science",
     "https://savannahstate.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Savannah State University\",\"science\",\"batch:50\"]",
     10800,
     "Savannah State University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 12 entries at probe")

RSSX(us_b50_savannahtech_edu, "us-b50-savannahtech-edu",
     "Savannah Technical College", "Savannah Technical College",
     "us_research", "science",
     "https://www.savannahtech.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Savannah Technical College\",\"science\",\"batch:50\"]",
     10800,
     "Savannah Technical College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_science_utah_edu, "us-b50-science-utah-edu",
     "University of Utah College of Science", "University of Utah College of Science",
     "us_research", "science",
     "https://science.utah.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Utah College of Science\",\"science\",\"batch:50\"]",
     10800,
     "University of Utah College of Science — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_scitexas_edu, "us-b50-scitexas-edu",
     "Southern Careers Institute", "Southern Careers Institute",
     "us_research", "science",
     "https://scitexas.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Southern Careers Institute\",\"science\",\"batch:50\"]",
     10800,
     "Southern Careers Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_shorensteincenter_org, "us-b50-shorensteincenter-org",
     "Shorenstein Center on Media, Politics and Public Policy", "Shorenstein Center on Media, Politics and Public Policy",
     "us_research", "science",
     "https://shorensteincenter.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Shorenstein Center on Media, Politics and Public Policy\",\"science\",\"batch:50\"]",
     10800,
     "Shorenstein Center on Media, Politics and Public Policy — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(us_b50_sjmc_wisc_edu, "us-b50-sjmc-wisc-edu",
     "University of Wisconsin–Madison School of Journalism & Mass Communication", "University of Wisconsin–Madison School of Journalism & Mass Communication",
     "us_research", "science",
     "https://sjmc.wisc.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Wisconsin–Madison School of Journalism & Mass Communication\",\"science\",\"batch:50\"]",
     10800,
     "University of Wisconsin–Madison School of Journalism & Mass Communication — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 100 entries at probe")

RSSX(us_b50_sjvseminary_org, "us-b50-sjvseminary-org",
     "St. John Vianney Seminary", "St. John Vianney Seminary",
     "us_research", "science",
     "https://sjvseminary.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:St. John Vianney Seminary\",\"science\",\"batch:50\"]",
     10800,
     "St. John Vianney Seminary — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_southcoastcollege_edu, "us-b50-southcoastcollege-edu",
     "South Coast College", "South Coast College",
     "us_research", "science",
     "https://southcoastcollege.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:South Coast College\",\"science\",\"batch:50\"]",
     10800,
     "South Coast College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 6 entries at probe")

RSSX(us_b50_sph_cuny_edu, "us-b50-sph-cuny-edu",
     "CUNY School of Public Health", "CUNY School of Public Health",
     "us_research", "science",
     "https://sph.cuny.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:CUNY School of Public Health\",\"science\",\"batch:50\"]",
     10800,
     "CUNY School of Public Health — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_staugustine_edu, "us-b50-staugustine-edu",
     "St. Augustine College", "St. Augustine College",
     "us_research", "science",
     "https://www.staugustine.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:St. Augustine College\",\"science\",\"batch:50\"]",
     10800,
     "St. Augustine College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 12 entries at probe")

RSSX(us_b50_stfrancis_edu, "us-b50-stfrancis-edu",
     "University of St. Francis", "University of St. Francis",
     "us_research", "science",
     "https://stfrancis.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of St. Francis\",\"science\",\"batch:50\"]",
     10800,
     "University of St. Francis — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 12 entries at probe")

RSSX(us_b50_strausscenter_org, "us-b50-strausscenter-org",
     "Robert S. Strauss Center for International Security and Law", "Robert S. Strauss Center for International Security and Law",
     "us_research", "science",
     "https://www.strausscenter.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Robert S. Strauss Center for International Security and Law\",\"science\",\"batch:50\"]",
     10800,
     "Robert S. Strauss Center for International Security and Law — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(us_b50_stthom_edu, "us-b50-stthom-edu",
     "University of St. Thomas", "University of St. Thomas",
     "us_research", "science",
     "https://stthom.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of St. Thomas\",\"science\",\"batch:50\"]",
     10800,
     "University of St. Thomas — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 20 entries at probe")

RSSX(us_b50_stu_edu, "us-b50-stu-edu",
     "St. Thomas University", "St. Thomas University",
     "us_research", "science",
     "https://www.stu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:St. Thomas University\",\"science\",\"batch:50\"]",
     10800,
     "St. Thomas University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_su_edu, "us-b50-su-edu",
     "Shenandoah University", "Shenandoah University",
     "us_research", "science",
     "https://www.su.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Shenandoah University\",\"science\",\"batch:50\"]",
     10800,
     "Shenandoah University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 100 entries at probe")

RSSX(us_b50_sulross_edu, "us-b50-sulross-edu",
     "Sul Ross State University", "Sul Ross State University",
     "us_research", "science",
     "https://www.sulross.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Sul Ross State University\",\"science\",\"batch:50\"]",
     10800,
     "Sul Ross State University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_swcc_edu_blog_feed, "us-b50-swcc-edu-blog-feed",
     "Southwestern Christian College — Southwestern Christian College - RSS", "Southwestern Christian College — Southwestern Christian College - RSS",
     "us_research", "science",
     "https://www.swcc.edu/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Southwestern Christian College — Southwestern Christian College - RSS\",\"science\",\"batch:50\"]",
     10800,
     "Southwestern Christian College — Southwestern Christian College - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 3 entries at probe")

RSSX(us_b50_taft_edu, "us-b50-taft-edu",
     "William Howard Taft University", "William Howard Taft University",
     "us_research", "science",
     "https://taft.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:William Howard Taft University\",\"science\",\"batch:50\"]",
     10800,
     "William Howard Taft University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 3 entries at probe")

RSSX(us_b50_tricities_wsu_edu, "us-b50-tricities-wsu-edu",
     "Washington State University Tri-Cities", "Washington State University Tri-Cities",
     "us_research", "science",
     "https://tricities.wsu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Washington State University Tri-Cities\",\"science\",\"batch:50\"]",
     10800,
     "Washington State University Tri-Cities — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_uaustin_org, "us-b50-uaustin-org",
     "University of Austin", "University of Austin",
     "us_research", "science",
     "https://uaustin.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Austin\",\"science\",\"batch:50\"]",
     10800,
     "University of Austin — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 9 entries at probe")

RSSX(us_b50_uav_edu, "us-b50-uav-edu",
     "University of Antelope Valley", "University of Antelope Valley",
     "us_research", "science",
     "https://uav.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Antelope Valley\",\"science\",\"batch:50\"]",
     10800,
     "University of Antelope Valley — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 3 entries at probe")

RSSX(us_b50_uca_edu, "us-b50-uca-edu",
     "University of Central Arkansas", "University of Central Arkansas",
     "us_research", "science",
     "https://uca.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Central Arkansas\",\"science\",\"batch:50\"]",
     10800,
     "University of Central Arkansas — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_ucb_edu_pr, "us-b50-ucb-edu-pr",
     "Bayamón Central University", "Bayamón Central University",
     "us_research", "science",
     "https://ucb.edu.pr/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Bayamón Central University\",\"science\",\"batch:50\"]",
     10800,
     "Bayamón Central University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_uftl_edu, "us-b50-uftl-edu",
     "University of Fort Lauderdale", "University of Fort Lauderdale",
     "us_research", "science",
     "https://uftl.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Fort Lauderdale\",\"science\",\"batch:50\"]",
     10800,
     "University of Fort Lauderdale — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 4 entries at probe")

RSSX(us_b50_umw_edu, "us-b50-umw-edu",
     "University of Mary Washington", "University of Mary Washington",
     "us_research", "science",
     "https://www.umw.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Mary Washington\",\"science\",\"batch:50\"]",
     10800,
     "University of Mary Washington — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 6 entries at probe")

RSSX(us_b50_unva_edu, "us-b50-unva-edu",
     "University of Northern Virginia", "University of Northern Virginia",
     "us_research", "science",
     "https://unva.edu/index.php/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Northern Virginia\",\"science\",\"batch:50\"]",
     10800,
     "University of Northern Virginia — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 3 entries at probe")

RSSX(us_b50_upra_edu, "us-b50-upra-edu",
     "University of Puerto Rico at Arecibo", "University of Puerto Rico at Arecibo",
     "us_research", "science",
     "https://upra.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Puerto Rico at Arecibo\",\"science\",\"batch:50\"]",
     10800,
     "University of Puerto Rico at Arecibo — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_uprag_edu, "us-b50-uprag-edu",
     "University of Puerto Rico at Aguadilla", "University of Puerto Rico at Aguadilla",
     "us_research", "science",
     "https://www.uprag.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Puerto Rico at Aguadilla\",\"science\",\"batch:50\"]",
     10800,
     "University of Puerto Rico at Aguadilla — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_uprm_edu_portada, "us-b50-uprm-edu-portada",
     "University of Puerto Rico at Mayaguez — Recinto Universitario de Mayagüez de la Universidad de Puert", "University of Puerto Rico at Mayaguez — Recinto Universitario de Mayagüez de la Universidad de Puert",
     "us_research", "science",
     "https://www.uprm.edu/portada/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Puerto Rico at Mayaguez — Recinto Universitario de Mayagüez de la Universidad de Puert\",\"science\",\"batch:50\"]",
     10800,
     "University of Puerto Rico at Mayaguez — Recinto Universitario de Mayagüez de la Universidad de Puert — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_uprovidence_edu, "us-b50-uprovidence-edu",
     "University of Providence", "University of Providence",
     "us_research", "science",
     "https://www.uprovidence.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Providence\",\"science\",\"batch:50\"]",
     10800,
     "University of Providence — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 6 entries at probe")

RSSX(us_b50_uprp_edu, "us-b50-uprp-edu",
     "University of Puerto Rico at Ponce", "University of Puerto Rico at Ponce",
     "us_research", "science",
     "https://www.uprp.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Puerto Rico at Ponce\",\"science\",\"batch:50\"]",
     10800,
     "University of Puerto Rico at Ponce — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 25 entries at probe")

RSSX(us_b50_usgs_gov_news_all, "us-b50-usgs-gov-news-all",
     "USGS News", "USGS News",
     "us_research", "science",
     "https://www.usgs.gov/news/all/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:USGS News\",\"science\",\"batch:50\"]",
     10800,
     "USGS News — science feed (30 entries at probe)")

RSSX(us_b50_usw_edu_blog_feed, "us-b50-usw-edu-blog-feed",
     "University of the Southwest — USW - RSS", "University of the Southwest — USW - RSS",
     "us_research", "science",
     "https://www.usw.edu/blog-feed.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:University of the Southwest — USW - RSS\",\"science\",\"batch:50\"]",
     10800,
     "University of the Southwest — USW - RSS — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 14 entries at probe")

RSSX(us_b50_utdallas_edu, "us-b50-utdallas-edu",
     "University of Texas at Dallas", "University of Texas at Dallas",
     "us_research", "science",
     "https://utdallas.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Texas at Dallas\",\"science\",\"batch:50\"]",
     10800,
     "University of Texas at Dallas — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 200 entries at probe")

RSSX(us_b50_utulsa_edu, "us-b50-utulsa-edu",
     "University of Tulsa", "University of Tulsa",
     "us_research", "science",
     "https://utulsa.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Tulsa\",\"science\",\"batch:50\"]",
     10800,
     "University of Tulsa — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 2470 entries at probe")

RSSX(us_b50_uwest_edu, "us-b50-uwest-edu",
     "University of the West", "University of the West",
     "us_research", "science",
     "https://www.uwest.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of the West\",\"science\",\"batch:50\"]",
     10800,
     "University of the West — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 4 entries at probe")

RSSX(us_b50_uwex_wisconsin_edu_aas, "us-b50-uwex-wisconsin-edu-aas",
     "University of Wisconsin Colleges Online", "University of Wisconsin Colleges Online",
     "us_research", "science",
     "https://uwex.wisconsin.edu/aas/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Wisconsin Colleges Online\",\"science\",\"batch:50\"]",
     10800,
     "University of Wisconsin Colleges Online — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(us_b50_uwp_edu, "us-b50-uwp-edu",
     "University of Wisconsin–Parkside", "University of Wisconsin–Parkside",
     "us_research", "science",
     "https://www.uwp.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Wisconsin–Parkside\",\"science\",\"batch:50\"]",
     10800,
     "University of Wisconsin–Parkside — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_uwsuper_edu, "us-b50-uwsuper-edu",
     "University of Wisconsin-Superior", "University of Wisconsin-Superior",
     "us_research", "science",
     "https://uwsuper.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Wisconsin-Superior\",\"science\",\"batch:50\"]",
     10800,
     "University of Wisconsin-Superior — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_vcsu_edu, "us-b50-vcsu-edu",
     "Valley City State University", "Valley City State University",
     "us_research", "science",
     "https://www.vcsu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Valley City State University\",\"science\",\"batch:50\"]",
     10800,
     "Valley City State University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 30 entries at probe")

RSSX(us_b50_vermontstate_edu, "us-b50-vermontstate-edu",
     "Castleton University", "Castleton University",
     "us_research", "science",
     "https://vermontstate.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Castleton University\",\"science\",\"batch:50\"]",
     10800,
     "Castleton University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_virginiaconference_org, "us-b50-virginiaconference-org",
     "Virginia Mennonite Conference", "Virginia Mennonite Conference",
     "us_research", "science",
     "https://virginiaconference.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Virginia Mennonite Conference\",\"science\",\"batch:50\"]",
     10800,
     "Virginia Mennonite Conference — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_wallawalla_edu_news_type_9818, "us-b50-wallawalla-edu-news-type-9818",
     "Walla Walla University", "Walla Walla University",
     "us_research", "science",
     "https://www.wallawalla.edu/news?type=9818",
     "en", "[\"gov\",\"country:us\",\"publisher:Walla Walla University\",\"science\",\"batch:50\"]",
     10800,
     "Walla Walla University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 30 entries at probe")

RSSX(us_b50_wau_edu, "us-b50-wau-edu",
     "Washington Adventist University", "Washington Adventist University",
     "us_research", "science",
     "https://www.wau.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Washington Adventist University\",\"science\",\"batch:50\"]",
     10800,
     "Washington Adventist University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_web_svuca_edu, "us-b50-web-svuca-edu",
     "Silicon Valley University", "Silicon Valley University",
     "us_research", "science",
     "http://web.svuca.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Silicon Valley University\",\"science\",\"batch:50\"]",
     10800,
     "Silicon Valley University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(us_b50_web_svuca_edu_shop, "us-b50-web-svuca-edu-shop",
     "Silicon Valley University — Document Services » Products Feed", "Silicon Valley University — Document Services » Products Feed",
     "us_research", "science",
     "http://web.svuca.edu/shop/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Silicon Valley University — Document Services » Products Feed\",\"science\",\"batch:50\"]",
     10800,
     "Silicon Valley University — Document Services » Products Feed — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 2 entries at probe")

RSSX(us_b50_webb_edu, "us-b50-webb-edu",
     "Webb Institute", "Webb Institute",
     "us_research", "science",
     "https://www.webb.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Webb Institute\",\"science\",\"batch:50\"]",
     10800,
     "Webb Institute — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_webber_edu, "us-b50-webber-edu",
     "Webber International University", "Webber International University",
     "us_research", "science",
     "https://www.webber.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Webber International University\",\"science\",\"batch:50\"]",
     10800,
     "Webber International University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_welch_edu, "us-b50-welch-edu",
     "Welch College", "Welch College",
     "us_research", "science",
     "https://welch.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Welch College\",\"science\",\"batch:50\"]",
     10800,
     "Welch College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_westbrooku_edu, "us-b50-westbrooku-edu",
     "Westbrook University", "Westbrook University",
     "us_research", "science",
     "https://westbrooku.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Westbrook University\",\"science\",\"batch:50\"]",
     10800,
     "Westbrook University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(us_b50_westcoastuniversity_edu, "us-b50-westcoastuniversity-edu",
     "West Coast University", "West Coast University",
     "us_research", "science",
     "https://westcoastuniversity.edu/feed",
     "en", "[\"gov\",\"country:us\",\"publisher:West Coast University\",\"science\",\"batch:50\"]",
     10800,
     "West Coast University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_westoahu_hawaii_edu, "us-b50-westoahu-hawaii-edu",
     "University of Hawaiʻi–West Oʻahu", "University of Hawaiʻi–West Oʻahu",
     "us_research", "science",
     "https://westoahu.hawaii.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:University of Hawaiʻi–West Oʻahu\",\"science\",\"batch:50\"]",
     10800,
     "University of Hawaiʻi–West Oʻahu — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

RSSX(us_b50_williamwoods_edu, "us-b50-williamwoods-edu",
     "William Woods University", "William Woods University",
     "us_research", "science",
     "https://www.williamwoods.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:William Woods University\",\"science\",\"batch:50\"]",
     10800,
     "William Woods University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_wiltech_edu, "us-b50-wiltech-edu",
     "Williamsburg Technical College", "Williamsburg Technical College",
     "us_research", "science",
     "https://www.wiltech.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Williamsburg Technical College\",\"science\",\"batch:50\"]",
     10800,
     "Williamsburg Technical College — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 2 entries at probe")

RSSX(us_b50_wnmu_edu, "us-b50-wnmu-edu",
     "Western New Mexico University", "Western New Mexico University",
     "us_research", "science",
     "https://wnmu.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Western New Mexico University\",\"science\",\"batch:50\"]",
     10800,
     "Western New Mexico University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 10 entries at probe")

RSSX(us_b50_www2_trinitydc_edu, "us-b50-www2-trinitydc-edu",
     "Trinity Washington University", "Trinity Washington University",
     "us_research", "science",
     "https://www2.trinitydc.edu/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:Trinity Washington University\",\"science\",\"batch:50\"]",
     10800,
     "Trinity Washington University — RSS/Atom feed declared by the official website (Wikidata P856) of this science; 1 entries at probe")

/* ── us_state ────────────────────────────────────────────────────────── */

RSSX(us_b50_gov_ca_gov, "us-b50-gov-ca-gov",
     "California Governor News", "California Governor News",
     "us_state", "government",
     "https://www.gov.ca.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:California Governor News\",\"government\",\"batch:50\"]",
     3600,
     "California Governor News — state government feed (10 entries at probe)")

RSSX(us_b50_gov_georgia_gov, "us-b50-gov-georgia-gov",
     "Georgia Governor News", "Georgia Governor News",
     "us_state", "government",
     "https://gov.georgia.gov/rss.xml",
     "en", "[\"gov\",\"country:us\",\"publisher:Georgia Governor News\",\"government\",\"batch:50\"]",
     3600,
     "Georgia Governor News — state government feed (10 entries at probe)")

RSSX(us_b50_gov_texas_gov_news, "us-b50-gov-texas-gov-news",
     "Texas Governor News", "Texas Governor News",
     "us_state", "government",
     "https://gov.texas.gov/news/rss",
     "en", "[\"gov\",\"country:us\",\"publisher:Texas Governor News\",\"government\",\"batch:50\"]",
     3600,
     "Texas Governor News — state government feed (10 entries at probe)")

/* ── browser-profile hosts (restored 2026-10-21) ─────────────────────
 * Rejected at authoring for refusing every self-identifying agent; fetched
 * now with the per-host browser profile in core/host_agents.def, HA_BROWSER. */

RSSX(us_b50_cpo_noaa_gov, "us-b50-cpo-noaa-gov",
     "NOAA Climate Program Office", "NOAA Climate Program Office",
     "us_government", "government",
     "https://cpo.noaa.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:NOAA Climate Program Office\",\"government\",\"batch:50\"]",
     3600,
     "NOAA Climate Program Office — official feed; the host admits browser-profile requests only (core/host_agents.def, HA_BROWSER)")

RSSX(us_b50_globalocean_noaa_gov, "us-b50-globalocean-noaa-gov",
     "NOAA Global Ocean Monitoring and Observing Program", "NOAA Global Ocean Monitoring and Observing Program",
     "us_government", "government",
     "https://globalocean.noaa.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:NOAA Global Ocean Monitoring and Observing Program\",\"government\",\"batch:50\"]",
     3600,
     "NOAA Global Ocean Monitoring and Observing Program — official feed; the host admits browser-profile requests only (core/host_agents.def, HA_BROWSER)")

RSSX(us_b50_oss_research_noaa_gov, "us-b50-oss-research-noaa-gov",
     "NOAA Office of Science Support", "NOAA Office of Science Support",
     "us_government", "government",
     "https://oss.research.noaa.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:NOAA Office of Science Support\",\"government\",\"batch:50\"]",
     3600,
     "NOAA Office of Science Support — official feed; the host admits browser-profile requests only (core/host_agents.def, HA_BROWSER)")

RSSX(us_b50_wpo_noaa_gov, "us-b50-wpo-noaa-gov",
     "NOAA Weather Program Office", "NOAA Weather Program Office",
     "us_government", "government",
     "https://wpo.noaa.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:NOAA Weather Program Office\",\"government\",\"batch:50\"]",
     3600,
     "NOAA Weather Program Office — official feed; the host admits browser-profile requests only (core/host_agents.def, HA_BROWSER)")

RSSX(us_b50_noaa_gov_rss, "us-b50-noaa-gov-rss",
     "NOAA News", "NOAA News",
     "us_government", "government",
     "https://www.noaa.gov/rss",
     "en", "[\"gov\",\"country:us\",\"publisher:NOAA News\",\"government\",\"batch:50\"]",
     3600,
     "NOAA News — official feed; the host admits browser-profile requests only (core/host_agents.def, HA_BROWSER)")

RSSX(us_b50_bls_gov_bls_latest, "us-b50-bls-gov-bls-latest",
     "BLS — Latest Numbers (Major Economic Indicators)", "BLS — Latest Numbers (Major Economic Indicators)",
     "statistics", "statistics",
     "https://www.bls.gov/feed/bls_latest.rss",
     "en", "[\"gov\",\"country:us\",\"publisher:BLS — Latest Numbers (Major Economic Indicators)\",\"statistics\",\"batch:50\"]",
     3600,
     "BLS — Latest Numbers (Major Economic Indicators) — official feed; the host admits browser-profile requests only (core/host_agents.def, HA_BROWSER)")

RSSX(us_b50_bls_gov_cpi, "us-b50-bls-gov-cpi",
     "BLS — Consumer Price Index", "BLS — Consumer Price Index",
     "statistics", "statistics",
     "https://www.bls.gov/feed/cpi.rss",
     "en", "[\"gov\",\"country:us\",\"publisher:BLS — Consumer Price Index\",\"statistics\",\"batch:50\"]",
     3600,
     "BLS — Consumer Price Index — official feed; the host admits browser-profile requests only (core/host_agents.def, HA_BROWSER)")

RSSX(us_b50_diplomaticrooms_state_gov, "us-b50-diplomaticrooms-state-gov",
     "US Department of State — Office of Fine Arts (Diplomatic Reception Rooms)", "US Department of State — Office of Fine Arts (Diplomatic Reception Rooms)",
     "us_government", "government",
     "https://www.diplomaticrooms.state.gov/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:US Department of State — Office of Fine Arts (Diplomatic Reception Rooms)\",\"government\",\"batch:50\"]",
     3600,
     "US Department of State — Office of Fine Arts (Diplomatic Reception Rooms) — official feed; the host admits browser-profile requests only (core/host_agents.def, HA_BROWSER)")

RSSX(us_b50_jpl_nasa_gov_news, "us-b50-jpl-nasa-gov-news",
     "NASA JPL News", "NASA JPL News",
     "us_research", "science",
     "https://www.jpl.nasa.gov/feeds/news/",
     "en", "[\"gov\",\"country:us\",\"publisher:NASA JPL News\",\"science\",\"batch:50\"]",
     3600,
     "NASA JPL News — official feed; the host admits browser-profile requests only (core/host_agents.def, HA_BROWSER)")

RSSX(us_b50_sdcwa_org, "us-b50-sdcwa-org",
     "San Diego County Water Authority", "San Diego County Water Authority",
     "us_local", "government",
     "https://www.sdcwa.org/feed/",
     "en", "[\"gov\",\"country:us\",\"publisher:San Diego County Water Authority\",\"government\",\"batch:50\"]",
     3600,
     "San Diego County Water Authority — official feed; the host admits browser-profile requests only (core/host_agents.def, HA_BROWSER)")
