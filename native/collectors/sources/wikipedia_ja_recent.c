/* collectors/social/sources/wikipedia_ja_recent.c
 * Port of server/src/collectors/wikipediaJaRecent.js.
 * ja.wikipedia recentchanges (ns0, rclimit=200) → FeatureCollection at TOKYO.
 * Keyless. Fetch-fail → 0 features (correctness-neutral). */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://ja.wikipedia.org/w/api.php?action=query&list=recentchanges&rcprop=title|user|timestamp|comment|sizes|ids&rcnamespace=0&rclimit=200&format=json&origin=*"

static void passthru(cJSON *p, const char *k, cJSON *rc, const char *ik) {
  cJSON *v = cJSON_GetObjectItem(rc, ik);
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

/* x ?? 0  (number) */
static double n0(cJSON *rc, const char *k) {
  cJSON *v = cJSON_GetObjectItem(rc, k);
  if (v && cJSON_IsNumber(v)) return v->valuedouble;
  return 0;
}

/* encodeURIComponent: percent-encode all but A-Za-z0-9 -_.!~*'() */
static void enc_uri_comp(const char *s, char *o, size_t cap) {
  static const char *unres = "-_.!~*'()";
  size_t j = 0;
  for (; *s && j + 4 < cap; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(unres, c)) {
      o[j++] = (char)c;
    } else {
      static const char hx[] = "0123456789ABCDEF";
      o[j++] = '%'; o[j++] = hx[c >> 4]; o[j++] = hx[c & 0xF];
    }
  }
  o[j] = '\0';
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* NO PER-ROW USER-AGENT. This row used to send `user-agent:
   * japanosint-collector` — a bare product token with no URL and no contact
   * route — and ja.wikipedia.org answers that with HTTP 429 "You are making
   * too many requests", which is what Wikimedia's user-agent policy asks a
   * client without contact details to expect. Measured 2026-11-07: 1 request,
   * 3,443 bytes, 0 records.
   *
   * The remedy is subtraction rather than a new header. core/httpclient.c's
   * precedence is "a host in core/host_agents.def gets the table's agent
   * whatever the caller set; any other host keeps the caller's header, else
   * JO_USER_AGENT" — and no wikimedia host is in that table, so this row's
   * header was overriding the tree's own good default, which already carries
   * the product, a repository URL and a contact route. Dropping it is the
   * cheapest rung of host_agents.def's own admission ladder, and if 429
   * persists the next rung is measured there rather than guessed here.
   *
   * No hostgate gap is added: a budget cannot be measured from behind the
   * limit (rule 1's Archive-It note), and `http_request` retries ON 429, so
   * each tick could spend up to three requests against it. */
  cJSON *json = feed_get_json(ctx->http, API_URL, 12000);

  cJSON *query = json ? cJSON_GetObjectItem(json, "query") : NULL;
  cJSON *arr = query ? cJSON_GetObjectItem(query, "recentchanges") : NULL;

  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(arr)) {
    int i = 0;
    cJSON *rc;
    cJSON_ArrayForEach(rc, arr) {
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      /* NO GEOMETRY. Every row here used to be emitted at Tokyo Station
       * (35.6895, 139.6917). What this source reports has no location,
       * and stacking every row on one pin is the fabrication the
       * 2026-07-31 audit deleted from cisa-kev-jp, poc-in-github and
       * peeringdb-jp. Confirmed live by tests/contract/source_contract.py.
       * lib/geojson.c handles an absent geometry correctly — do NOT
       * reintroduce a fallback coordinate. */

      cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
      cJSON_AddNumberToObject(p, "idx", i);
      passthru(p, "title", rc, "title");
      passthru(p, "user", rc, "user");
      passthru(p, "timestamp", rc, "timestamp");
      passthru(p, "comment", rc, "comment");
      passthru(p, "pageid", rc, "pageid");
      passthru(p, "revid", rc, "revid");
      passthru(p, "old_revid", rc, "old_revid");
      passthru(p, "oldlen", rc, "oldlen");
      passthru(p, "newlen", rc, "newlen");
      cJSON_AddNumberToObject(p, "delta", n0(rc, "newlen") - n0(rc, "oldlen"));
      cJSON *tv = cJSON_GetObjectItem(rc, "title");
      if (tv && cJSON_IsString(tv) && tv->valuestring[0]) {
        char repl[512];
        size_t j = 0;
        for (const char *s = tv->valuestring; *s && j + 1 < sizeof repl; s++)
          repl[j++] = (*s == ' ') ? '_' : *s;
        repl[j] = '\0';
        char enc[1536];
        enc_uri_comp(repl, enc, sizeof enc);
        char ub[1600];
        snprintf(ub, sizeof ub, "https://ja.wikipedia.org/wiki/%s", enc);
        cJSON_AddStringToObject(p, "url", ub);
      } else {
        cJSON_AddItemToObject(p, "url", cJSON_CreateNull());
      }
      cJSON_AddStringToObject(p, "source", "wp_ja_recentchanges");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  int parsed = cJSON_IsArray(arr);
  if (json) cJSON_Delete(json);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[wikipedia-ja-recent] emitted %d\n", n);
  /* The header said "Fetch-fail → 0 features (correctness-neutral)" and
   * `return n >= 0 ? 0 : -1` cannot be -1, so a 429 read exactly like one of
   * the busiest wikis in the world publishing nothing for ten minutes.
   *
   * The test is the PARSE and not the record count: `query.recentchanges` is
   * an array even when it is empty, so a genuinely quiet ten minutes still
   * returns 0 — only a body that did not give us that array is a failure. */
  if (!parsed) {
    fprintf(stderr, "[wikipedia-ja-recent] no query.recentchanges array in the "
                    "response — reporting a failed fetch, not a wiki with no "
                    "recent changes\n");
    return -1;
  }
  return 0;
}

static const source_def wikipedia_ja_recent_def = {
  .id = "wikipedia-ja-recent", .collector = "social",
  .name = "Wikipedia (ja) recent", .name_ja = "ウィキペディア最新",
   .update_interval_sec = 600, .run = run,
};
REGISTER_SOURCE(wikipedia_ja_recent_def)
