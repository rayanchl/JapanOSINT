/* core/reportapi.c — roadmap item 16: the case report builder. See
 * reportapi.h for the design contract, the optional DDL and the httpd.c
 * wiring block.
 *
 * Organisation: ONE document model, TWO emitters. Every section is written
 * once, against d_heading/d_para/d_table/d_code/d_link, and the `html` flag on
 * the emitter decides the bytes. Nothing below the emitter layer knows which
 * format is being produced, which is the only structural defence against the
 * two formats drifting — the failure mode of every dual-format report writer.
 *
 * Two deliberate duplications, documented rather than refactored because the
 * alternative was editing files this task must not touch:
 *
 *  1. `ostream` (64 KB batches → export_write_fn) is a copy of exportapi.c's,
 *     which is static there. The writer *contract* is shared for real (the
 *     typedef in reportapi.h), so only the buffer plumbing is duplicated. If
 *     exportapi ever exports its ostream, delete this one. The one addition is
 *     a `total` byte counter, which the report's audit payload reports and the
 *     exporter has no use for.
 *  2. ctext()/iso_now() are the house idioms from alertsapi.c/casesapi.c,
 *     static in each of them.
 *
 * Three passes over `case_items` (findings, entities, citations) rather than
 * one pass into an in-memory model. The passes are cheap (pins per case are
 * tens, not millions) and each one streams straight out, so the document is
 * never materialised. Building a model first would have re-introduced exactly
 * the unbounded allocation the streaming contract exists to avoid.
 */
#include "reportapi.h"
#include "entityapi.h"
#include "audit.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

/* ── caps ────────────────────────────────────────────────────────────────
 * Every cap below exists because the mongoose send buffer holds the WHOLE
 * response (see the constraint block in reportapi.h). They bound the document,
 * not the module's memory, and every one of them announces itself in the
 * output when it bites — a silently short report is the failure this file is
 * most concerned with avoiding. */
#define CAP_SUMMARY     65536   /* case.summary is capped at 8191 upstream   */
#define CAP_NOTE        65536   /* annotations.body_md accepts up to 64 KB   */
#define CAP_FIELD        4096   /* one snapshot scalar (intel `body` is big) */
#define CAP_INLINE        512   /* labels, names, ids in headings and cells  */
#define CAP_JSON        16384   /* one nested snapshot value as a code block */
#define MAX_AUDIT_ROWS    200   /* audit excerpt; +1 probed to detect more   */
#define MAX_ENTITIES     5000   /* dedup set ceiling, announced when hit     */

/* ── output stream: bounded batches through the caller's writer ─────────── */

#define OB_CAP 65536                     /* one flush == one HTTP chunk */

typedef struct {
  report_write_fn fn;
  void *ctx;
  size_t len;
  long   total;                          /* bytes handed to fn(), for audit */
  int    broken;                         /* writer said stop; latch it      */
  char   buf[OB_CAP];
} ostream;

static int ob_flush(ostream *o) {
  if (o->broken) return 1;
  if (o->len == 0) return 0;
  if (o->fn(o->ctx, o->buf, o->len) != 0) { o->broken = 1; return 1; }
  o->total += (long)o->len;
  o->len = 0;
  return 0;
}
static int ob_put(ostream *o, const char *p, size_t n) {
  if (o->broken) return 1;
  while (n) {
    size_t room = OB_CAP - o->len;
    if (room == 0) { if (ob_flush(o)) return 1; room = OB_CAP; }
    size_t take = n < room ? n : room;
    memcpy(o->buf + o->len, p, take);
    o->len += take; p += take; n -= take;
  }
  return 0;
}
static int ob_s(ostream *o, const char *s) {
  return s ? ob_put(o, s, strlen(s)) : 0;
}
static int ob_fmt(ostream *o, const char *fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  if (n < 0) return 0;
  if ((size_t)n < sizeof buf) return ob_put(o, buf, (size_t)n);
  char *big = malloc((size_t)n + 1);
  if (!big) return ob_put(o, buf, sizeof buf - 1);
  va_start(ap, fmt);
  vsnprintf(big, (size_t)n + 1, fmt, ap);
  va_end(ap);
  int r = ob_put(o, big, (size_t)n);
  free(big);
  return r;
}

/* ── house idioms ───────────────────────────────────────────────────────── */

static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}
static void iso_now(char *buf, size_t n) {
  struct timeval tv; gettimeofday(&tv, NULL);
  struct tm tm; gmtime_r(&tv.tv_sec, &tm);
  snprintf(buf, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec / 1000));
}
static void stamp_now(char *buf, size_t n) {          /* filename-safe */
  time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
  strftime(buf, n, "%Y%m%dT%H%M%SZ", &g);
}
static const char *jstr(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}
/* First non-empty string among `keys` — how the citation row finds a source /
 * link / timestamp across ref_types whose snapshots use different field names,
 * without this file growing a per-ref_type schema it would have to maintain. */
static const char *pick_str(cJSON *o, const char *const *keys, int n) {
  for (int i = 0; i < n; i++) {
    const char *v = jstr(o, keys[i]);
    if (v && *v) return v;
  }
  return NULL;
}

/* ── schema probes (both item-17 interop and template seam depend on these) ─ */

static int table_exists(db_handle *db, const char *name) {
  sqlite3_stmt *s = NULL; int ok = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, name, -1, SQLITE_TRANSIENT);
    ok = sqlite3_step(s) == SQLITE_ROW;
  }
  sqlite3_finalize(s);
  return ok;
}
/* PRAGMA table_info can't be parameterised, and the table name here is always
 * a compile-time literal from this file — never user input. */
static int column_exists(db_handle *db, const char *table, const char *col) {
  char sql[128];
  snprintf(sql, sizeof sql, "PRAGMA table_info(%s)", table);
  sqlite3_stmt *s = NULL; int ok = 0;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK) {
    while (sqlite3_step(s) == SQLITE_ROW) {
      const char *n = (const char *)sqlite3_column_text(s, 1);
      if (n && !strcmp(n, col)) { ok = 1; break; }
    }
  }
  sqlite3_finalize(s);
  return ok;
}
/* First of `cands` that exists on `table`, or NULL. */
static const char *first_column(db_handle *db, const char *table,
                                const char *const *cands, int n) {
  for (int i = 0; i < n; i++)
    if (column_exists(db, table, cands[i])) return cands[i];
  return NULL;
}

/* ── evidence table (roadmap 17, may not exist yet) ─────────────────────── */

typedef struct {
  int  available;          /* 0 → omit the SHA-256 column entirely */
  char sql[320];           /* prepared per lookup; ?1 key [?2 ref_type] [?3 tenant] */
  int  bind_ref_type;
  int  bind_tenant;
} evidence_probe;

static void evidence_probe_init(db_handle *db, evidence_probe *ep) {
  memset(ep, 0, sizeof *ep);
  if (!table_exists(db, "evidence")) return;
  static const char *DIGEST[] = { "content_sha256", "sha256", "content_hash",
                                  "digest", "hash" };
  static const char *KEY[]    = { "ref_id", "item_uid", "uid", "ref" };
  const char *dc = first_column(db, "evidence", DIGEST, 5);
  const char *kc = first_column(db, "evidence", KEY, 4);
  if (!dc || !kc) return;                 /* unrecognisable shape → omit */
  ep->bind_ref_type = column_exists(db, "evidence", "ref_type");
  ep->bind_tenant   = column_exists(db, "evidence", "tenant_id");
  /* dc/kc are pointers into the allowlists above, not user input; the only
   * interpolation into SQL text in this file, and it is closed-vocabulary. */
  snprintf(ep->sql, sizeof ep->sql,
           "SELECT %s FROM evidence WHERE %s=?1%s%s AND %s IS NOT NULL "
           "ORDER BY rowid DESC LIMIT 1",
           dc, kc,
           ep->bind_ref_type ? " AND ref_type=?2" : "",
           ep->bind_tenant   ? (ep->bind_ref_type ? " AND tenant_id=?3"
                                                  : " AND tenant_id=?2") : "",
           dc);
  ep->available = 1;
}
/* Digest for one pinned reference, or "" when there is none. Never fails the
 * report: a lookup error is indistinguishable from "no evidence row". */
