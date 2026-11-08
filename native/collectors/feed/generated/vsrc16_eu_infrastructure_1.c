/* Verified-live eu_infrastructure sources (5), part 1.
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
VGEO(eu_emodnet_ha_desalination, "eu-emodnet-ha-desalination", "EMODnet desalination plants", "EMODnet desalination plants",
  "eu_infrastructure", "infrastructure",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:desalination&outputFormat=application/json&count=1000&startIndex=0",
  "en", "[\"eu\",\"infrastructure\",\"batch16\",\"high-penetrancy\"]", 86400,
  "Desalination facilities: objectid, location and comm_name, country, NUTS3, project name, award_date and online_date, location_type, capacity_m3d, unit count and unit_size_m3d, technology (RO etc.), feedwater type, plant_status and customer_type.");

VGEO(eu_emodnet_ha_lighthouses, "eu-emodnet-ha-lighthouses", "EMODnet lighthouses", "EMODnet lighthouses",
  "eu_infrastructure", "infrastructure",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:lighthouses&outputFormat=application/json&count=1000&startIndex=0",
  "en", "[\"eu\",\"infrastructure\",\"batch16\",\"high-penetrancy\"]", 86400,
  "Navigational light structures with arlhs_number, name, country, status (removed/relocated/destroyed or not), distance_to_shore_m, position relative to coastline and comments.");

VGEO(eu_emodnet_ha_pipelines, "eu-emodnet-ha-pipelines", "EMODnet subsea pipelines", "EMODnet subsea pipelines",
  "eu_infrastructure", "infrastructure",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:pipelines&outputFormat=application/json&count=1000&startIndex=0",
  "en", "[\"eu\",\"infrastructure\",\"batch16\",\"high-penetrancy\"]", 86400,
  "Pipeline routes as MultiLineString with name, status, medium carried, operator, size_in (diameter), length_m, year, from_loc/to_loc endpoints, country code and source agency note.");

VJSON(ripe_atlas_probe_detail, "ripe-atlas-probe-detail", "RIPE Atlas probe detail", "RIPE Atlas probe detail",
  "eu_infrastructure", "infrastructure",
  "https://atlas.ripe.net/api/v2/probes/6001/",
  "tags",
  "en", "[\"eu\",\"infrastructure\",\"batch16\",\"high-penetrancy\"]", 86400,
  "One measurement probe: public IPv4/IPv6 address, asn_v4/asn_v6, announced prefix_v4/prefix_v6, country, description, firmware version, first_connected/last_connected epochs, status history, geometry point, and tags. Tree only had the probe list.");
