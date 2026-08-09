/* cert_cve_program.c — the CVE Program's own two public datasets.
 *
 *   cve-program-record-api  https://cveawg.mitre.org/api/cve/<CVE-ID>
 *       Authoritative CVE Record v5.1 straight from the CVE Program (not NVD's
 *       re-publication). ENTITY-PIVOT source: update_interval_sec = 0, driven by
 *       ctx->entity, which must be a CVE id (CVE-YYYY-NNNN...). With no entity,
 *       or an entity that is not a CVE id, it logs one line and returns 0 — it
 *       never invents a lookup row (R1/R6 style gating).
 *       Emits: cveId, assignerShortName, state, datePublished/dateUpdated, the
 *       CNA container title and description, CVSS baseScore + vectorString,
 *       CWE ids, affected vendor/product list, first reference URL.
 *
 *   cve-cna-directory       https://raw.githubusercontent.com/CVEProject/
 *                           cve-website/dev/src/assets/data/CNAsList.json
 *       All CVE Numbering Authorities with assignment scope, PSIRT contact,
 *       disclosure policy and advisory-feed URLs — the index that says which
 *       organisation owns a given CVE assignment and where its advisories live.
 *       Emits: shortName, cnaID, organizationName, scope, and the fetched
 *       contact / disclosurePolicy / securityAdvisories subtrees verbatim.
 *
 * Every URL stored is one the upstream document contained; none is constructed
 * from a name (R1). No geometry: a CNA is an organisation, not a place, and the
 * document publishes no coordinate (R2). Fetch failure -> -1; a fetch that
 * simply matched nothing -> 0 (R3).
 *
 * Keyless. Licence: CVE Program terms of use — free to use with attribution to
 * the CVE Program. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CNA_LIST_URL "https://raw.githubusercontent.com/CVEProject/cve-website/" \
                     "dev/src/assets/data/CNAsList.json"

/* First string value stored under object key `key` anywhere in the subtree.
 * The CNA list nests url/email a few levels deep and the exact nesting has
 * changed between revisions, so this reads whatever shape is actually served
 * instead of assuming one. Returns a pointer into the document. */
static const char *deep_str(cJSON *node, const char *key, int depth) {
  if (!node || depth > 6) return NULL;
  cJSON *e;
  cJSON_ArrayForEach(e, node) {
    if (e->string && !strcmp(e->string, key) && cJSON_IsString(e) &&
        e->valuestring && e->valuestring[0])
      return e->valuestring;
    if (cJSON_IsObject(e) || cJSON_IsArray(e)) {
      const char *r = deep_str(e, key, depth + 1);
      if (r) return r;
    }
  }
  return NULL;
}

/* ---- cve-program-record-api ---------------------------------------------- */

static int is_cve_id(const char *s) {
  if (!s) return 0;
  if (strncasecmp(s, "CVE-", 4) != 0) return 0;
  const char *p = s + 4;
  int d = 0;
  while (isdigit((unsigned char)*p)) { p++; d++; }
  if (d != 4 || *p != '-') return 0;
  p++; d = 0;
  while (isdigit((unsigned char)*p)) { p++; d++; }
  return d >= 4 && *p == '\0';
}

