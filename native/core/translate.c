/* core/translate.c — JA→EN machine translation at ingest + the English half
 * of FTS. See translate.h for the design rationale (why the English text goes
 * into the existing `keywords` FTS column instead of new FTS columns, why the
 * rowid watermark exists, and the language-detection threshold). This file is
 * the mechanics. */
#include "translate.h"
#include "fts.h"
#include "progress.h"
#include "prompts.h"
#include "../source.h"
#include "../third_party/sqlite3.h"
#include "../third_party/cJSON.h"
#include <openssl/rand.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── small shared helpers (same idioms as core/alertsapi.c) ─────────────── */

static void uuid4(char out[37]) {
  unsigned char b[16];
  RAND_bytes(b, 16);
  b[6] = (b[6] & 0x0F) | 0x40; b[8] = (b[8] & 0x3F) | 0x80;
  snprintf(out, 37,
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
    b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
}

static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}

static char *dupcol(sqlite3_stmt *s, int i) {
  const char *c = ctext(s, i);
  return c ? strdup(c) : NULL;
}

static char *err_env(int *st, int code, const char *msg) {
  if (st) *st = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", msg);
  char *j = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return j;
}

static int env_int(const char *k, int dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  char *end = NULL;
  long n = strtol(v, &end, 10);
  if (end == v) return dflt;
  return (int)n;
}

/* Engine label written into the response so the UI can say WHAT translated
 * this, not merely that a machine did. */
static const char *engine_id(void) {
  const char *m = getenv("LLM_MODEL");
  return (m && *m) ? m : "llama-server/local";
}

/* first balanced {...} → cJSON. Same tolerance as entity_enrich.c's
 * extract_json: models prepend prose to JSON often enough that a strict parse
 * of the whole completion throws away good translations. */
static cJSON *extract_json(const char *raw) {
  if (!raw) return NULL;
  const char *s = strchr(raw, '{');
  if (!s) return NULL;
  int d = 0;
  for (const char *p = s; *p; p++) {
    if (*p == '{') d++;
    else if (*p == '}' && --d == 0) {
      char *sub = strndup(s, (size_t)(p - s + 1));
      cJSON *j = sub ? cJSON_Parse(sub) : NULL;
      free(sub);
      return j;
    }
  }
  return NULL;
}

/* ── UTF-8 + language detection ─────────────────────────────────────────── */

/* Minimal decoder, mirroring core/fts.c's (well-formed input assumed). */
static unsigned utf8_next(const unsigned char *s, int *adv) {
  if (s[0] < 0x80) { *adv = 1; return s[0]; }
  if ((s[0] & 0xE0) == 0xC0) { *adv = 2; return ((s[0] & 0x1Fu) << 6) | (s[1] & 0x3Fu); }
  if ((s[0] & 0xF0) == 0xE0) { *adv = 3;
    return ((s[0] & 0x0Fu) << 12) | ((s[1] & 0x3Fu) << 6) | (s[2] & 0x3Fu); }
  if ((s[0] & 0xF8) == 0xF0) { *adv = 4;
    return ((s[0] & 0x07u) << 18) | ((s[1] & 0x3Fu) << 12) |
           ((s[2] & 0x3Fu) << 6) | (s[3] & 0x3Fu); }
  *adv = 1; return s[0];
}

/* Identical range set to core/fts.c fts_has_japanese (jpTokenizer.js JP_RE).
 * fts.h only exposes a boolean; the detector needs a count, hence the copy. */
static int cp_is_jp(unsigned cp) {
  return (cp >= 0x3040 && cp <= 0x309F) || (cp >= 0x30A0 && cp <= 0x30FF) ||
         (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
         (cp >= 0xFF66 && cp <= 0xFF9F);
}

/* Digits, punctuation, whitespace and symbols are counted in NEITHER bucket:
 * the ratio is CJK share of letter mass, so a Japanese table of figures is
 * not diluted into "English" by its numbers. */
static void cp_counts(const char *s, long *cjk, long *latin) {
  if (!s) return;
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    int adv;
    unsigned cp = utf8_next(p, &adv);
    if (cp_is_jp(cp)) (*cjk)++;
    else if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) (*latin)++;
    p += adv;
  }
}

static int lang_says_ja(const char *language) {
  if (!language) return 0;
  return (language[0] == 'j' || language[0] == 'J') &&
         (language[1] == 'a' || language[1] == 'A');
}

