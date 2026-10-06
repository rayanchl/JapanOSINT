/* US EPA Greenhouse Gas Reporting Program facility emissions (Envirofacts).
 * Endpoints (keyless REST, row window is MANDATORY in the path grammar
 *   /efservice/<table>/<col>/<value>/rows/<start>:<end>/JSON):
 *   https://data.epa.gov/efservice/pub_facts_sector_ghg_emission/year/2023/rows/0:1999/JSON
 *   https://data.epa.gov/efservice/pub_dim_facility/year/2023/rows/0:1999/JSON
 * Emits one row per (facility, sector, subsector, gas) reported: co2e_emission
 * (UNIT: metric tonnes CO2e; JSON null where the row reports none — never 0),
 * reporting year, sector_id/subsector_id/gas_id, facility name, NAICS code and
 * parent company, joined on facility_id.
 *
 * Both tables are windowed by the API itself, so a single window silently
 * caps the collector well below the true size of either table (house rule
 * 2). Both are walked whole, sized from each table's own /count (see
 * walk_table); a window that fails is disclosed in a collector-truncation-
 * notice scoped to its table, and everything else is still emitted.
 *
 * GEO (R2): latitude/longitude come from pub_dim_facility only, and a row with
 * fac latitude/longitude of 0 (a real defect in that table) is emitted WITHOUT
 * geometry rather than pinned to Null Island.
 * Licence: US EPA Envirofacts, public domain, keyless. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/keyqual.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "epa-ghgrp-emissions"
#define YEAR "2023"

#define EF "https://data.epa.gov/efservice/"
#define FAC_TABLE "pub_dim_facility/year/" YEAR
#define EM_TABLE  "pub_facts_sector_ghg_emission/year/" YEAR

/* Rows per request. Envirofacts' window `rows/<a>:<b>` is INCLUSIVE at both
 * ends — rows/0:3 returns four rows — so a page is a:a+PAGE-1. This used to
 * ask for 0:1000, 1000:2000, … : 1,001 rows a page, each page overlapping the
 * last by one, every boundary row fetched twice and counted twice. */
#define PAGE 2000
/* Only used when the table's own /count could not be read: the walk then
 * stops on the first short page, and this is the ceiling on a walk that never
 * sees one. Hitting it is disclosed. */
#define WALK_GUARD_PAGES 500   /* exhaustive-ok: runaway guard used only when /count failed; hitting it emits a scoped collector-truncation-notice */

typedef struct { long long id; double lat, lon; int has_geo;
                 char *name, *naics, *parent, *state;
                 const cJSON *row; } fac_t;

static long long iv(const cJSON *o, const char *k, long long dflt) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (cJSON_IsNumber(v)) return (long long)v->valuedouble;
  if (cJSON_IsString(v) && v->valuestring[0]) return atoll(v->valuestring);
  return dflt;
}

static char *dupsv(const cJSON *o, const char *k) {
  const char *s = jo_sv(o, k);
  return (s && s[0]) ? strdup(s) : NULL;
}

static int fac_cmp(const void *a, const void *b) {
  long long x = ((const fac_t *)a)->id, y = ((const fac_t *)b)->id;
  return (x > y) - (x < y);
}

static const fac_t *find_fac(const fac_t *f, int n, long long id) {
  fac_t k = {0};
  k.id = id;
  return n ? bsearch(&k, f, (size_t)n, sizeof *f, fac_cmp) : NULL;
}

static void free_facs(fac_t *facs, int n) {
  for (int i = 0; i < n; i++) {
    free(facs[i].name); free(facs[i].naics);
    free(facs[i].parent); free(facs[i].state);
  }
  free(facs);
}

/* The table's own row count (`<table>/count/JSON` →
 * [{"TOTALQUERYRESULTS": N}]), or -1 when it could not be read. */
static long table_count(const source_ctx *ctx, const char *table) {
  char url[256];
  snprintf(url, sizeof url, EF "%s/count/JSON", table);
  cJSON *d = feed_get_json(ctx->http, url, 60000);
  long n = -1;
  const cJSON *o = cJSON_IsArray(d) ? cJSON_GetArrayItem(d, 0) : d;  /* exhaustive-ok: /count answers a one-element array holding the total */
  const cJSON *v = o ? cJSON_GetObjectItem(o, "TOTALQUERYRESULTS") : NULL;
  if (cJSON_IsNumber(v) && v->valuedouble >= 0) n = (long)v->valuedouble;
  cJSON_Delete(d);
  return n;
}