static int cve_record_run(const source_ctx *c, intel_sink *s) {
  if (!c->entity || !*c->entity || !is_cve_id(c->entity)) {
    fprintf(stderr, "[cve-program-record-api] no CVE id in ctx->entity; "
                    "nothing to look up\n");
    return 0;                        /* on-demand pivot, honest empty (R3) */
  }
  char cve[32];
  size_t j = 0;
  for (const char *p = c->entity; *p && j + 1 < sizeof cve; p++)
    cve[j++] = (char)toupper((unsigned char)*p);
  cve[j] = 0;

  char url[128];
  snprintf(url, sizeof url, "https://cveawg.mitre.org/api/cve/%s", cve);
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(c->http, url, hdrs, 20000);
  if (!doc) {
    fprintf(stderr, "[cve-program-record-api] fetch/parse failed for %s\n", cve);
    return -1;
  }

  cJSON *meta = cJSON_GetObjectItem(doc, "cveMetadata");
  cJSON *cont = cJSON_GetObjectItem(doc, "containers");
  cJSON *cna  = cont ? cJSON_GetObjectItem(cont, "cna") : NULL;
  const char *cve_id = meta ? jo_sv(meta, "cveId") : NULL;
  if (!cve_id) {                     /* no record body -> no row (R1) */
    cJSON_Delete(doc);
    fprintf(stderr, "[cve-program-record-api] %s: no cveMetadata.cveId\n", cve);
    return 0;
  }

  /* description: containers.cna.descriptions[] -> prefer lang "en" */
  const char *desc = NULL;
  cJSON *descs = cna ? cJSON_GetObjectItem(cna, "descriptions") : NULL;
  if (cJSON_IsArray(descs)) {
    cJSON *d;
    cJSON_ArrayForEach(d, descs) {
      const char *val = jo_sv(d, "value");
      if (!val) continue;
      const char *lang = jo_sv(d, "lang");
      if (!desc) desc = val;
      if (lang && !strncasecmp(lang, "en", 2)) { desc = val; break; }
    }
  }
  const char *cna_title = cna ? jo_sv(cna, "title") : NULL;

  char title[600];
  if (cna_title) snprintf(title, sizeof title, "%s: %s", cve_id, cna_title);
  else if (desc) snprintf(title, sizeof title, "%s: %s", cve_id, desc);
  else           snprintf(title, sizeof title, "%s", cve_id);
  jo_utf8_trunc(title, 320);

  const char *link = cna ? deep_str(cJSON_GetObjectItem(cna, "references"),
                                    "url", 0) : NULL;

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "cve_id", cve_id);
  if (meta) {
    const char *v;
    if ((v = jo_sv(meta, "assignerShortName"))) cJSON_AddStringToObject(p, "assigner", v);
    if ((v = jo_sv(meta, "assignerOrgId")))     cJSON_AddStringToObject(p, "assigner_org_id", v);
    if ((v = jo_sv(meta, "state")))             cJSON_AddStringToObject(p, "state", v);
    if ((v = jo_sv(meta, "datePublished")))     cJSON_AddStringToObject(p, "date_published", v);
    if ((v = jo_sv(meta, "dateUpdated")))       cJSON_AddStringToObject(p, "date_updated", v);
  }
  if (cna) {
    cJSON *metrics = cJSON_GetObjectItem(cna, "metrics");
    const char *vec = deep_str(metrics, "vectorString", 0);
    if (vec) cJSON_AddStringToObject(p, "cvss_vector", vec);
    /* baseScore is a number, so deep_str cannot reach it — walk the one level
     * the CVE 5.x schema actually uses. */
    if (cJSON_IsArray(metrics)) {
      cJSON *m;
      cJSON_ArrayForEach(m, metrics) {
        cJSON *kid;
        cJSON_ArrayForEach(kid, m) {
          cJSON *bs = cJSON_GetObjectItem(kid, "baseScore");
          if (bs && cJSON_IsNumber(bs)) {
            cJSON_AddNumberToObject(p, "cvss_base_score", bs->valuedouble);
            const char *sev = jo_sv(kid, "baseSeverity");
            if (sev) cJSON_AddStringToObject(p, "cvss_severity", sev);
            break;
          }
        }
        if (cJSON_GetObjectItem(p, "cvss_base_score")) break;
      }
    }
    cJSON *pt = cJSON_GetObjectItem(cna, "problemTypes");
    const char *cwe = deep_str(pt, "cweId", 0);
    if (cwe) cJSON_AddStringToObject(p, "cwe_id", cwe);
    cJSON *aff = cJSON_GetObjectItem(cna, "affected");
    if (cJSON_IsArray(aff)) {
      cJSON *list = cJSON_CreateArray();
      cJSON *a;
      cJSON_ArrayForEach(a, aff) {
        const char *ven = jo_sv(a, "vendor"), *pro = jo_sv(a, "product");
        if (!ven && !pro) continue;
        char buf[256];
        snprintf(buf, sizeof buf, "%s%s%s", ven ? ven : "",
                 (ven && pro) ? " " : "", pro ? pro : "");
        cJSON_AddItemToArray(list, cJSON_CreateString(buf));
      }
      cJSON_AddItemToObject(p, "affected", list);
    }
  }
  if (link) cJSON_AddStringToObject(p, "reference_url", link);
  cJSON_AddStringToObject(p, "source", "cve_program_cveawg");
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  intel_item it = {0};
  it.remote_key      = cve_id;
  it.title           = title;
  it.body            = desc;
  it.summary         = desc;
  it.link            = link;
  it.lang            = "en";
  it.published_at    = meta ? jo_sv(meta, "datePublished") : NULL;
  it.record_type     = "vulnerability";
  it.properties_json = pj ? pj : "{}";
  it.tags_json       = "[\"vulnerability\",\"cyber\",\"cve\",\"cve-program\"]";
  int n = (s->emit(s, &it) >= 0) ? 1 : 0;
  free(pj);
  cJSON_Delete(doc);
  fprintf(stderr, "[cve-program-record-api] %s emitted %d\n", cve, n);
  return 0;
}

/* ---- cve-cna-directory --------------------------------------------------- */

