/* Verified-live africa_hazard sources (18), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* ArcGIS rows in this file: `orderByFields=<the layer's own objectIdField>
 * ASC` (2026-11-07). These walk on `resultOffset` (geojson_emit_paged's
 * mechanism 1, on the server's own exceededTransferLimit) with no order
 * clause, and ArcGIS promises no stable order without one. The field was READ
 * per layer from `<layer>?f=json` (objectIdField, else the esriFieldTypeOID
 * field — spellings here include OBJECTID, objectid, FID, OBJECTID_1, ObjectId,
 * oid, RowID), every layer declares supportsOrderBy, and the sorted query was
 * asked once (resultRecordCount=3 at offsets 0 and 3): HTTP 200, no error
 * body, pages disjoint. This is STABILISING, not a recovery — no record was
 * measured lost without it. */
VGEO(afr_ekur_ekurhuleni_ekurhuleni_firehydrants_fire_hydr_0, "afr-ekur-ekurhuleni-ekurhuleni-firehydrants-fire-hydr-0", "City of Ekurhuleni — fire hydrants", "City of Ekurhuleni — fire hydrants",
  "africa_hazard", "hazard",
  "https://gis.ekurhuleni.gov.za/arcgis/rest/services/Ekurhuleni/Ekurhuleni_FireHydrants/MapServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=500&f=geojson&orderByFields=OBJECTID%20ASC",
  "en", "[\"zaf\",\"ekurhuleni\",\"geospatial\",\"municipal\"]", 86400,
  "Fire hydrant asset locations across the City of Ekurhuleni.");

VRSS(afr_gdacs_eq, "afr-gdacs-eq", "GDACS — Earthquake alerts (24h)", "GDACS — Earthquake alerts (24h)",
  "africa_hazard", "hazard",
  "https://www.gdacs.org/xml/rss_eq_24h.xml",
  "en", "[\"africa\",\"mena\",\"earthquake\"]", 1800,
  "Global Disaster Alert and Coordination System earthquake alerts with magnitude, location and alert colour.");

VGEO(afr_grid3_exposure_profile_by_state_nga_exposure_profi_0, "afr-grid3-exposure-profile-by-state-nga-exposure-profi-0", "GRID3 — Nigeria exposure risk profile by state", "GRID3 — Nigeria exposure risk profile by state",
  "africa_hazard", "hazard",
  "https://services3.arcgis.com/BU6Aadhn6tbBEdyk/arcgis/rest/services/Exposure_Profile_by_state/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=500&f=geojson&orderByFields=OBJECTID%20ASC",
  "en", "[\"nga\",\"cod\",\"geospatial\",\"grid3\"]", 86400,
  "State-level exposure risk indicators used in Nigerian epidemic and disaster preparedness planning.");

VGEO(afr_grid3_grid3_nga_fire_stations_nga_fire_stations_gr_0, "afr-grid3-grid3-nga-fire-stations-nga-fire-stations-gr-0", "GRID3 — Nigeria fire stations", "GRID3 — Nigeria fire stations",
  "africa_hazard", "hazard",
  "https://services3.arcgis.com/BU6Aadhn6tbBEdyk/arcgis/rest/services/GRID3_NGA_Fire_Stations/FeatureServer/0/query?where=1%3D1&outFields=*&outSR=4326&resultRecordCount=500&f=geojson&orderByFields=FID%20ASC",
  "en", "[\"nga\",\"cod\",\"geospatial\",\"grid3\"]", 86400,
  "Geolocated Nigerian fire stations, useful for emergency-response coverage analysis.");

VJSON(afr_ibb_ds_cankurtaran_verisi_aebcd2, "afr-ibb-ds-cankurtaran-verisi-aebcd2", "Istanbul open data — lifeguard intervention records", "Istanbul open data — lifeguard intervention records",
  "africa_hazard", "hazard",
  "https://data.ibb.gov.tr/api/3/action/datastore_search?resource_id=aebcd207-3dbe-4637-875f-11695451e527&limit=100",
  "result.records",
  "tr", "[\"tur\",\"opendata\",\"datastore\",\"istanbul\"]", 21600,
  "Beach lifeguard rescue and intervention counts across Istanbul coastal districts.");

VJSON(afr_ibb_ds_itfai_istatistikler_yangin_794a5d, "afr-ibb-ds-itfai-istatistikler-yangin-794a5d", "Istanbul open data — fire incident statistics", "Istanbul open data — fire incident statistics",
  "africa_hazard", "hazard",
  "https://data.ibb.gov.tr/api/3/action/datastore_search?resource_id=794a5d20-915d-4b63-9a94-6b56ecf03de3&limit=100",
  "result.records",
  "tr", "[\"tur\",\"opendata\",\"datastore\",\"istanbul\"]", 64800,
  "Istanbul fire-brigade incident counts by district and cause.");

