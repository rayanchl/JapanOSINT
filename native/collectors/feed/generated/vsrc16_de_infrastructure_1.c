/* Verified-live de_infrastructure sources (1), part 1.
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
VGEO(eu_emodnet_ha_bshcontiscables, "eu-emodnet-ha-bshcontiscables", "EMODnet German (BSH CONTIS) subsea cables", "EMODnet German (BSH CONTIS) subsea cables",
  "de_infrastructure", "infrastructure",
  "https://ows.emodnet-humanactivities.eu/wfs?service=WFS&version=2.0.0&request=GetFeature&typeName=emodnet:bshcontiscables&outputFormat=application/json&count=1000&startIndex=0",
  "en", "[\"de\",\"infrastructure\",\"batch16\",\"high-penetrancy\"]", 86400,
  "German Bight and Baltic cable corridors from the BSH CONTIS register, high-vertex MultiLineString geometry with the CONTIS attribute set.");
