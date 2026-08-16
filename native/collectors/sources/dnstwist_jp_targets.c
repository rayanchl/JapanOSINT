/* collectors/cyber/sources/dnstwist_jp_targets.c
 * Port of server/src/collectors/dnstwistJpTargets.js (intelEnvelope).
 * Key-free: generate dnstwist-style permutations for a fixed JP corp/gov
 * domain list, DNS-resolve via public DoH (dns.google/resolve type=A), emit
 * only resolving candidates. Honest empty if 0 probed. No seed.
 * uid = dnstwist-jp-targets|<candidate>. */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TARGETS[] = {
  "mufg.jp", "smbc.co.jp", "mizuhobank.co.jp", "ntt.co.jp", "softbank.jp",
  "rakuten.co.jp", "toyota.jp", "sony.co.jp", "jp.gov", "mhlw.go.jp",
  "mod.go.jp", "npa.go.jp", "soumu.go.jp", "jreast.co.jp",
};
static const char *TLD_SWAPS[] = { "com", "net", "org", "co", "jp.com" };

/* HOMOGLYPHS: o→0 l→1 i→1 e→3 a→4 s→5 */
static char homoglyph(char c) {
  switch (c) {
    case 'o': return '0';
    case 'l': return '1';
    case 'i': return '1';
    case 'e': return '3';
    case 'a': return '4';
    case 's': return '5';
    default:  return 0;
  }
}

#define MAX_PERMS 512

static int perm_has(char **set, int ns, const char *s) {
  for (int i = 0; i < ns; i++) if (!strcmp(set[i], s)) return 1;
  return 0;
}
static void perm_add(char **set, int *ns, const char *s) {
  if (*ns >= MAX_PERMS) return;
  if (perm_has(set, *ns, s)) return;
  set[(*ns)++] = strdup(s);
}

/* Build permutations of `domain` (JS Set order: omission, transposition,
 * repetition, homoglyph, hyphenation, tld-swap; then delete original). */
static int permutations(const char *domain, char **set) {
  const char *dot = strchr(domain, '.');
  if (!dot) return 0;
  size_t sldn = (size_t)(dot - domain);
  const char *tld = dot + 1;
  char sld[128];
  if (sldn >= sizeof sld) return 0;
  memcpy(sld, domain, sldn); sld[sldn] = '\0';
  int ns = 0;
  char buf[300];

  /* character omission */
  for (size_t i = 0; i < sldn; i++) {
    char t[128]; size_t k = 0;
    for (size_t j = 0; j < sldn; j++) if (j != i) t[k++] = sld[j];
    t[k] = '\0';
    snprintf(buf, sizeof buf, "%s.%s", t, tld);
    perm_add(set, &ns, buf);
  }
  /* adjacent transposition */
  for (size_t i = 0; i + 1 < sldn; i++) {
    char t[128];
    memcpy(t, sld, sldn); t[sldn] = '\0';
    char tmp = t[i]; t[i] = t[i + 1]; t[i + 1] = tmp;
    snprintf(buf, sizeof buf, "%s.%s", t, tld);
    perm_add(set, &ns, buf);
  }
  /* repetition: sld[0..i] + sld[i] + sld[i+1..] */
  for (size_t i = 0; i < sldn; i++) {
    char t[130]; size_t k = 0;
    for (size_t j = 0; j <= i; j++) t[k++] = sld[j];
    t[k++] = sld[i];
    for (size_t j = i + 1; j < sldn; j++) t[k++] = sld[j];
    t[k] = '\0';
    snprintf(buf, sizeof buf, "%s.%s", t, tld);
    perm_add(set, &ns, buf);
  }
  /* homoglyph substitution */
  for (size_t i = 0; i < sldn; i++) {
    char sub = homoglyph(sld[i]);
    if (!sub) continue;
    char t[128];
    memcpy(t, sld, sldn); t[sldn] = '\0';
    t[i] = sub;
    snprintf(buf, sizeof buf, "%s.%s", t, tld);
    perm_add(set, &ns, buf);
  }
  /* hyphenation: i from 1 */
  for (size_t i = 1; i < sldn; i++) {
    char t[130]; size_t k = 0;
    for (size_t j = 0; j < i; j++) t[k++] = sld[j];
    t[k++] = '-';
    for (size_t j = i; j < sldn; j++) t[k++] = sld[j];
    t[k] = '\0';
    snprintf(buf, sizeof buf, "%s.%s", t, tld);
    perm_add(set, &ns, buf);
  }
  /* TLD swap */
  for (size_t i = 0; i < sizeof(TLD_SWAPS) / sizeof(TLD_SWAPS[0]); i++) {
    snprintf(buf, sizeof buf, "%s.%s", sld, TLD_SWAPS[i]);
    perm_add(set, &ns, buf);
  }

  /* set.delete(domain) — drop any entry equal to original */
  for (int i = 0; i < ns; i++) {
    if (!strcmp(set[i], domain)) {
      free(set[i]);
      for (int j = i; j + 1 < ns; j++) set[j] = set[j + 1];
      ns--;
      break;
    }
  }
  return ns;
}

