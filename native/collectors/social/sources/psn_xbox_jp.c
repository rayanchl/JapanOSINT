/* collectors/social/sources/psn_xbox_jp.c
 * Port of server/src/collectors/psnXboxJp.js (psn-xbox-jp).
 * Keyed (anyOf): PSN_NPSSO / XBOX_API_KEY. Watchlist from PSN_XBOX_WATCHLIST
 * ("psn:<id>,xbl:<gamertag>"). Non-spatial intel: public gamer profiles.
 *
 * FIDELITY NOTE: the Node PSN path needs a redirect:'manual' fetch that reads
 * the 302 `location` header to extract the OAuth code. feedlib exposes no
 * redirect-header primitive (only feed_get_json/text/post which follow
 * redirects), so the PSN access-token exchange cannot be reproduced here; the
 * PSN branch is therefore not pursued (no fabricated profiles — RULE 8). The
 * Xbox/OpenXBL branch (plain GET + X-Authorization header) is ported fully.
 * honest empty when gated / no watchlist / on failure.
 * uid = psn-xbox-jp|xbl:<gamertag>. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOURCE_ID "psn-xbox-jp"

static const char *sv(const cJSON *o, const char *k) {
  if (!o) return NULL;
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

/* settings[]: {id,value} → value of given id (string). */
static const char *setting(const cJSON *prof, const char *id) {
  if (!prof) return NULL;
  cJSON *s = cJSON_GetObjectItem(prof, "settings");
  if (!cJSON_IsArray(s)) return NULL;
  cJSON *e;
  cJSON_ArrayForEach(e, s) {
    const char *sid = sv(e, "id");
    if (sid && strcmp(sid, id) == 0) return sv(e, "value");
  }
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *npsso  = getenv("PSN_NPSSO");
  const char *xblKey = getenv("XBOX_API_KEY");
  const char *wl     = getenv("PSN_XBOX_WATCHLIST");

  if ((!npsso || !*npsso) && (!xblKey || !*xblKey)) {
    fprintf(stderr, "[psn-xbox-jp] gated (no PSN_NPSSO / XBOX_API_KEY)\n");
    return 0;
  }

  /* parseWatchlist: split on ',', trim, prefix psn:/xbl: */
  char **xbl = NULL; int xn = 0, xcap = 0;
  int psnCount = 0;
  if (wl && *wl) {
    char *dup = strdup(wl), *save = NULL;
    for (char *tok = strtok_r(dup, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
      while (*tok == ' ') tok++;
      char *e = tok + strlen(tok);
      while (e > tok && e[-1] == ' ') *--e = '\0';
      if (!*tok) continue;
      if (strncmp(tok, "psn:", 4) == 0) psnCount++;
      else if (strncmp(tok, "xbl:", 4) == 0) {
        if (xn == xcap) { xcap = xcap ? xcap * 2 : 8;
          xbl = realloc(xbl, xcap * sizeof(char *)); }
        xbl[xn++] = strdup(tok + 4);
      }
    }
    free(dup);
  }

  if (psnCount == 0 && xn == 0) {
    fprintf(stderr, "[psn-xbox-jp] no watchlist -> honest empty\n");
    free(xbl);
    return -1;  /* JS: unavailable, no fabricated IDs */
  }

  int n = 0;

  /* ---- Xbox (OpenXBL) — keyed GET, fully portable via feedlib ---- */
  if (xblKey && *xblKey && xn > 0) {
    char auth[512];
    snprintf(auth, sizeof auth, "X-Authorization: %s", xblKey);
    const char *hdrs[] = { auth, "Accept: application/json", NULL };
    for (int i = 0; i < xn; i++) {
      char url[256];
      snprintf(url, sizeof url, "https://xbl.io/api/v2/account/%s", xbl[i]);
      cJSON *data = feed_get_json_h(ctx->http, url, hdrs, 15000);
      if (!data) continue;

      cJSON *pu = cJSON_GetObjectItem(data, "profileUsers");
      cJSON *prof = (pu && cJSON_IsArray(pu)) ? cJSON_GetArrayItem(pu, 0) : NULL;
      const char *gt = setting(prof, "Gamertag");
      if (!gt) gt = xbl[i];
      const char *bio = setting(prof, "Bio");
      const char *gs  = setting(prof, "Gamerscore");

      char uidb[160], title[160], link[256];
      snprintf(uidb, sizeof uidb, "%s|xbl:%s", SOURCE_ID, xbl[i]);
      snprintf(title, sizeof title, "Xbox %s", gt);
      snprintf(link, sizeof link,
               "https://www.xbox.com/play/user/%s", xbl[i]);

      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("xbox"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("xbox-live"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("profile"));
      char *tj = cJSON_PrintUnformatted(tags);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "network", "xbox");
      cJSON_AddStringToObject(props, "gamertag", gt);
      const char *xid = prof ? sv(prof, "id") : NULL;
      if (xid) cJSON_AddStringToObject(props, "xuid", xid);
      else cJSON_AddNullToObject(props, "xuid");
      if (gs) cJSON_AddStringToObject(props, "gamerscore", gs);
      else cJSON_AddNullToObject(props, "gamerscore");
      cJSON_AddStringToObject(props, "source", "openxbl_api");
      char *pj = cJSON_PrintUnformatted(props);

      intel_item it = {0};
      it.uid = uidb;
      it.title = title;
      it.summary = bio;
      it.body = bio;
      it.link = link;
      it.author = gt;
      it.record_type = SOURCE_ID;
      it.tags_json = tj;
      it.properties_json = pj;
      if (sink->emit(sink, &it) >= 0) n++;

      free(tj); free(pj);
      cJSON_Delete(tags); cJSON_Delete(props);
      cJSON_Delete(data);
    }
  }

  for (int i = 0; i < xn; i++) free(xbl[i]);
  free(xbl);
  fprintf(stderr, "[psn-xbox-jp] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def psn_xbox_jp_def = {
  .id = "psn-xbox-jp", .collector = "social",
  .name = "PSN / Xbox Live (JP gamertags)",
  .name_ja = "PSN / Xbox Live (日本)",
  .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(psn_xbox_jp_def)
