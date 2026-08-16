/* collectors/sources/denki_yoho.h — shared reader for the ten Japanese TSO
 * "denki-yoho" (電力需給) daily demand CSVs.
 *
 * WHY THIS EXISTS (audit slice 09, 2026-07-31):
 * the ten *_power.c collectors were byte-identical copies of the JS
 * makeDenkiYohoCollector port, and all ten were broken:
 *
 *   1. Nine of the ten CSV URLs 404 / no longer resolve. Every TSO moved to a
 *      DATE-STAMPED daily file (juyo_<area>_YYYYMMDD.csv); the old static
 *      "current" files were retired.
 *   2. The one URL that still answered (kansai juyo1_kansai.csv) has been
 *      frozen at 2025-12-25 for seven months, so it served stale data with a
 *      fresh `updated_at`.
 *   3. parse_last_load() split the row with strtok_r(",") — which COLLAPSES
 *      empty fields. The trailing 5-minute rows of the day are "DATE,HH:MM,,"
 *      so it saw two columns, took "23:55" as the last column and reported
 *      load_mw = 23. The published number was the minute field of a timestamp.
 *   4. No `title` property, so geojson_emit_features left intel_items.title
 *      NULL and the row was unreadable in every UI.
 *   5. `load_mw` was misnamed: the CSV unit is 万kW (10 MW), never MW.
 *   6. No published_at.
 *
 * The CSV (Shift_JIS, CRLF) carries two data sections, each introduced by an
 * ASCII "DATE,TIME" header:
 *   DATE,TIME,当日実績(万kW),予想値(万kW),使用率(%),供給力(万kW)        hourly
 *   DATE,TIME,当日実績(５分間隔値)(万kW),太陽光発電実績(５分間隔値)(万kW) 5-minute
 * Field 3 is the ACTUAL demand in both. We take the most recent row that has a
 * positive actual, preferring the 5-minute section, and carry the hourly
 * usage-rate / supply-capacity alongside it.
 *
 * Only ASCII is ever copied out of the CSV (the data rows are pure ASCII);
 * the Shift_JIS header text is never persisted, so nothing here can put
 * non-UTF-8 bytes into a TEXT column. */
#ifndef JO_DENKI_YOHO_H
#define JO_DENKI_YOHO_H

#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include "lib/probe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  const char *source_id;
  const char *meta_src;      /* JS collector name, kept in properties.source */
  const char *operator_ja;
  const char *area_code;     /* OCCTO area code, "01".."10" */
  const char *url_fmt;       /* one %s = YYYYMMDD (JST), or a static URL */
  double lat, lon;           /* control-centre / HQ coordinates */
} denki_cfg;

typedef struct {
  int    ok;
  char   date[16], time[16];
  double demand;             /* 当日実績, 万kW */
  int    has_solar;  double solar;      /* 太陽光発電実績, 万kW */
  int    has_rate;   double usage_pct;  /* 使用率, %            */
  int    has_supply; double supply;     /* 供給力, 万kW         */
  int    interval_min;
  char   raw[192];
} denki_obs;

/* Split `line` in place on ',' WITHOUT collapsing empty fields (the bug that
 * made strtok_r report a clock minute as a megawatt figure). */
static int dy_split(char *line, char **f, int maxf) {
  int n = 0; char *p = line;
  f[n++] = p;
  while (*p && n < maxf) {
    if (*p == ',') { *p = 0; f[n++] = p + 1; }
    p++;
  }
  return n;
}

static void dy_rstrip(char *s) {
  size_t n = strlen(s);
  while (n && (s[n-1]=='\r' || s[n-1]=='\n' || s[n-1]==' ' || s[n-1]=='\t'))
    s[--n] = 0;
}

/* "2026/8/1" — digits and two slashes, nothing else. */
static int dy_is_date(const char *s) {
  int sl = 0, dg = 0;
  for (; *s; s++) {
    if (*s == '/') sl++;
    else if (*s >= '0' && *s <= '9') dg++;
    else return 0;
  }
  return sl == 2 && dg >= 4;
}

/* "2:55" / "02:55" */
static int dy_is_time(const char *s) {
  int co = 0, dg = 0;
  for (; *s; s++) {
    if (*s == ':') co++;
    else if (*s >= '0' && *s <= '9') dg++;
    else return 0;
  }
  return co == 1 && dg >= 2;
}

