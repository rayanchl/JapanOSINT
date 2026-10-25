/* France — national EV charging point registry (IRVE): data.gouv.fr's
 * consolidated file, "Base nationale des IRVE … données statiques" (Etalab's
 * daily consolidation of every submitter's schema-irve-statique resource; the
 * dataset the "Fichier consolidé des Bornes de Recharge pour Véhicules
 * Électriques" page now redirects to).
 *
 * Endpoint: the resource API names the current file —
 *   https://www.data.gouv.fr/api/1/datasets/5448d3e0c751df01f85d0572/
 *           resources/eb76d20a-8501-400e-b336-d85724de5435/
 * → `url` (a dated static.data.gouv.fr CSV, immutable once published),
 *   `filesize`, `title`. That resource is "Consolidation de la dernière
 * version à date du schéma": created 2024-02-08 and replaced in place every
 * day, so its id is the stable handle while the file name carries the date.
 * When the resource API does not answer, the resource's own permalink
 * (/api/1/datasets/r/<id>, a 302 to the same file) is read instead.
 *
 * Emits: one intel row per charge point — every row of the file, every
 * column verbatim (52 on 2026-10-24: operator and site owner with SIREN and
 * contacts, station and charge-point roaming and local ids, address, INSEE
 * commune, coordinates, power, connector types, payment, pricing, access,
 * opening hours, PMR access, grid connection and PDL number, dates, the
 * submitting data.gouv dataset/resource/organisation, and the consolidator's
 * corrected coordinates, postcode and commune with its verification flags),
 * plus the readable names below. Keyless.
 * Licence: Licence Ouverte / Open Licence (Etalab) v2.0.
 *
 * WHY THE FILE AND NOT AN API (2026-10-25). This read ODRE's Opendatasoft
 * mirror (`odre.opendatasoft.com/.../bornes-irve/records`), whose records API
 * refuses offset+limit above 10,000 and whose text fields refuse a `>`
 * comparison, so neither an offset walk nor a keyset walk could pass the
 * first 10,000 charge points: the weekly run stored 10,000 of 223,566
 * (measured 2026-10-24: records=10000 stored=10001 with its notice, 100
 * requests, 19.6 MB, 37 s). data.gouv's tabular API on the consolidated file
 * has the same 10,000-record offset ceiling. The consolidated CSV is the
 * whole register in ONE request: 100,262,866 bytes and 143,445 rows on
 * 2026-10-24 (the tabular API's own `meta.total` says 143,445 too).
 *
 * THE TWO REGISTERS DO NOT HOLD THE SAME ROWS, and the difference is the
 * upstream's, not this collector's. ODRE's mirror was last processed
 * 2026-09-30 and still carries 79,636 rows from QualiCharge and 12,128 from
 * Alizé that Etalab's October consolidation does not: its own reference table
 * (resource 41b0514d…, "Documentation sur la consolidation") lists
 * QualiCharge's static resource as `wrong-file-format` and Alizé's as failing
 * schema 2.3.1, both `is_schema_to_drop`. This source now reads what the
 * national consolidation publishes today; QualiCharge's own static file
 * (dataset "Infrastructures de recharge … Données ouvertes QualiCharge",
 * resource 8bb0a6e2…) is a separate upstream and is not read here.
 *
 * THE BODY CEILING. 100 MB is over core/httpclient.c's 64 MB default, which
 * fails CLOSED (the transfer aborts and nothing is stored). The fetch asks for
 * this call's own ceiling through http_request_max(): the operator's
 * `http_body_ceiling_bytes` for this source when one is set
 * (ctx->body_ceiling), else IRVE_MAX_BODY. An explicit $JO_HTTP_MAX_BODY still
 * clamps it, and the ceiling is still enforced on the decompressed stream
 * (static.data.gouv.fr serves the CSV uncompressed anyway). The ceiling is
 * this row's alone and is restored when the fetch returns, so the 64 MB
 * default guarding every other source is untouched — the precedent is
 * od_ods_geonames.c. Measured 2026-10-25 (macOS, `--run`, scratch DB): the
 * 100,262,866-byte file in 2 requests, 143,445 emitted / 143,445 stored, 66 s
 * on an empty database and 7.7 s on the re-run, peak memory footprint 237 MB
 * (max RSS 332 MB) against the paged walk's 162 MB for 10,000 rows. With
 * JO_HTTP_MAX_BODY=67108864 set explicitly the fetch fails closed — rc=-1,
 * nothing stored, the client naming the 64 MB ceiling that bit.
 *
 * THE PARSE. One csv_parse over the whole file would build a cJSON node per
 * cell — ~7.5 million, gigabytes — so the file is cut into slices of whole
 * records with csv_record_end() (lib/csv.c, the parser's own quote rule) and
 * each slice is parsed on its own; peak memory is the body plus one slice.
 * The slicer and the parser are cross-checked per slice: a slice that parses
 * to a different number of records than it was cut into is counted and
 * disclosed, never silently accepted.
 *
 * COORDINATES. `coordonneesXY` is the schema's "[longitude, latitude]" text,
 * in that order here — the swapped-key trap of ODRE's `coordonneesxy` object
 * ({"lon": <latitude>, "lat": <longitude>}) does not exist in the file. On
 * 2026-10-24 it equals consolidated_longitude/latitude on all 143,445 rows;
 * those are the fallback when coordonneesXY does not parse.
 */