int translate_is_japanese(const char *language, const char *title,
                          const char *summary, const char *body) {
  long cjk = 0, latin = 0;
  cp_counts(title, &cjk, &latin);
  cp_counts(summary, &cjk, &latin);
  cp_counts(body, &cjk, &latin);
  if (cjk == 0) return 0;

  /* An explicit `ja` tag is a positive signal only — it lowers the floor to a
   * single CJK codepoint but does not bypass the ratio, and its absence (or a
   * contradictory "en") never vetoes a plainly-Japanese row. */
  int min_cjk = env_int("TRANSLATE_JP_MIN_CJK", 4);
  if (min_cjk < 1) min_cjk = 1;
  if (lang_says_ja(language)) min_cjk = 1;
  if (cjk < min_cjk) return 0;

  int pct = env_int("TRANSLATE_JP_RATIO_PCT", 20);
  if (pct < 1) pct = 1;
  if (pct > 100) pct = 100;
  return cjk * 100 >= (cjk + latin) * (long)pct;
}

/* ── the pending predicate ──────────────────────────────────────────────── */

/* ONE literal, shared by the partial index and by every query that wants it.
 * This is not tidiness: SQLite only uses a partial index when the query's
 * WHERE clause *syntactically* implies the index's WHERE clause. A bound
 * parameter (`translate_failed < ?1`) can never be proven to imply
 * `translate_failed < 3`, so the retry cap CANNOT be an env knob without
 * silently turning every pending scan into a full table scan of intel_items.
 * Three attempts is therefore a compiled-in constant, baked into the DDL.
 *
 *   translated_at IS NULL   → no translation yet
 *   translate_failed >= 0   → excludes the -1 "not Japanese" sentinel
 *   translate_failed <  3   → excludes rows that have burned their retries */
#define TR_PENDING_WHERE \
  "translated_at IS NULL AND translate_failed >= 0 AND translate_failed < 3"

/* ── migration ──────────────────────────────────────────────────────────── */

static pthread_mutex_t g_mig_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_migrated = 0;

void translate_migrate(db_handle *db) {
  if (!db || !db->h) return;
  pthread_mutex_lock(&g_mig_lock);
  if (g_migrated) { pthread_mutex_unlock(&g_mig_lock); return; }

  /* Columns FIRST — the partial index below references them. */
  ensure_column(db, "intel_items", "title_en",         "TEXT");
  ensure_column(db, "intel_items", "body_en",           "TEXT");
  ensure_column(db, "intel_items", "summary_en",        "TEXT");
  ensure_column(db, "intel_items", "translated_at",     "TEXT");
  ensure_column(db, "intel_items", "translate_failed",  "INTEGER DEFAULT 0");

  static const char *DDL =
    "CREATE TABLE IF NOT EXISTS translate_state ("
    "  k TEXT PRIMARY KEY,"
    "  v TEXT NOT NULL"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_intel_items_translate_pending"
    "  ON intel_items(fetched_at DESC)"
    "  WHERE " TR_PENDING_WHERE ";"
    "CREATE INDEX IF NOT EXISTS idx_intel_items_fts_uid_map_rowid"
    "  ON intel_items_fts_uid_map(rowid);";
  char *e = NULL;
  if (sqlite3_exec(db->h, DDL, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "[translate] migrate: %s\n", e ? e : "?");
    sqlite3_free(e);
  }
  g_migrated = 1;
  pthread_mutex_unlock(&g_mig_lock);
}

/* ── translate_state key/value ──────────────────────────────────────────── */

static const char *WM_KEY = "fts_rowid_watermark";

static sqlite3_int64 state_get_i64(sqlite3 *h, const char *k, int *found) {
  sqlite3_int64 v = 0;
  if (found) *found = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, "SELECT v FROM translate_state WHERE k=?1",
                         -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, k, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      const char *t = (const char *)sqlite3_column_text(s, 0);
      if (t) { v = strtoll(t, NULL, 10); if (found) *found = 1; }
    }
    sqlite3_finalize(s);
  }
  return v;
}

static void state_put_i64(sqlite3 *h, const char *k, sqlite3_int64 v) {
  char buf[32];
  snprintf(buf, sizeof buf, "%lld", (long long)v);
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "INSERT INTO translate_state(k,v) VALUES(?1,?2)"
      " ON CONFLICT(k) DO UPDATE SET v=excluded.v", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, k, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, buf, -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }
}

/* ── FTS: push the English text into the (otherwise unused) keywords col ─── */

/* Join the three English fields into the one indexed blob. fts_segment() is
 * used for symmetry with core/intel.c's write path: it is a pure passthrough
 * for Latin text (fts.h contract) and correctly segments any Japanese the
 * model left behind (proper nouns it chose not to romanise), so those stay
 * searchable rather than becoming one unmatchable token. */
