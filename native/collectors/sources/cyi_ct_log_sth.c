/* collectors/sources/cyi_ct_log_sth.c
 * RFC-6962 signed tree head (get-sth) for the live Certificate Transparency
 * logs — certificate count + merkle root per log; the delta of tree_size
 * between runs is the global certificate issuance rate.
 * Endpoints: https://www.gstatic.com/ct/log_list/v3/log_list.json (log inventory)
 *            <log url>ct/v1/get-sth  e.g. https://ct.cloudflare.com/logs/nimbus2026/ct/v1/get-sth
 * TRAP (parse_notes): "take log URLs from ct-log-list rather than hardcoding,
 * because temporal shards rotate every 6 months (the 2026h1 Google shard
 * already 404s)" — so this reads log_list.json first and only probes logs whose
 * state is `usable`.
 * Emits: log url, tree_size, timestamp (ms since epoch, converted to ISO),
 * sha256_root_hash — all straight from the log's own STH. Keyless.
 * No coordinates upstream -> has_geo 0 (R2).
 * Licence: RFC 6962 public log endpoints, no stated restriction.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LIST_URL "https://www.gstatic.com/ct/log_list/v3/log_list.json"
#define MAX_LOGS 12          /* bound one run; usable logs only */

/* epoch milliseconds -> ISO-8601 UTC. Returns out, or NULL when ms is not a
 * plausible timestamp (guarded numeric parse, R2-style). */
static const char *ms_to_iso(double ms, char *out, size_t n) {
  if (!(ms > 946684800000.0 && ms < 4102444800000.0)) return NULL;
  time_t t = (time_t)(ms / 1000.0);
  struct tm tmv;
#if defined(_WIN32)
  if (gmtime_s(&tmv, &t) != 0) return NULL;
#else
  if (!gmtime_r(&t, &tmv)) return NULL;
#endif
  if (strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tmv) == 0) return NULL;
  return out;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *list = feed_get_json(ctx->http, LIST_URL, 25000);
  if (!list) { fprintf(stderr, "[ct-log-sth] log list fetch failed\n"); return -1; }

  int n = 0, tried = 0;
  const cJSON *ops = cJSON_GetObjectItem(list, "operators");
  const cJSON *op;
  cJSON_ArrayForEach(op, ops) {
    if (tried >= MAX_LOGS) break;
    const char *opname = jo_sv(op, "name");
    const cJSON *logs = cJSON_GetObjectItem(op, "logs");
    const cJSON *lg;
    cJSON_ArrayForEach(lg, logs) {
      if (tried >= MAX_LOGS) break;
      const char *url = jo_sv(lg, "url");
      if (!url) continue;
      const cJSON *st = cJSON_GetObjectItem(lg, "state");
      if (!cJSON_IsObject(st) || !st->child) continue;
      if (strcmp(st->child->string, "usable") != 0) continue;   /* dead shards */
      const char *desc = jo_sv(lg, "description");

      char sth_url[512];
      size_t ul = strlen(url);
      snprintf(sth_url, sizeof sth_url, "%s%sct/v1/get-sth",
               url, (ul && url[ul - 1] == '/') ? "" : "/");
      tried++;

      cJSON *sth = feed_get_json(ctx->http, sth_url, 15000);
      if (!sth) { fprintf(stderr, "[ct-log-sth] no STH from %s\n", sth_url); continue; }

      const cJSON *tsz = cJSON_GetObjectItem(sth, "tree_size");
      const cJSON *tms = cJSON_GetObjectItem(sth, "timestamp");
      const char *root = jo_sv(sth, "sha256_root_hash");
      if (!cJSON_IsNumber(tsz)) { cJSON_Delete(sth); continue; }

      char iso[40];
      const char *when = cJSON_IsNumber(tms) ? ms_to_iso(tms->valuedouble, iso, sizeof iso)
                                             : NULL;

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "log_url", url);
      if (desc)   cJSON_AddStringToObject(p, "description", desc);
      if (opname) cJSON_AddStringToObject(p, "operator", opname);
      cJSON_AddNumberToObject(p, "tree_size", tsz->valuedouble);
      if (cJSON_IsNumber(tms)) cJSON_AddNumberToObject(p, "timestamp_ms", tms->valuedouble);
      if (when) cJSON_AddStringToObject(p, "sth_time", when);
      if (root) cJSON_AddStringToObject(p, "sha256_root_hash", root);
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      char summary[256];
      snprintf(summary, sizeof summary, "tree_size %.0f%s%s",
               tsz->valuedouble, when ? " @ " : "", when ? when : "");

      intel_item it = {0};
      it.remote_key      = url;
      it.title           = desc ? desc : url;
      it.summary         = summary;
      it.link            = sth_url;
      it.lang            = "en";
      it.published_at    = when;
      it.record_type     = "ct-log-sth";
      it.properties_json = pj;
      it.tags_json       = "[\"cyber\",\"certificate-transparency\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(pj);
      cJSON_Delete(sth);
    }
  }

  cJSON_Delete(list);
  fprintf(stderr, "[ct-log-sth] emitted %d of %d logs probed\n", n, tried);
  /* Every probed log failing is a genuine fetch failure; zero *usable* logs in
   * the list, or a clean run, is not (R3). */
  if (tried > 0 && n == 0) return -1;
  return 0;
}

static const source_def cyi_ct_log_sth_def = {
  .id = "ct-log-sth", .collector = "cyber",
  .name = "CT log signed tree head (Cloudflare Nimbus / Google Argon)",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://ct.cloudflare.com/logs/nimbus2026/ct/v1/get-sth",
  .description = "RFC 6962 get-sth for the live CT logs: certificate count and merkle root per log; tree_size deltas are the global certificate issuance rate.",
  .license = "RFC 6962 public log endpoints, keyless, no stated restriction.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_ct_log_sth_def)
