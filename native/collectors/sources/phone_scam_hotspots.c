/* collectors/crime/sources/phone_scam_hotspots.c
 * Port of server/src/collectors/phoneScamHotspots.js — tryNpaStats() only.
 * SEED_HOTSPOTS / generateSeedData / _meta NOT ported (HARD RULE 8).
 *
 * JS flow: GET NPA index, require /特殊詐欺|tokushusagi|振り込め/ in body.
 * Then for each of 10 pref police pages: GET, require /特殊詐欺|被害|認知件数/,
 *   incidents = /認知件数[^<>]{0,40}?([0-9,]+)\s*件/
 *   damage    = /被害(?:総)?額[^<>]{0,40}?([0-9,]+)\s*(?:円|万円|億円)/
 *               (×10000 if 万円, ×100000000 if 億円)
 * skip if no incidents && no damage. Feature props (exact order):
 *   ward_id, ward, prefecture, incidents_yr, damage_yen, source_url,
 *   country, source.  ward_id="LIVE_SCAM_<idx>" (1-based over matched rows).
 * properties has no NATIVE_ID key → uid = sha1(JSON.stringify{g,p})[:16]. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct pref { const char *name, *host, *path; double lat, lon; const char *pref; };

static const struct pref PAGES[] = {
  {"警視庁","www.keishicho.metro.tokyo.lg.jp","/kurashi/higai/tokusyusagi/index.html",35.6783,139.7528,"東京都"},
  {"大阪府警察","www.police.pref.osaka.lg.jp","/seian/tokushusagi/index.html",34.6864,135.5197,"大阪府"},
  {"神奈川県警察","www.police.pref.kanagawa.jp","/mes/mesf2007.htm",35.4437,139.6380,"神奈川県"},
  {"愛知県警察","www.pref.aichi.jp","/police/anzen/sagi/index.html",35.1814,136.9069,"愛知県"},
  {"埼玉県警察","www.police.pref.saitama.lg.jp","/anzen/tokusagi/index.html",35.8617,139.6455,"埼玉県"},
  {"千葉県警察","www.police.pref.chiba.jp","/seianbu/tokusyusagi.html",35.6083,140.1233,"千葉県"},
  {"兵庫県警察","www.police.pref.hyogo.lg.jp","/seian/tokushu_sagi.htm",34.6913,135.1830,"兵庫県"},
  {"北海道警察","www.police.pref.hokkaido.lg.jp","/info/jiken/tokusyu/index.html",43.0628,141.3478,"北海道"},
  {"福岡県警察","www.police.pref.fukuoka.jp","/seikatsu_anzen/tokusyusagi/index.html",33.5904,130.4017,"福岡県"},
  {"京都府警察","www.pref.kyoto.jp","/fukei/anzen/tokushu_sagi.html",35.0116,135.7681,"京都府"},
};
#define NPAGES ((int)(sizeof PAGES / sizeof PAGES[0]))

/* digits-with-commas → long (strip ','); 0 if none. */
static long parse_num(const char *digits, int len) {
  long v = 0; int any = 0;
  for (int i = 0; i < len; i++) {
    char c = digits[i];
    if (c == ',') continue;
    if (c < '0' || c > '9') break;
    v = v * 10 + (c - '0'); any = 1;
  }
  return any ? v : -1;
}

/* JS m = html.match(/<label>[^<>]{0,40}?([0-9,]+)\s*<unit>/) — first occurrence
 * of `label`, then within next 40 chars (no '<' or '>') the first [0-9,]+ run
 * followed (after optional ws) by one of the unit strings. Sets out_val to
 * the parsed number and out_unit_idx to the matched units[] index.
 * units[] is NULL-terminated; suffix detected on the char after the digits. */
static int match_after_label(const char *html, const char *label,
                             const char *const *units,
                             long *out_val, int *out_unit_idx) {
  size_t llen = strlen(label);
  const char *p = html;
  while ((p = strstr(p, label)) != NULL) {
    const char *q = p + llen;
    /* scan up to 40 chars, no '<' or '>', looking for a digit start */
    const char *win_end = q;
    int w = 0;
    while (w < 40 && *win_end && *win_end != '<' && *win_end != '>') { win_end++; w++; }
    const char *d = q;
    while (d < win_end && !((*d >= '0' && *d <= '9'))) d++;
    if (d < win_end && *d >= '0' && *d <= '9') {
      const char *e = d;
      while (*e && ((*e >= '0' && *e <= '9') || *e == ',')) e++;
      long v = parse_num(d, (int)(e - d));
      if (v >= 0) {
        const char *u = e;
        while (*u == ' ' || *u == '\t' || *u == '\n' || *u == '\r') u++;
        for (int ui = 0; units[ui]; ui++) {
          if (strncmp(u, units[ui], strlen(units[ui])) == 0) {
            *out_val = v; *out_unit_idx = ui; return 1;
          }
        }
      }
    }
    p += llen;
  }
  return 0;
}