#include "lib/jocore.h"
#include "lib/keyqual.h"
#include "lib/csv.h"
#include "trn_common.inc"

#define IRVE_ID "france-irve-charging-stations"
#define IRVE_DATASET "5448d3e0c751df01f85d0572"
#define IRVE_RESOURCE "eb76d20a-8501-400e-b336-d85724de5435"
#define IRVE_META_URL "https://www.data.gouv.fr/api/1/datasets/" IRVE_DATASET \
                      "/resources/" IRVE_RESOURCE "/"
#define IRVE_PERMALINK "https://www.data.gouv.fr/api/1/datasets/r/" IRVE_RESOURCE
/* 100 MB measured; 256 MB leaves room for the register to grow (it gains
 * charge points every week) without the fetch failing closed, and a body past
 * it is still refused rather than truncated. The FALLBACK, not the policy:
 * the operator's per-source ceiling arrives on ctx->body_ceiling and wins. */
#define IRVE_MAX_BODY (256u * 1024u * 1024u)
/* Records per csv_parse_wellformed() call — a memory bound on one parse, not
 * on the run: every slice is read. */
#define IRVE_SLICE 2000

/* The columns the identity and the readable names need, by header position. */
typedef struct {
  char **name; int n;
  int pdc, sta, nom, res, op, own, adr, insee, impl, npdc, pw, mes, xy, clon, clat;
} irve_cols;

typedef struct {
  long rows, blank, ragged, unkeyed, disagree, emitted, folded;
  int cancelled;
} irve_stats;

static int irve_col(const irve_cols *c, const char *want) {
  for (int i = 0; i < c->n; i++)
    if (c->name[i] && !strcmp(c->name[i], want)) return i;
  return -1;
}

