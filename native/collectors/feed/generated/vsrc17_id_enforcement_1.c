/* Verified-live id_enforcement sources (1), part 1.
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
VGEO(id_jakarta_green_space_violations, "id-jakarta-green-space-violations", "Jakarta Inventory of Spatial-Planning Violations in Green Zones (RTH)", "Jakarta Inventory of Spatial-Planning Violations in Green Zones (RTH)",
  "id_enforcement", "enforcement",
  "https://jakartasatu.jakarta.go.id/server/rest/services/Pelangggaran_Ruang/Inventarisasi_Pelanggaran_RTH/FeatureServer/0/query?where=1%3D1&outFields=*&returnGeometry=false&resultRecordCount=1000&f=geojson&orderByFields=OBJECTID%20ASC",
  "id", "[\"id\",\"enforcement\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Buildings inventoried as encroaching on designated green-belt zones. properties: OSM_ID/OSM_WAY_ID, ADDR_STREE, BUILDING, LUAS (area), X/Y, ID_WILAYAH/KECAMATAN/KELURAHAN, KODE_BLOK, SUB_ZONA, ZONA ('Zona Jalur Hijau'), KODE_OPS operational case code, KET_EV. A per-parcel enforcement case list.");