/* NPA gate page. The old /bureau/criminal/souni/tokushusagi/ path 404s (and
 * the tokusyusagi spelling 403s) since the NPA site restructure, so the gate
 * fetch returned NULL and the source returned -1 on every run without ever
 * reaching the prefectural pages. SOS47 is the live 特殊詐欺 landing page. */
#define NPA_GATE_URL "https://www.npa.go.jp/bureau/safetylife/sos47/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *idx_html = feed_get_text(ctx->http, NPA_GATE_URL, 8000);
  if (!idx_html) {
    fprintf(stderr, "[phone-scam-hotspots] NPA gate fetch failed: %s\n",
            NPA_GATE_URL);
    return -1;
  }
  if (!strstr(idx_html, "特殊詐欺") && !strstr(idx_html, "tokushusagi")
      && !strstr(idx_html, "振り込め")) { free(idx_html); return 0; }
  free(idx_html);

  cJSON *features = cJSON_CreateArray();
  int idx = 0;
  for (int i = 0; i < NPAGES; i++) {
    char url[256];
    snprintf(url, sizeof url, "https://%s%s", PAGES[i].host, PAGES[i].path);
    char *html = feed_get_text(ctx->http, url, 10000);
    if (!html) { fprintf(stderr, "[phone-scam-hotspots] unreachable: %s\n", url);
                 continue; }
    if (!strstr(html, "特殊詐欺") && !strstr(html, "被害")
        && !strstr(html, "認知件数")) {
      fprintf(stderr, "[phone-scam-hotspots] no scam content: %s\n", url);
      free(html); continue;
    }

    /* incidents: /認知件数[^<>]{0,40}?([0-9,]+)\s*件/ */
    long incidents = -1; int iu = 0;
    static const char *iu_units[] = { "件", NULL };
    int has_inc = match_after_label(html, "認知件数", iu_units, &incidents, &iu);

    /* damage: /被害(?:総)?額[^<>]{0,40}?([0-9,]+)\s*(?:円|万円|億円)/
     * then ×10000 if matched "万円", ×1e8 if "億円". The JS unit alternation
     * is `円|万円|億円`; on real pages the digits are immediately followed by
     * the full unit token, so probe 億円/万円 before plain 円 (longest-unit
     * precedence) to classify scale correctly. Try 被害総額 then 被害額. */
    long damage = -1; int du = 0;
    static const char *du_units[] = { "億円", "万円", "円", NULL };
    int has_dmg = match_after_label(html, "被害総額", du_units, &damage, &du);
    if (!has_dmg) has_dmg = match_after_label(html, "被害額", du_units, &damage, &du);
    if (has_dmg) {
      if (du == 0)      damage *= 100000000L;  /* 億円 */
      else if (du == 1) damage *= 10000L;      /* 万円 */
      /* du==2 → 円, ×1 */
    }

    if (!has_inc && !has_dmg) { free(html); continue; }
    idx++;

    cJSON *f = gj_point_feature(PAGES[i].lon, PAGES[i].lat);

    cJSON *p = cJSON_CreateObject();   /* EXACT JS key order */
    char wid[32]; snprintf(wid, sizeof wid, "LIVE_SCAM_%d", idx);
    cJSON_AddStringToObject(p, "ward_id", wid);
    cJSON_AddStringToObject(p, "ward", PAGES[i].name);
    cJSON_AddStringToObject(p, "prefecture", PAGES[i].pref);
    cJSON_AddItemToObject(p, "incidents_yr",
      has_inc ? cJSON_CreateNumber((double)incidents) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "damage_yen",
      has_dmg ? cJSON_CreateNumber((double)damage) : cJSON_CreateNull());
    cJSON_AddStringToObject(p, "source_url", url);
    cJSON_AddStringToObject(p, "country", "JP");
    cJSON_AddStringToObject(p, "source", "pref_police_html");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
    free(html);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[phone-scam-hotspots] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def phone_scam_hotspots_def = {
  .id = "phone-scam-hotspots", .collector = "crime",
  .name = "Phone Scam Hotspots", .name_ja = "特殊詐欺発生地点",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(phone_scam_hotspots_def)