static void evidence_lookup(db_handle *db, const evidence_probe *ep,
                            const char *tenant_id, const char *ref_type,
                            const char *ref_id, char *out, size_t n) {
  out[0] = 0;
  if (!ep->available) return;
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, ep->sql, -1, &s, NULL) == SQLITE_OK) {
    int idx = 1;
    sqlite3_bind_text(s, idx++, ref_id, -1, SQLITE_TRANSIENT);
    if (ep->bind_ref_type) sqlite3_bind_text(s, idx++, ref_type, -1, SQLITE_TRANSIENT);
    if (ep->bind_tenant)   sqlite3_bind_text(s, idx++, tenant_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *v = ctext(s, 0);
      if (v) snprintf(out, n, "%s", v);
    }
  }
  sqlite3_finalize(s);
}

/* ── per-tenant template (paid-tier seam) ───────────────────────────────── */

static void tpl_copy(sqlite3_stmt *s, int col, char *dst, size_t n) {
  const char *v = col >= 0 ? ctext(s, col) : NULL;
  if (v) snprintf(dst, n, "%s", v);
}
int report_template_load(db_handle *db, const char *tenant_id,
                         report_template *out) {
  memset(out, 0, sizeof *out);
  if (!db || !db->h || !tenant_id || !*tenant_id) return 0;
  if (!table_exists(db, "tenant_report_templates")) return 0;
  /* Each column is probed independently so a table created later with only a
   * subset of the documented columns still works — this is a seam for a
   * feature this module does not own. */
  int c_cls = column_exists(db, "tenant_report_templates", "classification");
  int c_hdr = column_exists(db, "tenant_report_templates", "header_note");
  int c_ftr = column_exists(db, "tenant_report_templates", "footer_md");
  int c_logo= column_exists(db, "tenant_report_templates", "logo_url");
  char sql[320];
  snprintf(sql, sizeof sql,
           "SELECT %s,%s,%s,%s FROM tenant_report_templates WHERE tenant_id=?1",
           c_cls ? "classification" : "NULL", c_hdr ? "header_note" : "NULL",
           c_ftr ? "footer_md"      : "NULL", c_logo ? "logo_url"  : "NULL");
  sqlite3_stmt *s = NULL; int found = 0;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, tenant_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      tpl_copy(s, 0, out->classification, sizeof out->classification);
      tpl_copy(s, 1, out->header_note,    sizeof out->header_note);
      tpl_copy(s, 2, out->footer_md,      sizeof out->footer_md);
      tpl_copy(s, 3, out->logo_url,       sizeof out->logo_url);
      found = 1;
    }
  }
  sqlite3_finalize(s);
  out->present = found;
  return found;
}

/* ── request model ──────────────────────────────────────────────────────── */

#define RS_HEADER    0x01
#define RS_SUMMARY   0x02
#define RS_FINDINGS  0x04
#define RS_ENTITIES  0x08
#define RS_CITATIONS 0x10
#define RS_AUDIT     0x20
#define RS_ALL       0x3F

typedef struct { int html; unsigned sections; } report_req;

static unsigned section_bit(const char *n) {
  if (!n) return 0;
  if (!strcmp(n, "header"))    return RS_HEADER;
  if (!strcmp(n, "summary"))   return RS_SUMMARY;
  if (!strcmp(n, "findings"))  return RS_FINDINGS;
  if (!strcmp(n, "entities"))  return RS_ENTITIES;
  if (!strcmp(n, "citations")) return RS_CITATIONS;
  if (!strcmp(n, "audit"))     return RS_AUDIT;
  return 0;
}
/* 0 on success; -1 with `errcode` filled otherwise. `body` may be NULL. */
static int req_parse(const char *body, report_req *rq, char *errcode, size_t en) {
  rq->html = 0;
  rq->sections = RS_ALL;
  if (!body || !*body) return 0;
  cJSON *j = cJSON_Parse(body);
  if (!j) { snprintf(errcode, en, "bad_request"); return -1; }
  int rc = 0;
  const char *f = jstr(j, "format");
  if (f && *f) {
    if (!strcmp(f, "md") || !strcmp(f, "markdown")) rq->html = 0;
    else if (!strcmp(f, "html"))                    rq->html = 1;
    else { snprintf(errcode, en, "bad_format"); rc = -1; }
  }
  cJSON *secs = cJSON_GetObjectItem(j, "sections");
  if (!rc && secs && !cJSON_IsNull(secs)) {
    if (!cJSON_IsArray(secs)) { snprintf(errcode, en, "bad_request"); rc = -1; }
    else {
      unsigned bits = 0;
      cJSON *it = NULL;
      cJSON_ArrayForEach(it, secs) {
        if (!cJSON_IsString(it)) { snprintf(errcode, en, "unknown_section"); rc = -1; break; }
        unsigned b = section_bit(it->valuestring);
        /* A typo must never silently drop a section from a deliverable. */
        if (!b) { snprintf(errcode, en, "unknown_section"); rc = -1; break; }
        bits |= b;
      }
      if (!rc) rq->sections = bits ? bits : RS_ALL;
    }
  }
  cJSON_Delete(j);
  return rc;
}

/* ── the case header row ────────────────────────────────────────────────── */

typedef struct {
  char id[128], name[512], status[32], created_by[64];
  char created_at[48], updated_at[48], closed_at[48];
  int  priority;
  char *summary;                     /* malloc'd; may be NULL */
  char author_email[256], author_name[256];
} case_hdr;

static void case_hdr_free(case_hdr *h) { free(h->summary); h->summary = NULL; }

/* THE tenant gate. A case id that fails this is indistinguishable from one
 * that does not exist (404, never 403 — existence itself leaks). Same stance
 * and same predicate as casesapi.c's case_in_tenant(). */
