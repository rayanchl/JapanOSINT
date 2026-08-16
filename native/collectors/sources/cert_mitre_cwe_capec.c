/* cert_mitre_cwe_capec.c — MITRE's two weakness/attack-pattern catalogues.
 *
 *   mitre-cwe-weakness-api  https://cwe-api.mitre.org/api/v1/cwe/weakness/<id[,id]>
 *       ENTITY-PIVOT (update_interval_sec = 0): turns a bare CWE id off a CVE
 *       record into the full weakness definition. ctx->entity may be "79",
 *       "CWE-79" or "CWE-79,CWE-89"; the digits are extracted and joined with
 *       commas (the API accepts a batch). With no usable entity it logs one
 *       line and returns 0 — no invented row.
 *       Emits from doc.Weaknesses[]: ID, Name, Abstraction, Structure, Status,
 *       Description, ExtendedDescription, and the RelatedWeaknesses CWE ids.
 *
 *   mitre-capec-catalog     https://capec.mitre.org/data/xml/capec_latest.xml
 *       The full CAPEC attack-pattern catalogue as XML (~3.8 MB, 615 patterns),
 *       each mapped to the CWEs it exploits, so an advisory's CWE can be turned
 *       into concrete adversary techniques. Monthly cadence.
 *       Emits per <Attack_Pattern>: @ID, @Name, @Abstraction, @Status, the
 *       <Description> text and the Related_Weakness CWE_ID list.
 *
 * The CAPEC document is XML, not JSON: parsed with a bounded tolerant scan
 * (open-tag attributes via lib/htmlparse html_attr, body text via html_strip)
 * rather than a DOM. Every field is copied out of the fetched bytes; nothing is
 * synthesized, and no lookup URL is constructed (R1).
 *
 * R2: no geometry. R3: fetch/parse failure -> -1; a fetch that yielded no
 * matching record -> 0.
 *
 * Keyless. Licence: MITRE CWE and CAPEC terms of use — free to use and
 * redistribute with attribution to MITRE. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPEC_URL "https://capec.mitre.org/data/xml/capec_latest.xml"

/* In-place decode of the five predefined XML entities. html_strip() removes
 * markup but leaves entities, so CAPEC names would otherwise persist as
 * "Fingerprinting &quot;Foo&quot;". */
static void xml_unescape(char *s) {
  char *o = s;
  for (char *r = s; *r; ) {
    if      (!strncmp(r, "&amp;",  5)) { *o++ = '&';  r += 5; }
    else if (!strncmp(r, "&lt;",   4)) { *o++ = '<';  r += 4; }
    else if (!strncmp(r, "&gt;",   4)) { *o++ = '>';  r += 4; }
    else if (!strncmp(r, "&quot;", 6)) { *o++ = '"';  r += 6; }
    else if (!strncmp(r, "&apos;", 6)) { *o++ = '\''; r += 6; }
    else if (!strncmp(r, "&#39;",  5)) { *o++ = '\''; r += 5; }
    else *o++ = *r++;
  }
  *o = 0;
}

/* ---- mitre-cwe-weakness-api ---------------------------------------------- */

/* "CWE-79, cwe-89" -> "79,89"; returns 0 when the entity holds no CWE number. */
static int cwe_ids_from_entity(const char *entity, char *out, size_t n) {
  out[0] = 0;
  if (!entity) return 0;
  size_t j = 0;
  int wrote = 0, in_num = 0;
  for (const char *p = entity; *p; p++) {
    if (isdigit((unsigned char)*p)) {
      if (!in_num && wrote && j + 1 < n) out[j++] = ',';
      in_num = 1; wrote = 1;
      if (j + 1 < n) out[j++] = *p;
    } else {
      in_num = 0;
    }
  }
  out[j] = 0;
  return j > 0;
}

