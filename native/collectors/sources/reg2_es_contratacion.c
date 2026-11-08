/* collectors/sources/reg2_es_contratacion.c
 *
 * Spain — Plataforma de Contratacion del Sector Publico, CODICE/UBL syndication.
 *   GET https://contrataciondelestado.es/sindicacion/sindicacion_643/
 *       licitacionesPerfilesContratanteCompleto3.atom
 *
 * Atom 1.0 where each <entry> embeds the full CODICE (UBL) contract XML with
 * cac:/cbc: namespaces. Per entry we emit only what is actually present in the
 * document: the entry <title>, the entry <summary> (which the platform fills
 * with "Id licitación / Órgano de Contratación / Importe / Estado"), the
 * alternate <link href>, <updated>, cbc:ContractFolderID, the contracting
 * party name taken from inside the LocatedContractingParty block, the
 * ProcurementProject name, cbc:TotalAmount and the folder status code.
 * Anything the entry does not carry is omitted — never filled in.
 *
 * Keyless. The feed publishes no coordinates, so no row claims geo (R2).
 * Licence: official syndication service of the Ministerio de Hacienda; reuse
 * permitted under the Spanish public-sector reuse regime (Ley 37/2007).
 *
 * Separate feeds exist for contratos menores (sindicacion_1044) and for awards;
 * only the probed one above is wired here.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"
#include "lib/seenset.h"

/* THE FEED IS A CHAIN, and the cap was a discard (2026-11-07). `ES_MAX 120`
 * stopped at 120 entries while the head file held 310 (measured that day;
 * the audit's 231 a few days earlier), and the head file links its
 * predecessor with <link rel="next"> — a chain of archive files back through
 * the platform's history (head 2026-11-06T14:40 → 2026-11-07T18:31, the next
 * file 498 entries covering 13:31 → 14:40 of the 6th, 15 MB, and it links
 * another). Every entry of every file read is emitted now; the chain is
 * followed back until it reaches the newest entry the PREVIOUS run saw (a
 * watermark stored through source_ctx resume_in/resume_out), and at most
 * ES_MAX_FILES files a run, a stop at that ceiling disclosed. An entry whose
 * folder was already emitted from a NEWER file this run is skipped: the sink
 * upserts on the folder id, so the older revision would overwrite the newer. */
#define ES_MAX_FILES 8  /* exhaustive-ok: file-chain ceiling; a stop short of the watermark files a collector-truncation-notice */
#define ES_FEED "https://contrataciondelestado.es/sindicacion/sindicacion_643/" \
                "licitacionesPerfilesContratanteCompleto3.atom"

static void es_unescape(char *s) {
  char *w = s;
  for (char *p = s; *p; ) {
    if (*p == '&') {
      if (strncmp(p, "&amp;", 5) == 0)  { *w++ = '&';  p += 5; continue; }
      if (strncmp(p, "&lt;", 4) == 0)   { *w++ = '<';  p += 4; continue; }
      if (strncmp(p, "&gt;", 4) == 0)   { *w++ = '>';  p += 4; continue; }
      if (strncmp(p, "&quot;", 6) == 0) { *w++ = '"';  p += 6; continue; }
      if (strncmp(p, "&apos;", 6) == 0) { *w++ = '\''; p += 6; continue; }
      if (strncmp(p, "&#39;", 5) == 0)  { *w++ = '\''; p += 5; continue; }
    }
    *w++ = *p++;
  }
  *w = 0;
}

/* Collapse runs of whitespace and trim; returns s. */
static char *es_squash(char *s) {
  char *w = s;
  int sp = 0;
  for (char *p = s; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
      if (!sp && w != s) *w++ = ' ';
      sp = 1;
    } else { *w++ = (char)c; sp = 0; }
  }
  while (w > s && w[-1] == ' ') w--;
  *w = 0;
  return s;
}

/* Inner text of the first <tag> at/after `from`, unescaped + squashed. */
static char *es_tag(const char *from, const char *tag) {
  if (!from) return NULL;
  const char *cur = from;
  char *v = jo_tag_inner(&cur, tag);
  if (!v) return NULL;
  es_unescape(v);
  es_squash(v);
  if (!v[0]) { free(v); return NULL; }
  return v;
}

/* "2026-11-06T14:40:35.131+01:00" -> UTC epoch seconds, 0 if unparsable.
 * Civil-day arithmetic by hand so no timegm() (not declared under this tree's
 * feature macros). */
