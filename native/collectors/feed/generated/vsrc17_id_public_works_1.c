/* Verified-live id_public-works sources (1), part 1.
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
VGEO(id_jakarta_dsda_drainage_contractors, "id-jakarta-dsda-drainage-contractors", "Jakarta DSDA - Drainage Wells Built by Contracted Providers", "Jakarta DSDA - Drainage Wells Built by Contracted Providers",
  "id_public-works", "public-works",
  "https://jakartasatu.jakarta.go.id/server/rest/services/DSDA_DV_Perusahaan_Penyedia/FeatureServer/0/query?where=1%3D1&outFields=*&returnGeometry=false&resultRecordCount=1000&f=geojson&orderByFields=OBJECTID%20ASC",
  "id", "[\"id\",\"public-works\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Per-asset record of infiltration wells delivered by contracted providers. properties: IDDV (asset code 'DVJB - 04432'), LOKASI, WADMKK/WADMKC/WADMKD, LATITUDE/LONGITUDE, JMLTTK, dimensions, JNSSUMUR, SPESIFIKASI, PERUSAHAAN (contractor), BLNPKRJAAN (works month), FOTO evidence link, VERIF_ASET. Contractor-to-delivered-asset pivot.");
