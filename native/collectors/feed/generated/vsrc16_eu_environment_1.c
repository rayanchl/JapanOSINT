/* Verified-live eu_environment sources (5), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

/* WFS rows in this file: `startIndex=0` (2026-11-07). geojson_emit_paged()
 * advances a WFS cursor only when the URL already names one AND the body's own
 * numberMatched/totalFeatures exceeds what was read (mechanism 3), so a row
 * without `startIndex` read ONE page of a declared total and stopped. Measured
 * per row (count=5 at startIndex 0 and 5): where the server accepted paging
 * without a sort — GeoServer does that only when it can order by the
 * feature type's primary key — the two pages were disjoint and the row gets
 * `startIndex=0` alone. Where it refused ("Cannot do natural order without a
 * primary key"), the row also gets `sortBy=<a property distinct on the
 * row's first 200 features>`, checked the same way (pages disjoint, total
 * unchanged; a bogus sortBy is HTTP 400 "Illegal property name", so the
 * server reads it). Probe-sized pages (count 2/3/20/50) are 1000 now. The
 * 20-page geojson ceiling still bounds a run; the remainder is disclosed. */
VGEO(eu_emodnet_ha_aggregates, "eu-emodnet-ha-aggregates", "EMODnet marine aggregate extraction", "EMODnet marine aggregate extraction",
  "eu_environment", "environment",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:aggregates&outputFormat=application/json&count=1000&startIndex=0",
  "en", "[\"eu\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "Sand and gravel extraction sites: areaid, extraction_area name, country, sea basin, year, material, area_of_activity_km2, permitted vs requested vs actually extracted amounts in m3 and tonnes, extraction_type and distance to coast. Licence-vs-actual volumes per named area.");

VGEO(eu_emodnet_ha_aquaculture, "eu-emodnet-ha-aquaculture", "EMODnet aquaculture zones", "EMODnet aquaculture zones",
  "eu_environment", "environment",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:aquaculture&outputFormat=application/json&count=1000&startIndex=0",
  "en", "[\"eu\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "Licensed aquaculture production areas as MultiPolygon with country, species group and zone attributes. 337 KB even at count=3 because of polygon density.");

VGEO(eu_emodnet_ha_dischargepoints, "eu-emodnet-ha-dischargepoints", "EMODnet waste water discharge points", "EMODnet waste water discharge points",
  "eu_environment", "environment",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:dischargepoints&outputFormat=application/json&count=1000&startIndex=0",
  "en", "[\"eu\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "Discharge outfalls: dcpcode, dcpname, country, NUTS3 region, state, year, the uwwcode of the plant discharging (join key to treatmentplants), receiving water body type and id, WFD river basin district, sensitive-area classification and source dataset version.");

VGEO(eu_emodnet_ha_munitions, "eu-emodnet-ha-munitions", "EMODnet dumped munitions", "EMODnet dumped munitions",
  "eu_environment", "environment",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:munitions&outputFormat=application/json&count=1000&startIndex=0",
  "en", "[\"eu\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "Located munitions on the European seabed: munition_type, free-text description (calibre, dimensions, chemical or not, condition, whether neutralised or blasted), reporting date and infoprov source (OSPAR), distance to coast.");

VGEO(eu_emodnet_ha_treatmentplants, "eu-emodnet-ha-treatmentplants", "EMODnet urban waste water treatment plants", "EMODnet urban waste water treatment plants",
  "eu_environment", "environment",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:treatmentplants&outputFormat=application/json&count=1000&startIndex=0",
  "en", "[\"eu\",\"environment\",\"batch16\",\"high-penetrancy\"]", 10800,
  "UWWTD plants: uwwcode, uwwname, country, status, capacit_pe, treatment level, nremoval/premoval flags, UV/chlorination/ozonation/sand/micro filtration flags, and compliance pass-fail per parameter (bod5_tp, cod_tp, tss_tp, ntot_tp, ptot_tp) with load in population equivalent. Regulatory compliance per named facility.");