static int case_load(db_handle *db, const char *tenant_id, const char *case_id,
                     case_hdr *h) {
  memset(h, 0, sizeof *h);
  if (!case_id || !*case_id) return 0;
  sqlite3_stmt *s = NULL; int found = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT id,name,summary,status,priority,created_by,created_at,"
        "updated_at,closed_at FROM cases WHERE id=?1 AND tenant_id=?2",
        -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, case_id,   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, tenant_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *v;
      if ((v = ctext(s,0))) snprintf(h->id,   sizeof h->id,   "%s", v);
      if ((v = ctext(s,1))) snprintf(h->name, sizeof h->name, "%s", v);
      if ((v = ctext(s,2))) h->summary = strdup(v);
      if ((v = ctext(s,3))) snprintf(h->status, sizeof h->status, "%s", v);
      h->priority = sqlite3_column_int(s, 4);
      if ((v = ctext(s,5))) snprintf(h->created_by, sizeof h->created_by, "%s", v);
      if ((v = ctext(s,6))) snprintf(h->created_at, sizeof h->created_at, "%s", v);
      if ((v = ctext(s,7))) snprintf(h->updated_at, sizeof h->updated_at, "%s", v);
      if ((v = ctext(s,8))) snprintf(h->closed_at,  sizeof h->closed_at,  "%s", v);
      found = 1;
    }
  }
  sqlite3_finalize(s);
  if (found && h->created_by[0]) {
    sqlite3_stmt *u = NULL;
    if (sqlite3_prepare_v2(db->h,
          "SELECT email,display_name FROM users WHERE id=?1", -1, &u, NULL) == SQLITE_OK) {
      sqlite3_bind_text(u, 1, h->created_by, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(u) == SQLITE_ROW) {
        const char *v;
        if ((v = ctext(u,0))) snprintf(h->author_email, sizeof h->author_email, "%s", v);
        if ((v = ctext(u,1))) snprintf(h->author_name,  sizeof h->author_name,  "%s", v);
      }
    }
    sqlite3_finalize(u);
  }
  return found;
}
/* casesapi.h: priority is 0..5, 0 = none … 5 = critical. */
static const char *priority_label(int p) {
  switch (p) {
    case 0: return "none";
    case 1: return "low";
    case 2: return "moderate";
    case 3: return "high";
    case 4: return "severe";
    case 5: return "critical";
    default: return "out-of-range";
  }
}

/* ── document model: one model, two emitters ────────────────────────────── */

typedef struct {
  ostream *o;
  int html;
  const report_template *tpl;
} demit;

/* Inline vs block is the only distinction the escaper needs.
 *  INLINE — headings, table cells, link text: Markdown-active characters are
 *           backslash-escaped and newlines fold to spaces, so a case name with
 *           a `|` cannot break a table and one with a backtick cannot open a
 *           code span.
 *  BLOCK  — the case summary and annotation body_md, which ARE Markdown by
 *           contract (annotationsapi.h stores body_md raw for the client to
 *           render). In Markdown they pass through verbatim; in HTML they are
 *           escaped and whitespace-preserved, NOT rendered — see reportapi.h. */
#define TX_INLINE 0
#define TX_BLOCK  1

static void d_text(demit *d, const char *s, int block, size_t cap) {
  if (!s) return;
  size_t n = strlen(s);
  int cut = 0;
  if (cap && n > cap) {
    n = cap; cut = 1;
    /* Do not sever a UTF-8 sequence; back off over continuation bytes. */
    while (n && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
  }
  for (size_t i = 0; i < n; i++) {
    char ch = s[i];
    if (d->html) {
      switch (ch) {
        case '&':  ob_s(d->o, "&amp;");  break;
        case '<':  ob_s(d->o, "&lt;");   break;
        case '>':  ob_s(d->o, "&gt;");   break;
        case '"':  ob_s(d->o, "&quot;"); break;
        case '\'': ob_s(d->o, "&#39;");  break;
        case '\r': break;
        case '\n': ob_s(d->o, block ? "\n" : " "); break;
        default:   ob_put(d->o, &ch, 1);
      }
    } else if (block) {
      if (ch == '\r') continue;
      ob_put(d->o, &ch, 1);
    } else {
      if (ch == '\r') continue;
      if (ch == '\n' || (unsigned char)ch < 0x20) { ob_s(d->o, " "); continue; }
      if (strchr("\\`*_[]<|", ch)) ob_put(d->o, "\\", 1);
      ob_put(d->o, &ch, 1);
    }
  }
  if (cut) ob_s(d->o, d->html ? " &hellip;[truncated]" : " \\[truncated\\]");
}
static void d_heading(demit *d, int lvl, const char *text) {
  if (d->html) {
    ob_fmt(d->o, "<h%d>", lvl);
    d_text(d, text, TX_INLINE, CAP_INLINE);
    ob_fmt(d->o, "</h%d>\n", lvl);
  } else {
    ob_s(d->o, "\n");
    for (int i = 0; i < lvl; i++) ob_put(d->o, "#", 1);
    ob_s(d->o, " ");
    d_text(d, text, TX_INLINE, CAP_INLINE);
    ob_s(d->o, "\n\n");
  }
}
/* Composite headings ("Finding F-3 — <label>") build between begin/end. */
static void d_heading_begin(demit *d, int lvl) {
  if (d->html) ob_fmt(d->o, "<h%d>", lvl);
  else { ob_s(d->o, "\n"); for (int i = 0; i < lvl; i++) ob_put(d->o, "#", 1); ob_s(d->o, " "); }
}
static void d_heading_end(demit *d, int lvl) {
  if (d->html) ob_fmt(d->o, "</h%d>\n", lvl);
  else ob_s(d->o, "\n\n");
}
/* A paragraph of already-safe literal text (never user content). */
static void d_note(demit *d, const char *literal) {
  if (d->html) ob_fmt(d->o, "<p class=\"note\">%s</p>\n", literal);
  else         ob_fmt(d->o, "%s\n\n", literal);
}
/* A block of user content: Markdown verbatim, HTML escaped + pre-wrapped. */
static void d_block(demit *d, const char *s, size_t cap) {
  if (!s || !*s) return;
  if (d->html) {
    ob_s(d->o, "<div class=\"blk\">");
    d_text(d, s, TX_BLOCK, cap);
    ob_s(d->o, "</div>\n");
  } else {
    d_text(d, s, TX_BLOCK, cap);
    ob_s(d->o, "\n\n");
  }
}
static void d_em(demit *d, const char *literal) {
  if (d->html) ob_fmt(d->o, "<p><em>%s</em></p>\n", literal);
  else         ob_fmt(d->o, "_%s_\n\n", literal);
}
/* Code span. A Markdown code span cannot contain a backtick without a longer
 * delimiter run, so content with one falls back to escaped inline text rather
 * than emitting a broken span. */
static void d_code_inline(demit *d, const char *s) {
  if (!s || !*s) { d_text(d, "—", TX_INLINE, 0); return; }
  if (d->html) {
    ob_s(d->o, "<code>");
    d_text(d, s, TX_INLINE, CAP_INLINE);
    ob_s(d->o, "</code>");
    return;
  }
  if (strchr(s, '`')) { d_text(d, s, TX_INLINE, CAP_INLINE); return; }
  ob_s(d->o, "`");
  d_text(d, s, TX_INLINE, CAP_INLINE);
  ob_s(d->o, "`");
}
/* Fenced block for one-line JSON. Unformatted cJSON output contains no raw
 * newline, so the payload can never begin a line with ``` and therefore can
 * never close the fence — which is why the JSON is printed unformatted here
 * rather than pretty. */
static void d_code_block(demit *d, const char *lang, const char *s) {
  if (d->html) {
    ob_s(d->o, "<pre><code>");
    d_text(d, s, TX_BLOCK, CAP_JSON);
    ob_s(d->o, "</code></pre>\n");
  } else {
    ob_fmt(d->o, "```%s\n", lang ? lang : "");
    d_text(d, s, TX_BLOCK, CAP_JSON);
    ob_s(d->o, "\n```\n\n");
  }
}
/* http/https/mailto only. Anything else (javascript:, data:, or a URL with
 * whitespace/controls/quotes in it) is rendered as an inert code span — the
 * HTML report must not turn a pinned snapshot field into a live vector. */
static int safe_url(const char *u) {
  if (!u || !*u) return 0;
  if (strncmp(u, "http://", 7) && strncmp(u, "https://", 8) &&
      strncmp(u, "mailto:", 7)) return 0;
  for (const char *p = u; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c < 0x21 || c == '"' || c == '\'' || c == '<' || c == '>') return 0;
  }
  return 1;
}
static void d_url(demit *d, const char *u) {
  if (!u || !*u) { ob_s(d->o, d->html ? "&mdash;" : "—"); return; }
  if (!safe_url(u)) { d_code_inline(d, u); return; }
  if (d->html) {
    ob_s(d->o, "<a href=\"");
    d_text(d, u, TX_INLINE, 0);                 /* attribute-escaped */
    ob_s(d->o, "\" rel=\"noopener noreferrer\">");
    d_text(d, u, TX_INLINE, CAP_INLINE);
    ob_s(d->o, "</a>");
  } else {
    /* Angle-bracket destination: safe_url() has already excluded '<', '>',
     * whitespace and quotes, so no escaping of the destination is needed. */
    ob_s(d->o, "[");
    d_text(d, u, TX_INLINE, CAP_INLINE);
    ob_fmt(d->o, "](<%s>)", u);
  }
}