/* DoH A-record resolve → cJSON array of strings, or NULL. Caller frees. */
static cJSON *resolves(http_client *http, const char *domain) {
  char enc[256], url[384];
  size_t o = 0;
  for (const char *s = domain; *s && o + 4 < sizeof enc; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')') {
      enc[o++] = (char)c;
    } else { snprintf(enc + o, sizeof enc - o, "%%%02X", c); o += 3; }
  }
  enc[o] = '\0';
  snprintf(url, sizeof url,
    "https://dns.google/resolve?name=%s&type=A", enc);
  cJSON *data = feed_get_json(http, url, 8000);
  if (!data) return NULL;
  cJSON *st = cJSON_GetObjectItem(data, "Status");
  cJSON *ans = cJSON_GetObjectItem(data, "Answer");
  if (!(st && cJSON_IsNumber(st) && st->valuedouble == 0) ||
      !cJSON_IsArray(ans)) {
    cJSON_Delete(data);
    return NULL;
  }
  cJSON *out = cJSON_CreateArray();
  cJSON *r;
  cJSON_ArrayForEach(r, ans) {
    cJSON *ty = cJSON_GetObjectItem(r, "type");
    cJSON *dt = cJSON_GetObjectItem(r, "data");
    if (ty && cJSON_IsNumber(ty) && ty->valuedouble == 1 &&
        dt && cJSON_IsString(dt))
      cJSON_AddItemToArray(out, cJSON_CreateString(dt->valuestring));
  }
  cJSON_Delete(data);
  if (cJSON_GetArraySize(out) == 0) { cJSON_Delete(out); return NULL; }
  return out;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  long probed = 0, n = 0;
  for (size_t ti = 0; ti < sizeof(TARGETS) / sizeof(TARGETS[0]); ti++) {
    const char *target = TARGETS[ti];
    char *set[MAX_PERMS];
    int np = permutations(target, set);
    for (int i = 0; i < np; i++) {
      const char *cand = set[i];
      probed++;
      cJSON *ips = resolves(ctx->http, cand);
      if (!ips) continue;

      /* ips.join(', ') */
      char joined[1024]; joined[0] = '\0';
      int first = 1;
      cJSON *ip;
      cJSON_ArrayForEach(ip, ips) {
        if (!first) strncat(joined, ", ",
                            sizeof joined - strlen(joined) - 1);
        strncat(joined, ip->valuestring,
                sizeof joined - strlen(joined) - 1);
        first = 0;
      }

      char title[320], summary[1280], body[2048], link[300];
      snprintf(title, sizeof title, "%s (typosquat of %s)", cand, target);
      snprintf(summary, sizeof summary,
        "Resolving permutation of %s \xE2\x86\x92 %s", target, joined);
      snprintf(body, sizeof body,
        "Candidate phishing/typosquat domain \"%s\" resolves to A record(s): "
        "%s. Original target: %s.", cand, joined, target);
      snprintf(link, sizeof link, "http://%s", cand);

      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("typosquat"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("phishing"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("dnstwist"));
      char ttag[64];
      snprintf(ttag, sizeof ttag, "target:%s", target);
      cJSON_AddItemToArray(tags, cJSON_CreateString(ttag));
      char *tj = cJSON_PrintUnformatted(tags);

      cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
      cJSON_AddStringToObject(p, "candidate", cand);
      cJSON_AddStringToObject(p, "target", target);
      cJSON_AddItemToObject(p, "a_records", cJSON_Duplicate(ips, 1));
      cJSON_AddStringToObject(p, "source", "doh_google");
      char *pj = cJSON_PrintUnformatted(p);

      intel_item it = {0};
      it.remote_key     = cand;
      it.title          = title;
      it.summary        = summary;
      it.body           = body;
      it.link           = link;
      it.lang           = "en";
      it.record_type    = "dnstwist-jp-targets";
      it.properties_json = pj;
      it.tags_json      = tj;
      if (sink->emit(sink, &it) >= 0) n++;

      free(pj); free(tj);
      cJSON_Delete(p); cJSON_Delete(tags);
      cJSON_Delete(ips);
    }
    for (int i = 0; i < np; i++) free(set[i]);
  }

  if (probed == 0) {
    fprintf(stderr, "[dnstwist-jp-targets] unavailable (0 probed)\n");
    return -1;
  }
  /* Zero resolving permutations is a valid honest result. */
  fprintf(stderr, "[dnstwist-jp-targets] probed=%ld emitted=%ld\n", probed, n);
  return 0;
}

static const source_def dnstwist_jp_targets_def = {
  .id = "dnstwist-jp-targets", .collector = "cyber",
  .name = "DNSTwist (JP-corp typosquats)",
  .name_ja = "DNSTwist \xE9\xA1\x9E\xE4\xBC\xBC\xE3\x83\x89\xE3\x83\xA1\xE3\x82\xA4\xE3\x83\xB3",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(dnstwist_jp_targets_def)
