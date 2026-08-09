/* cert_metasploit_index.c — Metasploit Framework module metadata index: the
 * weaponisation lookup for a CVE (is there a working, ranked exploit module?).
 *
 * Endpoint: https://raw.githubusercontent.com/rapid7/metasploit-framework/
 *           master/db/modules_metadata_base.json   (keyless, ~11 MB)
 * Shape:    a JSON OBJECT keyed by module path — not an array. Each value
 *           carries name, fullname, rank, disclosure_date, type, author,
 *           description, references[] and targets[].
 *
 * SCOPE: the file holds ~7,100 modules. This collector emits the modules that
 * carry at least one CVE reference (capped at MSF_MAX_ROWS), because that is
 * the subset that answers "does this CVE have an exploit module" — the reason
 * the source is here. Modules with no CVE reference are skipped rather than
 * padded with an invented identifier.
 *
 * Emits: module fullname (remote_key), name, description, type, rank (raw
 * numeric plus the framework's own rank label), disclosure_date, author list,
 * the CVE ids parsed out of references[], and the first URL- reference as
 * `link`. Splitting "CVE-2007-4387" out of the fetched references array is
 * local computation over fetched bytes; no URL is assembled from a name (R1).
 *
 * R2: no geometry. R3: fetch/parse failure -> -1, otherwise 0.
 *
 * Licence: Metasploit Framework is BSD-licensed (metasploit_framework
 * licence); this metadata file ships in the repository. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSF_URL "https://raw.githubusercontent.com/rapid7/metasploit-framework/" \
                "master/db/modules_metadata_base.json"
#define MSF_MAX_ROWS 6000

/* Metasploit's own reliability scale, as documented in the framework. Decoding
 * a fetched numeric field, not inventing one. */
static const char *rank_label(double r) {
  int v = (int)r;
  if (v >= 600) return "Excellent";
  if (v >= 500) return "Great";
  if (v >= 400) return "Good";
  if (v >= 300) return "Normal";
  if (v >= 200) return "Average";
  if (v >= 100) return "Low";
  return "Manual";
}

static int run(const source_ctx *c, intel_sink *s) {
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(c->http, MSF_URL, hdrs, 90000);
  if (!doc) {
    fprintf(stderr, "[metasploit-module-index] fetch/parse failed\n");
    return -1;
  }
  int n = 0, seen = 0;
  cJSON *m;
  /* cJSON_ArrayForEach walks an object's children too; m->string is the key. */
  cJSON_ArrayForEach(m, doc) {
    if (n >= MSF_MAX_ROWS) break;
    if (!cJSON_IsObject(m)) continue;
    seen++;
    const char *full = jo_sv(m, "fullname");
    if (!full) full = m->string;
    const char *name = jo_sv(m, "name");
    if (!full || !name) continue;            /* nothing fetched -> no row (R1) */

    /* references[] mixes "CVE-…", "OSVDB-…", "URL-https://…", "EDB-…". */
    cJSON *refs = cJSON_GetObjectItem(m, "references");
    cJSON *cves = cJSON_CreateArray();
    const char *link = NULL;
    char cvelist[512];
    size_t cl = 0;
    cvelist[0] = 0;
    if (cJSON_IsArray(refs)) {
      cJSON *r;
      cJSON_ArrayForEach(r, refs) {
        if (!cJSON_IsString(r) || !r->valuestring) continue;
        const char *v = r->valuestring;
        if (!strncasecmp(v, "CVE-", 4)) {
          cJSON_AddItemToArray(cves, cJSON_CreateString(v));
          if (cl && cl + 2 < sizeof cvelist) { cvelist[cl++] = ','; cvelist[cl++] = ' '; }
          for (const char *q = v; *q && cl + 1 < sizeof cvelist; q++) cvelist[cl++] = *q;
          cvelist[cl] = 0;
        } else if (!link && !strncasecmp(v, "URL-", 4) && v[4]) {
          link = v + 4;                      /* a URL the metadata contained */
        }
      }
    }
    if (cJSON_GetArraySize(cves) == 0) {     /* see SCOPE in the header */
      cJSON_Delete(cves);
      continue;
    }

    const char *disc = jo_sv(m, "disclosure_date");
    const char *desc = jo_sv(m, "description");
    const char *type = jo_sv(m, "type");
    cJSON *rk = cJSON_GetObjectItem(m, "rank");

    char title[700];
    snprintf(title, sizeof title, "%s (%s%s%s)", name, full,
             cvelist[0] ? " / " : "", cvelist[0] ? cvelist : "");
    jo_utf8_trunc(title, 320);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "fullname", full);
    cJSON_AddStringToObject(p, "module_name", name);
    if (type) cJSON_AddStringToObject(p, "module_type", type);
    if (rk && cJSON_IsNumber(rk)) {
      cJSON_AddNumberToObject(p, "rank", rk->valuedouble);
      cJSON_AddStringToObject(p, "rank_label", rank_label(rk->valuedouble));
    }
    if (disc) cJSON_AddStringToObject(p, "disclosure_date", disc);
    if (link) cJSON_AddStringToObject(p, "reference_url", link);
    cJSON_AddItemToObject(p, "cves", cves);            /* ownership moves to p */
    cJSON *au = cJSON_GetObjectItem(m, "author");
    if (cJSON_IsArray(au)) cJSON_AddItemToObject(p, "author", cJSON_Duplicate(au, 1));
    cJSON *tg = cJSON_GetObjectItem(m, "targets");
    if (cJSON_IsArray(tg))
      cJSON_AddNumberToObject(p, "target_count", cJSON_GetArraySize(tg));
    cJSON_AddStringToObject(p, "source", "metasploit_modules_metadata");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    intel_item it = {0};
    it.remote_key      = full;
    it.title           = title;
    it.body            = desc;
    it.summary         = desc;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = jo_looks_iso(disc) ? disc : NULL;
    it.record_type     = "exploit-module";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"exploit\",\"cyber\",\"metasploit\",\"weaponisation\"]";
    if (s->emit(s, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[metasploit-module-index] emitted %d of %d modules "
                  "(CVE-referencing subset)\n", n, seen);
  return 0;                                     /* fetched fine (R3) */
}

static const source_def cert_metasploit_def = {
  .id = "metasploit-module-index", .collector = "cyber",
  .name = "Metasploit Framework Module Metadata Index",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "dataset", .url = MSF_URL,
  .description = "Metasploit modules with exploit rank, disclosure date, "
                 "targets and CVE/EDB references - the weaponisation index for "
                 "a CVE: does a working, ranked exploit module exist.",
  .license = "Metasploit Framework is BSD-licensed; the metadata file ships in "
             "the repository.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_metasploit_def)