/* Cell i of a positional row as a NON-EMPTY string, else NULL. */
static const char *irve_cell(const cJSON *row, int i) {
  if (i < 0) return NULL;
  const cJSON *v = cJSON_GetArrayItem(row, i);
  return (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
           ? v->valuestring : NULL;
}

/* The record's base key: the charge-point roaming id, else the station's,
 * else the station name. NULL for a row carrying none of the three. */
static const char *irve_base(const irve_cols *c, const cJSON *row) {
  const char *k = irve_cell(row, c->pdc);
  if (!k) k = irve_cell(row, c->sta);
  if (!k) k = irve_cell(row, c->nom);
  return k;
}

/* "[lon,lat]" per the schema; the consolidator's corrected pair otherwise. */
static int irve_coords(const irve_cols *c, const cJSON *row,
                       double *lat, double *lon) {
  const char *xy = irve_cell(row, c->xy);
  double a, b;
  if (xy && sscanf(xy, " [ %lf , %lf ]", &a, &b) == 2 && trn_geo_ok(b, a)) {
    *lon = a; *lat = b;
    return 1;
  }
  const char *slo = irve_cell(row, c->clon), *sla = irve_cell(row, c->clat);
  char *e1 = NULL, *e2 = NULL;
  if (!slo || !sla) return 0;
  a = strtod(slo, &e1);
  b = strtod(sla, &e2);
  if (e1 == slo || e2 == sla || !trn_geo_ok(b, a)) return 0;
  *lon = a; *lat = b;
  return 1;
}

static void irve_put_numstr(cJSON *o, const char *k, const char *s) {
  if (!s) return;
  char *end = NULL;
  double d = strtod(s, &end);
  if (end && end != s && !*end && isfinite(d)) cJSON_AddNumberToObject(o, k, d);
}

typedef void (*irve_row_fn)(void *ud, const cJSON *row);

/* Walk every record after the header, IRVE_SLICE at a time. The body is
 * NUL-terminated at each slice's end for the parse and restored after (the
 * HTTP client allocates len + 1, so body[len] is ours to write). */
static void irve_walk(const source_ctx *ctx, char *body, size_t len,
                      size_t start, irve_stats *st, irve_row_fn fn, void *ud) {
  size_t at = start;
  while (at < len) {
    if (ctx->cancel && *ctx->cancel) { st->cancelled = 1; return; }
    size_t end = at;
    long cut = 0;
    while (cut < IRVE_SLICE && end < len) {
      end = csv_record_end(body, len, end, ',');
      cut++;
    }
    char keep = body[end];
    body[end] = 0;
    cJSON *rows = csv_parse_wellformed(body + at, 0, NULL, 0, NULL);
    body[end] = keep;
    long got = cJSON_GetArraySize(rows);
    if (got != cut) st->disagree += labs(got - cut);
    const cJSON *row;
    cJSON_ArrayForEach(row, rows) {
      if (cJSON_GetArraySize(row) == 1 && !irve_cell(row, 0)) { st->blank++; continue; }
      fn(ud, row);
    }
    cJSON_Delete(rows);
    at = end;
  }
}

typedef struct {
  const irve_cols *c;
  irve_stats *st;
  keyqual *kq;
  intel_sink *sink;
} irve_pass;

static void irve_census(void *ud, const cJSON *row) {
  irve_pass *p = ud;
  p->st->rows++;
  const char *b = irve_base(p->c, row);
  if (b) keyqual_add(p->kq, b, irve_cell(row, p->c->res));
}

static void irve_emit(void *ud, const cJSON *row) {
  irve_pass *p = ud;
  const irve_cols *c = p->c;
  const char *base = irve_base(c, row);
  if (!base) { p->st->unkeyed++; return; }
  const char *pdc = irve_cell(row, c->pdc), *sta = irve_cell(row, c->sta);
  const char *nom = irve_cell(row, c->nom), *res = irve_cell(row, c->res);
  const char *op = irve_cell(row, c->op), *ad = irve_cell(row, c->adr);
  int nc = cJSON_GetArraySize(row);
  if (nc != c->n) p->st->ragged++;

  char keybuf[512];
  const char *rk = base;
  if (keyqual_count(p->kq, base) > 1) {
    char *raw = cJSON_PrintUnformatted(row);
    rk = keyqual_uid(p->kq, base, res, raw ? raw : "", keybuf, sizeof keybuf);
    free(raw);
    if (!keyqual_claim(p->kq, base, rk)) { p->st->folded++; return; }
  }

  cJSON *pr = cJSON_CreateObject();
  trn_put_str(pr, "station_name", nom);
  trn_put_str(pr, "operator", op);
  trn_put_str(pr, "site_owner", irve_cell(row, c->own));
  trn_put_str(pr, "station_roaming_id", sta);
  trn_put_str(pr, "charge_point_roaming_id", pdc);
  trn_put_str(pr, "datagouv_resource_id", res);
  trn_put_str(pr, "address", ad);
  trn_put_str(pr, "insee_commune", irve_cell(row, c->insee));
  trn_put_str(pr, "implantation", irve_cell(row, c->impl));
  irve_put_numstr(pr, "charge_point_count", irve_cell(row, c->npdc));
  irve_put_numstr(pr, "power_kw", irve_cell(row, c->pw));
  trn_put_str(pr, "date_mise_en_service", irve_cell(row, c->mes));
  /* ...and every column of the row, verbatim, under the file's own header
   * names (house rule 2). Empty cells are skipped, as JSON nulls were. A
   * ragged row's cells past the header keep a positional name, as
   * csv_parse's named mode does — nothing is invented, nothing dropped. */
  for (int i = 0; i < nc; i++) {
    const char *v = irve_cell(row, i);
    if (!v) continue;
    char pos[24];
    const char *k = (i < c->n && c->name[i] && c->name[i][0]) ? c->name[i] : NULL;
    if (!k) { snprintf(pos, sizeof pos, "col%d", i); k = pos; }
    if (cJSON_GetObjectItem(pr, k)) continue;
    cJSON_AddStringToObject(pr, k, v);
  }
  char *pj = cJSON_PrintUnformatted(pr);

  char title[320], summary[256];
  snprintf(title, sizeof title, "%s", nom ? nom : (sta ? sta : base));
  if (op && ad) snprintf(summary, sizeof summary, "%s · %s", op, ad);
  else if (ad)  snprintf(summary, sizeof summary, "%s", ad);
  else if (op)  snprintf(summary, sizeof summary, "%s", op);
  else summary[0] = 0;

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.summary         = summary[0] ? summary : NULL;
  it.lang            = "fr";
  it.record_type     = "ev-charging-station";
  it.properties_json = pj ? pj : "{}";
  it.tags_json       = "[\"transport\",\"ev-charging\",\"france\"]";
  double la, lo;
  if (irve_coords(c, row, &la, &lo)) { it.has_geo = 1; it.lat = la; it.lon = lo; }
  if (p->sink->emit(p->sink, &it) >= 0) p->st->emitted++;
  free(pj);
  cJSON_Delete(pr);
}

/* GET the whole file under this call's own body ceiling. The body is taken
 * over from the response rather than copied: a strdup would hold the file
 * twice at the peak. */
static char *irve_fetch(const source_ctx *ctx, const char *url, size_t *len) {
  size_t ceiling = ctx->body_ceiling ? ctx->body_ceiling : IRVE_MAX_BODY;
  http_response r = {0};
  int rc = http_request_max(ctx->http, "GET", url, NULL, NULL, 0, 600000, 2,
                            ceiling, &r);
  if (rc != 0 || r.status < 200 || r.status >= 300 || !r.body) {
    /* The ceiling named is the one ASKED for: an explicit $JO_HTTP_MAX_BODY
     * clamps it below that, and core/httpclient.c logs the ceiling that bit
     * ("response exceeded N byte ceiling") on the line before this one. */
    fprintf(stderr, "[" IRVE_ID "] GET %s failed: status=%ld transport=%d "
            "(asked for a %zu-byte body ceiling, which an explicit "
            "$JO_HTTP_MAX_BODY clamps; a file past it needs this source's "
            "http_body_ceiling_bytes setting raised)\n", url, r.status,
            http_last_transport_error(), ceiling);
    http_response_free(&r);
    return NULL;
  }
  char *b = r.body;
  *len = r.body_len;
  r.body = NULL;
  http_response_free(&r);
  return b;
}

static void irve_cols_free(irve_cols *c) {
  for (int i = 0; c->name && i < c->n; i++) free(c->name[i]);
  free(c->name);
  c->name = NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* Which file is current, and how big the publisher says it is. */
  char url[1024];
  snprintf(url, sizeof url, "%s", IRVE_PERMALINK);
  long declared_size = -1;
  char edition[256] = "";
  cJSON *meta = feed_get_json(ctx->http, IRVE_META_URL, 30000);
  if (meta) {
    const char *u = jo_sv(meta, "url");
    if (u && (!strncmp(u, "https://", 8) || !strncmp(u, "http://", 7)))
      snprintf(url, sizeof url, "%s", u);
    const cJSON *fs = cJSON_GetObjectItem(meta, "filesize");
    if (cJSON_IsNumber(fs) && fs->valuedouble > 0) declared_size = (long)fs->valuedouble;
    const char *t = jo_sv(meta, "title");
    if (t) snprintf(edition, sizeof edition, "%s", t);
    cJSON_Delete(meta);
  } else {
    fprintf(stderr, "[" IRVE_ID "] resource API unanswered; reading the "
            "permalink %s\n", IRVE_PERMALINK);
  }

  size_t len = 0;
  char *body = irve_fetch(ctx, url, &len);
  if (!body) return -1;

  /* The header row names every column. */
  irve_cols c = {0};
  c.pdc = c.sta = c.nom = c.res = c.op = c.own = c.adr = c.insee = c.impl =
    c.npdc = c.pw = c.mes = c.xy = c.clon = c.clat = -1;
  size_t hdr_end = csv_record_end(body, len, 0, ',');
  char keep = body[hdr_end];
  body[hdr_end] = 0;
  cJSON *hrows = csv_parse_wellformed(body, 0, NULL, 0, NULL);
  body[hdr_end] = keep;
  cJSON *hdr = cJSON_GetArrayItem(hrows, 0);  /* exhaustive-ok: the header row of a one-row parse, not a record */
  c.n = cJSON_GetArraySize(hdr);
  c.name = calloc((size_t)(c.n > 0 ? c.n : 1), sizeof(char *));
  for (int i = 0; c.name && i < c.n; i++) {
    const cJSON *h = cJSON_GetArrayItem(hdr, i);
    const char *s = (h && h->valuestring) ? h->valuestring : "";
    while (*s == ' ' || *s == '\t') s++;
    char *t = strdup(s);
    if (!t) continue;
    size_t L = strlen(t);
    while (L && (t[L - 1] == ' ' || t[L - 1] == '\t' || t[L - 1] == '\r')) t[--L] = 0;
    c.name[i] = t;
  }
  cJSON_Delete(hrows);
  if (c.name) {
    c.pdc = irve_col(&c, "id_pdc_itinerance");
    c.sta = irve_col(&c, "id_station_itinerance");
    c.nom = irve_col(&c, "nom_station");
    c.res = irve_col(&c, "datagouv_resource_id");
    c.op = irve_col(&c, "nom_operateur");
    c.own = irve_col(&c, "nom_amenageur");
    c.adr = irve_col(&c, "adresse_station");
    c.insee = irve_col(&c, "code_insee_commune");
    c.impl = irve_col(&c, "implantation_station");
    c.npdc = irve_col(&c, "nbre_pdc");
    c.pw = irve_col(&c, "puissance_nominale");
    c.mes = irve_col(&c, "date_mise_en_service");
    c.xy = irve_col(&c, "coordonneesXY");
    c.clon = irve_col(&c, "consolidated_longitude");
    c.clat = irve_col(&c, "consolidated_latitude");
  }
  if (c.pdc < 0 && c.sta < 0 && c.nom < 0) {
    /* The schema moved under us: no identity column at all. Storing rows
     * keyed on nothing would merge them; say so instead. */
    fprintf(stderr, "[" IRVE_ID "] header has none of id_pdc_itinerance, "
            "id_station_itinerance, nom_station (%d columns) — schema changed\n",
            c.n);
    jo_truncation_notice_ex(sink, IRVE_ID, NULL, 0, -1,
      "the consolidated file's header carries none of the identity columns "
      "(id_pdc_itinerance, id_station_itinerance, nom_station), so no row "
      "could be keyed", "re-read the schema-irve-statique version the "
      "consolidation now publishes and map its identity columns", NULL);
    irve_cols_free(&c);
    free(body);
    return -1;
  }

  /* IDENTITY, unchanged from the paged version. The upstream's own
   * charge-point roaming id is the key — but the consolidated file is a union
   * of per-submitter data.gouv resources, and one id_pdc_itinerance recurs
   * across resources as DIFFERENT rows (2026-10-24, the whole file: 143,445
   * byte-distinct rows, 132,584 distinct base keys, 10,121 of them repeated —
   * "Non concerné", the schema's "not applicable", on 117 rows;
   * id_pdc_itinerance + datagouv_resource_id is unique on all 143,445).
   * Pass 1 counts every key over the WHOLE file; pass 2 qualifies EVERY row
   * of a repeated id by its data.gouv resource, falling back to a hash of the
   * row. See lib/keyqual.h. The census now covers the register, not the
   * first 10,000 of it, so a key that repeats anywhere is qualified
   * everywhere. */
  irve_stats st = {0};
  keyqual kq = {0};
  irve_pass p = { &c, &st, &kq, sink };
  irve_walk(ctx, body, len, hdr_end, &st, irve_census, &p);
  keyqual_seal(&kq);
  long census_rows = st.rows;
  st.blank = 0;
  st.disagree = 0;
  if (!st.cancelled) irve_walk(ctx, body, len, hdr_end, &st, irve_emit, &p);
  keyqual_free(&kq);
  int ncols = c.n;
  irve_cols_free(&c);
  free(body);

  fprintf(stderr, "[" IRVE_ID "] emitted %ld of %ld rows in %s (%zu bytes; "
          "declared filesize %ld; %d columns; %ld ragged, %ld unkeyed, %ld "
          "byte-identical repeats folded, %ld blank lines, %ld slice/parse "
          "disagreements)\n", st.emitted, census_rows,
          edition[0] ? edition : url, len, declared_size, ncols, st.ragged,
          st.unkeyed, st.folded, st.blank, st.disagree);

  /* Anything not stored is said in the data (house rule 2). A complete read
   * files nothing. */
  int short_body = declared_size > 0 && (long)len < declared_size;
  if (st.cancelled || st.unkeyed || st.disagree || short_body) {
    char reason[512];
    snprintf(reason, sizeof reason, "%s%s%s%s",
             st.cancelled ? "the run was cancelled before every row was emitted; " : "",
             st.unkeyed ? "rows carried no charge-point id, station id or "
                          "station name to key on and were not stored; " : "",
             st.disagree ? "slices of the file parsed to a different number of "
                           "records than they were cut into; " : "",
             short_body ? "the body was shorter than the resource's declared "
                          "filesize; " : "");
    cJSON *extra = cJSON_CreateObject();
    if (extra) {
      cJSON_AddStringToObject(extra, "file_url", url);
      if (edition[0]) cJSON_AddStringToObject(extra, "edition", edition);
      cJSON_AddNumberToObject(extra, "bytes_read", (double)len);
      if (declared_size > 0)
        cJSON_AddNumberToObject(extra, "declared_filesize", (double)declared_size);
      cJSON_AddNumberToObject(extra, "rows_unkeyed", (double)st.unkeyed);
      cJSON_AddNumberToObject(extra, "slice_parse_disagreements", (double)st.disagree);
    }
    jo_truncation_notice_ex(sink, IRVE_ID, NULL, st.emitted,
      short_body ? -1 : census_rows, reason,
      "re-run; if it repeats, read the rows the counts name in the file "
      "itself", extra);
  }
  return 0;
}

static const source_def trn_france_irve_charging_stations_def = {
  .id = "france-irve-charging-stations", .collector = "transport",
  .name = "France — national EV charging point registry (IRVE)",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset",
  .url = "https://www.data.gouv.fr/datasets/" IRVE_DATASET "/",
  .description = "The official consolidated French IRVE charge-point registry "
                 "(data.gouv.fr / Etalab), read whole every run — operator, "
                 "site owner, address, INSEE commune, roaming ids, power, "
                 "connectors, pricing, access and coordinates.",
  .license = "Licence Ouverte / Open Licence (Etalab) v2.0.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_france_irve_charging_stations_def)
