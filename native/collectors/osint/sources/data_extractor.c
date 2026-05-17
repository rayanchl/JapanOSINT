/* collectors/osint/sources/data_extractor.c
 * OSINT service — faithful port of OSINTsaas osint_tools/data_extractor.c
 * (data_extract / handle_data_extractor). Canonical SERVICE id in
 * osint_dispatcher.c: {SERVICE_IOC_CHECKER, handle_data_extractor,
 * "DATA_EXTRACTOR", true} (alias IOC_LOOKUP maps to a different handler →
 * not this file). Entity = free text; pure-compute POSIX-regex extraction of
 * emails/IPv4/urls/domains/hashes/btc/eth/mac/phones/cards/ssns. No network,
 * no key. Mirrors the upstream `root` object exactly; success always true,
 * confidence 90 if any item else 50. Emits one osint_service_result row whose
 * body is the {success,confidence,data} envelope, like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include <ctype.h>
#include <regex.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

static void add_set(cJSON *extracted, cJSON *stats, const char *key,
                     cJSON *arr, int *total) {
  int n = cJSON_GetArraySize(arr);
  if (n > 0) {
    cJSON_AddItemToObject(extracted, key, arr);
    cJSON_AddNumberToObject(stats, key, n);
    *total += n;
  } else {
    cJSON_Delete(arr);
  }
}

static int emit_result(intel_sink *sink, const char *entity, int success,
                       int confidence, cJSON *data /*owned*/) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  if (data) cJSON_AddItemToObject(env, "data", data);
  else cJSON_AddNullToObject(env, "data");
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "DATA_EXTRACTOR");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char title[160];
  snprintf(title, sizeof title, "DATA_EXTRACTOR — %d items",
           data ? 1 : 0);

  intel_item it = {0};
  it.remote_key      = "dataextract:input";
  it.title           = title;
  it.body            = bj;
  it.summary         = "structured data extracted";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"DATA_EXTRACTOR\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *text = ctx->entity;
  if (!text || !*text) return -1;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));
  cJSON_AddNumberToObject(root, "input_length", (double)strlen(text));

  cJSON *stats = cJSON_CreateObject();
  cJSON *extracted = cJSON_CreateObject();
  int total = 0;

  add_set(extracted, stats, "emails",
          extract_pattern(text, PATTERNS[0].pattern, NULL), &total);
  add_set(extracted, stats, "ipv4_addresses",
          extract_pattern(text, PATTERNS[1].pattern, is_valid_ipv4), &total);
  add_set(extracted, stats, "urls",
          extract_pattern(text, PATTERNS[2].pattern, NULL), &total);

  /* domains: upstream filters then re-adds; reproduce its (buggy-but-faithful)
   * predicate which, due to C || precedence, keeps essentially every domain
   * with a dot and length>4 — i.e. pass-through. */
  {
    cJSON *domains = extract_pattern(text, PATTERNS[3].pattern, NULL);
    int dc = cJSON_GetArraySize(domains);
    if (dc > 0) {
      cJSON *fd = cJSON_CreateArray();
      for (int i = 0; i < dc; i++) {
        cJSON *d = cJSON_GetArrayItem(domains, i);
        if (d && cJSON_IsString(d)) {
          const char *s = d->valuestring;
          size_t L = strlen(s);
          if (L > 4 && strchr(s, '.'))
            cJSON_AddItemToArray(fd, cJSON_CreateString(s));
        }
      }
      cJSON_Delete(domains);
      int fn = cJSON_GetArraySize(fd);
      if (fn > 0) {
        cJSON_AddItemToObject(extracted, "domains", fd);
        cJSON_AddNumberToObject(stats, "domains", fn);
        total += fn;
      } else cJSON_Delete(fd);
    } else cJSON_Delete(domains);
  }

  add_set(extracted, stats, "md5_hashes",
          extract_pattern(text, PATTERNS[4].pattern, NULL), &total);
  add_set(extracted, stats, "sha1_hashes",
          extract_pattern(text, PATTERNS[5].pattern, NULL), &total);
  add_set(extracted, stats, "sha256_hashes",
          extract_pattern(text, PATTERNS[6].pattern, NULL), &total);
  add_set(extracted, stats, "bitcoin_addresses",
          extract_pattern(text, PATTERNS[7].pattern, NULL), &total);
  add_set(extracted, stats, "ethereum_addresses",
          extract_pattern(text, PATTERNS[8].pattern, NULL), &total);
  add_set(extracted, stats, "mac_addresses",
          extract_pattern(text, PATTERNS[9].pattern, NULL), &total);
  add_set(extracted, stats, "phone_numbers",
          extract_pattern(text, PATTERNS[10].pattern, NULL), &total);

  cJSON *cards = extract_pattern(text, PATTERNS[11].pattern, luhn_check);
  int card_count = cJSON_GetArraySize(cards);
  if (card_count > 0) {
    cJSON_AddItemToObject(extracted, "potential_credit_cards", cards);
    cJSON_AddNumberToObject(stats, "potential_credit_cards", card_count);
    cJSON_AddStringToObject(extracted, "credit_card_warning",
      "These may be valid credit card numbers - handle with care");
    total += card_count;
  } else cJSON_Delete(cards);

  cJSON *ssns = extract_pattern(text, PATTERNS[12].pattern, NULL);
  int ssn_count = cJSON_GetArraySize(ssns);
  if (ssn_count > 0) {
    cJSON_AddItemToObject(extracted, "potential_ssns", ssns);
    cJSON_AddNumberToObject(stats, "potential_ssns", ssn_count);
    cJSON_AddStringToObject(extracted, "ssn_warning",
      "These match SSN pattern but may be other number sequences");
    total += ssn_count;
  } else cJSON_Delete(ssns);

  cJSON_AddItemToObject(root, "extracted", extracted);
  cJSON_AddNumberToObject(stats, "total_items", total);
  cJSON_AddItemToObject(root, "statistics", stats);

  if (card_count > 0 || ssn_count > 0) {
    cJSON *w = cJSON_CreateArray();
    cJSON_AddItemToArray(w, cJSON_CreateString(
      "Potential PII (Personally Identifiable Information) detected"));
    cJSON_AddItemToArray(w, cJSON_CreateString(
      "Handle this data according to privacy regulations (GDPR, CCPA, etc.)"));
    cJSON_AddItemToObject(root, "warnings", w);
  }

  return emit_result(sink, text, 1, total > 0 ? 90 : 50, root);
}

static const source_def data_extractor_def = {
  .id = "DATA_EXTRACTOR", .collector = "osint",
  .name = "Data Extractor", .name_ja = "データ抽出",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(data_extractor_def)