VJSON(afr_ibb_org_afet_isleri_dairesi_baskanligi, "afr-ibb-org-afet-isleri-dairesi-baskanligi", "Istanbul Metropolitan Municipality open data — Disaster Affairs Department", "Istanbul Metropolitan Municipality open data — Disaster Affairs Department",
  "africa_hazard", "hazard",
  "https://data.ibb.gov.tr/api/3/action/package_search?fq=organization:afet-isleri-dairesi-baskanligi&rows=50&sort=id%20asc",
  "result.results",
  "tr", "[\"tur\",\"opendata\",\"ckan\",\"istanbul\"]", 21600,
  "CKAN dataset listing for the Disaster Affairs Department on Istanbul's municipal open-data portal.");

VJSON(afr_ibb_org_deprem_risk_yonetimi_ve_kentsel_iy, "afr-ibb-org-deprem-risk-yonetimi-ve-kentsel-iy", "Istanbul Metropolitan Municipality open data — Earthquake Risk Management and Urban Improvement Department", "Istanbul Metropolitan Municipality open data — Earthquake Risk Management and Urban Improvement Department",
  "africa_hazard", "hazard",
  "https://data.ibb.gov.tr/api/3/action/package_search?fq=organization:deprem-risk-yonetimi-ve-kentsel-iyilestirme-daire-baskanligi&rows=50&sort=id%20asc",
  "result.results",
  "tr", "[\"tur\",\"opendata\",\"ckan\",\"istanbul\"]", 43200,
  "CKAN dataset listing for the Earthquake Risk Management and Urban Improvement Department on Istanbul's municipal open-data portal.");

VJSON(afr_ibb_org_itfaiye_daire_baskanligi, "afr-ibb-org-itfaiye-daire-baskanligi", "Istanbul Metropolitan Municipality open data — Istanbul Fire Brigade", "Istanbul Metropolitan Municipality open data — Istanbul Fire Brigade",
  "africa_hazard", "hazard",
  "https://data.ibb.gov.tr/api/3/action/package_search?fq=organization:itfaiye-daire-baskanligi&rows=50&sort=id%20asc",
  "result.results",
  "tr", "[\"tur\",\"opendata\",\"ckan\",\"istanbul\"]", 64800,
  "CKAN dataset listing for the Istanbul Fire Brigade on Istanbul's municipal open-data portal.");

VJSON(afr_ilgov_org_firefightingcommission, "afr-ilgov-org-firefightingcommission", "data.gov.il — National Fire and Rescue Authority datasets", "data.gov.il — National Fire and Rescue Authority datasets",
  "africa_hazard", "hazard",
  "https://data.gov.il/api/3/action/package_search?fq=organization:firefightingcommission&rows=50&sort=id%20asc",
  "result.results",
  "he", "[\"isr\",\"opendata\",\"ckan\"]", 21600,
  "CKAN dataset listing published by National Fire and Rescue Authority on Israel's national open-data portal.");

VJSON(afr_ilgov_org_meteorological_service, "afr-ilgov-org-meteorological-service", "data.gov.il — Israel Meteorological Service datasets", "data.gov.il — Israel Meteorological Service datasets",
  "africa_hazard", "hazard",
  "https://data.gov.il/api/3/action/package_search?fq=organization:meteorological_service&rows=50&sort=id%20asc",
  "result.results",
  "he", "[\"isr\",\"opendata\",\"ckan\"]", 43200,
  "CKAN dataset listing published by Israel Meteorological Service on Israel's national open-data portal.");

/* datastore_search rows below carry `sort=_id asc` (2026-11-07). An offset
 * walk with no declared order relies on the backend's default; CKAN's
 * datastore `_id` is its own sequential row id. Measured one or two rows per
 * host (izmir, data.gov.il, data.gov.au, QLD, VIC, Mauritius, Panama,
 * data.gov.sg): `_id desc` answers the opposite end, a bogus sort is refused
 * (409, or a 404 page on data.gov.il), and the total does not move. The
 * unsorted first page was already _id 1,2,3 on every one, so this is
 * STABILISING, not a recovery: 0 records were measured lost without it. */