static long es_epoch(const char *s) {
  int Y, M, D, h, m, sec;
  if (!s || sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &Y, &M, &D, &h, &m, &sec) != 6) return 0;
  const char *z = s + 19;
  while (*z == '.' || isdigit((unsigned char)*z)) z++;
  long off = 0;
  if (*z == '+' || *z == '-') {
    int oh = 0, om = 0;
    if (sscanf(z + 1, "%2d:%2d", &oh, &om) >= 1) off = (oh * 3600L + om * 60L) * (*z == '-' ? -1 : 1);
  }
  int y = Y - (M <= 2);
  long era = (y >= 0 ? y : y - 399) / 400;
  long yoe = y - era * 400;
  long doy = (153L * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long days = era * 146097 + doe - 719468;
  return days * 86400L + h * 3600L + m * 60L + sec - off;
}

/* The archive file before this one — the feed-level <link rel="next">, read
 * only from the header before the first <entry> — or NULL. malloc'd. */
static char *es_next_file(const char *xml) {
  const char *limit = strstr(xml, "<entry");
  const char *p = xml;
  while ((p = strstr(p, "<link ")) != NULL && (!limit || p < limit)) {
    const char *e = strchr(p, '>');
    if (!e) return NULL;
    const char *rel = strstr(p, "rel=\"next\"");
    const char *h = strstr(p, "href=\"");
    if (rel && rel < e && h && h < e) {
      h += 6;
      const char *he = strchr(h, '"');
      if (he && he < e) {
        char *u = strndup(h, (size_t)(he - h));
        if (u) es_unescape(u);
        return u;
      }
    }
    p = e;
  }
  return NULL;
}

/* Emit every entry of one file. Tracks the oldest/newest <updated>. */
static int es_emit_file(intel_sink *sink, const char *xml, seen_set *done,
                        long *oldest, long *newest) {
  int n = 0;
  const char *cur = xml;
  for (;;) {
    const char *hit = jo_next_entry(&cur);
    if (!hit) break;
    const char *close = strstr(hit, "</entry>");
    if (!close) break;
    size_t len = (size_t)(close - hit);
    char *chunk = (char *)malloc(len + 1);
    if (!chunk) break;
    memcpy(chunk, hit, len);
    chunk[len] = 0;
    cur = close + 8;

    char *title = es_tag(chunk, "title");
    if (!title) { free(chunk); continue; }    /* no real title -> no row (R1) */

    char *summary = es_tag(chunk, "summary");
    char *updated = es_tag(chunk, "updated");
    long ue = es_epoch(updated);
    if (ue) {
      if (!*oldest || ue < *oldest) *oldest = ue;
      if (ue > *newest) *newest = ue;
    }
    char *folder  = es_tag(chunk, "cbc:ContractFolderID");
    char *amount  = es_tag(chunk, "cbc:TotalAmount");
    char *status  = es_tag(chunk, "cbc-place-ext:ContractFolderStatusCode");
    if (!status)  status = es_tag(chunk, "cbc:ContractFolderStatusCode");

    /* Buyer: the cbc:Name inside the LocatedContractingParty block. */
    char *buyer = NULL;
    const char *lc = strstr(chunk, "LocatedContractingParty");
    if (lc) buyer = es_tag(lc, "cbc:Name");

    /* Project title: the cbc:Name inside ProcurementProject. */
    char *project = NULL;
    const char *pp = strstr(chunk, "ProcurementProject");
    if (pp) project = es_tag(pp, "cbc:Name");

    char link[900] = "";
    const char *h = strstr(chunk, "href=\"");
    if (h) {
      h += 6;
      const char *he = strchr(h, '"');
      if (he && (size_t)(he - h) < sizeof link) {
        snprintf(link, sizeof link, "%.*s", (int)(he - h), h);
        es_unescape(link);
      }
    }

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "es-contratacion-estado");
    cJSON_AddStringToObject(props, "source", "contrataciondelestado.es");
    cJSON_AddStringToObject(props, "title", title);
    if (summary) cJSON_AddStringToObject(props, "entry_summary", summary);
    if (folder)  cJSON_AddStringToObject(props, "contract_folder_id", folder);
    if (buyer)   cJSON_AddStringToObject(props, "contracting_party", buyer);
    if (project) cJSON_AddStringToObject(props, "procurement_project", project);
    if (amount)  cJSON_AddStringToObject(props, "total_amount", amount);
    if (status)  cJSON_AddStringToObject(props, "status_code", status);
    if (updated) cJSON_AddStringToObject(props, "updated", updated);
    if (link[0]) cJSON_AddStringToObject(props, "href", link);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    /* the same folder from a NEWER file this run wins (see the header) */
    const char *rk = folder ? folder : (link[0] ? link : title);
    if (!seen_add(done, rk)) {
      free(pj);
      free(title); free(summary); free(updated); free(folder);
      free(amount); free(status); free(buyer); free(project);
      free(chunk);
      continue;
    }

    char sum[600];
    snprintf(sum, sizeof sum, "%s%s%s",
             buyer ? buyer : "", (buyer && summary) ? " · " : "",
             summary ? summary : "");

    intel_item it = {0};
    it.remote_key      = rk;
    it.title           = title;
    it.summary         = sum[0] ? sum : NULL;
    it.body            = sum[0] ? sum : title;   /* text, not the props JSON (2026-11-07) */
    it.link            = link[0] ? link : ES_FEED;
    it.lang            = "es";
    it.published_at    = updated;
    it.record_type     = "procurement-notice";
    it.properties_json = pj;
    it.tags_json       = "[\"procurement\",\"spain\",\"codice\"]";
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj);
    free(title); free(summary); free(updated); free(folder);
    free(amount); free(status); free(buyer); free(project);
    free(chunk);
  }
  return n;
}