/* Finite number in the field? Empty / non-numeric → 0. */
static int dy_num(const char *s, double *out) {
  if (!s || !*s) return 0;
  char *end; double v = strtod(s, &end);
  if (end == s || v != v) return 0;
  while (*end == ' ' || *end == '\r') end++;
  if (*end) return 0;
  *out = v; return 1;
}

static void dy_ascii_copy(char *dst, size_t n, const char *src) {
  size_t o = 0;
  for (; *src && o + 1 < n; src++)
    if ((unsigned char)*src >= 0x20 && (unsigned char)*src < 0x7f) dst[o++] = *src;
  dst[o] = 0;
}

/* Walk every "DATE,TIME" section; keep the last row with a positive actual.
 * A 4-column section is the 5-minute feed (field 4 = solar), a 6-column one
 * is hourly (fields 5/6 = usage rate / supply capacity). The 5-minute section
 * wins when both have data because it is the fresher observation. */
static int denki_parse(char *csv, denki_obs *o) {
  memset(o, 0, sizeof *o);
  denki_obs best5 = {0}, best60 = {0};
  int cols = 0;
  char *save = NULL;
  for (char *ln = strtok_r(csv, "\n", &save); ln;
       ln = strtok_r(NULL, "\n", &save)) {
    dy_rstrip(ln);
    if (!*ln) continue;
    if (strncmp(ln, "DATE,TIME", 9) == 0) {   /* rest of the header is SJIS */
      cols = 1; for (const char *p = ln; *p; p++) if (*p == ',') cols++;
      continue;
    }
    if (!cols) continue;
    char buf[512];
    size_t bl = strlen(ln); if (bl >= sizeof buf) bl = sizeof buf - 1;
    memcpy(buf, ln, bl); buf[bl] = 0;
    char *f[16]; int nf = dy_split(buf, f, 16);
    if (nf < 3) continue;
    if (!dy_is_date(f[0]) || !dy_is_time(f[1])) continue;
    double actual;
    if (!dy_num(f[2], &actual) || actual <= 0) continue;

    denki_obs c = {0};
    c.ok = 1;
    snprintf(c.date, sizeof c.date, "%s", f[0]);
    snprintf(c.time, sizeof c.time, "%s", f[1]);
    c.demand = actual;
    dy_ascii_copy(c.raw, sizeof c.raw, ln);
    if (nf >= 6) {                       /* hourly */
      c.interval_min = 60;
      c.has_rate   = dy_num(f[4], &c.usage_pct);
      c.has_supply = dy_num(f[5], &c.supply);
      best60 = c;
    } else {                             /* 5-minute */
      c.interval_min = 5;
      c.has_solar = dy_num(f[3], &c.solar);
      best5 = c;
    }
  }
  if (best5.ok) {
    *o = best5;
    /* carry the hourly context onto the fresher 5-minute observation */
    if (best60.ok) {
      o->has_rate = best60.has_rate;     o->usage_pct = best60.usage_pct;
      o->has_supply = best60.has_supply; o->supply    = best60.supply;
    }
    return 1;
  }
  if (best60.ok) { *o = best60; return 1; }
  return 0;
}

/* "2026/8/1" + "2:55" → "2026-08-01T02:55:00+09:00" (the CSVs are JST). */
static void dy_iso_jst(const denki_obs *o, char *out, size_t n) {
  int y = 0, mo = 0, d = 0, h = 0, mi = 0;
  if (sscanf(o->date, "%d/%d/%d", &y, &mo, &d) != 3 ||
      sscanf(o->time, "%d:%d", &h, &mi) != 2) { out[0] = 0; return; }
  snprintf(out, n, "%04d-%02d-%02dT%02d:%02d:00+09:00", y, mo, d, h, mi);
}

/* One run(): resolve today's JST file (falling back to yesterday's before the
 * new file is cut just after midnight JST), parse, emit ONE point. */
