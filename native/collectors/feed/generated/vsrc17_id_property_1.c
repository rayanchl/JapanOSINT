/* Verified-live id_property sources (2), part 1.
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
VGEO(id_jakarta_land_nominative_list, "id-jakarta-land-nominative-list", "Jakarta Nominative Land Register (city land assets)", "Jakarta Nominative Land Register (city land assets)",
  "id_property", "property",
  "https://jakartasatu.jakarta.go.id/server/rest/services/Lahan_Daftar_Nominatif/FeatureServer/0/query?where=1%3D1&outFields=*&returnGeometry=false&resultRecordCount=1000&f=geojson&orderByFields=OBJECTID%20ASC",
  "id", "[\"id\",\"property\",\"batch17\",\"high-penetrancy\"]", 86400,
  "City-owned land parcels. properties: GUIDASET (asset GUID), KIB (asset book code), KODWIL, WADMKK/WADMKC/WADMKD administrative names and further asset columns. Asset-GUID pivot into Jakarta's asset books.");

VGEO(id_jakarta_sip_occupancy, "id-jakarta-sip-occupancy", "Jakarta SIP - Occupancy / Persil Register", "Jakarta SIP - Occupancy / Persil Register",
  "id_property", "property",
  "https://jakartasatu.jakarta.go.id/server/rest/services/DATA_SIP_JAKARTA/FeatureServer/0/query?where=1%3D1&outFields=*&returnGeometry=false&resultRecordCount=1000&f=geojson&orderByFields=OBJECTID%20ASC",
  "id", "[\"id\",\"property\",\"batch17\",\"high-penetrancy\"]", 86400,
  "Per-parcel occupancy register. properties: ALAMAT_RUMAH_PERSIL, NO_RUMAH, NO_FORM, NAMA_PENGHUNI (named occupant), PENGELUARAN, BERAKHIR (expiry) and related columns. Occupant-to-address pivot.");