/* One table, walked whole. Every page that comes back is appended to `pages`
 * (a JSON array of page arrays, kept alive so records can point into it).
 *
 * The walk is SIZED FROM THE TABLE'S OWN COUNT: it asks for every window up to
 * `count`, and a page that fails is recorded and stepped over rather than
 * ending the run — a transient failure on one 2,000-row page used to make
 * run() return -1 before emitting anything, discarding everything already
 * fetched (456 s of it, once). Only when /count is unreadable does it fall
 * back to "stop on a short page", where a failure has to stop the walk because
 * nothing says what lies beyond it.
 *
 * `w` reports the declared count, rows read, pages tried / failed, and
 * whether the no-count guard fired. */
typedef struct { long count, rows; int failed_pages, capped, pages; } walk_t;

static void walk_table(const source_ctx *ctx, const char *table, cJSON *pages,
                       walk_t *w) {
  memset(w, 0, sizeof *w);
  w->count = table_count(ctx, table);
  long npages = w->count >= 0 ? (w->count + PAGE - 1) / PAGE : WALK_GUARD_PAGES;
  for (long page = 0; ; page++) {
    if (page >= npages) {
      if (w->count < 0) w->capped = 1;     /* guard hit with no count to trust */
      break;
    }
    long long start = (long long)page * PAGE, end = start + PAGE - 1;
    char url[320];
    snprintf(url, sizeof url, EF "%s/rows/%lld:%lld/JSON", table, start, end);
    cJSON *doc = feed_get_json(ctx->http, url, 60000);
    w->pages++;
    if (!cJSON_IsArray(doc)) {
      cJSON_Delete(doc);
      fprintf(stderr, "[" SRC "] %s rows %lld:%lld failed\n", table, start, end);
      w->failed_pages++;
      if (w->count < 0) break;             /* no count: cannot know what follows */
      continue;
    }
    int got = cJSON_GetArraySize(doc);
    w->rows += got;
    cJSON_AddItemToArray(pages, doc);
    if (got < PAGE) {
      if (w->count < 0 || start + got >= w->count) break;   /* the end */
      /* a short page before the declared end: keep walking to the count */
    } else if (w->count >= 0 && page + 1 >= npages) {
      npages++;                            /* the table grew mid-walk */
    }
  }
}

static void walk_notice(intel_sink *sink, const char *table, const walk_t *w,
                        long used, const char *what) {
  if (!w->failed_pages && !w->capped && (w->count < 0 || w->rows >= w->count))
    return;
  char reason[384], ep[160];
  snprintf(ep, sizeof ep, EF "%s/rows/<a>:<b>/JSON", table);
  if (w->failed_pages)
    snprintf(reason, sizeof reason,
             "%d of %d %s pages (%d rows each) failed to fetch; %ld of %s rows "
             "were read and everything read was used",
             w->failed_pages, w->pages, what, PAGE, w->rows,
             w->count >= 0 ? "the declared" : "an unknown number of");
  else if (w->capped)
    snprintf(reason, sizeof reason,
             "the %s table's /count was unreadable and the short-page walk hit "
             "WALK_GUARD_PAGES (%d) first", what, WALK_GUARD_PAGES);
  else
    snprintf(reason, sizeof reason,
             "the %s table declared %ld rows and the walk read %ld", what,
             w->count, w->rows);
  jo_trunc_notice_scoped(sink, SRC, table, ep, used,
                         w->count >= 0 ? w->count : -1, reason,
                         "re-run; failed windows are fetched again on the next "
                         "pass");
}

/* Emission rows have no id of their own. (facility, sector, subsector, gas) is
 * the record — `facility|year|gas` was not: one live page of 918 rows keyed
 * onto 880 uids, because a facility reports the same gas under several
 * subsectors. Even the full tuple repeats on 3 of the 26,272 rows of 2023
 * (same facility/sector/subsector/gas, different tonnage), so a key that
 * recurs within the run qualifies EVERY member of its group by the reported
 * value ("…|co2e=<t>", or "…|co2e=null" for a row reporting none), not just
 * the second one — nothing is merged that differs, and which member gets the
 * plain key does not depend on the order the API served them in. Where the
 * value is shared inside a group too, the member is qualified by a hash of the
 * row (lib/keyqual.h). */
typedef struct { char key[96]; const cJSON *row; } em_t;

static int em_cmp(const void *a, const void *b) {
  return strcmp(((const em_t *)a)->key, ((const em_t *)b)->key);
}