/* Tables. Markdown needs the header + separator up front; HTML needs tags. */
static void d_table_begin(demit *d, const char *const *heads, int n) {
  if (d->html) {
    ob_s(d->o, "<table>\n<thead><tr>");
    for (int i = 0; i < n; i++) {
      ob_s(d->o, "<th>"); d_text(d, heads[i], TX_INLINE, CAP_INLINE); ob_s(d->o, "</th>");
    }
    ob_s(d->o, "</tr></thead>\n<tbody>\n");
  } else {
    ob_s(d->o, "\n|");
    for (int i = 0; i < n; i++) {
      ob_s(d->o, " "); d_text(d, heads[i], TX_INLINE, CAP_INLINE); ob_s(d->o, " |");
    }
    ob_s(d->o, "\n|");
    for (int i = 0; i < n; i++) ob_s(d->o, " --- |");
    ob_s(d->o, "\n");
  }
}
static void d_table_end(demit *d) {
  if (d->html) ob_s(d->o, "</tbody>\n</table>\n");
  else         ob_s(d->o, "\n");
}
static void d_tr_begin(demit *d) { ob_s(d->o, d->html ? "<tr>" : "|"); }
static void d_tr_end(demit *d)   { ob_s(d->o, d->html ? "</tr>\n" : "\n"); }
static void d_td_begin(demit *d) { ob_s(d->o, d->html ? "<td>" : " "); }
static void d_td_end(demit *d)   { ob_s(d->o, d->html ? "</td>" : " |"); }
/* Two-column key/value row with a plain-text value. */
static void d_kv(demit *d, const char *k, const char *v) {
  d_tr_begin(d);
  d_td_begin(d); d_text(d, k, TX_INLINE, CAP_INLINE); d_td_end(d);
  d_td_begin(d);
  if (v && *v) d_text(d, v, TX_INLINE, CAP_FIELD);
  else         ob_s(d->o, d->html ? "&mdash;" : "—");
  d_td_end(d);
  d_tr_end(d);
}
static void d_kv_code(demit *d, const char *k, const char *v) {
  d_tr_begin(d);
  d_td_begin(d); d_text(d, k, TX_INLINE, CAP_INLINE); d_td_end(d);
  d_td_begin(d); d_code_inline(d, v); d_td_end(d);
  d_tr_end(d);
}

/* ── document chrome ────────────────────────────────────────────────────── */

static const char *HTML_CSS =
  "body{font:15px/1.55 -apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,"
  "'Hiragino Kaku Gothic ProN','Noto Sans JP',sans-serif;color:#16181d;"
  "background:#fff;margin:0;padding:2.5rem clamp(1rem,4vw,3rem);"
  "max-width:60rem}"
  "h1{font-size:1.7rem;margin:0 0 .4rem}h2{font-size:1.25rem;margin:2.2rem 0 .6rem;"
  "border-bottom:1px solid #d8dbe0;padding-bottom:.3rem}"
  "h3{font-size:1.05rem;margin:1.6rem 0 .4rem}h4{font-size:.95rem;margin:1.1rem 0 .3rem;"
  "color:#444}"
  "table{border-collapse:collapse;width:100%;margin:.6rem 0;font-size:.9rem;"
  "display:block;overflow-x:auto}"
  "th,td{border:1px solid #d8dbe0;padding:.35rem .55rem;text-align:left;"
  "vertical-align:top}th{background:#f4f5f7;font-weight:600}"
  "code{font-family:ui-monospace,SFMono-Regular,Menlo,monospace;font-size:.85em;"
  "background:#f4f5f7;padding:.1em .35em;border-radius:3px;word-break:break-all}"
  "pre{background:#f4f5f7;padding:.7rem;border-radius:4px;overflow-x:auto}"
  "pre code{background:none;padding:0}"
  ".blk{white-space:pre-wrap;word-wrap:break-word;margin:.5rem 0}"
  ".note{color:#5a6070;font-size:.85rem}"
  ".banner{background:#7a1020;color:#fff;font-weight:700;letter-spacing:.06em;"
  "text-align:center;padding:.5rem;margin:0 0 1.4rem;border-radius:3px}"
  "footer{margin-top:3rem;padding-top:.8rem;border-top:1px solid #d8dbe0;"
  "color:#5a6070;font-size:.82rem}"
  "@media print{body{padding:0;max-width:none}.banner{-webkit-print-color-adjust:exact;"
  "print-color-adjust:exact}}"
  "@media (prefers-color-scheme:dark){body{background:#14161a;color:#e6e8ec}"
  "th{background:#20242b}code,pre{background:#20242b}"
  "th,td,h2,footer{border-color:#333842}.note,footer{color:#9aa1ad}}";

static void doc_open(demit *d, const case_hdr *h) {
  if (!d->html) return;
  ob_s(d->o, "<!doctype html>\n<html lang=\"en\">\n<head>\n"
             "<meta charset=\"utf-8\">\n"
             "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
             "<title>Case report — ");
  d_text(d, h->name[0] ? h->name : h->id, TX_INLINE, CAP_INLINE);
  ob_s(d->o, "</title>\n<style>");
  ob_s(d->o, HTML_CSS);
  ob_s(d->o, "</style>\n</head>\n<body>\n");
}
static void doc_close(demit *d, const char *generated_at) {
  /* Footer: the provenance line is emitted in both formats; the tenant footer
   * (paid-tier template) is appended verbatim beneath it when present. */
  if (d->html) ob_s(d->o, "<footer>\n");
  else         ob_s(d->o, "\n---\n\n");
  if (d->html) {
    ob_s(d->o, "<p>Generated by JapanOSINT at ");
    d_text(d, generated_at, TX_INLINE, CAP_INLINE);
    ob_s(d->o, " (UTC). Findings are rendered from the snapshots captured at "
               "pin time, not from a live re-read of the source corpora.</p>\n");
  } else {
    ob_s(d->o, "_Generated by JapanOSINT at ");
    d_text(d, generated_at, TX_INLINE, CAP_INLINE);
    ob_s(d->o, " (UTC). Findings are rendered from the snapshots captured at "
               "pin time, not from a live re-read of the source corpora._\n\n");
  }
  if (d->tpl && d->tpl->footer_md[0]) d_block(d, d->tpl->footer_md, sizeof d->tpl->footer_md);
  if (d->html) ob_s(d->o, "</footer>\n</body>\n</html>\n");
}
/* The classification banner is emitted regardless of the `sections` request:
 * a marking the caller can switch off is not a marking. */
