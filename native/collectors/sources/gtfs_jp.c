/* collectors/transport/sources/gtfs_jp.c — port of
 * server/src/collectors/gtfsJp.js. GTFS-JP nationwide bus aggregator. Ports
 * the LIVE path (tryLive): list orgs via api.gtfs-data.jp/v2/organizations,
 * per-org list feeds, fetch each feed's stops.txt and emit GeoJSON Point
 * features. The OSM fetchOverpassTiled fallback (tryOsmFallback) is the
 * offline/no-network branch and is dropped per the seed/fallback rule (live
 * rows only). The naive comma-split of stops.txt is mirrored verbatim (JS
 * does not use a quoted-CSV parser for the data rows). _meta dropped.
 *   props order: stop_id, name, operator, feed_id, source */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_BASE "https://api.gtfs-data.jp/v2"

/* org.X || org.Y || ... → first present non-empty string field */
static const char *jstr3(cJSON *o, const char *a, const char *b, const char *c) {
  const char *keys[3] = { a, b, c };
  for (int i = 0; i < 3; i++) {
    if (!keys[i]) continue;
    cJSON *v = cJSON_GetObjectItem(o, keys[i]);
    if (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
      return v->valuestring;
    if (v && cJSON_IsNumber(v)) {
      static char nb[32]; snprintf(nb, sizeof nb, "%g", v->valuedouble);
      return nb;
    }
  }
  return NULL;
}

/* strip one leading and one trailing '"' then trim ASCII ws; in place */
static char *strip_q_trim(char *s) {
  size_t L = strlen(s);
  if (L >= 1 && s[0] == '"') { memmove(s, s + 1, L); L--; }
  if (L >= 1 && s[L-1] == '"') s[--L] = 0;
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
  L = strlen(s);
  while (L && (s[L-1]==' '||s[L-1]=='\t'||s[L-1]=='\r'||s[L-1]=='\n')) s[--L]=0;
  return s;
}

/* remove all '"' from s in place (JS .replace(/"/g,'')) */
static void strip_all_q(char *s) {
  char *o = s;
  for (char *p = s; *p; p++) if (*p != '"') *o++ = *p;
  *o = 0;
}

static int finite_dbl(const char *s, double *out) {
  if (!s || !s[0]) return 0;
  char *end;
  double v = strtod(s, &end);
  if (end == s) return 0;
  *out = v;
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  long feed_limit = 20, stop_cap = 50000;
  const char *e1 = getenv("GTFS_JP_FEED_LIMIT");
  if (e1 && e1[0]) { long v = strtol(e1, NULL, 10); feed_limit = v; }
  const char *e2 = getenv("GTFS_JP_STOP_CAP");
  if (e2 && e2[0]) { long v = strtol(e2, NULL, 10); stop_cap = v; }

  cJSON *orgsw = feed_get_json(ctx->http, API_BASE "/organizations", 15000);
  cJSON *orgs = orgsw ? cJSON_GetObjectItem(orgsw, "body") : NULL;
  if (!orgs || !cJSON_IsArray(orgs) || cJSON_GetArraySize(orgs) == 0) {
    if (orgsw) cJSON_Delete(orgsw);
    return -1;                          /* tryLive null; fallback dropped */
  }

  cJSON *features = cJSON_CreateArray();
  long feeds_processed = 0;
  int stop_total = 0;
  int done = 0;

  cJSON *org;
  cJSON_ArrayForEach(org, orgs) {
    if (done) break;
    const char *orgId = jstr3(org, "organization_id", "id", "organizationID");
    const char *orgName = jstr3(org, "organization_name", "name", NULL);
    if (!orgName) orgName = orgId;
    if (!orgId) continue;

    char furl[256];
    snprintf(furl, sizeof furl, API_BASE "/organizations/%s/feeds", orgId);
    cJSON *fw = feed_get_json(ctx->http, furl, 15000);
    cJSON *feeds = fw ? cJSON_GetObjectItem(fw, "body") : NULL;
    if (feeds && cJSON_IsArray(feeds)) {
      cJSON *feed;
      cJSON_ArrayForEach(feed, feeds) {
        const char *feedId = jstr3(feed, "feed_id", "id", NULL);
        if (!feedId) continue;

        char surl[320];
        snprintf(surl, sizeof surl,
                 API_BASE "/organizations/%s/feeds/%s/files/stops.txt",
                 orgId, feedId);
        char *text = feed_get_text(ctx->http, surl, 20000);
        if (text && text[0]) {
          /* split on \r?\n, drop empty lines */
          char *save = text;
          char **lines = NULL; int nlines = 0, cap = 0;
          for (char *ln = text; *ln; ) {
            char *nl = ln;
            while (*nl && *nl != '\n' && *nl != '\r') nl++;
            int empty = (nl == ln);
            char term = *nl;
            if (*nl) { *nl = 0; }
            if (!empty) {
              if (nlines == cap) {
                cap = cap ? cap * 2 : 64;
                lines = realloc(lines, cap * sizeof(char *));
              }
              lines[nlines++] = ln;
            }
            if (!term) break;
            ln = nl + 1;
            if (term == '\r' && *ln == '\n') ln++;
          }
          (void)save;

          if (nlines >= 2) {
            /* header split on ',' with quote-strip+trim per cell */
            char *hdr = strdup(lines[0]);
            int iId = -1, iName = -1, iLat = -1, iLon = -1, hc = 0;
            for (char *tok = hdr, *p = hdr;; p++) {
              if (*p == ',' || *p == 0) {
                char saved = *p; *p = 0;
                char *cell = strip_q_trim(tok);
                if (strcmp(cell, "stop_id") == 0 && iId < 0) iId = hc;
                else if (strcmp(cell, "stop_name") == 0 && iName < 0) iName = hc;
                else if (strcmp(cell, "stop_lat") == 0 && iLat < 0) iLat = hc;
                else if (strcmp(cell, "stop_lon") == 0 && iLon < 0) iLon = hc;
                hc++;
                if (saved == 0) break;
                tok = p + 1;
              }
            }
            free(hdr);

            if (iLat >= 0 && iLon >= 0) {
              for (int r = 1; r < nlines && !done; r++) {
                /* naive split on ',' (mirrors JS lines[r].split(',')) */
                char *row = strdup(lines[r]);
                char *cols[64]; int nc = 0;
                cols[nc++] = row;
                for (char *p = row; *p && nc < 64; p++) {
                  if (*p == ',') { *p = 0; cols[nc++] = p + 1; }
                }
                const char *cLat = (iLat < nc) ? cols[iLat] : "";
                const char *cLon = (iLon < nc) ? cols[iLon] : "";
                double lat = 0, lon = 0;
                int geocoded = finite_dbl(cLat, &lat) &&
                               finite_dbl(cLon, &lon);

                cJSON *f = cJSON_CreateObject();
                cJSON_AddStringToObject(f, "type", "Feature");
                if (geocoded) {
                  cJSON *g = cJSON_CreateObject();
                  cJSON_AddStringToObject(g, "type", "Point");
                  cJSON *co = cJSON_CreateArray();
                  cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
                  cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
                  cJSON_AddItemToObject(g, "coordinates", co);
                  cJSON_AddItemToObject(f, "geometry", g);
                } else {
                  cJSON_AddItemToObject(f, "geometry", cJSON_CreateNull());
                }
                cJSON *pr = cJSON_CreateObject();   /* EXACT JS key order */
                /* stop_id = `GTFSJP_${orgId}_${(cols[iId]||r).replace(/"/g,'')}` */
                char idpart[256];
                if (iId >= 0 && iId < nc && cols[iId][0]) {
                  snprintf(idpart, sizeof idpart, "%s", cols[iId]);
                } else {
                  snprintf(idpart, sizeof idpart, "%d", r);
                }
                strip_all_q(idpart);
                char sid[384];
                snprintf(sid, sizeof sid, "GTFSJP_%s_%s", orgId, idpart);
                cJSON_AddStringToObject(pr, "stop_id", sid);
                /* name = (cols[iName]||'').replace(/^"|"$/g,'') || null */
                char *nm = NULL;
                if (iName >= 0 && iName < nc) {
                  nm = strdup(cols[iName]);
                  size_t L = strlen(nm);
                  if (L && nm[0] == '"') { memmove(nm, nm + 1, L); L--; }
                  if (L && nm[L-1] == '"') nm[--L] = 0;
                }
                if (nm && nm[0]) cJSON_AddStringToObject(pr, "name", nm);
                else cJSON_AddNullToObject(pr, "name");
                free(nm);
                /* operator = source_label || orgId */
                cJSON_AddStringToObject(pr, "operator",
                                        orgName ? orgName : orgId);
                cJSON_AddStringToObject(pr, "feed_id", feedId);
                cJSON_AddStringToObject(pr, "source", "gtfs_jp");
                cJSON_AddItemToObject(f, "properties", pr);
                cJSON_AddItemToArray(features, f);
                stop_total++;
                free(row);
                if (stop_total >= stop_cap) done = 1;
              }
            }
          }
          free(lines);
        }
        free(text);

        feeds_processed++;
        if (feeds_processed >= feed_limit) { done = 1; }
        if (stop_total >= stop_cap) { done = 1; }
        if (done) break;
      }
    }
    if (fw) cJSON_Delete(fw);
  }

  cJSON_Delete(orgsw);

  /* JS: allStops.length ? slice(STOP_CAP) : null  (cap already enforced) */
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[gtfs-jp] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def gtfs_jp_def = {
  .id = "gtfs-jp", .collector = "transport",
  .name = "GTFS-JP Nationwide Bus", .name_ja = "GTFS-JP Nationwide Bus",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(gtfs_jp_def)
