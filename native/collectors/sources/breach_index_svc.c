/* collectors/sources/breach_index_svc.c
 * Four on-demand OSINT services that answer "is this exposed, and in which
 * breach?" against the LOCAL self-hosted index (core/breach_index.c) — no
 * network. The OSINT dispatcher runs them with ctx->entity set to the pivot:
 *   BREACH_INDEX_EMAIL / _USERNAME / _PHONE / _PASSWORD.
 *
 * Privacy: these anonymous services call breach_index_lookup(..., reveal=0),
 * so they NEVER return leaked plaintext — only hit/count/breach names (and a
 * prevalence count for passwords). Returning the actual leaked credential to a
 * user is a separate, authenticated, ownership-verified path (reveal=1), which
 * belongs in an /api/breach/mine endpoint gated by auth — NOT here. See
 * docs/breach-check-pipeline.md §5/§6. */
#include "source.h"
#include "core/breach_index.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int emit_lookup(const source_ctx *ctx, intel_sink *sink,
                       breach_type t, const char *svc) {
  const char *v = ctx->entity;
  if (!v || !*v) return 0;

  cJSON *out = NULL;
  breach_index_lookup(t, v, 0 /* reveal=0: anonymous, never plaintext */, &out);
  if (!out) return 0;
  char *bj = cJSON_PrintUnformatted(out);

  cJSON *found = cJSON_GetObjectItem(out, "found");
  cJSON *count = cJSON_GetObjectItem(out, "count");
  int is_found = found && cJSON_IsTrue(found);
  int n = count ? (int)count->valuedouble : 0;

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", svc);
  cJSON_AddBoolToObject(props, "found", is_found);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[32], title[96], tags[96];
  breach_index_keyid(t, v, rk, sizeof rk);   /* "<type>:<sha1prefix>" — never echoes v */
  snprintf(title, sizeof title, "breach exposure: %s%s",
           breach_type_name(t), is_found ? " — EXPOSED" : " — not found");
  snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", svc);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = is_found ? "found in local breach index" : "not in local breach index";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = tags;
  sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(props);
  cJSON_Delete(out);
  (void)n;
  return 0;
}

static int run_email(const source_ctx *c, intel_sink *s)    { return emit_lookup(c, s, BT_EMAIL, "BREACH_INDEX_EMAIL"); }
static int run_username(const source_ctx *c, intel_sink *s) { return emit_lookup(c, s, BT_USERNAME, "BREACH_INDEX_USERNAME"); }
static int run_phone(const source_ctx *c, intel_sink *s)    { return emit_lookup(c, s, BT_PHONE, "BREACH_INDEX_PHONE"); }
static int run_password(const source_ctx *c, intel_sink *s) { return emit_lookup(c, s, BT_PASSWORD, "BREACH_INDEX_PASSWORD"); }

static const source_def breach_index_email = {
  .id = "BREACH_INDEX_EMAIL", .collector = "osint",
  .name = "Breach Index (email)", .name_ja = "漏洩インデックス（メール）",
  .update_interval_sec = 0, .run = run_email,
  .category = "cyber", .type = "dataset", .url = "internal://osint/breach-index/email",
  .description = "Local self-hosted breach index — is this email in an ingested breach?",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(breach_index_email)

static const source_def breach_index_username = {
  .id = "BREACH_INDEX_USERNAME", .collector = "osint",
  .name = "Breach Index (username)", .name_ja = "漏洩インデックス（ユーザー名）",
  .update_interval_sec = 0, .run = run_username,
  .category = "cyber", .type = "dataset", .url = "internal://osint/breach-index/username",
  .description = "Local self-hosted breach index — is this username in an ingested breach?",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(breach_index_username)

static const source_def breach_index_phone = {
  .id = "BREACH_INDEX_PHONE", .collector = "osint",
  .name = "Breach Index (phone)", .name_ja = "漏洩インデックス（電話）",
  .update_interval_sec = 0, .run = run_phone,
  .category = "cyber", .type = "dataset", .url = "internal://osint/breach-index/phone",
  .description = "Local self-hosted breach index — is this phone number in an ingested breach?",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(breach_index_phone)

static const source_def breach_index_password = {
  .id = "BREACH_INDEX_PASSWORD", .collector = "osint",
  .name = "Breach Index (password)", .name_ja = "漏洩インデックス（パスワード）",
  .update_interval_sec = 0, .run = run_password,
  .category = "cyber", .type = "dataset", .url = "internal://osint/breach-index/password",
  .description = "Local self-hosted password prevalence index (Pwned Passwords bulk).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(breach_index_password)