static int es_run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "Accept: application/atom+xml, application/xml", NULL };
  long wm = 0;
  if (ctx->resume_in && strncmp(ctx->resume_in, "w=", 2) == 0)
    wm = strtol(ctx->resume_in + 2, NULL, 10);

  seen_set done = {0};
  char *url = strdup(ES_FEED);
  int n = 0, files = 0, ceiling = 0, cut = 0;
  long head_newest = 0, reached = 0;
  while (url) {
    char *xml = jo_get_t(ctx, url, hdrs, "es-contratacion-estado", 90000);
    if (!xml) {
      if (files == 0) {
        free(url); seen_free(&done);
        fprintf(stderr, "[es-contratacion-estado] fetch failed\n");
        return jo_fail_rc(jo_last_status(), 0);
      }
      cut = 1;
      break;
    }
    long oldest = 0, newest = 0;
    n += es_emit_file(sink, xml, &done, &oldest, &newest);
    files++;
    if (files == 1) head_newest = newest;
    if (oldest) reached = oldest;
    char *next = es_next_file(xml);
    free(xml);
    free(url);
    url = NULL;
    if (!next) break;                          /* the start of the archive */
    if (wm && oldest && oldest <= wm) { free(next); break; }  /* caught up */
    if (files >= ES_MAX_FILES) { ceiling = 1; free(next); break; }
    url = next;
  }
  free(url);
  seen_free(&done);
  if (ceiling || cut) {
    char why[320];
    snprintf(why, sizeof why,
             "%s after %d archive file(s); entries older than epoch %ld were not "
             "read this run%s", cut ? "an archive file failed" : "the file-chain ceiling stopped the walk",
             files, reached, wm ? " and the gap to the previous run's newest entry remains" : "");
    jo_trunc_notice_scoped(sink, "es-contratacion-estado", "archive-chain", ES_FEED,
                           n, -1, why,
                           cut ? "re-run; a refused archive file is one reading"
                               : "raise ES_MAX_FILES in reg2_es_contratacion.c, or run more often");
  }
  if (ctx->resume_out && head_newest) {
    char tok[48];
    snprintf(tok, sizeof tok, "w=%ld", head_newest);
    ctx->resume_out(ctx, tok);
  }
  fprintf(stderr, "[es-contratacion-estado] emitted %d from %d file(s)\n", n, files);
  return 0;
}

static const source_def reg2_es_contratacion_def = {
  .id = "es-contratacion-estado", .collector = "government",
  .name = "Spain Plataforma de Contratacion del Sector Publico (CODICE Atom)",
  .update_interval_sec = 10800, .run = es_run,
  .category = "government", .type = "api",
  .url = ES_FEED,
  .description = "Spain's national contracting platform syndicates every tender "
                 "and award as Atom entries carrying full CODICE/UBL contract "
                 "XML — buyer, object, amount and status. Keyless.",
  .license = "Official syndication service of the Ministerio de Hacienda; reuse "
             "permitted under the Spanish public-sector reuse regime (Ley 37/2007).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg2_es_contratacion_def)
