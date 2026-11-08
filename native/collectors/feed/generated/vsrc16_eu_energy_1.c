/* Verified-live eu_energy sources (4), part 1.
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
VGEO(eu_emodnet_ha_hydrocarbons, "eu-emodnet-ha-hydrocarbons", "EMODnet offshore hydrocarbon wells", "EMODnet offshore hydrocarbon wells",
  "eu_energy", "energy",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:hydrocarbons&outputFormat=application/json&count=1000&startIndex=0",
  "en", "[\"eu\",\"energy\",\"batch16\",\"high-penetrancy\"]", 86400,
  "Every offshore well in European waters as a point: code, name, country, year, purpose (Exploration/Production), content (Natural Gas/Oil), status (Abandoned/Producing), named operator and company, water depth and distance to coast. Paginable with count/startIndex.");

VGEO(eu_emodnet_ha_nuclear, "eu-emodnet-ha-nuclear", "EMODnet coastal nuclear power plants", "EMODnet coastal nuclear power plants",
  "eu_energy", "energy",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:nuclear&outputFormat=application/json&count=1000&sortBy=reactor_nm&startIndex=0",
  "en", "[\"eu\",\"energy\",\"batch16\",\"high-penetrancy\"]", 86400,
  "Per-reactor records: site code and name, country, status (Operational / Permanently shut down), reactor count, thermal/gross/net capacity MW, reactor name, reactor type and model, fuel, cooling water source, named operator, NSSS vendor, start year.");

VGEO(eu_emodnet_ha_oenergy, "eu-emodnet-ha-oenergy", "EMODnet ocean energy projects", "EMODnet ocean energy projects",
  "eu_energy", "energy",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:oenergy&outputFormat=application/json&count=3",
  "en", "[\"eu\",\"energy\",\"batch16\",\"high-penetrancy\"]", 86400,
  "Wave and tidal energy installations as points with country, sea basin, project attributes and coordinates. Companion layer to oenergytests.");

VGEO(eu_emodnet_ha_platforms, "eu-emodnet-ha-platforms", "EMODnet offshore platforms", "EMODnet offshore platforms",
  "eu_energy", "energy",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:platforms&outputFormat=application/json&count=1000&startIndex=0",
  "en", "[\"eu\",\"energy\",\"batch16\",\"high-penetrancy\"]", 86400,
  "Offshore installations: platformid, name, country, current_status, category (Fixed steel etc.), function, named operator, location_blocks (licence block ids), primary_production, topside and substructure weights, production_start, valid_from/valid_to, water_depth and remarks.");