/* The reported value as a discriminator: "co2e=<t>" or "co2e=null". */
static const char *em_disc(const cJSON *e, char *buf, size_t cap) {
  const cJSON *q = cJSON_GetObjectItem(e, "co2e_emission");
  if (cJSON_IsNumber(q)) snprintf(buf, cap, "co2e=%.17g", q->valuedouble);
  else snprintf(buf, cap, "co2e=null");
  return buf;
}

static void em_key(const cJSON *e, char *out, size_t cap) {
  snprintf(out, cap, "%lld|%s|%lld|%lld|%lld", iv(e, "facility_id", -1), YEAR,
           iv(e, "sector_id", -1), iv(e, "subsector_id", -1), iv(e, "gas_id", -1));
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* ── facility dimension: the reporting year's own rows ──────────────────
   * pub_dim_facility holds one row per facility PER YEAR (136,005 rows across
   * all years). Only the reporting year's are joined, which is both the right
   * row — the name/parent/NAICS the facility reported in 2023 — and 11,281
   * rows instead of 136,005; every facility_id in the 2023 emission table has
   * one (checked live, 2026-10-02: 11,235 of 11,235). */
  cJSON *fac_pages = cJSON_CreateArray();
  walk_t fw;
  walk_table(ctx, FAC_TABLE, fac_pages, &fw);
  int fn = 0, fcap = 0;
  fac_t *facs = NULL;
  const cJSON *pg, *f;
  cJSON_ArrayForEach(pg, fac_pages) cJSON_ArrayForEach(f, pg) {
    long long id = iv(f, "facility_id", -1);
    if (id < 0) continue;
    if (fn == fcap) {
      int nc = fcap ? fcap * 2 : 1024;
      fac_t *nf = realloc(facs, (size_t)nc * sizeof *nf);
      if (!nf) break;
      facs = nf; fcap = nc;
    }
    fac_t *e = &facs[fn++];
    memset(e, 0, sizeof *e);
    e->id = id;
    e->row = f;
    e->name   = dupsv(f, "facility_name");
    e->naics  = dupsv(f, "naics_code");
    e->parent = dupsv(f, "parent_company");
    e->state  = dupsv(f, "state");
    cJSON *la = cJSON_GetObjectItem(f, "latitude");
    cJSON *lo = cJSON_GetObjectItem(f, "longitude");
    if (cJSON_IsNumber(la) && cJSON_IsNumber(lo)) {
      e->lat = la->valuedouble; e->lon = lo->valuedouble;
      /* zero coordinates are a known defect in this table — not a location */
      if (e->lat != 0.0 && e->lon != 0.0 &&
          e->lat >= -90 && e->lat <= 90 && e->lon >= -180 && e->lon <= 180)
        e->has_geo = 1;
    }
  }
  if (fn) qsort(facs, (size_t)fn, sizeof *facs, fac_cmp);

  /* ── emission facts ───────────────────────────────────────────────────── */
  cJSON *em_pages = cJSON_CreateArray();
  walk_t ew;
  walk_table(ctx, EM_TABLE, em_pages, &ew);
  if (ew.rows == 0) {
    walk_notice(sink, EM_TABLE, &ew, 0, "emission");
    walk_notice(sink, FAC_TABLE, &fw, fn, "facility");
    fprintf(stderr, "[" SRC "] no emission rows fetched\n");
    free_facs(facs, fn); cJSON_Delete(fac_pages); cJSON_Delete(em_pages);
    return -1;
  }

  em_t *ems = malloc((size_t)ew.rows * sizeof *ems);
  if (!ems) { free_facs(facs, fn); cJSON_Delete(fac_pages); cJSON_Delete(em_pages); return -1; }
  long ne = 0;
  const cJSON *e;
  keyqual kq = {0};
  cJSON_ArrayForEach(pg, em_pages) cJSON_ArrayForEach(e, pg) {
    em_key(e, ems[ne].key, sizeof ems[ne].key);
    ems[ne].row = e;
    char db[48];
    keyqual_add(&kq, ems[ne].key, em_disc(e, db, sizeof db));
    ne++;
  }
  qsort(ems, (size_t)ne, sizeof *ems, em_cmp);
  keyqual_seal(&kq);

  /* A row whose co2e_emission is null is still a reported (facility, sector,
   * subsector, gas) row — 3,839 of 2023's were discarded here as "no
   * measurement". It is emitted with the value stated as null (never 0) and
   * the title saying it was not reported. */
  int n = 0, unjoined = 0, null_co2e = 0, folded = 0;
  for (long i = 0; i < ne; i++) {
    e = ems[i].row;
    long long fid = iv(e, "facility_id", -1);
    cJSON *q = cJSON_GetObjectItem(e, "co2e_emission");
    int has_q = cJSON_IsNumber(q);
    const fac_t *fa = fid >= 0 ? find_fac(facs, fn, fid) : NULL;

    char kbuf[256], db[48];
    const char *key = ems[i].key;
    if (keyqual_count(&kq, key) > 1) {
      char *raw = cJSON_PrintUnformatted(e);
      key = keyqual_uid(&kq, ems[i].key, em_disc(e, db, sizeof db),
                        raw ? raw : "", kbuf, sizeof kbuf);
      free(raw);
      if (!keyqual_claim(&kq, ems[i].key, key)) { folded++; continue; }
    }
    if (!fa) unjoined++;
    if (!has_q) null_co2e++;

    cJSON *p = cJSON_CreateObject();
    if (fid >= 0) cJSON_AddNumberToObject(p, "facility_id", (double)fid);
    else          cJSON_AddNullToObject(p, "facility_id");
    if (fa && fa->name)   cJSON_AddStringToObject(p, "facility_name", fa->name);
    if (fa && fa->state)  cJSON_AddStringToObject(p, "state", fa->state);
    if (fa && fa->naics)  cJSON_AddStringToObject(p, "naics_code", fa->naics);
    if (fa && fa->parent) cJSON_AddStringToObject(p, "parent_company", fa->parent);
    if (has_q) cJSON_AddNumberToObject(p, "co2e_emission", q->valuedouble);
    else       cJSON_AddNullToObject(p, "co2e_emission");   /* not reported: null, never 0 */
    cJSON_AddStringToObject(p, "unit", "tonnes CO2e");
    cJSON_AddNumberToObject(p, "reporting_year", (double)iv(e, "year", 0));
    cJSON_AddNumberToObject(p, "sector_id", (double)iv(e, "sector_id", -1));
    cJSON_AddNumberToObject(p, "subsector_id", (double)iv(e, "subsector_id", -1));
    cJSON_AddNumberToObject(p, "gas_id", (double)iv(e, "gas_id", -1));
    /* the whole facility row we paid for — address, county, FRS id, reported
     * subparts, monitoring plan … — not just the six fields picked above */
    if (fa && fa->row) cJSON_AddItemToObject(p, "facility", cJSON_Duplicate(fa->row, 1));
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[288];
    if (has_q)
      snprintf(title, sizeof title, "%s (GHGRP %lld) %s: %.1f t CO2e",
               fa && fa->name ? fa->name : "US GHGRP facility", fid, YEAR,
               q->valuedouble);
    else
      snprintf(title, sizeof title, "%s (GHGRP %lld) %s: CO2e not reported",
               fa && fa->name ? fa->name : "US GHGRP facility", fid, YEAR);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = "https://www.epa.gov/ghgreporting";
    row.record_type     = "emitting-facility";
    row.has_geo         = fa ? fa->has_geo : 0;
    row.lat = fa ? fa->lat : 0; row.lon = fa ? fa->lon : 0;
    row.properties_json = pj;
    row.tags_json       = "[\"industry\",\"emissions\",\"epa\",\"usa\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }

  walk_notice(sink, FAC_TABLE, &fw, fn, "facility");
  walk_notice(sink, EM_TABLE, &ew, n, "emission");

  keyqual_free(&kq);
  free(ems);
  free_facs(facs, fn);
  cJSON_Delete(fac_pages);
  cJSON_Delete(em_pages);
  fprintf(stderr, "[" SRC "] emitted %d of %ld emission rows (%d with co2e null, "
          "%d byte-identical repeats folded; facilities=%d of %ld, unjoined=%d, "
          "failed pages fac=%d em=%d)\n", n, ew.rows, null_co2e, folded, fn,
          fw.count, unjoined, fw.failed_pages, ew.failed_pages);
  return 0;
}

static const source_def eni_epa_ghgrp_def = {
  .id = SRC, .collector = "industry",
  .name = "US EPA GHGRP facility emissions (Envirofacts)",
  .update_interval_sec = 604800, .run = run,
  .category = "industry", .type = "api",
  .url = "https://data.epa.gov/efservice/pub_facts_sector_ghg_emission/year/2023/rows/0:1999/JSON",
  .description = "Reported annual greenhouse gas emissions (tonnes CO2e) for large US industrial emitters, joined to facility name, NAICS, parent company and coordinates.",
  .license = "US EPA Envirofacts, public domain, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_epa_ghgrp_def)
