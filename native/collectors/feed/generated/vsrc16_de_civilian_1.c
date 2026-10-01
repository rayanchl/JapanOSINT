/* Verified-live de_civilian sources (3), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(global_osm_map_bbox, "global-osm-map-bbox", "OpenStreetMap raw map extract by bbox", "OpenStreetMap raw map extract by bbox",
  "de_civilian", "civilian",
  "https://api.openstreetmap.org/api/0.6/map.json?bbox=13.40,52.51,13.41,52.52",
  "elements",
  "en", "[\"de\",\"civilian\",\"batch16\",\"high-penetrancy\"]", 21600,
  "Every node, way and relation inside a bounding box with full tags plus per-element version, timestamp, changeset, user and uid. 600 KB for a 1 km square — a full local feature inventory with edit provenance, without touching Overpass.");

/* global-osm-node-history: every element in a history response carries the
 * SAME "id" (it is one node's history — 169 versions, 1 distinct id, live
 * 2026-09-06) and jsonlist keyed on it, so 168 of 169 versions were
 * overwritten every run. The record here is a VERSION, and "version" is
 * unique within one element's history (169 distinct), so key on it. */
VJSON_KEYED(global_osm_node_history, "global-osm-node-history", "OpenStreetMap element version history", "OpenStreetMap element version history",
  "de_civilian", "civilian",
  "https://api.openstreetmap.org/api/0.6/node/240109189/history.json",
  "elements",
  "en", "[\"de\",\"civilian\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 21600,
  "Every historical version of one element: version number, timestamp, changeset id, user and uid, and the complete tag set as it stood at that version. Shows who changed what about a feature and when.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.",
  "version");

VJSON(global_osm_node_relations, "global-osm-node-relations", "OpenStreetMap relations containing an element", "OpenStreetMap relations containing an element",
  "de_civilian", "civilian",
  "https://api.openstreetmap.org/api/0.6/node/240109189/relations.json",
  "elements",
  "en", "[\"de\",\"civilian\",\"batch16\",\"high-penetrancy\"]", 21600,
  "Every relation a node/way belongs to, with the relation's full member list, roles, tags, version, changeset and last editor. Upward hop from a point to the boundaries, routes and networks it is part of.");