static int cna_dir_run(const source_ctx *c, intel_sink *s) {
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(c->http, CNA_LIST_URL, hdrs, 30000);
  if (!doc) {
    fprintf(stderr, "[cve-cna-directory] fetch/parse failed\n");
    return -1;
  }
  /* Bare JSON array at root, one object per CNA. */
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "data");
  int n = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *e;
    cJSON_ArrayForEach(e, arr) {
      const char *shortname = jo_sv(e, "shortName");
      const char *org       = jo_sv(e, "organizationName");
      const char *cna_id    = jo_sv(e, "cnaID");
      if (!shortname && !org) continue;      /* no fetched name -> no row (R1) */
      const char *scope     = jo_sv(e, "scope");

      char title[512];
      if (org && shortname) snprintf(title, sizeof title, "%s (CNA: %s)", org, shortname);
      else snprintf(title, sizeof title, "%s", org ? org : shortname);
      jo_utf8_trunc(title, 300);

      cJSON *advis  = cJSON_GetObjectItem(e, "securityAdvisories");
      cJSON *policy = cJSON_GetObjectItem(e, "disclosurePolicy");
      cJSON *contact= cJSON_GetObjectItem(e, "contact");
      const char *link = deep_str(advis, "url", 0);
      if (!link) link = deep_str(policy, "url", 0);
      const char *email = deep_str(contact, "emailAddr", 0);
      if (!email) email = deep_str(contact, "email", 0);

      cJSON *p = cJSON_CreateObject();
      if (shortname) cJSON_AddStringToObject(p, "short_name", shortname);
      if (cna_id)    cJSON_AddStringToObject(p, "cna_id", cna_id);
      if (org)       cJSON_AddStringToObject(p, "organization_name", org);
      if (scope)     cJSON_AddStringToObject(p, "scope", scope);
      if (email)     cJSON_AddStringToObject(p, "contact_email", email);
      if (link)      cJSON_AddStringToObject(p, "advisories_url", link);
      /* keep the fetched subtrees verbatim rather than flattening away data */
      if (advis)   cJSON_AddItemToObject(p, "security_advisories", cJSON_Duplicate(advis, 1));
      if (policy)  cJSON_AddItemToObject(p, "disclosure_policy",  cJSON_Duplicate(policy, 1));
      if (contact) cJSON_AddItemToObject(p, "contact",            cJSON_Duplicate(contact, 1));
      cJSON_AddStringToObject(p, "source", "cve_program_cna_list");
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      intel_item it = {0};
      it.remote_key      = cna_id ? cna_id : shortname;
      it.title           = title;
      it.body            = scope;
      it.summary         = scope;
      it.link            = link;
      it.lang            = "en";
      it.record_type     = "cna";
      it.properties_json = pj ? pj : "{}";
      it.tags_json       = "[\"cyber\",\"cve\",\"cna\",\"directory\"]";
      if (s->emit(s, &it) >= 0) n++;
      free(pj);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[cve-cna-directory] emitted %d\n", n);
  return 0;                              /* fetched fine (R3) */
}

static int run(const source_ctx *c, intel_sink *s) {
  if (!c || !c->source_id) return -1;
  if (!strcmp(c->source_id, "cve-program-record-api")) return cve_record_run(c, s);
  if (!strcmp(c->source_id, "cve-cna-directory"))      return cna_dir_run(c, s);
  fprintf(stderr, "[cert_cve_program] unknown source id %s\n", c->source_id);
  return -1;
}

static const source_def cert_cve_record_def = {
  .id = "cve-program-record-api", .collector = "cyber",
  .name = "CVE Program CVE Record API (cveawg)",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://cveawg.mitre.org/api/cve/",
  .description = "Authoritative CVE Record v5.1 straight from the CVE Program "
                 "(not NVD's re-publication): CNA container, assigner, affected "
                 "products, CVSS and references before NVD enrichment lands. "
                 "Entity pivot on a CVE id.",
  .license = "CVE Program terms of use: free to use with attribution to the "
             "CVE Program.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_cve_record_def)

static const source_def cert_cna_dir_def = {
  .id = "cve-cna-directory", .collector = "cyber",
  .name = "CVE Numbering Authority (CNA) Directory",
  .update_interval_sec = 604800, .run = run,
  .category = "cyber", .type = "dataset", .url = CNA_LIST_URL,
  .description = "Every CVE Numbering Authority with its assignment scope, "
                 "PSIRT contact address, disclosure policy and advisory-feed "
                 "URLs - the index that says who owns a CVE assignment and "
                 "where their advisories live.",
  .license = "CVE Program public data; reuse with attribution.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_cna_dir_def)
