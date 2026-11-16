/* Verified-live eu_transport sources (2), part 1.
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
VGEO(eu_emodnet_ha_portlocations, "eu-emodnet-ha-portlocations", "EMODnet port locations", "EMODnet port locations",
  "eu_transport", "transport",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:portlocations&outputFormat=application/json&count=1000&startIndex=0",
  "en", "[\"eu\",\"transport\",\"batch16\",\"high-penetrancy\"]", 3600,
  "Port point registry with port_id (BE003, BEANR...), port and portname, country, coordinates, data source and a traffic flag. port_id is the join key into EMODnet's portgoods / portvessels / portpassengers throughput tables.");

VGEO(eu_emodnet_ha_wwshipwrecks, "eu-emodnet-ha-wwshipwrecks", "EMODnet worldwide shipwrecks", "EMODnet worldwide shipwrecks",
  "eu_transport", "transport",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:wwshipwrecks&outputFormat=application/json&count=1000&startIndex=0",
  "en", "[\"eu\",\"transport\",\"batch16\",\"high-penetrancy\"]", 3600,
  "Wreck register: wreck_id, wreck_cate (dangerous/non-dangerous), status, position and horizontal datum, depth with method/quality/accuracy, water_dept, water level regime, vessel name, type, flag state, length, width, draught and sonar dimensions.");