static int cwe_run(const source_ctx *c, intel_sink *s) {
  char ids[128];
  if (!c->entity || !*c->entity || !cwe_ids_from_entity(c->entity, ids, sizeof ids)) {
    fprintf(stderr, "[mitre-cwe-weakness-api] no CWE id in ctx->entity; "
                    "nothing to look up\n");
    return 0;                          /* on-demand pivot, honest empty (R3) */
  }
  char url[256];
  snprintf(url, sizeof url, "https://cwe-api.mitre.org/api/v1/cwe/weakness/%s", ids);
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(c->http, url, hdrs, 20000);
  if (!doc) {
    fprintf(stderr, "[mitre-cwe-weakness-api] fetch/parse failed for %s\n", ids);
    return -1;
  }
  cJSON *arr = cJSON_GetObjectItem(doc, "Weaknesses");
  int n = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *w;
    cJSON_ArrayForEach(w, arr) {
      const char *id   = jo_sv(w, "ID");
      const char *name = jo_sv(w, "Name");
      if (!id || !name) continue;      /* no fetched definition -> no row (R1) */
      const char *desc = jo_sv(w, "Description");
      const char *ext  = jo_sv(w, "ExtendedDescription");

      char rk[64], title[512];
      snprintf(rk, sizeof rk, "CWE-%s", id);
      snprintf(title, sizeof title, "CWE-%s: %s", id, name);
      jo_utf8_trunc(title, 300);

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "cwe_id", rk);
      cJSON_AddStringToObject(p, "name", name);
      const char *v;
      if ((v = jo_sv(w, "Abstraction"))) cJSON_AddStringToObject(p, "abstraction", v);
      if ((v = jo_sv(w, "Structure")))   cJSON_AddStringToObject(p, "structure", v);
      if ((v = jo_sv(w, "Status")))      cJSON_AddStringToObject(p, "status", v);
      if (ext) cJSON_AddStringToObject(p, "extended_description", ext);
      cJSON *rel = cJSON_GetObjectItem(w, "RelatedWeaknesses");
      if (cJSON_IsArray(rel)) {
        cJSON *list = cJSON_CreateArray();
        cJSON *r;
        cJSON_ArrayForEach(r, rel) {
          const char *cid = jo_sv(r, "CweID");
          const char *nat = jo_sv(r, "Nature");
          if (!cid) continue;
          char buf[64];
          snprintf(buf, sizeof buf, "%s CWE-%s", nat ? nat : "Related", cid);
          cJSON_AddItemToArray(list, cJSON_CreateString(buf));
        }
        cJSON_AddItemToObject(p, "related_weaknesses", list);
      }
      cJSON_AddStringToObject(p, "source", "mitre_cwe_api");
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      intel_item it = {0};
      it.remote_key      = rk;
      it.title           = title;
      it.body            = desc;
      it.summary         = desc;
      it.lang            = "en";
      it.record_type     = "weakness";
      it.properties_json = pj ? pj : "{}";
      it.tags_json       = "[\"cyber\",\"cwe\",\"weakness\",\"mitre\"]";
      if (s->emit(s, &it) >= 0) n++;
      free(pj);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[mitre-cwe-weakness-api] %s emitted %d\n", ids, n);
  return 0;
}

/* ---- mitre-capec-catalog -------------------------------------------------- */