static char *join_en(const char *title, const char *summary, const char *body) {
  size_t n = (title ? strlen(title) : 0) + (summary ? strlen(summary) : 0) +
             (body ? strlen(body) : 0) + 4;
  char *raw = malloc(n);
  if (!raw) return NULL;
  raw[0] = '\0';
  if (title && *title) { strcat(raw, title); strcat(raw, " "); }
  if (summary && *summary) { strcat(raw, summary); strcat(raw, " "); }
  if (body && *body) strcat(raw, body);
  char *seg = fts_segment(raw);
  free(raw);
  return seg;
}

/* UPDATE, not DELETE+INSERT: intel_items_fts is a regular (content-carrying)
 * fts5 table, so an in-place column update is legal and — critically — keeps
 * the rowid, which means it does NOT look like a re-ingest to the watermark
 * pass and does not invalidate intel_items_fts_uid_map. */
static int fts_put_en(sqlite3 *h, const char *uid, const char *kw) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "UPDATE intel_items_fts SET keywords=?1"
      " WHERE rowid=(SELECT rowid FROM intel_items_fts_uid_map WHERE uid=?2)",
      -1, &s, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, kw ? kw : "", -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, uid, -1, SQLITE_TRANSIENT);
  int ok = sqlite3_step(s) == SQLITE_DONE;
  sqlite3_finalize(s);
  return ok;
}

static int fts_put_en_by_rowid(sqlite3 *h, sqlite3_int64 rid, const char *kw) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, "UPDATE intel_items_fts SET keywords=?1"
                            " WHERE rowid=?2", -1, &s, NULL) != SQLITE_OK)
    return 0;
  sqlite3_bind_text(s, 1, kw ? kw : "", -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(s, 2, rid);
  int ok = sqlite3_step(s) == SQLITE_DONE;
  sqlite3_finalize(s);
  return ok;
}

int translate_fts_resync(db_handle *db, int limit, volatile int *cancel) {
  if (!db || !db->h) return 0;
  translate_migrate(db);
  sqlite3 *h = db->h;
  if (limit <= 0) limit = env_int("TRANSLATE_RESYNC_BATCH", 2000);
  if (limit <= 0) limit = 2000;

  /* Seed on first ever run: nothing is translated yet, so no FTS row can be
   * stale, so the entire existing corpus can be skipped in one statement.
   * This is what keeps enabling this module from sweeping the corpus. */
  int found = 0;
  sqlite3_int64 wm = state_get_i64(h, WM_KEY, &found);
  if (!found) {
    sqlite3_int64 mx = 0;
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(h,
        "SELECT COALESCE(MAX(rowid),0) FROM intel_items_fts_uid_map",
        -1, &s, NULL) == SQLITE_OK) {
      if (sqlite3_step(s) == SQLITE_ROW) mx = sqlite3_column_int64(s, 0);
      sqlite3_finalize(s);
    }
    state_put_i64(h, WM_KEY, mx);
    return 0;
  }

  /* Read the window first, finalize, then write — never hold a cursor over
   * the table we are about to UPDATE. */
  typedef struct { sqlite3_int64 rid; char *uid, *t, *s, *b; } wrow;
  wrow *rows = calloc((size_t)limit, sizeof *rows);
  if (!rows) return 0;
  int nr = 0;
  sqlite3_int64 maxseen = wm;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT m.rowid, i.uid, i.title_en, i.summary_en, i.body_en"
      " FROM intel_items_fts_uid_map m"
      " JOIN intel_items i ON i.uid = m.uid"
      " WHERE m.rowid > ?1"
      " ORDER BY m.rowid LIMIT ?2", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int64(s, 1, wm);
    sqlite3_bind_int(s, 2, limit);
    while (nr < limit && sqlite3_step(s) == SQLITE_ROW) {
      rows[nr].rid = sqlite3_column_int64(s, 0);
      rows[nr].uid = dupcol(s, 1);
      rows[nr].t   = dupcol(s, 2);
      rows[nr].s   = dupcol(s, 3);
      rows[nr].b   = dupcol(s, 4);
      if (rows[nr].rid > maxseen) maxseen = rows[nr].rid;
      nr++;
    }
    sqlite3_finalize(s);
  }

  int wrote = 0;
  if (nr > 0) {
    sqlite3_exec(h, "BEGIN", NULL, NULL, NULL);
    for (int i = 0; i < nr; i++) {
      if (cancel && *cancel) break;
      /* Untranslated rows are walked over, not written: they exist only to let
       * the watermark advance past them. */
      if (!rows[i].t && !rows[i].s && !rows[i].b) continue;
      char *kw = join_en(rows[i].t, rows[i].s, rows[i].b);
      if (kw && fts_put_en_by_rowid(h, rows[i].rid, kw)) wrote++;
      free(kw);
    }
    sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
    /* Advance only over what was actually examined — a cancel mid-window just
     * means the next pass redoes the tail. */
    state_put_i64(h, WM_KEY, maxseen);
  }

  for (int i = 0; i < nr; i++) {
    free(rows[i].uid); free(rows[i].t); free(rows[i].s); free(rows[i].b);
  }
  free(rows);
  if (wrote)
    fprintf(stderr, "[translate] fts resync: scanned=%d rewritten=%d wm=%lld\n",
            nr, wrote, (long long)maxseen);
  return wrote;
}

