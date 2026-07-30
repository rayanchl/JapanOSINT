/* collectors/osint/sources/hash_lookup.c
 * OSINT service — HASH_LOOKUP (osint_dispatcher.c service_registry[]).
 * On-demand (interval 0); ctx->entity = a file hash (MD5/SHA1/SHA256/SHA512).
 * Sources: MalwareBazaar (mb-api.abuse.ch, POST form, no key) + VirusTotal v3
 * files endpoint (only when VIRUSTOTAL_API_KEY is present; absent → that
 * enrichment is simply skipped, no fabricated note/row).
 *
 * PER-RECORD EMIT: emits ONE intel row keyed "hash:<hash>" ONLY when the hash
 * is actually found in MalwareBazaar and/or VirusTotal. title is the
 * MalwareBazaar/VT verdict (MALICIOUS / known file). body = the real
 * MalwareBazaar fields (+ VirusTotal block when keyed) plus the computed
 * assessment. No {success,confidence,data} envelope. Invalid hash, not-found,
 * or total fetch failure → emit nothing (return 0). The fixed
 * analysis_resources URL list has been removed. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

typedef enum { HASH_UNKNOWN=0, HASH_MD5, HASH_SHA1, HASH_SHA256, HASH_SHA512 } hash_type_t;

static hash_type_t detect_hash_type(const char *hash) {
  if (!hash) return HASH_UNKNOWN;
  size_t len = strlen(hash);
  for (size_t i = 0; i < len; i++)
    if (!isxdigit((unsigned char)hash[i])) return HASH_UNKNOWN;
  switch (len) {
    case 32: return HASH_MD5;
    case 40: return HASH_SHA1;
    case 64: return HASH_SHA256;
    case 128: return HASH_SHA512;
    default: return HASH_UNKNOWN;
  }
}
static const char *hash_type_name(hash_type_t t) {
  switch (t) {
    case HASH_MD5: return "MD5";
    case HASH_SHA1: return "SHA-1";
    case HASH_SHA256: return "SHA-256";
    case HASH_SHA512: return "SHA-512";
    default: return "Unknown";
  }
}

/* query_virustotal: key-gated; without VIRUSTOTAL_API_KEY → NULL (note). */
static cJSON *query_virustotal(http_client *http, const char *hash) {
  const char *api_key = getenv("VIRUSTOTAL_API_KEY");
  if (!api_key || !*api_key) return NULL;

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "VirusTotal");

  char hl[130];
  size_t len = strlen(hash);
  if (len >= sizeof hl) len = sizeof hl - 1;
  for (size_t i = 0; i < len; i++) hl[i] = (char)tolower((unsigned char)hash[i]);
  hl[len] = '\0';

  char url[512];
  snprintf(url, sizeof url, "https://www.virustotal.com/api/v3/files/%s", hl);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || !hr.body) {
    http_response_free(&hr);
    cJSON_AddStringToObject(result, "error", "Request failed");
    return result;
  }
  if (hr.status == 404) {
    http_response_free(&hr);
    cJSON_AddStringToObject(result, "status", "not_found");
    cJSON_AddStringToObject(result, "message",
      "Hash not found in VirusTotal database");
    return result;
  }
  if (hr.status != 200) {
    http_response_free(&hr);
    cJSON_AddStringToObject(result, "error", "API request failed");
    return result;
  }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json) { cJSON_AddStringToObject(result, "error", "Invalid JSON response"); return result; }

  cJSON *data = cJSON_GetObjectItem(json, "data");
  if (!data) {
    cJSON_Delete(json);
    cJSON_AddStringToObject(result, "error", "Unexpected response format");
    return result;
  }
  cJSON *attr = cJSON_GetObjectItem(data, "attributes");
  if (attr) {
    cJSON_AddStringToObject(result, "status", "found");
    cJSON *td = cJSON_GetObjectItem(attr, "type_description");
    if (td && cJSON_IsString(td)) cJSON_AddStringToObject(result, "file_type", td->valuestring);
    cJSON *size = cJSON_GetObjectItem(attr, "size");
    if (size && cJSON_IsNumber(size)) cJSON_AddNumberToObject(result, "file_size", size->valuedouble);
    cJSON *names = cJSON_GetObjectItem(attr, "names");
    if (names && cJSON_IsArray(names)) {
      cJSON *no = cJSON_CreateArray();
      int c = cJSON_GetArraySize(names);
      for (int i = 0; i < c && i < 10; i++) {
        cJSON *nm = cJSON_GetArrayItem(names, i);
        if (nm && cJSON_IsString(nm)) cJSON_AddItemToArray(no, cJSON_CreateString(nm->valuestring));
      }
      cJSON_AddItemToObject(result, "known_names", no);
    }
    cJSON *stats = cJSON_GetObjectItem(attr, "last_analysis_stats");
    if (stats) {
      cJSON *m = cJSON_GetObjectItem(stats, "malicious");
      cJSON *s = cJSON_GetObjectItem(stats, "suspicious");
      cJSON *u = cJSON_GetObjectItem(stats, "undetected");
      cJSON *h = cJSON_GetObjectItem(stats, "harmless");
      int mal = m ? (int)m->valuedouble : 0;
      int sus = s ? (int)s->valuedouble : 0;
      int und = u ? (int)u->valuedouble : 0;
      int har = h ? (int)h->valuedouble : 0;
      cJSON *det = cJSON_CreateObject();
      cJSON_AddNumberToObject(det, "malicious", mal);
      cJSON_AddNumberToObject(det, "suspicious", sus);
      cJSON_AddNumberToObject(det, "undetected", und);
      cJSON_AddNumberToObject(det, "harmless", har);
      cJSON_AddNumberToObject(det, "total", mal + sus + und + har);
      int total = mal + sus + und + har;
      if (total > 0)
        cJSON_AddNumberToObject(det, "detection_ratio",
          (double)(mal + sus) / total * 100);
      cJSON_AddItemToObject(result, "detection_stats", det);
      const char *tl;
      if (mal >= 10) tl = "HIGH";
      else if (mal >= 5 || sus >= 3) tl = "MEDIUM";
      else if (mal > 0 || sus > 0) tl = "LOW";
      else tl = "CLEAN";
      cJSON_AddStringToObject(result, "threat_level", tl);
    }
    cJSON *pop = cJSON_GetObjectItem(attr, "popular_threat_classification");
    if (pop) {
      cJSON *label = cJSON_GetObjectItem(pop, "suggested_threat_label");
      if (label && cJSON_IsString(label))
        cJSON_AddStringToObject(result, "threat_label", label->valuestring);
      cJSON *cat = cJSON_GetObjectItem(pop, "popular_threat_category");
      if (cat && cJSON_IsArray(cat) && cJSON_GetArraySize(cat) > 0) {
        cJSON *first = cJSON_GetArrayItem(cat, 0);  /* exhaustive-ok: display pick; threat_categories_all carries every category */
        cJSON_AddItemToObject(result, "threat_categories_all", cJSON_Duplicate(cat, 1));
        cJSON *val = cJSON_GetObjectItem(first, "value");
        if (val && cJSON_IsString(val))
          cJSON_AddStringToObject(result, "threat_category", val->valuestring);
      }
    }
    cJSON *md5 = cJSON_GetObjectItem(attr, "md5");
    cJSON *sha1 = cJSON_GetObjectItem(attr, "sha1");
    cJSON *sha256 = cJSON_GetObjectItem(attr, "sha256");
    cJSON *hashes = cJSON_CreateObject();
    if (md5 && cJSON_IsString(md5)) cJSON_AddStringToObject(hashes, "md5", md5->valuestring);
    if (sha1 && cJSON_IsString(sha1)) cJSON_AddStringToObject(hashes, "sha1", sha1->valuestring);
    if (sha256 && cJSON_IsString(sha256)) cJSON_AddStringToObject(hashes, "sha256", sha256->valuestring);
    cJSON_AddItemToObject(result, "hashes", hashes);
    cJSON *la = cJSON_GetObjectItem(attr, "last_analysis_date");
    if (la && cJSON_IsNumber(la)) cJSON_AddNumberToObject(result, "last_analysis_timestamp", la->valuedouble);
    cJSON *fs = cJSON_GetObjectItem(attr, "first_submission_date");
    if (fs && cJSON_IsNumber(fs)) cJSON_AddNumberToObject(result, "first_seen_timestamp", fs->valuedouble);
  }
  cJSON_Delete(json);
  return result;
}

