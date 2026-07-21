/* collectors/osint/sources/data_extractor.c
 * OSINT service — faithful port of OSINTsaas osint_tools/data_extractor.c
 * (data_extract / handle_data_extractor). Canonical SERVICE id in
 * osint_dispatcher.c: {SERVICE_IOC_CHECKER, handle_data_extractor,
 * "DATA_EXTRACTOR", true} (alias IOC_LOOKUP maps to a different handler →
 * not this file). Entity = free text; pure-compute POSIX-regex extraction of
 * emails/IPv4/urls/domains/hashes/btc/eth/mac/phones/cards/ssns. No network,
 * no key.
 *
 * PER-RECORD EMIT: emits ONE intel_item per UNIQUE extracted indicator
 * (remote_key="ioc:<type>:<value>"), deduped within the run across all types.
 * Each row's body is {type,value}; title "<type>: <value>"; summary=type.
 * Nothing extracted → emits nothing, returns 0 (honest empty). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include <ctype.h>
#include <regex.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_MATCHES 100

typedef struct { const char *name, *pattern; } xp_t;

/* POSIX ERE patterns — identical to OSINTsaas PATTERNS[]. */
static const xp_t PATTERNS[] = {
  {"email",       "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}"},
  {"ipv4",        "[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}"},
  {"url",         "https?://[a-zA-Z0-9./?=_&%-]+"},
  {"domain",      "[a-zA-Z0-9][a-zA-Z0-9-]*\\.[a-zA-Z]{2,}"},
  {"md5",         "[a-fA-F0-9]{32}"},
  {"sha1",        "[a-fA-F0-9]{40}"},
  {"sha256",      "[a-fA-F0-9]{64}"},
  {"bitcoin",     "[13][a-km-zA-HJ-NP-Z1-9]{25,34}"},
  {"ethereum",    "0x[a-fA-F0-9]{40}"},
  {"mac_address", "[0-9a-fA-F]{2}[:-][0-9a-fA-F]{2}[:-][0-9a-fA-F]{2}[:-][0-9a-fA-F]{2}[:-][0-9a-fA-F]{2}[:-][0-9a-fA-F]{2}"},
  {"phone_us",    "\\+?1?[-. ]?\\(?[0-9]{3}\\)?[-. ]?[0-9]{3}[-. ]?[0-9]{4}"},
  {"credit_card", "[0-9]{4}[-. ]?[0-9]{4}[-. ]?[0-9]{4}[-. ]?[0-9]{4}"},
  {"ssn",         "[0-9]{3}[-. ]?[0-9]{2}[-. ]?[0-9]{4}"},
  {NULL, NULL}
};

static int is_valid_ipv4(const char *s) {
  int n[4];
  if (sscanf(s, "%d.%d.%d.%d", &n[0], &n[1], &n[2], &n[3]) != 4) return 0;
  for (int i = 0; i < 4; i++) if (n[i] < 0 || n[i] > 255) return 0;
  return 1;
}

static int luhn_check(const char *number) {
  char digits[20]; int j = 0;
  for (size_t i = 0; number[i] && j < 19; i++)
    if (isdigit((unsigned char)number[i])) digits[j++] = number[i];
  digits[j] = 0;
  int len = (int)strlen(digits);
  if (len < 13 || len > 19) return 0;
  int sum = 0, parity = len % 2;
  for (int i = 0; i < len; i++) {
    int d = digits[i] - '0';
    if (i % 2 == parity) { d *= 2; if (d > 9) d -= 9; }
    sum += d;
  }
  return (sum % 10) == 0;
}

/* Port of OSINTsaas extract_pattern: dedup-unique matches, optional validator. */
static cJSON *extract_pattern(const char *text, const char *pat,
                              int (*validator)(const char *)) {
  cJSON *matches = cJSON_CreateArray();
  regex_t re;
  if (regcomp(&re, pat, REG_EXTENDED | REG_ICASE) != 0) return matches;
  regmatch_t m;
  const char *cur = text;
  int mc = 0;
  char *seen[MAX_MATCHES]; int sc = 0;
  while (mc < MAX_MATCHES && regexec(&re, cur, 1, &m, 0) == 0) {
    int len = (int)(m.rm_eo - m.rm_so);
    if (len > 0 && len < 256) {
      char *found = calloc(len + 1, 1);
      if (found) {
        memcpy(found, cur + m.rm_so, len);
        int valid = validator ? validator(found) : 1;
        if (valid) {
          int dup = 0;
          for (int i = 0; i < sc; i++)
            if (strcmp(seen[i], found) == 0) { dup = 1; break; }
          if (!dup && sc < MAX_MATCHES) {
            cJSON_AddItemToArray(matches, cJSON_CreateString(found));
            seen[sc++] = strdup(found);
          }
        }
        free(found);
      }
    }
    cur += m.rm_eo;
    if (*cur == 0) break;
    mc++;
  }
  regfree(&re);
  for (int i = 0; i < sc; i++) free(seen[i]);
  return matches;
}

