/* collectors/sources/cyi_misp_galaxy_threat_actors.c
 * MISP Galaxy threat-actor cluster — the community-maintained threat-actor name
 * graph that resolves "APT28 = Fancy Bear = Sofacy" so actor names from
 * different feeds join up.
 * Endpoint: https://raw.githubusercontent.com/MISP/misp-galaxy/main/clusters/
 *           threat-actor.json                                         (keyless)
 * parse_notes: "Rows are values[]; meta.synonyms is the alias list that makes
 * this useful, meta.country is the suspected sponsor (often absent — NEVER
 * invent it)." Accordingly meta.country is emitted only when upstream has it.
 * Emits: value (actor name), uuid, description, meta.synonyms[], meta.country,
 * meta.refs[], plus the cluster authors/version.
 * No coordinates -> has_geo 0 (R2): a suspected sponsor country is an
 * attribution string, never a map pin.
 * Licence: MISP Galaxy is CC0 / public domain.
 */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://raw.githubusercontent.com/MISP/misp-galaxy/main/clusters/threat-actor.json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 60000);
  if (!doc) { fprintf(stderr, "[misp-galaxy-threat-actors] fetch/parse failed\n"); return -1; }

  char authors[512] = "";
  const cJSON *au = cJSON_GetObjectItem(doc, "authors");
  if (cJSON_IsArray(au)) {
    const cJSON *a;
    cJSON_ArrayForEach(a, au) {
      if (!cJSON_IsString(a) || !a->valuestring) continue;
      size_t l = strlen(authors);
      snprintf(authors + l, sizeof authors - l, "%s%s", l ? ", " : "", a->valuestring);
    }
  }
  const cJSON *ver = cJSON_GetObjectItem(doc, "version");

  int n = 0;
  const cJSON *v;
  cJSON_ArrayForEach(v, cJSON_GetObjectItem(doc, "values")) {
    const char *name = jo_sv(v, "value");
    if (!name) continue;
    const char *uuid = jo_sv(v, "uuid");
    const char *desc = jo_sv(v, "description");
    const cJSON *meta = cJSON_GetObjectItem(v, "meta");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "actor", name);
    if (uuid) cJSON_AddStringToObject(p, "uuid", uuid);
    if (cJSON_IsObject(meta)) {
      const cJSON *syn = cJSON_GetObjectItem(meta, "synonyms");
      if (cJSON_IsArray(syn) && cJSON_GetArraySize(syn) > 0)
        cJSON_AddItemToObject(p, "synonyms", cJSON_Duplicate(syn, 1));
      const char *cc = jo_sv(meta, "country");
      if (cc) cJSON_AddStringToObject(p, "suspected_sponsor_country", cc);
      const cJSON *refs = cJSON_GetObjectItem(meta, "refs");
      if (cJSON_IsArray(refs) && cJSON_GetArraySize(refs) > 0)
        cJSON_AddItemToObject(p, "references", cJSON_Duplicate(refs, 1));
      const char *since = jo_sv(meta, "since");
      if (since) cJSON_AddStringToObject(p, "active_since", since);
    }
    if (authors[0]) cJSON_AddStringToObject(p, "cluster_authors", authors);
    if (cJSON_IsNumber(ver)) cJSON_AddNumberToObject(p, "cluster_version", ver->valuedouble);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    /* first reference, when upstream published one */
    const char *link = NULL;
    if (cJSON_IsObject(meta)) {
      const cJSON *refs = cJSON_GetObjectItem(meta, "refs");
      if (cJSON_IsArray(refs)) {
        const cJSON *r0 = cJSON_GetArrayItem(refs, 0);  /* exhaustive-ok: display link only — the whole refs array is kept above as properties.references */
        if (cJSON_IsString(r0) && r0->valuestring && r0->valuestring[0])
          link = r0->valuestring;
      }
    }

    char summary[512];
    const char *cc = cJSON_IsObject(meta) ? jo_sv(meta, "country") : NULL;
    snprintf(summary, sizeof summary, "Threat actor%s%s",
             cc ? " · suspected sponsor " : "", cc ? cc : "");

    intel_item it = {0};
    it.remote_key      = uuid ? uuid : name;
    it.title           = name;
    it.summary         = summary;
    it.body            = desc;
    it.link            = link;
    it.lang            = "en";
    it.record_type     = "threat-actor";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"threat-actor\",\"misp-galaxy\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[misp-galaxy-threat-actors] emitted %d\n", n);
  return 0;
}

static const source_def cyi_misp_galaxy_threat_actors_def = {
  .id = "misp-galaxy-threat-actors", .collector = "cyber",
  .name = "MISP Galaxy threat-actor cluster",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "The community-maintained threat-actor name graph: every APT alias mapped to one entity with suspected sponsor country and references.",
  .license = "MISP Galaxy — CC0 / public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_misp_galaxy_threat_actors_def)