static void doc_banner(demit *d, const case_hdr *h) {
  const report_template *tp = d->tpl;
  if (d->html && tp->logo_url[0] && safe_url(tp->logo_url)) {
    ob_s(d->o, "<p><img alt=\"\" style=\"max-height:48px\" src=\"");
    d_text(d, tp->logo_url, TX_INLINE, 0);
    ob_s(d->o, "\"></p>\n");
  }
  if (tp->classification[0]) {
    if (d->html) {
      ob_s(d->o, "<p class=\"banner\">");
      d_text(d, tp->classification, TX_INLINE, CAP_INLINE);
      ob_s(d->o, "</p>\n");
    } else {
      ob_s(d->o, "> **");
      d_text(d, tp->classification, TX_INLINE, CAP_INLINE);
      ob_s(d->o, "**\n\n");
    }
  }
  d_heading_begin(d, 1);
  d_text(d, "Case report — ", TX_INLINE, 0);
  d_text(d, h->name[0] ? h->name : h->id, TX_INLINE, CAP_INLINE);
  d_heading_end(d, 1);
  if (tp->header_note[0]) d_block(d, tp->header_note, sizeof tp->header_note);
}

/* ── sections ───────────────────────────────────────────────────────────── */

static void sec_header(demit *d, const tenant_ctx *t, const case_hdr *h,
                       const char *generated_at) {
  static const char *HEADS[] = { "Field", "Value" };
  d_heading(d, 2, "1. Case");
  d_table_begin(d, HEADS, 2);
  d_kv(d, "Case name", h->name);
  d_kv_code(d, "Case ID", h->id);
  d_kv(d, "Status", h->status);
  { char p[64];
    snprintf(p, sizeof p, "%d (%s)", h->priority, priority_label(h->priority));
    d_kv(d, "Priority", p); }
  { char a[560];
    if (h->author_name[0] && h->author_email[0])
      snprintf(a, sizeof a, "%s <%s>", h->author_name, h->author_email);
    else if (h->author_email[0]) snprintf(a, sizeof a, "%s", h->author_email);
    else if (h->created_by[0])   snprintf(a, sizeof a, "%s", h->created_by);
    else a[0] = 0;
    d_kv(d, "Author (case owner)", a); }
  d_kv(d, "Tenant", t->tname[0] ? t->tname : t->tenant_id);
  { char g[560];
    const char *dn = t->has_display ? t->display_name : NULL;
    if (dn && *dn && t->email[0]) snprintf(g, sizeof g, "%s <%s>", dn, t->email);
    else if (t->email[0])         snprintf(g, sizeof g, "%s", t->email);
    else                          snprintf(g, sizeof g, "%s", t->user_id);
    d_kv(d, "Generated by", g); }
  d_kv(d, "Generated at (UTC)", generated_at);
  d_kv(d, "Created at", h->created_at);
  d_kv(d, "Last updated", h->updated_at);
  if (h->closed_at[0]) d_kv(d, "Closed at", h->closed_at);
  d_table_end(d);
  /* Stated in the document, not only in the API docs: a reader handed an
   * .html file must not be left wondering whether a "real" PDF exists. */
  d_note(d, "This report is delivered as Markdown or HTML. The server does not "
            "produce PDF; convert out of process (browser print-to-PDF, pandoc, "
            "weasyprint) if a PDF deliverable is required.");
}

static void sec_summary(demit *d, const case_hdr *h) {
  d_heading(d, 2, "2. Executive summary");
  if (h->summary && *h->summary) d_block(d, h->summary, CAP_SUMMARY);
  else d_em(d, "No executive summary recorded on this case.");
}

/* Render one snapshot envelope's display copy. Scalars go into a key/value
 * table; nested objects/arrays follow as one-line JSON code blocks. Generic on
 * purpose — a per-ref_type field schema here would be a second copy of the
 * corpora's shapes and would drift from casesapi's synthesis the first time
 * either side gained a field. */
static void render_snapshot(demit *d, cJSON *env) {
  static const char *HEADS[] = { "Field", "Value" };
  cJSON *data = env ? cJSON_GetObjectItem(env, "data") : NULL;
  if (!data || cJSON_IsNull(data)) {
    d_em(d, "The referenced record could not be resolved when this item was "
            "pinned; the reference is recorded but no display copy exists.");
    return;
  }
  if (!cJSON_IsObject(data)) {
    char *js = cJSON_PrintUnformatted(data);
    if (js) { d_code_block(d, "json", js); free(js); }
    return;
  }
  int scalars = 0;
  cJSON *m = NULL;
  cJSON_ArrayForEach(m, data) {
    if (cJSON_IsObject(m) || cJSON_IsArray(m)) continue;
    if (!scalars++) d_table_begin(d, HEADS, 2);
    const char *k = m->string ? m->string : "";
    if (cJSON_IsString(m)) {
      d_kv(d, k, m->valuestring);
    } else if (cJSON_IsNumber(m)) {
      char n[48];
      if (m->valuedouble == (double)(long long)m->valuedouble)
        snprintf(n, sizeof n, "%lld", (long long)m->valuedouble);
      else snprintf(n, sizeof n, "%g", m->valuedouble);
      d_kv(d, k, n);
    } else if (cJSON_IsBool(m)) {
      d_kv(d, k, cJSON_IsTrue(m) ? "true" : "false");
    } else {
      d_kv(d, k, NULL);                       /* null → em dash */
    }
  }
  if (scalars) d_table_end(d);
  cJSON_ArrayForEach(m, data) {
    if (!cJSON_IsObject(m) && !cJSON_IsArray(m)) continue;
    d_heading(d, 5, m->string ? m->string : "(nested)");
    char *js = cJSON_PrintUnformatted(m);
    if (js) { d_code_block(d, "json", js); free(js); }
  }
}

/* Notes attached to the same (ref_type, ref_id). Scoped to the tenant AND to
 * this case (or to no case at all): a note another case attached to the same
 * intel item is the same tenant's data but is not this deliverable's context.
 * Tombstoned notes are shown as tombstones, not omitted — annotationsapi.h's
 * whole point is that a deleted note leaves a visible hole, not a silent one. */
static int sec_annotations(demit *d, db_handle *db, const char *tenant_id,
                           const char *case_id, const char *ref_type,
                           const char *ref_id) {
  sqlite3_stmt *s = NULL;
  int n = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT a.body_md,a.author_id,u.email,u.display_name,a.created_at,"
        "a.updated_at,a.deleted_at FROM annotations a "
        "LEFT JOIN users u ON u.id=a.author_id "
        "WHERE a.tenant_id=?1 AND a.ref_type=?2 AND a.ref_id=?3 "
        "AND (a.case_id IS NULL OR a.case_id=?4) "
        "ORDER BY a.created_at ASC", -1, &s, NULL) != SQLITE_OK) {
    sqlite3_finalize(s);
    return 0;
  }
  sqlite3_bind_text(s, 1, tenant_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, ref_type,  -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, ref_id,    -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 4, case_id,   -1, SQLITE_TRANSIENT);
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *body = ctext(s, 0), *email = ctext(s, 2), *disp = ctext(s, 3);
    const char *aid = ctext(s, 1), *cat = ctext(s, 4);
    const char *upd = ctext(s, 5), *del = ctext(s, 6);
    if (!n++) d_heading(d, 4, "Analyst notes");
    d_heading_begin(d, 5);
    d_text(d, disp ? disp : (email ? email : (aid ? aid : "unknown analyst")),
           TX_INLINE, CAP_INLINE);
    d_text(d, " — ", TX_INLINE, 0);
    d_text(d, cat ? cat : "", TX_INLINE, CAP_INLINE);
    if (upd) { d_text(d, " (edited ", TX_INLINE, 0);
               d_text(d, upd, TX_INLINE, CAP_INLINE);
               d_text(d, ")", TX_INLINE, 0); }
    d_heading_end(d, 5);
    if (del) {
      d_em(d, "This note was deleted; its body is withheld from the report. "
              "The tombstone is shown because the record of it having existed "
              "is itself part of the case history.");
    } else {
      d_block(d, body, CAP_NOTE);
    }
  }
  sqlite3_finalize(s);
  return n;
}