static int denki_run(const source_ctx *ctx, intel_sink *sink,
                     const denki_cfg *cfg) {
  int dated = strstr(cfg->url_fmt, "%s") != NULL;
  char url[512] = {0};
  denki_obs obs = {0};
  time_t now = time(NULL);

  for (int back = 0; back < (dated ? 2 : 1) && !obs.ok; back++) {
    if (dated) {
      time_t t = now + 9 * 3600 - (time_t)back * 86400;
      struct tm tm; gmtime_r(&t, &tm);
      char ymd[16]; strftime(ymd, sizeof ymd, "%Y%m%d", &tm);
      snprintf(url, sizeof url, cfg->url_fmt, ymd);
    } else {
      snprintf(url, sizeof url, "%s", cfg->url_fmt);
    }
    char *csv = feed_get_text(ctx->http, url, 10000);
    if (!csv) continue;
    denki_parse(csv, &obs);
    free(csv);
  }

  if (!obs.ok) {
    fprintf(stderr, "[%s] %s_unavailable (%s)\n",
            cfg->source_id, cfg->meta_src, url);
    return 0;                            /* honest empty, never rc=-1 */
  }

  char iso[40]; dy_iso_jst(&obs, iso, sizeof iso);
  char now_iso[32]; probe_iso_now(now_iso, sizeof now_iso);
  double demand_mw = obs.demand * 10.0;  /* 万kW → MW */

  char title[256];
  snprintf(title, sizeof title, "%s 電力需要 %.0f MW (%s JST)",
           cfg->operator_ja, demand_mw,
           iso[0] ? iso : obs.date);

  cJSON *features = cJSON_CreateArray();
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *co = cJSON_CreateArray();
  cJSON_AddItemToArray(co, cJSON_CreateNumber(cfg->lon));
  cJSON_AddItemToArray(co, cJSON_CreateNumber(cfg->lat));
  cJSON_AddItemToObject(g, "coordinates", co);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();
  /* One stable row per utility that UPDATES in place: without a natural id
   * geojson.c hashes {geometry,properties}, and properties carry the reading,
   * so every 300s poll used to mint a brand-new intel row. */
  char uid[96]; snprintf(uid, sizeof uid, "%s-current", cfg->source_id);
  cJSON_AddStringToObject(p, "uid", uid);
  cJSON_AddStringToObject(p, "source_id", cfg->source_id);
  cJSON_AddStringToObject(p, "record_type", "power-demand");
  cJSON_AddStringToObject(p, "title", title);
  cJSON_AddStringToObject(p, "operator", cfg->operator_ja);
  cJSON_AddStringToObject(p, "area_code", cfg->area_code);
  cJSON_AddNumberToObject(p, "demand_mw", demand_mw);
  cJSON_AddNumberToObject(p, "demand_man_kw", obs.demand);
  cJSON_AddNumberToObject(p, "load_mw", demand_mw);   /* legacy key, now real */
  if (obs.has_solar)  cJSON_AddNumberToObject(p, "solar_mw", obs.solar * 10.0);
  if (obs.has_rate)   cJSON_AddNumberToObject(p, "usage_rate_pct", obs.usage_pct);
  if (obs.has_supply) {
    cJSON_AddNumberToObject(p, "supply_capacity_mw", obs.supply * 10.0);
    if (obs.supply > 0)
      cJSON_AddNumberToObject(p, "reserve_margin_pct",
                              (obs.supply - obs.demand) / obs.supply * 100.0);
  }
  cJSON_AddNumberToObject(p, "interval_min", obs.interval_min);
  if (iso[0]) {
    cJSON_AddStringToObject(p, "observed_at", iso);
    cJSON_AddStringToObject(p, "published_at", iso);
  }
  cJSON_AddStringToObject(p, "raw", obs.raw);
  cJSON_AddStringToObject(p, "feed_url", url);
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "updated_at", now_iso);
  cJSON_AddStringToObject(p, "source", cfg->meta_src);
  /* The Point above is cfg->lat/lon — the utility's control centre / HQ, as
   * the struct comment says. The reading it carries is a whole-service-area
   * aggregate covering millions of people, so this row was pinning region-wide
   * demand to Tokyo / Nagoya / Osaka / Fukuoka every 300 s with nothing to say
   * so. The pin stays (it is a real place and the row needs somewhere to sit)
   * but it now carries the unified provenance vocabulary, and never "exact". */
  cJSON_AddStringToObject(p, "geo_provenance", "utility-control-centre");
  cJSON_AddStringToObject(p, "geo_precision", "service-area");
  cJSON_AddBoolToObject(p, "geo_uncertain", 1);
  cJSON_AddStringToObject(p, "geo_note",
    "demand is a whole-service-area aggregate; the point is the utility's "
    "control centre, not where the load is");
  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("power"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("electricity-demand"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("japan"));
  cJSON_AddItemToObject(p, "tags", tags);
  cJSON_AddItemToObject(f, "properties", p);
  cJSON_AddItemToArray(features, f);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[%s] emitted %d demand=%.0fMW at %s\n",
          cfg->source_id, n, demand_mw, iso);
  return n >= 0 ? 0 : -1;
}

#endif