/* ── the translator ─────────────────────────────────────────────────────── */

/* Clip on a UTF-8 boundary — a body cut mid-sequence would reach the model as
 * mojibake and poison the whole translation. */
static char *clip_utf8(const char *s, size_t maxb) {
  if (!s) return NULL;
  size_t n = strlen(s);
  if (n <= maxb) return strdup(s);
  size_t c = maxb;
  while (c > 0 && ((unsigned char)s[c] & 0xC0) == 0x80) c--;
  char *o = malloc(c + 1);
  if (!o) return NULL;
  memcpy(o, s, c);
  o[c] = '\0';
  return o;
}

static const char *SYS_PROMPT =
  "You are a professional Japanese-to-English translator working on an OSINT "
  "intelligence corpus. Translate the supplied fields into natural, faithful "
  "English.\n"
  "Rules:\n"
  "- Translate literally and completely. Do NOT summarise, shorten, "
  "editorialise or add commentary.\n"
  "- Preserve every proper noun, place name, organisation name, number, date, "
  "unit and identifier exactly. Use the established English form of a "
  "Japanese name where one exists; otherwise romanise it (Hepburn).\n"
  "- Keep the original meaning even when the source is terse or bureaucratic.\n"
  "- If an input field is empty, return an empty string for it.\n"
  "- Output ONLY a JSON object with exactly the keys \"title\", \"summary\" "
  "and \"body\". No prose before or after it.";

static char *build_messages(const char *title, const char *summary,
                            const char *body) {
  cJSON *arr = cJSON_CreateArray();
  cJSON *sys = cJSON_CreateObject();
  cJSON_AddStringToObject(sys, "role", "system");
  cJSON_AddStringToObject(sys, "content", SYS_PROMPT);
  cJSON_AddItemToArray(arr, sys);

  /* Build the user turn through cJSON so the source text — arbitrary bytes
   * from a scraped feed — is escaped correctly instead of splicing raw into a
   * hand-written JSON string. */
  cJSON *fields = cJSON_CreateObject();
  cJSON_AddStringToObject(fields, "title", title ? title : "");
  cJSON_AddStringToObject(fields, "summary", summary ? summary : "");
  cJSON_AddStringToObject(fields, "body", body ? body : "");
  char *fj = cJSON_PrintUnformatted(fields);
  cJSON_Delete(fields);
  if (!fj) { cJSON_Delete(arr); return NULL; }

  size_t need = strlen(fj) + 128;
  char *uc = malloc(need);
  if (!uc) { free(fj); cJSON_Delete(arr); return NULL; }
  snprintf(uc, need, "Translate these Japanese fields to English:\n%s", fj);
  free(fj);

  cJSON *usr = cJSON_CreateObject();
  cJSON_AddStringToObject(usr, "role", "user");
  cJSON_AddStringToObject(usr, "content", uc);
  free(uc);
  cJSON_AddItemToArray(arr, usr);

  char *out = cJSON_PrintUnformatted(arr);
  cJSON_Delete(arr);
  return out;
}

/* Mark a row so the pending index stops returning it. `failed` semantics:
 *   -1  not applicable (detector said this is not Japanese) — terminal
 *    N   consecutive failures; terminal at TRANSLATE_MAX_FAILURES */
static void mark_skip(sqlite3 *h, const char *uid) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "UPDATE intel_items SET translate_failed=-1 WHERE uid=?1",
      -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_step(s); sqlite3_finalize(s);
  }
}

static void mark_fail(sqlite3 *h, const char *uid) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "UPDATE intel_items"
      " SET translate_failed = CASE WHEN translate_failed < 0 THEN 1"
      "                             ELSE translate_failed + 1 END"
      " WHERE uid=?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
    sqlite3_step(s); sqlite3_finalize(s);
  }
}

static void bind_txt_or_null(sqlite3_stmt *s, int i, const char *v) {
  if (v && *v) sqlite3_bind_text(s, i, v, -1, SQLITE_TRANSIENT);
  else sqlite3_bind_null(s, i);
}

typedef struct { char *uid, *title, *body, *summary, *lang; } trow;

