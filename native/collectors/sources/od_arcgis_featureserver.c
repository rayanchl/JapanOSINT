/* ArcGIS REST FeatureServer layer query - the pattern every ArcGIS Hub
 * dataset resolves to, wired to the verified public layer.
 * Endpoint: https://services.arcgis.com/ePKBjXrBZ2vEEgWd/arcgis/rest/services/
 *           Emails_to_the_City_Council_Hotline/FeatureServer/0/query
 *           ?where=1%3D1&outFields=*&resultRecordCount=100&f=json
 * (where=1=1 must stay percent-encoded.) Emits, per feature, every scalar in
 * features[].attributes exactly as the service returned it (for this layer:
 * SentFrom, SentTo, ReceivedDate, EmailSubject).
 * R2: this hosted table is NON-SPATIAL, and geometry is only read when the
 * service actually returns a point AND declares WGS84 (wkid 4326/4326-latest)
 * - projected coordinates are left alone rather than guessed at.
 * exceededTransferLimit is emitted in the properties so truncation is visible.
 * Keyless (public unauthenticated service).
 * Licence: layer-level licence comes from the owning agency Hub metadata; the
 * service itself is public and unauthenticated. */
#include "od_shared.c"

#define SID "arcgis-featureserver-query"
static const char *URL =
  "https://services.arcgis.com/ePKBjXrBZ2vEEgWd/arcgis/rest/services/"
  "Emails_to_the_City_Council_Hotline/FeatureServer/0/query"
  "?where=1%3D1&outFields=*&resultRecordCount=100&f=json";
static const char *const TITLE_KEYS[] = { "EmailSubject", "NAME", "Name",
                                          "name", "TITLE", "Title", NULL };

/* Only WGS84 point geometry is trusted as lat/lon (R2). */
static int wgs84(const cJSON *sr) {
  double w = 0;
  if (!cJSON_IsObject(sr)) return 0;
  if (od_numv(sr, "wkid", &w) && (int)w == 4326) return 1;
  if (od_numv(sr, "latestWkid", &w) && (int)w == 4326) return 1;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 30000);
  if (!doc) return od_rc(SID, -1);
  const cJSON *feats = cJSON_GetObjectItem(doc, "features");
  if (!cJSON_IsArray(feats)) { cJSON_Delete(doc); return od_rc(SID, -1); }
  const cJSON *rootsr = cJSON_GetObjectItem(doc, "spatialReference");
  const cJSON *exceeded = cJSON_GetObjectItem(doc, "exceededTransferLimit");

  int n = 0;
  const cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    const cJSON *at = cJSON_IsObject(f) ? cJSON_GetObjectItem(f, "attributes")
                                        : NULL;
    if (!cJSON_IsObject(at)) continue;
    char tb[256];
    const char *title = od_pick(at, TITLE_KEYS, tb, sizeof tb);
    char oidb[64];
    const char *oid = od_scalar(at, "OBJECTID", oidb, sizeof oidb);
    if (!oid) oid = od_scalar(at, "objectid", oidb, sizeof oidb);
    if (!title) title = oid;
    if (!title) continue;

    cJSON *props = cJSON_CreateObject();
    if (!props) continue;
    od_copy_scalars(props, at);
    if (cJSON_IsBool(exceeded))
      cJSON_AddBoolToObject(props, "exceededTransferLimit",
                            cJSON_IsTrue(exceeded));

    double la = 0, lo = 0;
    int geo = 0;
    const cJSON *g = cJSON_GetObjectItem(f, "geometry");
    if (cJSON_IsObject(g)) {
      const cJSON *sr = cJSON_GetObjectItem(g, "spatialReference");
      if (wgs84(sr) || (!cJSON_IsObject(sr) && wgs84(rootsr))) {
        if (od_numv(g, "y", &la) && od_numv(g, "x", &lo))
          geo = od_ll_ok(la, lo);
      }
    }

    intel_item it = {0};
    it.title = title;
    it.remote_key = oid;
    it.link = URL;
    it.record_type = "arcgis-feature";
    it.tags_json = "[\"opendata\",\"arcgis\",\"gis\"]";
    if (geo) { it.has_geo = 1; it.lat = la; it.lon = lo; }
    n += od_emit(sink, &it, props);
  }
  cJSON_Delete(doc);
  return od_rc(SID, n);
}

static const source_def od_arcgis_featureserver_def = {
  .id = SID, .collector = "government",
  .name = "ArcGIS REST FeatureServer query",
  .update_interval_sec = 21600, .run = run,
  .category = "government", .type = "api",
  .url = "https://services.arcgis.com/ePKBjXrBZ2vEEgWd/arcgis/rest/services/Emails_to_the_City_Council_Hotline/FeatureServer/0/query?where=1%3D1&outFields=*&resultRecordCount=100&f=json",
  .description = "Attribute rows from a public ArcGIS FeatureServer layer (Boulder city council hotline correspondence), the query pattern every Hub dataset resolves to",
  .license = "Public unauthenticated service; layer licence per owning agency",
  .free_tier = 1,
};
REGISTER_SOURCE(od_arcgis_featureserver_def)
