/* collectors/sources/cyi_malpedia_families.c
 * Malpedia (Fraunhofer FKIE) malware family index — the canonical,
 * platform-prefixed malware family namespace used to normalise the family
 * strings arriving from ThreatFox, MalwareBazaar and vendor reports.
 * Endpoint: https://malpedia.caad.fkie.fraunhofer.de/api/list/families (keyless)
 * parse_notes: "Flat array of id strings; /api/get/family/<id> returns aliases,
 * references and attribution for one family (keyless). Diff against the
 * previous run to catch newly named families." Only the list endpoint is read
 * here; each row is a literal family id, split into its platform prefix and
 * family name (both taken from the id itself, nothing invented).
 * No coordinates -> has_geo 0 (R2).
 * Licence: Malpedia data is CC BY-SA 4.0 for the free tier; sample binaries are
 * restricted but this list endpoint is open.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://malpedia.caad.fkie.fraunhofer.de/api/list/families"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 30000);
  if (!doc) { fprintf(stderr, "[malpedia-families] fetch/parse failed\n"); return -1; }

  const cJSON *rows = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "families");

  int n = 0;
  const cJSON *e;
  cJSON_ArrayForEach(e, rows) {
    if (!cJSON_IsString(e) || !e->valuestring || !e->valuestring[0]) continue;
    const char *id = e->valuestring;

    /* ids are "<platform>.<name>"; both halves come from the id itself */
    char platform[32] = "";
    const char *dot = strchr(id, '.');
    if (dot && dot != id && (size_t)(dot - id) < sizeof platform)
      snprintf(platform, sizeof platform, "%.*s", (int)(dot - id), id);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "family_id", id);
    if (platform[0]) cJSON_AddStringToObject(p, "platform", platform);
    if (dot && dot[1]) cJSON_AddStringToObject(p, "family", dot + 1);
    cJSON_AddStringToObject(p, "namespace", "malpedia");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char link[256];
    snprintf(link, sizeof link,
             "https://malpedia.caad.fkie.fraunhofer.de/details/%s", id);
    char summary[128];
    snprintf(summary, sizeof summary, "Malpedia family%s%s",
             platform[0] ? " · platform " : "", platform[0] ? platform : "");

    intel_item it = {0};
    it.remote_key      = id;
    it.title           = id;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.record_type     = "malware-family";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"malware\",\"malpedia\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[malpedia-families] emitted %d\n", n);
  return 0;
}

static const source_def cyi_malpedia_families_def = {
  .id = "malpedia-families", .collector = "cyber",
  .name = "Malpedia malware family index",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "api", .url = CYI_URL,
  .description = "Fraunhofer FKIE's canonical, platform-prefixed malware family namespace — the naming authority for normalising family strings from other feeds.",
  .license = "Malpedia free tier — CC BY-SA 4.0; the list endpoint is open.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_malpedia_families_def)