int translate_run(db_handle *db, llm_client *llm, int limit,
                  volatile int *cancel) {
  if (!db || !db->h) return 0;
  translate_migrate(db);
  sqlite3 *h = db->h;

  /* Degrade, never spin: no LLM means this tick does nothing and returns.
   * Ingest is on another path entirely and is unaffected either way. */
  if (!llm || !llm_healthy(llm)) {
    fprintf(stderr, "[translate] llama-server unavailable; skipping tick\n");
    return 0;
  }

  if (limit <= 0) limit = env_int("TRANSLATE_BATCH", 25);
  if (limit <= 0) limit = 25;
  int body_max = env_int("TRANSLATE_BODY_MAX", 4000);
  if (body_max < 200) body_max = 200;
  int max_tokens = env_int("TRANSLATE_MAX_TOKENS", 3072);
  int timeout = env_int("TRANSLATE_TIMEOUT_MS", 120000);

  /* Read the batch and finalize BEFORE any generation — a read cursor held
   * across a two-minute LLM call is a two-minute read lock. */
  trow *rows = calloc((size_t)limit, sizeof *rows);
  if (!rows) return 0;
  int nr = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT uid,title,body,summary,language FROM intel_items"
      " WHERE " TR_PENDING_WHERE
      "   AND (title IS NOT NULL OR body IS NOT NULL OR summary IS NOT NULL)"
      " ORDER BY fetched_at DESC LIMIT ?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int(s, 1, limit);
    while (nr < limit && sqlite3_step(s) == SQLITE_ROW) {
      rows[nr].uid     = dupcol(s, 0);
      rows[nr].title   = dupcol(s, 1);
      rows[nr].body    = dupcol(s, 2);
      rows[nr].summary = dupcol(s, 3);
      rows[nr].lang    = dupcol(s, 4);
      if (rows[nr].uid) nr++;
    }
    sqlite3_finalize(s);
  }

  /* Optional response_format json_schema (grammars/translation.schema.json).
   * schema_load() returns "" when the file is absent; llm_chat treats NULL as
   * "unconstrained", and extract_json() copes either way — so a missing schema
   * file degrades quality slightly rather than breaking the module. */
  const char *schema = schema_load("translation");
  if (schema && !*schema) schema = NULL;
  int done = 0, skipped = 0, failed = 0;

  for (int r = 0; r < nr; r++) {
    if (cancel && *cancel) break;
    trow *tr = &rows[r];

    if (!translate_is_japanese(tr->lang, tr->title, tr->summary, tr->body)) {
      mark_skip(h, tr->uid);          /* terminal: never re-examined */
      skipped++;
      continue;
    }

    char *bclip = clip_utf8(tr->body, (size_t)body_max);
    char *msgs = build_messages(tr->title, tr->summary, bclip);
    free(bclip);
    if (!msgs) { mark_fail(h, tr->uid); failed++; continue; }

    char *raw = llm_chat(llm, msgs, schema, max_tokens, 0.1, timeout);
    free(msgs);
    cJSON *out = extract_json(raw);
    free(raw);

    cJSON *jt = out ? cJSON_GetObjectItem(out, "title") : NULL;
    cJSON *js = out ? cJSON_GetObjectItem(out, "summary") : NULL;
    cJSON *jb = out ? cJSON_GetObjectItem(out, "body") : NULL;
    const char *et = (jt && cJSON_IsString(jt)) ? jt->valuestring : NULL;
    const char *es = (js && cJSON_IsString(js)) ? js->valuestring : NULL;
    const char *eb = (jb && cJSON_IsString(jb)) ? jb->valuestring : NULL;

    /* A response with nothing usable in it is a failure, not a translation —
     * writing translated_at here would permanently hide the row. */
    if ((!et || !*et) && (!es || !*es) && (!eb || !*eb)) {
      if (out) cJSON_Delete(out);
      mark_fail(h, tr->uid);
      failed++;
      continue;
    }

    char *kw = join_en(et, es, eb);

    /* Short transaction, one row: the columns and the FTS row move together
     * or not at all. */
    sqlite3_exec(h, "BEGIN", NULL, NULL, NULL);
    int ok = 0;
    if (sqlite3_prepare_v2(h,
        "UPDATE intel_items SET title_en=?1, summary_en=?2, body_en=?3,"
        " translated_at=datetime('now'), translate_failed=0"
        " WHERE uid=?4", -1, &s, NULL) == SQLITE_OK) {
      bind_txt_or_null(s, 1, et);
      bind_txt_or_null(s, 2, es);
      bind_txt_or_null(s, 3, eb);
      sqlite3_bind_text(s, 4, tr->uid, -1, SQLITE_TRANSIENT);
      ok = sqlite3_step(s) == SQLITE_DONE;
      sqlite3_finalize(s);
    }
    if (ok && kw) fts_put_en(h, tr->uid, kw);
    if (ok) sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
    else    sqlite3_exec(h, "ROLLBACK", NULL, NULL, NULL);

    free(kw);
    cJSON_Delete(out);
    if (ok) done++; else { mark_fail(h, tr->uid); failed++; }
  }

  for (int r = 0; r < nr; r++) {
    free(rows[r].uid); free(rows[r].title); free(rows[r].body);
    free(rows[r].summary); free(rows[r].lang);
  }
  free(rows);
  if (nr)
    fprintf(stderr, "[translate] batch: attempted=%d translated=%d "
                    "not_japanese=%d failed=%d\n", nr, done, skipped, failed);
  return done;
}

