/* ArcGIS Hub federated dataset search (Esri).
 * Endpoint: https://hub.arcgis.com/api/v3/datasets?page%5Bsize%5D=20
 * (the page[size] bracket MUST stay percent-encoded). JSON:API, so the payload
 * is data[] with {id, type, attributes}. Emits, per dataset, the attributes
 * the API returned - name, owner, access, source, type, recordCount, modified,
 * tags - plus the machine-queryable service URL taken from
 * attributes.additionalResources[].url when the Hub supplies one. Keyless.
 * Licence: the Hub search API is public and keyless; each dataset carries its
 * publishing agency licence, which is read per record (attributes.license)
 * and never assumed. */
#include "od_shared.c"

#define SID "arcgis-hub-datasets"
static const char *URL = "https://hub.arcgis.com/api/v3/datasets?page%5Bsize%5D=20";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 30000);
  if (!doc) return od_rc(SID, -1);
  const cJSON *arr = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); return od_rc(SID, -1); }

  int n = 0;
  const cJSON *d;
  cJSON_ArrayForEach(d, arr) {
    if (!cJSON_IsObject(d)) continue;
    const cJSON *at = cJSON_GetObjectItem(d, "attributes");
    if (!cJSON_IsObject(at)) continue;
    const char *name = od_s(at, "name");
    if (!name) name = od_s(at, "title");
    if (!name) continue;

    cJSON *props = cJSON_CreateObject();
    if (!props) continue;
    od_copy_scalars(props, at);
    od_put_s(props, "dataset_id", od_s(d, "id"));

    /* the queryable service endpoint, when the Hub returns one */
    const char *svc = od_s(at, "url");
    const cJSON *addl = cJSON_GetObjectItem(at, "additionalResources");
    if (cJSON_IsArray(addl)) {
      const cJSON *a;
      cJSON_ArrayForEach(a, addl) {
        const char *u = cJSON_IsObject(a) ? od_s(a, "url") : NULL;
        if (u) { od_put_s(props, "service_url", u); if (!svc) svc = u; break; }
      }
    }

    intel_item it = {0};
    it.title = name;
    it.summary = od_s(at, "owner");
    it.remote_key = od_s(d, "id");
    it.published_at = od_s(at, "modified");
    it.link = svc;
    it.record_type = "arcgis-hub-dataset";
    it.tags_json = "[\"opendata\",\"arcgis\",\"gis\"]";
    n += od_emit(sink, &it, props);
  }
  cJSON_Delete(doc);
  return od_rc(SID, n);
}

static const source_def od_arcgis_hub_datasets_def = {
  .id = SID, .collector = "government",
  .name = "ArcGIS Hub dataset search API",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://hub.arcgis.com/api/v3/datasets?page%5Bsize%5D=20",
  .description = "Esri ArcGIS Hub federated search across public ArcGIS Open Data sites worldwide, with each hit carrying its queryable service URL",
  .license = "Public keyless search API; per-dataset licence read from the record",
  .free_tier = 1,
};
REGISTER_SOURCE(od_arcgis_hub_datasets_def)