/* query_malwarebazaar: POST query=get_info&hash=<h>, no key. */
static cJSON *query_malwarebazaar(http_client *http, const char *hash) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "MalwareBazaar");

  char post[256];
  snprintf(post, sizeof post, "query=get_info&hash=%s", hash);
  const char *hdrs[] = { "Content-Type: application/x-www-form-urlencoded", NULL };
  http_response hr = {0};
  int hc = http_request(http, "POST", "https://mb-api.abuse.ch/api/v1/",
                        hdrs, post, strlen(post), 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    http_response_free(&hr);
    cJSON_AddStringToObject(result, "error", "API request failed");
    return result;
  }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json) { cJSON_AddStringToObject(result, "error", "Invalid JSON response"); return result; }

  cJSON *qs = cJSON_GetObjectItem(json, "query_status");
  if (!qs || !cJSON_IsString(qs)) {
    cJSON_Delete(json);
    cJSON_AddStringToObject(result, "error", "Unexpected response");
    return result;
  }
  if (strcmp(qs->valuestring, "hash_not_found") == 0 ||
      strcmp(qs->valuestring, "no_results") == 0) {
    cJSON_Delete(json);
    cJSON_AddStringToObject(result, "status", "not_found");
    cJSON_AddStringToObject(result, "message", "Hash not found in MalwareBazaar");
    return result;
  }
  if (strcmp(qs->valuestring, "ok") != 0) {
    cJSON_AddStringToObject(result, "error", qs->valuestring);
    cJSON_Delete(json);
    return result;
  }
  cJSON *data = cJSON_GetObjectItem(json, "data");
  if (data && cJSON_IsArray(data) && cJSON_GetArraySize(data) > 0) {
    cJSON *first = cJSON_GetArrayItem(data, 0);  /* exhaustive-ok: hash lookup returns one sample object */
    if (first) {
      cJSON_AddStringToObject(result, "status", "found");
      cJSON *v;
      if ((v = cJSON_GetObjectItem(first, "sha256_hash")) && cJSON_IsString(v))
        cJSON_AddStringToObject(result, "sha256", v->valuestring);
      if ((v = cJSON_GetObjectItem(first, "sha1_hash")) && cJSON_IsString(v))
        cJSON_AddStringToObject(result, "sha1", v->valuestring);
      if ((v = cJSON_GetObjectItem(first, "md5_hash")) && cJSON_IsString(v))
        cJSON_AddStringToObject(result, "md5", v->valuestring);
      if ((v = cJSON_GetObjectItem(first, "file_type")) && cJSON_IsString(v))
        cJSON_AddStringToObject(result, "file_type", v->valuestring);
      if ((v = cJSON_GetObjectItem(first, "file_size")) && cJSON_IsNumber(v))
        cJSON_AddNumberToObject(result, "file_size", v->valuedouble);
      if ((v = cJSON_GetObjectItem(first, "signature")) && cJSON_IsString(v))
        cJSON_AddStringToObject(result, "malware_family", v->valuestring);
      cJSON *tags = cJSON_GetObjectItem(first, "tags");
      if (tags && cJSON_IsArray(tags))
        cJSON_AddItemToObject(result, "tags", cJSON_Duplicate(tags, 1));
      if ((v = cJSON_GetObjectItem(first, "first_seen")) && cJSON_IsString(v))
        cJSON_AddStringToObject(result, "first_seen", v->valuestring);
      if ((v = cJSON_GetObjectItem(first, "reporter")) && cJSON_IsString(v))
        cJSON_AddStringToObject(result, "reporter", v->valuestring);
      cJSON *intel = cJSON_GetObjectItem(first, "intelligence");
      if (intel) {
        cJSON *dl = cJSON_GetObjectItem(intel, "downloads");
        cJSON *ul = cJSON_GetObjectItem(intel, "uploads");
        if (dl && cJSON_IsString(dl)) cJSON_AddStringToObject(result, "download_count", dl->valuestring);
        if (ul && cJSON_IsString(ul)) cJSON_AddStringToObject(result, "upload_count", ul->valuestring);
      }
      cJSON *yara = cJSON_GetObjectItem(first, "yara_rules");
      if (yara && cJSON_IsArray(yara)) {
        cJSON *yr = cJSON_CreateArray();
        int c = cJSON_GetArraySize(yara);
        for (int i = 0; i < c && i < 10; i++) {
          cJSON *rule = cJSON_GetArrayItem(yara, i);
          cJSON *rn = cJSON_GetObjectItem(rule, "rule_name");
          if (rn && cJSON_IsString(rn)) cJSON_AddItemToArray(yr, cJSON_CreateString(rn->valuestring));
        }
        cJSON_AddItemToObject(result, "yara_matches", yr);
      }
    }
  }
  cJSON_Delete(json);
  return result;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hash = ctx->entity;
  if (!hash || !*hash) return 0;

  hash_type_t type = detect_hash_type(hash);
  if (type == HASH_UNKNOWN) return 0;   /* invalid hash → emit nothing */

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "hash", hash);
  cJSON_AddStringToObject(root, "hash_type", hash_type_name(type));

  /* VirusTotal only when keyed (NULL = skip enrichment, no fabricated note). */
  cJSON *vt = query_virustotal(ctx->http, hash);
  cJSON *mb = query_malwarebazaar(ctx->http, hash);

  int is_malicious = 0, is_found = 0;
  const char *threat_label = NULL;
  if (vt) {
    cJSON *st = cJSON_GetObjectItem(vt, "status");
    if (st && cJSON_IsString(st) && strcmp(st->valuestring, "found") == 0) {
      is_found = 1;
      cJSON *th = cJSON_GetObjectItem(vt, "threat_level");
      if (th && cJSON_IsString(th) &&
          (strcmp(th->valuestring, "HIGH") == 0 ||
           strcmp(th->valuestring, "MEDIUM") == 0))
        is_malicious = 1;
      cJSON *lb = cJSON_GetObjectItem(vt, "threat_label");
      if (lb && cJSON_IsString(lb)) threat_label = lb->valuestring;
    }
  }
  if (mb) {
    cJSON *st = cJSON_GetObjectItem(mb, "status");
    if (st && cJSON_IsString(st) && strcmp(st->valuestring, "found") == 0) {
      is_found = 1; is_malicious = 1;
      if (!threat_label) {
        cJSON *fam = cJSON_GetObjectItem(mb, "malware_family");
        if (fam && cJSON_IsString(fam)) threat_label = fam->valuestring;
      }
    }
  }

  /* Not found in any source → emit nothing (honest empty). */
  if (!is_found) {
    if (vt) cJSON_Delete(vt);
    if (mb) cJSON_Delete(mb);
    cJSON_Delete(root);
    return 0;
  }

  if (vt) cJSON_AddItemToObject(root, "virustotal", vt);
  if (mb) cJSON_AddItemToObject(root, "malwarebazaar", mb);

  cJSON *assess = cJSON_CreateObject();
  cJSON_AddBoolToObject(assess, "found", is_found);
  cJSON_AddBoolToObject(assess, "malicious", is_malicious);
  if (threat_label) cJSON_AddStringToObject(assess, "classification", threat_label);
  const char *verdict = is_malicious ? "MALICIOUS" : "POTENTIALLY_SAFE";
  cJSON_AddStringToObject(assess, "verdict", verdict);
  cJSON_AddItemToObject(root, "assessment", assess);

  char *bj = cJSON_PrintUnformatted(root);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "HASH_LOOKUP");
  cJSON_AddStringToObject(props, "entity", hash);
  cJSON_AddStringToObject(props, "verdict", verdict);
  if (threat_label) cJSON_AddStringToObject(props, "classification", threat_label);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "hash:%s", hash);
  char title[400];
  if (threat_label)
    snprintf(title, sizeof title, "%s — %s (%s)", hash, verdict, threat_label);
  else
    snprintf(title, sizeof title, "%s — %s", hash, verdict);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = is_malicious ? "malicious file hash" : "known file hash";
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"HASH_LOOKUP\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(root); cJSON_Delete(props);
  return rc >= 0 ? 0 : 0;
}

static const source_def hash_lookup_def = {
  .id = "HASH_LOOKUP", .collector = "osint",
  .name = "Hash Lookup", .name_ja = "ハッシュ照会",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/hash-lookup",
  .description = "Look up a file hash via MalwareBazaar and VirusTotal.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(hash_lookup_def)