/* ── backfill (opt-in, single-flighted, progress-reported) ──────────────── */

static pthread_mutex_t g_bf_lock = PTHREAD_MUTEX_INITIALIZER;
static int  g_bf_running = 0;
static volatile int g_bf_cancel = 0;
static char g_bf_id[40] = {0};

static long pending_count(sqlite3 *h) {
  long n = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
      "SELECT COUNT(*) FROM intel_items WHERE " TR_PENDING_WHERE,
      -1, &s, NULL) == SQLITE_OK) {
    if (sqlite3_step(s) == SQLITE_ROW) n = (long)sqlite3_column_int64(s, 0);
    sqlite3_finalize(s);
  }
  return n;
}

typedef struct { db_handle *db; char rid[40]; int max_items; } bf_arg;

/* Own http_client + llm_client, like searchapi.c's run_thread — a background
 * sweep must not share a connection pool with anything user-facing. The
 * db_handle IS shared: unlike breach_jobs.c (long bulk transactions, bulk
 * pragmas) every write here is a single-row transaction taken and released
 * between LLM calls that dominate wall clock, so the event loop never waits
 * meaningfully. SQLITE_THREADSAFE=1 makes the shared handle legal. */
static void *bf_thread(void *vp) {
  bf_arg *a = vp;
  osint_request *rp = progress_get(a->rid);
  http_client *http = http_client_new();
  llm_client llm;
  llm_init(&llm, http);              /* background lane (interactive = 0) */

  long total = pending_count(a->db->h);
  if (a->max_items > 0 && total > a->max_items) total = a->max_items;
  if (total < 1) total = 1;

  progress_set_phase(rp, "agents_working", 5);
  progress_assign_services(rp, (const char *[]){"translate"}, 1);

  int batch = env_int("TRANSLATE_BATCH", 25);
  if (batch <= 0) batch = 25;
  long done = 0, last_left = -1;
  const char *final_phase = "completed";

  while (!g_bf_cancel) {
    if (a->max_items > 0 && done >= a->max_items) break;
    int want = batch;
    if (a->max_items > 0 && a->max_items - done < want)
      want = (int)(a->max_items - done);
    if (!llm_healthy(&llm)) {
      progress_set_thinking(rp, "llama-server unavailable; stopping backfill");
      final_phase = "error";
      break;
    }
    int n = translate_run(a->db, &llm, want, &g_bf_cancel);
    /* translate_run returns only SUCCESSES; a batch that was entirely
     * non-Japanese or entirely failing also returns 0. Re-checking the
     * pending count distinguishes "drained" from "stuck" so the loop cannot
     * spin on a batch it can never translate. */
    long left = pending_count(a->db->h);
    if (n <= 0 && left <= 0) break;
    if (n <= 0 && left > 0) {
      /* No progress but work remains → every row in that window was skipped
       * or terminally failed and is now out of the pending index; keep going.
       * If nothing at all changed we would loop forever, so bail when the
       * window produced neither a success nor a shrink in the backlog. */
      if (last_left == left) break;
      last_left = left;
    }
    done += n;
    char msg[160];
    snprintf(msg, sizeof msg, "translated %ld, %ld pending", done, left);
    progress_set_thinking(rp, msg);
    progress_service_status(rp, "translate", "running", msg, (int)done, NULL);
    int pct = (int)(5 + (done * 90) / (total > 0 ? total : 1));
    if (pct > 95) pct = 95;
    progress_set_phase(rp, "agents_working", pct);
  }

  if (g_bf_cancel) final_phase = "error";
  progress_service_status(rp, "translate",
                          strcmp(final_phase, "completed") == 0 ? "completed"
                                                                : "error",
                          NULL, (int)done, NULL);
  progress_finish(rp, final_phase);

  http_client_free(http);
  pthread_mutex_lock(&g_bf_lock);
  g_bf_running = 0;
  pthread_mutex_unlock(&g_bf_lock);
  free(a);
  return NULL;
}