/* run-scoped dedup set across all indicator types (key = "<type>\0<value>"). */
typedef struct { char **v; int n, cap; } sset;
static int sset_seen(sset *s, const char *key) {
  for (int i = 0; i < s->n; i++) if (strcmp(s->v[i], key) == 0) return 1;
  if (s->n == s->cap) {
    int nc = s->cap ? s->cap * 2 : 32;
    char **nv = realloc(s->v, (size_t)nc * sizeof *nv);
    if (!nv) return 1;                 /* OOM → treat as seen (skip) */
    s->v = nv; s->cap = nc;
  }
  s->v[s->n++] = strdup(key);
  return 0;
}
static void sset_free(sset *s) {
  for (int i = 0; i < s->n; i++) free(s->v[i]);
  free(s->v);
}

/* Emit ONE intel_item for a single (type,value) indicator. Returns 1 if a row
 * was emitted (i.e. not a within-run duplicate and emit succeeded). */
static int emit_ioc(intel_sink *sink, sset *seen,
                    const char *type, const char *value) {
  if (!value || !*value) return 0;
  char key[600];
  snprintf(key, sizeof key, "%s\t%s", type, value);
  if (sset_seen(seen, key)) return 0;        /* dedup within run */

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "type", type);
  cJSON_AddStringToObject(data, "value", value);
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "DATA_EXTRACTOR");
  cJSON_AddStringToObject(props, "ioc_type", type);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[640];
  snprintf(rk, sizeof rk, "ioc:%s:%s", type, value);
  char title[600];
  snprintf(title, sizeof title, "%s: %s", type, value);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = type;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"DATA_EXTRACTOR\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* Extract one pattern, emit one row per unique match. arr is consumed. */
static int emit_matches(intel_sink *sink, sset *seen, const char *type,
                        cJSON *arr /*owned*/) {
  int emitted = 0, n = cJSON_GetArraySize(arr);
  for (int i = 0; i < n; i++) {
    cJSON *d = cJSON_GetArrayItem(arr, i);
    if (d && cJSON_IsString(d))
      emitted += emit_ioc(sink, seen, type, d->valuestring);
  }
  cJSON_Delete(arr);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *text = ctx->entity;
  if (!text || !*text) return -1;

  sset seen = {0};
  int emitted = 0;

  emitted += emit_matches(sink, &seen, "email",
                          extract_pattern(text, PATTERNS[0].pattern, NULL));
  emitted += emit_matches(sink, &seen, "ipv4",
                          extract_pattern(text, PATTERNS[1].pattern, is_valid_ipv4));
  emitted += emit_matches(sink, &seen, "url",
                          extract_pattern(text, PATTERNS[2].pattern, NULL));

  /* domains: upstream filters then re-adds; reproduce its (buggy-but-faithful)
   * predicate which, due to C || precedence, keeps essentially every domain
   * with a dot and length>4 — i.e. pass-through. */
  {
    cJSON *domains = extract_pattern(text, PATTERNS[3].pattern, NULL);
    int dc = cJSON_GetArraySize(domains);
    for (int i = 0; i < dc; i++) {
      cJSON *d = cJSON_GetArrayItem(domains, i);
      if (d && cJSON_IsString(d)) {
        const char *s = d->valuestring;
        if (strlen(s) > 4 && strchr(s, '.'))
          emitted += emit_ioc(sink, &seen, "domain", s);
      }
    }
    cJSON_Delete(domains);
  }

  emitted += emit_matches(sink, &seen, "md5",
                          extract_pattern(text, PATTERNS[4].pattern, NULL));
  emitted += emit_matches(sink, &seen, "sha1",
                          extract_pattern(text, PATTERNS[5].pattern, NULL));
  emitted += emit_matches(sink, &seen, "sha256",
                          extract_pattern(text, PATTERNS[6].pattern, NULL));
  emitted += emit_matches(sink, &seen, "bitcoin",
                          extract_pattern(text, PATTERNS[7].pattern, NULL));
  emitted += emit_matches(sink, &seen, "ethereum",
                          extract_pattern(text, PATTERNS[8].pattern, NULL));
  emitted += emit_matches(sink, &seen, "mac_address",
                          extract_pattern(text, PATTERNS[9].pattern, NULL));
  emitted += emit_matches(sink, &seen, "phone_us",
                          extract_pattern(text, PATTERNS[10].pattern, NULL));
  emitted += emit_matches(sink, &seen, "credit_card",
                          extract_pattern(text, PATTERNS[11].pattern, luhn_check));
  emitted += emit_matches(sink, &seen, "ssn",
                          extract_pattern(text, PATTERNS[12].pattern, NULL));

  sset_free(&seen);
  (void)emitted;
  return 0;                  /* honest empty (nothing extracted) is not error */
}

static const source_def data_extractor_def = {
  .id = "DATA_EXTRACTOR", .collector = "osint",
  .name = "Data Extractor", .name_ja = "データ抽出",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/data-extractor",
  .description = "Extracts emails, IPs, hashes, crypto and PII from free text via regex",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(data_extractor_def)