/* Pin order. case_items has no ordinal column (see casesapi.h's DDL), so the
 * pin order IS added_at ascending; ref_type/ref_id break ties deterministically
 * so two runs of the same report are byte-identical. */
static const char *SQL_ITEMS =
  "SELECT ci.ref_type,ci.ref_id,ci.label,ci.snapshot_json,ci.added_by,"
  "ci.added_at,u.email,u.display_name FROM case_items ci "
  "LEFT JOIN users u ON u.id=ci.added_by "
  "WHERE ci.case_id=?1 ORDER BY ci.added_at ASC, ci.ref_type ASC, ci.ref_id ASC";

static long sec_findings(demit *d, db_handle *db, const tenant_ctx *t,
                         const char *case_id) {
  static const char *HEADS[] = { "Field", "Value" };
  d_heading(d, 2, "3. Findings");
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, SQL_ITEMS, -1, &s, NULL) != SQLITE_OK) {
    sqlite3_finalize(s);
    d_em(d, "Findings could not be read.");
    return 0;
  }
  sqlite3_bind_text(s, 1, case_id, -1, SQLITE_TRANSIENT);
  long n = 0;
  while (!d->o->broken && sqlite3_step(s) == SQLITE_ROW) {
    const char *rt = ctext(s,0), *ri = ctext(s,1), *label = ctext(s,2);
    const char *snap = ctext(s,3), *by = ctext(s,4), *at = ctext(s,5);
    const char *bem = ctext(s,6), *bnm = ctext(s,7);
    n++;
    cJSON *env = snap && *snap ? cJSON_Parse(snap) : NULL;
    cJSON *data = env ? cJSON_GetObjectItem(env, "data") : NULL;
    const char *title = NULL;
    if (label && *label) title = label;
    else if (data && cJSON_IsObject(data)) {
      static const char *TK[] = { "title", "canonical", "value", "name", "label" };
      title = pick_str(data, TK, 5);
    }
    if (!title || !*title) title = ri;

    d_heading_begin(d, 3);
    ob_fmt(d->o, "F-%ld — ", n);
    d_text(d, title, TX_INLINE, CAP_INLINE);
    d_heading_end(d, 3);

    d_table_begin(d, HEADS, 2);
    d_kv(d, "Reference type", rt);
    d_kv_code(d, "Reference ID", ri);
    { char who[560];
      if (bnm && bem)      snprintf(who, sizeof who, "%s <%s>", bnm, bem);
      else if (bem)        snprintf(who, sizeof who, "%s", bem);
      else if (by)         snprintf(who, sizeof who, "%s", by);
      else                 who[0] = 0;
      d_kv(d, "Pinned by", who); }
    d_kv(d, "Pinned at", at);
    if (env) {
      d_kv(d, "Snapshot captured at", jstr(env, "captured_at"));
      d_kv(d, "Snapshot origin", jstr(env, "origin"));
      cJSON *res = cJSON_GetObjectItem(env, "resolved");
      d_kv(d, "Snapshot resolved", res ? (cJSON_IsTrue(res) ? "yes" : "no") : NULL);
    }
    d_table_end(d);

    d_heading(d, 4, "Snapshot (as pinned)");
    if (env) render_snapshot(d, env);
    else d_em(d, "No snapshot was stored with this pin.");

    if (rt && ri) sec_annotations(d, db, t->tenant_id, case_id, rt, ri);
    cJSON_Delete(env);
  }
  sqlite3_finalize(s);
  if (n == 0) d_em(d, "No items are pinned to this case.");
  return n;
}

/* ── entity roll-up ─────────────────────────────────────────────────────── */

typedef struct { char **ids; int n, cap, overflow; } strset;

static int strset_add(strset *s, const char *v) {
  if (!v || !*v) return 0;
  for (int i = 0; i < s->n; i++) if (!strcmp(s->ids[i], v)) return 0;
  if (s->n >= MAX_ENTITIES) { s->overflow = 1; return 0; }
  if (s->n == s->cap) {
    int nc = s->cap ? s->cap * 2 : 64;
    char **p = realloc(s->ids, (size_t)nc * sizeof *p);
    if (!p) { s->overflow = 1; return 0; }
    s->ids = p; s->cap = nc;
  }
  s->ids[s->n] = strdup(v);
  if (!s->ids[s->n]) { s->overflow = 1; return 0; }
  s->n++;
  return 1;
}
static void strset_free(strset *s) {
  for (int i = 0; i < s->n; i++) free(s->ids[i]);
  free(s->ids);
  s->ids = NULL; s->n = s->cap = 0;
}
/* entity_mentions keys breach records by their "breach:<keyid>" uid; a pin may
 * carry either form (casesapi accepts both). Try the raw ref first, then the
 * prefixed one, so neither pinning style loses its entities. */
static char *item_entities_json(db_handle *db, const char *ref_type, const char *ref_id) {
  char *js = entityapi_item_entities(db, ref_id);
  if (strcmp(ref_type, "breach_item") || strncmp(ref_id, "breach:", 7) == 0) return js;
  cJSON *j = js ? cJSON_Parse(js) : NULL;
  cJSON *arr = j ? cJSON_GetObjectItem(j, "data") : NULL;
  int empty = !arr || cJSON_GetArraySize(arr) == 0;
  cJSON_Delete(j);
  if (!empty) return js;
  free(js);
  char uid[640];
  snprintf(uid, sizeof uid, "breach:%s", ref_id);
  return entityapi_item_entities(db, uid);
}