char *translate_backfill_start(db_handle *db, int max_items, int *http_status) {
  if (!db || !db->h) return err_env(http_status, 500, "no_db");
  translate_migrate(db);

  pthread_mutex_lock(&g_bf_lock);
  if (g_bf_running) {
    pthread_mutex_unlock(&g_bf_lock);
    return err_env(http_status, 409, "backfill_already_running");
  }

  http_client *probe = http_client_new();
  llm_client llm;
  llm_init(&llm, probe);
  int healthy = llm_healthy(&llm);
  http_client_free(probe);
  if (!healthy) {
    pthread_mutex_unlock(&g_bf_lock);
    return err_env(http_status, 503, "llm_unavailable");
  }

  char rid[40];
  uuid4(rid);
  if (!progress_create(rid, "translate:backfill", 1)) {
    pthread_mutex_unlock(&g_bf_lock);
    return err_env(http_status, 500, "progress_create_failed");
  }

  bf_arg *a = calloc(1, sizeof *a);
  if (!a) {
    pthread_mutex_unlock(&g_bf_lock);
    return err_env(http_status, 500, "oom");
  }
  a->db = db;
  a->max_items = max_items;
  snprintf(a->rid, sizeof a->rid, "%s", rid);

  g_bf_cancel = 0;
  g_bf_running = 1;
  snprintf(g_bf_id, sizeof g_bf_id, "%s", rid);

  pthread_t t;
  if (pthread_create(&t, NULL, bf_thread, a) != 0) {
    g_bf_running = 0;
    pthread_mutex_unlock(&g_bf_lock);
    free(a);
    return err_env(http_status, 500, "thread_spawn_failed");
  }
  pthread_detach(t);
  long pend = pending_count(db->h);
  pthread_mutex_unlock(&g_bf_lock);

  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "request_id", rid);
  cJSON_AddStringToObject(d, "status", "processing");
  cJSON_AddNumberToObject(d, "pending", (double)pend);
  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", d);
  char *j = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  if (http_status) *http_status = 202;
  return j;
}

char *translate_backfill_status(db_handle *db) {
  pthread_mutex_lock(&g_bf_lock);
  int running = g_bf_running;
  char rid[40];
  snprintf(rid, sizeof rid, "%s", g_bf_id);
  pthread_mutex_unlock(&g_bf_lock);

  cJSON *d = cJSON_CreateObject();
  cJSON_AddBoolToObject(d, "running", running);
  if (rid[0]) cJSON_AddStringToObject(d, "request_id", rid);
  else cJSON_AddNullToObject(d, "request_id");
  if (db && db->h) {
    translate_migrate(db);
    cJSON_AddNumberToObject(d, "pending", (double)pending_count(db->h));
  }
  osint_request *rp = rid[0] ? progress_get(rid) : NULL;
  char *snap = rp ? progress_to_json(rp) : NULL;
  cJSON *pj = snap ? cJSON_Parse(snap) : NULL;
  free(snap);
  if (pj) cJSON_AddItemToObject(d, "progress", pj);
  else cJSON_AddNullToObject(d, "progress");

  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", d);
  char *j = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  return j;
}

char *translate_backfill_cancel(int *http_status) {
  pthread_mutex_lock(&g_bf_lock);
  int running = g_bf_running;
  if (running) g_bf_cancel = 1;
  pthread_mutex_unlock(&g_bf_lock);
  if (!running) return err_env(http_status, 409, "no_backfill_running");
  cJSON *d = cJSON_CreateObject();
  cJSON_AddBoolToObject(d, "cancelled", 1);
  cJSON *env = cJSON_CreateObject();
  cJSON_AddItemToObject(env, "data", d);
  char *j = cJSON_PrintUnformatted(env);
  cJSON_Delete(env);
  if (http_status) *http_status = 200;
  return j;
}

/* ── ?lang_view shaping ─────────────────────────────────────────────────── */

translate_view translate_view_parse(const char *lang_view) {
  if (!lang_view) return TRANSLATE_VIEW_JA;
  if (!strcmp(lang_view, "en"))   return TRANSLATE_VIEW_EN;
  if (!strcmp(lang_view, "both")) return TRANSLATE_VIEW_BOTH;
  return TRANSLATE_VIEW_JA;
}

/* Attach the translation to one item object. `s` is a prepared, reusable
 * statement over (title_en, summary_en, body_en, translated_at) by uid. */
