/* collectors/sources/cyi_rdap_bootstrap_dns.c
 * IANA RDAP bootstrap registry for DNS: which RDAP server is authoritative for
 * each TLD.  Endpoint: https://data.iana.org/rdap/dns.json  (keyless)
 * Emits one row per TLD (parse_notes: "services is an array of 2-element arrays
 * of arrays: [[tld,...],[url,...]]. Emit one row per TLD, not per service
 * block."), carrying the TLD, every RDAP base URL published for it, and the
 * registry publication timestamp. No coordinates upstream -> has_geo 0 (R2).
 * Licence: IANA public registry, free to redistribute.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://data.iana.org/rdap/dns.json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 25000);
  if (!doc) { fprintf(stderr, "[rdap-bootstrap-dns] fetch/parse failed\n"); return -1; }

  const cJSON *pub = cJSON_GetObjectItem(doc, "publication");
  const char *published = (cJSON_IsString(pub) && pub->valuestring && pub->valuestring[0])
                            ? pub->valuestring : NULL;
  const cJSON *ver = cJSON_GetObjectItem(doc, "version");

  int n = 0;
  const cJSON *services = cJSON_GetObjectItem(doc, "services");
  const cJSON *svc;
  cJSON_ArrayForEach(svc, services) {
    if (!cJSON_IsArray(svc) || cJSON_GetArraySize(svc) < 2) continue;
    const cJSON *tlds = cJSON_GetArrayItem(svc, 0);
    const cJSON *urls = cJSON_GetArrayItem(svc, 1);
    if (!cJSON_IsArray(tlds) || !cJSON_IsArray(urls)) continue;

    /* the RDAP base URLs published for this service block */
    cJSON *urlarr = cJSON_CreateArray();
    const char *first_url = NULL;
    const cJSON *u;
    cJSON_ArrayForEach(u, urls) {
      if (cJSON_IsString(u) && u->valuestring && u->valuestring[0]) {
        cJSON_AddItemToArray(urlarr, cJSON_CreateString(u->valuestring));
        if (!first_url) first_url = u->valuestring;
      }
    }

    const cJSON *t;
    cJSON_ArrayForEach(t, tlds) {
      if (!cJSON_IsString(t) || !t->valuestring || !t->valuestring[0]) continue;
      const char *tld = t->valuestring;

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "tld", tld);
      cJSON_AddItemToObject(p, "rdap_urls", cJSON_Duplicate(urlarr, 1));
      if (published) cJSON_AddStringToObject(p, "publication", published);
      if (cJSON_IsString(ver) && ver->valuestring)
        cJSON_AddStringToObject(p, "registry_version", ver->valuestring);
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      char title[128];
      snprintf(title, sizeof title, ".%s", tld);
      char summary[512];
      snprintf(summary, sizeof summary, "RDAP: %s", first_url ? first_url : "(none published)");

      intel_item it = {0};
      it.remote_key      = tld;
      it.title           = title;
      it.summary         = summary;
      it.link            = first_url;
      it.lang            = "en";
      it.published_at    = published;
      it.record_type     = "rdap-bootstrap-tld";
      it.properties_json = pj;
      it.tags_json       = "[\"cyber\",\"rdap\",\"dns\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(pj);
    }
    cJSON_Delete(urlarr);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[rdap-bootstrap-dns] emitted %d\n", n);
  return 0;
}

static const source_def cyi_rdap_bootstrap_dns_def = {
  .id = "rdap-bootstrap-dns", .collector = "cyber",
  .name = "IANA RDAP bootstrap registry (DNS)",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "Maps every TLD to its authoritative RDAP server, and shows which TLDs still have no RDAP service at all.",
  .license = "IANA public registry, free to redistribute.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_rdap_bootstrap_dns_def)