static long sec_entities(demit *d, db_handle *db, const char *case_id) {
  static const char *HEADS[] = { "Entity", "Type", "Entity ID", "First cited by" };
  d_heading(d, 2, "4. Entities referenced");
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, SQL_ITEMS, -1, &s, NULL) != SQLITE_OK) {
    sqlite3_finalize(s);
    d_em(d, "Entities could not be read.");
    return 0;
  }
  sqlite3_bind_text(s, 1, case_id, -1, SQLITE_TRANSIENT);
  strset seen; memset(&seen, 0, sizeof seen);
  long rows = 0, fi = 0;
  int opened = 0;
  while (!d->o->broken && sqlite3_step(s) == SQLITE_ROW) {
    const char *rt = ctext(s,0), *ri = ctext(s,1), *snap = ctext(s,3);
    fi++;
    if (!rt || !ri) continue;
    if (!strcmp(rt, "entity")) {
      /* A directly pinned entity is described by its own snapshot — the frozen
       * display copy, not a live re-read, exactly like every other finding. */
      cJSON *env = snap && *snap ? cJSON_Parse(snap) : NULL;
      cJSON *data = env ? cJSON_GetObjectItem(env, "data") : NULL;
      const char *eid = (data && cJSON_IsObject(data)) ? jstr(data, "entity_id") : NULL;
      if (!eid) eid = ri;
      if (strset_add(&seen, eid)) {
        static const char *LK[] = { "canonical", "name_ja", "name_romaji", "value", "label" };
        const char *lbl = (data && cJSON_IsObject(data)) ? pick_str(data, LK, 5) : NULL;
        const char *ty  = (data && cJSON_IsObject(data)) ? jstr(data, "type") : NULL;
        if (!opened) { d_table_begin(d, HEADS, 4); opened = 1; }
        d_tr_begin(d);
        d_td_begin(d); d_text(d, lbl ? lbl : eid, TX_INLINE, CAP_INLINE); d_td_end(d);
        d_td_begin(d); d_text(d, ty ? ty : "—", TX_INLINE, CAP_INLINE); d_td_end(d);
        d_td_begin(d); d_code_inline(d, eid); d_td_end(d);
        d_td_begin(d); ob_fmt(d->o, "F-%ld", fi); d_td_end(d);
        d_tr_end(d);
        rows++;
      }
      cJSON_Delete(env);
      continue;
    }
    if (strcmp(rt, "intel_item") && strcmp(rt, "breach_item")) continue;
    char *js = item_entities_json(db, rt, ri);
    cJSON *j = js ? cJSON_Parse(js) : NULL;
    free(js);
    cJSON *arr = j ? cJSON_GetObjectItem(j, "data") : NULL;
    cJSON *e = NULL;
    cJSON_ArrayForEach(e, arr) {
      const char *eid = jstr(e, "entity_id");
      if (!strset_add(&seen, eid)) continue;
      const char *lbl = jstr(e, "label"), *val = jstr(e, "value"), *ty = jstr(e, "type");
      if (!opened) { d_table_begin(d, HEADS, 4); opened = 1; }
      d_tr_begin(d);
      d_td_begin(d);
      d_text(d, (val && *val) ? val : ((lbl && *lbl) ? lbl : eid), TX_INLINE, CAP_INLINE);
      d_td_end(d);
      d_td_begin(d); d_text(d, ty ? ty : "—", TX_INLINE, CAP_INLINE); d_td_end(d);
      d_td_begin(d); d_code_inline(d, eid); d_td_end(d);
      d_td_begin(d); ob_fmt(d->o, "F-%ld", fi); d_td_end(d);
      d_tr_end(d);
      rows++;
    }
    cJSON_Delete(j);
  }
  sqlite3_finalize(s);
  if (opened) d_table_end(d);
  else d_em(d, "No entities are linked to the items pinned to this case.");
  if (seen.overflow)
    d_note(d, "Entity list truncated at the per-report ceiling; further "
              "entities referenced by this case are not listed.");
  strset_free(&seen);
  return rows;
}

/* ── source citations ───────────────────────────────────────────────────── */

static long sec_citations(demit *d, db_handle *db, const tenant_ctx *t,
                          const char *case_id, const evidence_probe *ep) {
  static const char *H5[] = { "Finding", "Source", "Link", "Captured / fetched",
                              "Content SHA-256" };
  static const char *H4[] = { "Finding", "Source", "Link", "Captured / fetched" };
  static const char *SRC[] = { "source_id", "source", "breach_name", "sub_source_id" };
  static const char *LNK[] = { "link", "url", "permalink", "href" };
  static const char *TS[]  = { "fetched_at", "published_at", "first_seen",
                               "observed_at", "captured_at" };
  int ncol = ep->available ? 5 : 4;
  d_heading(d, 2, "5. Source citations");
  if (!ep->available)
    d_note(d, "Content hashes are not available in this deployment "
              "(no evidence store is present), so the SHA-256 column is omitted.");
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, SQL_ITEMS, -1, &s, NULL) != SQLITE_OK) {
    sqlite3_finalize(s);
    d_em(d, "Citations could not be read.");
    return 0;
  }
  sqlite3_bind_text(s, 1, case_id, -1, SQLITE_TRANSIENT);
  d_table_begin(d, ep->available ? H5 : H4, ncol);
  long n = 0, fi = 0;
  while (!d->o->broken && sqlite3_step(s) == SQLITE_ROW) {
    const char *rt = ctext(s,0), *ri = ctext(s,1), *snap = ctext(s,3);
    fi++;
    cJSON *env = snap && *snap ? cJSON_Parse(snap) : NULL;
    cJSON *data = env ? cJSON_GetObjectItem(env, "data") : NULL;
    int obj = data && cJSON_IsObject(data);
    const char *src = obj ? pick_str(data, SRC, 4) : NULL;
    const char *lnk = obj ? pick_str(data, LNK, 4) : NULL;
    const char *ts  = obj ? pick_str(data, TS, 5)  : NULL;
    /* The envelope's captured_at is the pin-time capture instant and is the
     * fallback when the snapshot itself carries no timestamp — it is always
     * present, so a citation never lacks a time. */
    if (!ts) ts = env ? jstr(env, "captured_at") : NULL;
    char sha[80] = {0};
    if (ep->available && rt && ri)
      evidence_lookup(db, ep, t->tenant_id, rt, ri, sha, sizeof sha);

    d_tr_begin(d);
    d_td_begin(d); ob_fmt(d->o, "F-%ld", fi); d_td_end(d);
    d_td_begin(d); d_code_inline(d, src ? src : (rt ? rt : "")); d_td_end(d);
    d_td_begin(d); d_url(d, lnk); d_td_end(d);
    d_td_begin(d); d_text(d, ts ? ts : "—", TX_INLINE, CAP_INLINE); d_td_end(d);
    if (ep->available) {
      d_td_begin(d);
      if (sha[0]) d_code_inline(d, sha);
      else ob_s(d->o, d->html ? "&mdash;" : "—");
      d_td_end(d);
    }
    d_tr_end(d);
    n++;
    cJSON_Delete(env);
  }
  sqlite3_finalize(s);
  d_table_end(d);
  if (n == 0) d_em(d, "No citations: no items are pinned to this case.");
  return n;
}

/* ── audit excerpt ──────────────────────────────────────────────────────── */