static void attach_one(sqlite3_stmt *s, cJSON *item) {
  cJSON *uid = cJSON_GetObjectItem(item, "uid");
  if (!uid || !cJSON_IsString(uid) || !uid->valuestring) {
    cJSON_AddNullToObject(item, "translation");
    return;
  }
  sqlite3_reset(s);
  sqlite3_clear_bindings(s);
  sqlite3_bind_text(s, 1, uid->valuestring, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) != SQLITE_ROW) {
    cJSON_AddNullToObject(item, "translation");
    return;
  }
  const char *t = ctext(s, 0), *sm = ctext(s, 1), *b = ctext(s, 2),
             *at = ctext(s, 3);
  if (!at) { cJSON_AddNullToObject(item, "translation"); return; }

  cJSON *tr = cJSON_CreateObject();
  cJSON_AddStringToObject(tr, "lang", "en");
  cJSON_AddBoolToObject(tr, "machine", 1);   /* never present this as human */
  cJSON_AddStringToObject(tr, "engine", engine_id());
  cJSON_AddStringToObject(tr, "translated_at", at);
  cJSON_AddItemToObject(tr, "title",
                        t ? cJSON_CreateString(t) : cJSON_CreateNull());
  cJSON_AddItemToObject(tr, "summary",
                        sm ? cJSON_CreateString(sm) : cJSON_CreateNull());
  /* body_en only where the item itself carries a body (the full single-item
   * view) — list responses must not grow by a full document per row. */
  if (cJSON_GetObjectItem(item, "body"))
    cJSON_AddItemToObject(tr, "body",
                          b ? cJSON_CreateString(b) : cJSON_CreateNull());
  cJSON_AddItemToObject(item, "translation", tr);
}

char *translate_shape_items(db_handle *db, const char *envelope_json,
                            translate_view view) {
  if (view == TRANSLATE_VIEW_JA) return NULL;   /* default: ship as-is */
  if (!db || !db->h || !envelope_json) return NULL;
  translate_migrate(db);

  cJSON *root = cJSON_Parse(envelope_json);
  if (!root) return NULL;
  cJSON *data = cJSON_GetObjectItem(root, "data");
  if (!data) { cJSON_Delete(root); return NULL; }

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
      "SELECT title_en,summary_en,body_en,translated_at FROM intel_items"
      " WHERE uid=?1", -1, &s, NULL) != SQLITE_OK) {
    cJSON_Delete(root);
    return NULL;
  }

  if (cJSON_IsArray(data)) {
    cJSON *it;
    cJSON_ArrayForEach(it, data)
      if (cJSON_IsObject(it)) attach_one(s, it);
  } else if (cJSON_IsObject(data)) {
    attach_one(s, data);
  }
  sqlite3_finalize(s);

  cJSON *meta = cJSON_GetObjectItem(root, "meta");
  if (!meta || !cJSON_IsObject(meta)) {
    meta = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "meta", meta);
  }
  cJSON_DeleteItemFromObject(meta, "lang_view");
  cJSON_AddStringToObject(meta, "lang_view",
                          view == TRANSLATE_VIEW_EN ? "en" : "both");

  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return out;
}

/* ── scheduled pod ──────────────────────────────────────────────────────── */

/* `_enrich` (== collectors/sources/llm_enricher.c): scheduler.c skips
 * fetch_log_write() and anomaly_detect() for underscore collectors, which
 * this job needs — it emits zero intel_items, so it would log records=0
 * forever and trip records_drop against its own baseline, and an LLM-bound
 * tick blows past any duration baseline. It also inherits the scheduler's
 * skip-if-running serialisation, so two batches never overlap.
 *
 * 600s x TRANSLATE_BATCH is a deliberate trickle: new intel is translated
 * within ~10 minutes of ingest without the pod ever becoming a de-facto
 * corpus backfill running at full tilt behind the operator's back. Draining
 * an existing corpus is what translate_backfill_start() is for, and that is
 * opt-in. */
static int pod_run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;                          /* writes go to intel_items/_fts */
  if (!ctx || !ctx->db) return -1;

  /* Cheap, LLM-free, and correct even when llama-server is down: re-push
   * English for items a collector re-ingested (which wipes the FTS row). */
  translate_fts_resync(ctx->db, 0, ctx->cancel);

  const char *te = getenv("TRANSLATE_ENABLED");
  if (te && !strcmp(te, "false")) return 0;
  const char *le = getenv("LLM_ENABLED");
  if (!le || strcmp(le, "true") != 0) {
    fprintf(stderr, "[translate] skipped (LLM_ENABLED != true)\n");
    return 0;
  }
  if (ctx->cancel && *ctx->cancel) return 0;
  translate_run(ctx->db, ctx->llm, 0, ctx->cancel);
  return 0;
}

static const source_def translate_def = {
  .id = "translate", .collector = "_enrich",
  .name = "Machine Translation (JA to EN)", .name_ja = "機械翻訳",
  .update_interval_sec = 600, .run = pod_run };
REGISTER_SOURCE(translate_def)