VJSON(afr_izmir_ds_afet_ve_acil_durum_toplanma_e62687, "afr-izmir-ds-afet-ve-acil-durum-toplanma-e62687", "Izmir Metropolitan Municipality open data — disaster assembly areas", "Izmir Metropolitan Municipality open data — disaster assembly areas",
  "africa_hazard", "hazard",
  "https://acikveri.bizizmir.com/api/3/action/datastore_search?resource_id=e626876b-f121-4101-90c2-b6db03b13d6b&limit=100&sort=_id%20asc",
  "result.records",
  "tr", "[\"tur\",\"opendata\",\"ckan\",\"izmir\"]", 21600,
  "Izmir municipal open-data records for disaster assembly areas, published as CKAN JSON.");

VJSON(afr_izmir_grp_afet_ve_acil_durum_yonetimi, "afr-izmir-grp-afet-ve-acil-durum-yonetimi", "Izmir Metropolitan Municipality open data — disaster and emergency management theme", "Izmir Metropolitan Municipality open data — disaster and emergency management theme",
  "africa_hazard", "hazard",
  "https://acikveri.bizizmir.com/api/3/action/package_search?fq=groups:afet-ve-acil-durum-yonetimi&rows=50&sort=id%20asc",
  "result.results",
  "tr", "[\"tur\",\"opendata\",\"ckan\",\"izmir\"]", 28800,
  "Izmir municipal open-data records for disaster and emergency management theme, published as CKAN JSON.");

VJSON(afr_izmir_org_afet_isleri_daires, "afr-izmir-org-afet-isleri-daires", "Izmir Metropolitan Municipality open data — Disaster Affairs Department", "Izmir Metropolitan Municipality open data — Disaster Affairs Department",
  "africa_hazard", "hazard",
  "https://acikveri.bizizmir.com/api/3/action/package_search?fq=organization:afet-isleri-daires&rows=50&sort=id%20asc",
  "result.results",
  "tr", "[\"tur\",\"opendata\",\"ckan\",\"izmir\"]", 86400,
  "Izmir municipal open-data records for Disaster Affairs Department, published as CKAN JSON.");

VJSON(afr_izmir_org_itfaiye_dairesi_baskanligi, "afr-izmir-org-itfaiye-dairesi-baskanligi", "Izmir Metropolitan Municipality open data — Izmir Fire Brigade", "Izmir Metropolitan Municipality open data — Izmir Fire Brigade",
  "africa_hazard", "hazard",
  "https://acikveri.bizizmir.com/api/3/action/package_search?fq=organization:itfaiye-dairesi-baskanligi&rows=50&sort=id%20asc",
  "result.results",
  "tr", "[\"tur\",\"opendata\",\"ckan\",\"izmir\"]", 43200,
  "Izmir municipal open-data records for Izmir Fire Brigade, published as CKAN JSON.");

VJSON(afr_qa_deaths_and_injuries_in_traffic_acciden, "afr-qa-deaths-and-injuries-in-traffic-acciden", "data.gov.qa — deaths and injuries in traffic accidents", "data.gov.qa — deaths and injuries in traffic accidents",
  "africa_hazard", "hazard",
  "https://www.data.gov.qa/api/explore/v2.1/catalog/datasets/deaths-and-injuries-in-traffic-accidents-by-age-group-gender-and-role-of-injured/records?limit=50",
  "results",
  "en", "[\"qat\",\"opendata\",\"opendatasoft\"]", 64800,
  "Qatar national open-data records for deaths and injuries in traffic accidents, served as Opendatasoft JSON.");

VJSON(afr_qa_sdg_11_5_3_damage_to_critical_infrastr, "afr-qa-sdg-11-5-3-damage-to-critical-infrastr", "data.gov.qa — SDG 11.5.3 damage to critical infrastructure from disasters", "data.gov.qa — SDG 11.5.3 damage to critical infrastructure from disasters",
  "africa_hazard", "hazard",
  "https://www.data.gov.qa/api/explore/v2.1/catalog/datasets/sdg-11-5-3-damage-to-critical-infrastructure-and-number-of-disruptions-to-basic-services-attributed/records?limit=50",
  "results",
  "en", "[\"qat\",\"opendata\",\"opendatasoft\"]", 64800,
  "Qatar national open-data records for SDG 11.5.3 damage to critical infrastructure from disasters, served as Opendatasoft JSON.");

VRSS(afr_za_cgs_eq, "afr-za-cgs-eq", "Council for Geoscience South Africa — News and seismic notices", "Council for Geoscience South Africa — News and seismic notices",
  "africa_hazard", "hazard",
  "https://www.geoscience.org.za/feed/",
  "en", "[\"zaf\",\"seismic\"]", 21600,
  "CGS bulletins covering South African seismic events, mine tremors and geohazard studies.");