static long sec_audit(demit *d, db_handle *db, const tenant_ctx *t,
                      const char *case_id, int *truncated) {
  static const char *HEADS[] = { "Timestamp (UTC)", "Action", "Actor", "Detail" };
  d_heading(d, 2, "6. Audit trail excerpt");
  *truncated = 0;
  sqlite3_stmt *s = NULL;
  /* target = the case id (casesapi audits case.* with the case id) plus the
   * "<case_id>/..." shape any later module may adopt for sub-objects. */
  if (sqlite3_prepare_v2(db->h,
        "SELECT a.ts,a.action,a.user_id,u.email,u.display_name,a.target,"
        "a.payload_json FROM audit_events a LEFT JOIN users u ON u.id=a.user_id "
        "WHERE a.tenant_id=?1 AND (a.target=?2 OR a.target LIKE ?3) "
        "ORDER BY a.ts DESC LIMIT ?4", -1, &s, NULL) != SQLITE_OK) {
    sqlite3_finalize(s);
    d_em(d, "The audit trail could not be read.");
    return 0;
  }
  char like[192];
  snprintf(like, sizeof like, "%s/%%", case_id);
  sqlite3_bind_text(s, 1, t->tenant_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, case_id,      -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, like,         -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 4, MAX_AUDIT_ROWS + 1);   /* +1 → truncation is known */
  long n = 0;
  int opened = 0;
  while (!d->o->broken && sqlite3_step(s) == SQLITE_ROW) {
    if (n == MAX_AUDIT_ROWS) { *truncated = 1; break; }
    const char *ts = ctext(s,0), *ac = ctext(s,1), *uid = ctext(s,2);
    const char *em = ctext(s,3), *dn = ctext(s,4), *tg = ctext(s,5);
    const char *pl = ctext(s,6);
    if (!opened) { d_table_begin(d, HEADS, 4); opened = 1; }
    d_tr_begin(d);
    d_td_begin(d); d_text(d, ts ? ts : "—", TX_INLINE, CAP_INLINE); d_td_end(d);
    d_td_begin(d); d_code_inline(d, ac); d_td_end(d);
    d_td_begin(d);
    d_text(d, dn ? dn : (em ? em : (uid ? uid : "system")), TX_INLINE, CAP_INLINE);
    d_td_end(d);
    d_td_begin(d);
    { int wrote = 0;
      /* The target is only interesting when it is a sub-object of the case
       * ("<case_id>/..."); repeating the case id on every row is noise. */
      if (tg && strcmp(tg, case_id)) { d_code_inline(d, tg); wrote = 1; }
      /* Payloads are tenant-authored JSON; rendered as escaped inline text,
       * capped, never as markup. */
      if (pl && *pl && strcmp(pl, "{}")) {
        if (wrote) ob_s(d->o, " ");
        d_text(d, pl, TX_INLINE, 240);
        wrote = 1;
      }
      if (!wrote) ob_s(d->o, d->html ? "&mdash;" : "—"); }
    d_td_end(d);
    d_tr_end(d);
    n++;
  }
  sqlite3_finalize(s);
  if (opened) d_table_end(d);
  else d_em(d, "No audit events are recorded against this case.");
  if (*truncated) {
    char note[160];
    snprintf(note, sizeof note,
             "Excerpt only: the %d most recent audit events for this case are "
             "shown; older events remain in the audit log.", MAX_AUDIT_ROWS);
    d_note(d, note);
  }
  return n;
}

/* ── public API ─────────────────────────────────────────────────────────── */

/* Filename alphabet is [a-z0-9-] plus a generated timestamp, so the
 * Content-Disposition header can never carry user bytes. The case NAME is
 * deliberately absent for exactly that reason. */
static void safe_slug(const char *in, char *out, size_t n) {
  size_t o = 0;
  for (const char *p = in ? in : ""; *p && o + 1 < n; p++) {
    char c = *p;
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') out[o++] = c;
  }
  if (o == 0) { snprintf(out, n, "case"); return; }
  out[o] = 0;
}

int report_plan_request(db_handle *db, const tenant_ctx *t,
                        const char *case_id, const char *body,
                        report_plan *out, int *status) {
  memset(out, 0, sizeof *out);
  if (!db || !db->h || !t) {
    snprintf(out->error, sizeof out->error, "server_error");
    if (status) *status = 500;
    return 1;
  }
  report_req rq;
  char ec[48] = {0};
  if (req_parse(body, &rq, ec, sizeof ec) != 0) {
    snprintf(out->error, sizeof out->error, "%s", ec[0] ? ec : "bad_request");
    if (status) *status = 400;
    return 1;
  }
  case_hdr h;
  if (!case_load(db, t->tenant_id, case_id, &h)) {
    case_hdr_free(&h);
    /* 404 for "another tenant's case" as well: a 403 would confirm the id. */
    snprintf(out->error, sizeof out->error, "not_found");
    if (status) *status = 404;
    return 1;
  }
  case_hdr_free(&h);
  char slug[96], stamp[32];
  safe_slug(case_id, slug, sizeof slug);
  stamp_now(stamp, sizeof stamp);
  snprintf(out->content_type, sizeof out->content_type,
           rq.html ? "text/html; charset=utf-8" : "text/markdown; charset=utf-8");
  snprintf(out->filename, sizeof out->filename, "case-%.80s-%s.%s",
           slug, stamp, rq.html ? "html" : "md");
  if (status) *status = 200;
  return 0;
}

int report_run(db_handle *db, const tenant_ctx *t, const char *case_id,
               const char *body, report_write_fn write, void *write_ctx,
               long *out_bytes, int *status) {
  if (out_bytes) *out_bytes = 0;
  if (!db || !db->h || !t || !write) { if (status) *status = 500; return 1; }

  report_req rq;
  char ec[48] = {0};
  if (req_parse(body, &rq, ec, sizeof ec) != 0) { if (status) *status = 400; return 1; }

  case_hdr h;
  if (!case_load(db, t->tenant_id, case_id, &h)) {
    case_hdr_free(&h);
    if (status) *status = 404;
    return 1;
  }

  report_template tpl;
  report_template_load(db, t->tenant_id, &tpl);
  evidence_probe ep;
  evidence_probe_init(db, &ep);

  ostream *o = calloc(1, sizeof *o);
  if (!o) { case_hdr_free(&h); if (status) *status = 500; return 1; }
  o->fn = write; o->ctx = write_ctx;
  demit d = { o, rq.html, &tpl };

  char gen[40];
  iso_now(gen, sizeof gen);

  long findings = 0, entities = 0, citations = 0, audits = 0;
  int audit_trunc = 0;

  doc_open(&d, &h);
  doc_banner(&d, &h);
  if (rq.sections & RS_HEADER)    sec_header(&d, t, &h, gen);
  if (rq.sections & RS_SUMMARY)   sec_summary(&d, &h);
  if (rq.sections & RS_FINDINGS)  findings  = sec_findings(&d, db, t, case_id);
  if (rq.sections & RS_ENTITIES)  entities  = sec_entities(&d, db, case_id);
  if (rq.sections & RS_CITATIONS) citations = sec_citations(&d, db, t, case_id, &ep);
  if (rq.sections & RS_AUDIT)     audits    = sec_audit(&d, db, t, case_id, &audit_trunc);
  doc_close(&d, gen);
  ob_flush(o);

  int aborted = o->broken;
  long bytes = o->total;
  free(o);
  case_hdr_free(&h);

  /* Audit AFTER the walk so the counts are true, and on the aborted path too:
   * a half-delivered report is still an egress event — the bytes left. */
  {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "format", rq.html ? "html" : "md");
    cJSON *secs = cJSON_AddArrayToObject(p, "sections");
    static const struct { unsigned bit; const char *name; } SEC[] = {
      { RS_HEADER, "header" }, { RS_SUMMARY, "summary" },
      { RS_FINDINGS, "findings" }, { RS_ENTITIES, "entities" },
      { RS_CITATIONS, "citations" }, { RS_AUDIT, "audit" } };
    for (size_t i = 0; i < sizeof SEC / sizeof SEC[0]; i++)
      if (rq.sections & SEC[i].bit)
        cJSON_AddItemToArray(secs, cJSON_CreateString(SEC[i].name));
    cJSON_AddNumberToObject(p, "findings",   (double)findings);
    cJSON_AddNumberToObject(p, "entities",   (double)entities);
    cJSON_AddNumberToObject(p, "citations",  (double)citations);
    cJSON_AddNumberToObject(p, "audit_rows", (double)audits);
    cJSON_AddNumberToObject(p, "bytes",      (double)bytes);
    cJSON_AddBoolToObject(p, "evidence",        ep.available);
    cJSON_AddBoolToObject(p, "template",        tpl.present);
    cJSON_AddBoolToObject(p, "truncated_audit", audit_trunc);
    cJSON_AddBoolToObject(p, "aborted",         aborted);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);
    audit_write(db, t->tenant_id, t->user_id, "report.generate", case_id, pj);
    free(pj);
  }

  if (out_bytes) *out_bytes = bytes;
  if (status) *status = 200;
  return aborted ? 1 : 0;
}
