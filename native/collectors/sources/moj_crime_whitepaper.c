/* collectors/crime/sources/moj_crime_whitepaper.c
 * Port of server/src/collectors/mojCrimeWhitepaper.js (fetchHead+fetchText).
 * Probe a 5-edition window (thisYear-1951+1 .. -4) for the first reachable
 * JA edition page, then probe its EN counterpart. Emits 1 feature pin at MOJ
 * HQ + 1 (JA) [+1 EN if reachable] intel item. If no edition is reachable JS
 * emits 0 features + 0 intel; we emit nothing.
 *  probe(u) = fetchHead(u) OR (fetchText(u) && len>200)
 *  feature uid/title derived by geojson sink; intel uids =
 *  intelUid(SOURCE_ID,'ja_<ed>') / 'en_<ed>'. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/probe.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MOJ_HQ_LON 139.7531
#define MOJ_HQ_LAT 35.6735

static int probe_url(http_client *http, const char *url) {
  if (probe_head(http, url)) return 1;            /* fetchHead ok */
  char *t = feed_get_text(http, url, 6000);       /* GET fallback */
  int ok = (t && strlen(t) > 200);
  free(t);
  return ok;
}

/* firstReachable(candidates) -> matched url copied into out, or 0. */
static int first_reachable(http_client *http, const char *const *cand,
                           int ncand, char *out, size_t n) {
  for (int i = 0; i < ncand; i++) {
    if (probe_url(http, cand[i])) { snprintf(out, n, "%s", cand[i]); return 1; }
  }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char iso[32];
  time_t now = time(NULL);
  struct tm tmv; gmtime_r(&now, &tmv);
  strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%S.000Z", &tmv);
  int this_year = tmv.tm_year + 1900;

  int found = 0, edition = 0, year = 0, en_ok = 0;
  char ja_url[256] = {0}, en_url[256] = {0};

  /* for (edition = thisYear-1951+1; edition >= thisYear-1951-4; edition--) */
  for (int ed = this_year - 1951 + 1; ed >= this_year - 1951 - 4 && !found;
       ed--) {
    int yr = 1951 + ed;
    char ja0[256], ja1[256], en0[256], en1[256];
    snprintf(ja0, sizeof ja0,
      "https://hakusyo1.moj.go.jp/jp/%d/nfm/mokuji.html", ed);
    snprintf(ja1, sizeof ja1,
      "https://hakusyo1.moj.go.jp/jp/%d/nfm/n_%d_1_1_1_0_0.html", ed, ed);
    snprintf(en0, sizeof en0,
      "https://hakusyo1.moj.go.jp/en/%d/nfm/mokuji.html", ed);
    snprintf(en1, sizeof en1,
      "https://hakusyo1.moj.go.jp/en/%d/nfm/n_%d_1_2_0_0_0.html", ed, ed);
    const char *jac[] = { ja0, ja1 };
    char ju[256];
    if (first_reachable(ctx->http, jac, 2, ju, sizeof ju)) {
      const char *enc[] = { en0, en1 };
      char eu[256] = {0};
      en_ok = first_reachable(ctx->http, enc, 2, eu, sizeof eu);
      snprintf(ja_url, sizeof ja_url, "%s", ju);
      if (en_ok) snprintf(en_url, sizeof en_url, "%s", eu);
      edition = ed; year = yr; found = 1;
    }
  }

  if (!found) return -1;             /* JS: 0 features + 0 intel */

  /* ---- feature pin (geojson sink derives uid/title) ---- */
  cJSON *features = cJSON_CreateArray();
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *co = cJSON_CreateArray();
  cJSON_AddItemToArray(co, cJSON_CreateNumber(MOJ_HQ_LON));
  cJSON_AddItemToArray(co, cJSON_CreateNumber(MOJ_HQ_LAT));
  cJSON_AddItemToObject(g, "coordinates", co);
  cJSON_AddItemToObject(f, "geometry", g);
  cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
  char idbuf[32], namebuf[64], ym[16];
  snprintf(idbuf, sizeof idbuf, "MOJ_WP_%d", year);
  snprintf(namebuf, sizeof namebuf, "MOJ White Paper on Crime (%d)", year);
  snprintf(ym, sizeof ym, "%d-12", year);
  cJSON_AddStringToObject(p, "id", idbuf);
  cJSON_AddStringToObject(p, "name", namebuf);
  cJSON_AddNumberToObject(p, "year", (double)year);
  cJSON_AddNumberToObject(p, "edition", (double)edition);
  cJSON_AddStringToObject(p, "ja_url", ja_url);
  /* en_url: latest.enUrl — null when EN not reachable */
  if (en_ok) cJSON_AddStringToObject(p, "en_url", en_url);
  else cJSON_AddNullToObject(p, "en_url");
  cJSON_AddStringToObject(p, "year_month", ym);
  cJSON_AddStringToObject(p, "source", ctx->source_id);
  cJSON_AddItemToObject(f, "properties", p);
  cJSON_AddItemToArray(features, f);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);

  /* ---- intel item: JA ---- */
  char ja_rk[32], ja_title[160];
  snprintf(ja_rk, sizeof ja_rk, "ja_%d", edition);
  snprintf(ja_title, sizeof ja_title,
    "\xe7\x8a\xaf\xe7\xbd\xaa\xe7\x99\xbd\xe6\x9b\xb8 %d\xe5\xb9\xb4\xe7\x89\x88 (Edition %d) \xe2\x80\x94 \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",
    year, edition);
  cJSON *jp = cJSON_CreateObject();             /* {publisher,edition,year,landing} */
  cJSON_AddStringToObject(jp, "publisher",
    "Ministry of Justice (\xe6\xb3\x95\xe5\x8b\x99\xe7\x9c\x81)");
  cJSON_AddNumberToObject(jp, "edition", (double)edition);
  cJSON_AddNumberToObject(jp, "year", (double)year);
  cJSON_AddStringToObject(jp, "landing", ja_url);
  char *jpj = cJSON_PrintUnformatted(jp);

  intel_item ja = {0};
  ja.remote_key = ja_rk;
  ja.title = ja_title;
  ja.summary = "MOJ annual crime white paper \xe2\x80\x94 prosecution rates, "
               "sentencing, recidivism, juvenile justice, prison population.";
  ja.link = ja_url;
  ja.lang = "ja";
  ja.published_at = iso;
  ja.tags_json = "[\"crime\",\"justice\",\"white-paper\",\"moj\",\"national\"]";
  ja.properties_json = jpj;
  if (sink->emit(sink, &ja) >= 0 && n >= 0) n++;
  free(jpj); cJSON_Delete(jp);

  /* ---- intel item: EN (only if EN reachable) ---- */
  if (en_ok) {
    char en_rk[32], en_title[160];
    snprintf(en_rk, sizeof en_rk, "en_%d", edition);
    snprintf(en_title, sizeof en_title,
      "White Paper on Crime %d (English Edition %d)", year, edition);
    cJSON *ep = cJSON_CreateObject();
    cJSON_AddStringToObject(ep, "publisher",
      "Ministry of Justice (\xe6\xb3\x95\xe5\x8b\x99\xe7\x9c\x81)");
    cJSON_AddNumberToObject(ep, "edition", (double)edition);
    cJSON_AddNumberToObject(ep, "year", (double)year);
    cJSON_AddStringToObject(ep, "landing", en_url);
    char *epj = cJSON_PrintUnformatted(ep);

    intel_item en = {0};
    en.remote_key = en_rk;
    en.title = en_title;
    en.summary = "English edition of MOJ annual crime white paper.";
    en.link = en_url;
    en.lang = "en";
    en.published_at = iso;
    en.tags_json =
      "[\"crime\",\"justice\",\"white-paper\",\"moj\",\"national\",\"english\"]";
    en.properties_json = epj;
    if (sink->emit(sink, &en) >= 0 && n >= 0) n++;
    free(epj); cJSON_Delete(ep);
  }

  fprintf(stderr, "[moj-crime-whitepaper] edition=%d year=%d en=%d emitted %d\n",
          edition, year, en_ok, n);
  return n >= 0 ? 0 : -1;
}

static const source_def moj_crime_whitepaper_def = {
  .id = "moj-crime-whitepaper", .collector = "crime",
  .name = "MOJ Crime White Paper",
  .name_ja = "\xe6\xb3\x95\xe5\x8b\x99\xe7\x9c\x81 \xe7\x8a\xaf\xe7\xbd\xaa\xe7\x99\xbd\xe6\x9b\xb8",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(moj_crime_whitepaper_def)