static int capec_run(const source_ctx *c, intel_sink *s) {
  char *xml = feed_get_text(c->http, CAPEC_URL, 60000);
  if (!xml) {
    fprintf(stderr, "[mitre-capec-catalog] fetch failed\n");
    return -1;
  }
  int n = 0;
  const char *p = xml;
  while ((p = strstr(p, "<Attack_Pattern ")) != NULL) {
    const char *gt = strchr(p, '>');
    if (!gt) break;
    char tag[2048];
    size_t tl = (size_t)(gt - p);
    if (tl >= sizeof tag) tl = sizeof tag - 1;
    memcpy(tag, p, tl);
    tag[tl] = 0;

    char id[32] = {0}, name[512] = {0}, abst[64] = {0}, status[64] = {0};
    html_attr(tag, "ID", id, sizeof id);
    html_attr(tag, "Name", name, sizeof name);
    html_attr(tag, "Abstraction", abst, sizeof abst);
    html_attr(tag, "Status", status, sizeof status);
    xml_unescape(name);

    const char *endp = strstr(gt, "</Attack_Pattern>");
    if (!endp) endp = gt + strlen(gt);

    /* <Description> may hold xhtml children; strip to text, bounded to the
     * current pattern block so a missing tag cannot read into the next one. */
    char *desc = NULL;
    const char *ds = strstr(gt, "<Description>");
    if (ds && ds < endp) {
      const char *de = strstr(ds, "</Description>");
      if (de && de < endp) {
        size_t dl = (size_t)(de - (ds + 13));
        char *raw = (char *)malloc(dl + 1);
        if (raw) {
          memcpy(raw, ds + 13, dl);
          raw[dl] = 0;
          desc = html_strip(raw);
          free(raw);
          if (desc) xml_unescape(desc);
        }
      }
    }

    if (id[0] && name[0]) {
      char rk[64], title[600];
      snprintf(rk, sizeof rk, "CAPEC-%s", id);
      snprintf(title, sizeof title, "CAPEC-%s: %s", id, name);
      jo_utf8_trunc(title, 320);

      cJSON *pr = cJSON_CreateObject();
      cJSON_AddStringToObject(pr, "capec_id", rk);
      cJSON_AddStringToObject(pr, "name", name);
      if (abst[0])   cJSON_AddStringToObject(pr, "abstraction", abst);
      if (status[0]) cJSON_AddStringToObject(pr, "status", status);
      /* Related_Weakness CWE_ID="NNN" occurrences inside this block only */
      cJSON *cwes = cJSON_CreateArray();
      for (const char *w = gt; (w = strstr(w, "CWE_ID=\"")) != NULL && w < endp; ) {
        w += 8;
        char num[16];
        size_t k = 0;
        while (isdigit((unsigned char)*w) && k + 1 < sizeof num) num[k++] = *w++;
        num[k] = 0;
        if (k) {
          char buf[32];
          snprintf(buf, sizeof buf, "CWE-%s", num);
          cJSON_AddItemToArray(cwes, cJSON_CreateString(buf));
        }
      }
      cJSON_AddItemToObject(pr, "related_cwes", cwes);
      cJSON_AddStringToObject(pr, "source", "mitre_capec");
      char *pj = cJSON_PrintUnformatted(pr);
      cJSON_Delete(pr);

      intel_item it = {0};
      it.remote_key      = rk;
      it.title           = title;
      it.body            = desc;
      it.summary         = desc;
      it.lang            = "en";
      it.record_type     = "attack-pattern";
      it.properties_json = pj ? pj : "{}";
      it.tags_json       = "[\"cyber\",\"capec\",\"attack-pattern\",\"mitre\"]";
      if (s->emit(s, &it) >= 0) n++;
      free(pj);
    }
    free(desc);
    p = (endp > gt) ? endp : gt + 1;
  }
  free(xml);
  fprintf(stderr, "[mitre-capec-catalog] emitted %d\n", n);
  return 0;                                        /* fetched fine (R3) */
}

static int run(const source_ctx *c, intel_sink *s) {
  if (!c || !c->source_id) return -1;
  if (!strcmp(c->source_id, "mitre-cwe-weakness-api")) return cwe_run(c, s);
  if (!strcmp(c->source_id, "mitre-capec-catalog"))    return capec_run(c, s);
  fprintf(stderr, "[cert_mitre_cwe_capec] unknown source id %s\n", c->source_id);
  return -1;
}

static const source_def cert_cwe_api_def = {
  .id = "mitre-cwe-weakness-api", .collector = "cyber",
  .name = "MITRE CWE Weakness API",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://cwe-api.mitre.org/api/v1/cwe/weakness/",
  .description = "Entity pivot that turns a bare CWE id from a CVE record into "
                 "the full weakness definition: name, abstraction level, "
                 "description, status and related weaknesses. Batch lookup via "
                 "comma-separated ids.",
  .license = "MITRE CWE Terms of Use: free to use and redistribute with "
             "attribution.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_cwe_api_def)

static const source_def cert_capec_def = {
  .id = "mitre-capec-catalog", .collector = "cyber",
  .name = "MITRE CAPEC Attack Pattern Catalog",
  .update_interval_sec = 2592000, .run = run,
  .category = "cyber", .type = "dataset", .url = CAPEC_URL,
  .description = "The full CAPEC attack-pattern catalogue as XML, each pattern "
                 "mapped to the CWEs it exploits - lets an advisory's CWE be "
                 "translated into concrete adversary techniques.",
  .license = "MITRE CAPEC Terms of Use: free to use and redistribute with "
             "attribution.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_capec_def)
