/* core/breach_meta.c — see breach_meta.h. */
#include "breach_meta.h"
#include "../third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef JO_REPO_ROOT
#define JO_REPO_ROOT "/Users/rayan/JapanOSINT"
#endif

static const char *corpus_path(void) {
  const char *e = getenv("JO_BREACH_CORPUS");
  return (e && *e) ? e : JO_REPO_ROOT "/docs/breach-corpus.json";
}

const char *breach_meta_seed_path(void) {
  const char *e = getenv("JO_BREACH_SEED");
  return (e && *e) ? e : JO_REPO_ROOT "/docs/breach-corpus.seed.tsv";
}

/* ── slug(): a byte-exact port of scripts/fetch-breach-corpus.mjs ──────────
 *
 *   name.toLowerCase().normalize('NFKD').replace(/[^\w]+/g, '-').replace(/^-|-$/g, '')
 *
 * `\w` in a non-unicode JS regex is [A-Za-z0-9_], so the NFKD pass is what makes
 * "Café" fold to "cafe" (é decomposes to 'e' + a combining mark, and the mark —
 * being a non-word char — becomes the separator) while "Ø", "ß" and "æ" have no
 * decomposition and vanish entirely. Anything outside Latin (Cyrillic, Hangul,
 * CJK) is likewise non-word and drops out, so a few catalog names legitimately
 * slug to "" and cannot be stored.
 *
 * The table below is GENERATED, not hand-written. For every codepoint in
 * U+00A0..U+024F it records the NFKD expansion with each run of non-word chars
 * replaced by a single '-' MARKER. The marker matters: "é" decomposes to 'e'
 * plus a combining acute, and that mark is itself a non-word char, which is why
 * the script turns "Yonéma" into "yone-ma" and "¼" into "1-4" rather than
 * "yonema"/"14". Dropping it silently produced wrong slugs for 39 catalog rows.
 * Codepoints with no word char at all (Æ, Ø, ß, ð, þ, and everything non-Latin)
 * are omitted and act as pure separators. Regenerate with, in node:
 *   for (let cp = 0xA0; cp <= 0x24F; cp++) { const c = String.fromCodePoint(cp);
 *     const k = c.toLowerCase().normalize('NFKD').replace(/[^\w]+/g, '-');
 *     if (/[a-z0-9_]/.test(k)) console.log(cp.toString(16), k); }
 *
 * These slugs are the source_id on every breach_items row and on
 * entity_mentions.source_id. Changing them orphans stored records. */
typedef struct { unsigned short cp; const char *ascii; } nfkd_ent;

static const nfkd_ent NFKD_ASCII[] = {
  {0x00AA,"a"}, {0x00B2,"2"}, {0x00B3,"3"}, {0x00B9,"1"}, {0x00BA,"o"},
  {0x00BC,"1-4"}, {0x00BD,"1-2"}, {0x00BE,"3-4"}, {0x00C0,"a-"},
  {0x00C1,"a-"}, {0x00C2,"a-"}, {0x00C3,"a-"}, {0x00C4,"a-"},
  {0x00C5,"a-"}, {0x00C7,"c-"}, {0x00C8,"e-"}, {0x00C9,"e-"},
  {0x00CA,"e-"}, {0x00CB,"e-"}, {0x00CC,"i-"}, {0x00CD,"i-"},
  {0x00CE,"i-"}, {0x00CF,"i-"}, {0x00D1,"n-"}, {0x00D2,"o-"},
  {0x00D3,"o-"}, {0x00D4,"o-"}, {0x00D5,"o-"}, {0x00D6,"o-"},
  {0x00D9,"u-"}, {0x00DA,"u-"}, {0x00DB,"u-"}, {0x00DC,"u-"},
  {0x00DD,"y-"}, {0x00E0,"a-"}, {0x00E1,"a-"}, {0x00E2,"a-"},
  {0x00E3,"a-"}, {0x00E4,"a-"}, {0x00E5,"a-"}, {0x00E7,"c-"},
  {0x00E8,"e-"}, {0x00E9,"e-"}, {0x00EA,"e-"}, {0x00EB,"e-"},
  {0x00EC,"i-"}, {0x00ED,"i-"}, {0x00EE,"i-"}, {0x00EF,"i-"},
  {0x00F1,"n-"}, {0x00F2,"o-"}, {0x00F3,"o-"}, {0x00F4,"o-"},
  {0x00F5,"o-"}, {0x00F6,"o-"}, {0x00F9,"u-"}, {0x00FA,"u-"},
  {0x00FB,"u-"}, {0x00FC,"u-"}, {0x00FD,"y-"}, {0x00FF,"y-"},
  {0x0100,"a-"}, {0x0101,"a-"}, {0x0102,"a-"}, {0x0103,"a-"},
  {0x0104,"a-"}, {0x0105,"a-"}, {0x0106,"c-"}, {0x0107,"c-"},
  {0x0108,"c-"}, {0x0109,"c-"}, {0x010A,"c-"}, {0x010B,"c-"},
  {0x010C,"c-"}, {0x010D,"c-"}, {0x010E,"d-"}, {0x010F,"d-"},
  {0x0112,"e-"}, {0x0113,"e-"}, {0x0114,"e-"}, {0x0115,"e-"},
  {0x0116,"e-"}, {0x0117,"e-"}, {0x0118,"e-"}, {0x0119,"e-"},
  {0x011A,"e-"}, {0x011B,"e-"}, {0x011C,"g-"}, {0x011D,"g-"},
  {0x011E,"g-"}, {0x011F,"g-"}, {0x0120,"g-"}, {0x0121,"g-"},
  {0x0122,"g-"}, {0x0123,"g-"}, {0x0124,"h-"}, {0x0125,"h-"},
  {0x0128,"i-"}, {0x0129,"i-"}, {0x012A,"i-"}, {0x012B,"i-"},
  {0x012C,"i-"}, {0x012D,"i-"}, {0x012E,"i-"}, {0x012F,"i-"},
  {0x0130,"i-"}, {0x0132,"ij"}, {0x0133,"ij"}, {0x0134,"j-"},
  {0x0135,"j-"}, {0x0136,"k-"}, {0x0137,"k-"}, {0x0139,"l-"},
  {0x013A,"l-"}, {0x013B,"l-"}, {0x013C,"l-"}, {0x013D,"l-"},
  {0x013E,"l-"}, {0x013F,"l-"}, {0x0140,"l-"}, {0x0143,"n-"},
  {0x0144,"n-"}, {0x0145,"n-"}, {0x0146,"n-"}, {0x0147,"n-"},
  {0x0148,"n-"}, {0x0149,"-n"}, {0x014C,"o-"}, {0x014D,"o-"},
  {0x014E,"o-"}, {0x014F,"o-"}, {0x0150,"o-"}, {0x0151,"o-"},
  {0x0154,"r-"}, {0x0155,"r-"}, {0x0156,"r-"}, {0x0157,"r-"},
  {0x0158,"r-"}, {0x0159,"r-"}, {0x015A,"s-"}, {0x015B,"s-"},
  {0x015C,"s-"}, {0x015D,"s-"}, {0x015E,"s-"}, {0x015F,"s-"},
  {0x0160,"s-"}, {0x0161,"s-"}, {0x0162,"t-"}, {0x0163,"t-"},
  {0x0164,"t-"}, {0x0165,"t-"}, {0x0168,"u-"}, {0x0169,"u-"},
  {0x016A,"u-"}, {0x016B,"u-"}, {0x016C,"u-"}, {0x016D,"u-"},
  {0x016E,"u-"}, {0x016F,"u-"}, {0x0170,"u-"}, {0x0171,"u-"},
  {0x0172,"u-"}, {0x0173,"u-"}, {0x0174,"w-"}, {0x0175,"w-"},
  {0x0176,"y-"}, {0x0177,"y-"}, {0x0178,"y-"}, {0x0179,"z-"},
  {0x017A,"z-"}, {0x017B,"z-"}, {0x017C,"z-"}, {0x017D,"z-"},
  {0x017E,"z-"}, {0x017F,"s"}, {0x01A0,"o-"}, {0x01A1,"o-"}, {0x01AF,"u-"},
  {0x01B0,"u-"}, {0x01C4,"dz-"}, {0x01C5,"dz-"}, {0x01C6,"dz-"},
  {0x01C7,"lj"}, {0x01C8,"lj"}, {0x01C9,"lj"}, {0x01CA,"nj"},
  {0x01CB,"nj"}, {0x01CC,"nj"}, {0x01CD,"a-"}, {0x01CE,"a-"},
  {0x01CF,"i-"}, {0x01D0,"i-"}, {0x01D1,"o-"}, {0x01D2,"o-"},
  {0x01D3,"u-"}, {0x01D4,"u-"}, {0x01D5,"u-"}, {0x01D6,"u-"},
  {0x01D7,"u-"}, {0x01D8,"u-"}, {0x01D9,"u-"}, {0x01DA,"u-"},
  {0x01DB,"u-"}, {0x01DC,"u-"}, {0x01DE,"a-"}, {0x01DF,"a-"},
  {0x01E0,"a-"}, {0x01E1,"a-"}, {0x01E6,"g-"}, {0x01E7,"g-"},
  {0x01E8,"k-"}, {0x01E9,"k-"}, {0x01EA,"o-"}, {0x01EB,"o-"},
  {0x01EC,"o-"}, {0x01ED,"o-"}, {0x01F0,"j-"}, {0x01F1,"dz"},
  {0x01F2,"dz"}, {0x01F3,"dz"}, {0x01F4,"g-"}, {0x01F5,"g-"},
  {0x01F8,"n-"}, {0x01F9,"n-"}, {0x01FA,"a-"}, {0x01FB,"a-"},
  {0x0200,"a-"}, {0x0201,"a-"}, {0x0202,"a-"}, {0x0203,"a-"},
  {0x0204,"e-"}, {0x0205,"e-"}, {0x0206,"e-"}, {0x0207,"e-"},
  {0x0208,"i-"}, {0x0209,"i-"}, {0x020A,"i-"}, {0x020B,"i-"},
  {0x020C,"o-"}, {0x020D,"o-"}, {0x020E,"o-"}, {0x020F,"o-"},
  {0x0210,"r-"}, {0x0211,"r-"}, {0x0212,"r-"}, {0x0213,"r-"},
  {0x0214,"u-"}, {0x0215,"u-"}, {0x0216,"u-"}, {0x0217,"u-"},
  {0x0218,"s-"}, {0x0219,"s-"}, {0x021A,"t-"}, {0x021B,"t-"},
  {0x021E,"h-"}, {0x021F,"h-"}, {0x0226,"a-"}, {0x0227,"a-"},
  {0x0228,"e-"}, {0x0229,"e-"}, {0x022A,"o-"}, {0x022B,"o-"},
  {0x022C,"o-"}, {0x022D,"o-"}, {0x022E,"o-"}, {0x022F,"o-"},
  {0x0230,"o-"}, {0x0231,"o-"}, {0x0232,"y-"}, {0x0233,"y-"},
};

static const char *nfkd_ascii(unsigned cp) {
  if (cp < 0x00A0 || cp > 0x024F) return NULL;
  int lo = 0, hi = (int)(sizeof NFKD_ASCII / sizeof NFKD_ASCII[0]) - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    unsigned c = NFKD_ASCII[mid].cp;
    if (c == cp) return NFKD_ASCII[mid].ascii;
    if (c < cp) lo = mid + 1; else hi = mid - 1;
  }
  return NULL;
}

/* Decode one UTF-8 codepoint. Malformed bytes advance by 1 and yield U+FFFD,
 * which has no ASCII base and so acts as a separator — same as JS, where an
 * unpaired surrogate is simply a non-word char. */
static unsigned utf8_next(const unsigned char *s, int *adv) {
  unsigned c = s[0];
  *adv = 1;
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
    *adv = 2; return ((c & 0x1Fu) << 6) | (s[1] & 0x3Fu);
  }
  if ((c & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
    *adv = 3; return ((c & 0x0Fu) << 12) | ((s[1] & 0x3Fu) << 6) | (s[2] & 0x3Fu);
  }
  if ((c & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 &&
      (s[3] & 0xC0) == 0x80) {
    *adv = 4;
    return ((c & 0x07u) << 18) | ((s[1] & 0x3Fu) << 12) |
           ((s[2] & 0x3Fu) << 6) | (s[3] & 0x3Fu);
  }
  return 0xFFFD;
}

/* Runs of non-word chars collapse to a single '-', and a separator is only
 * emitted lazily before the next kept char — which reproduces both the `+` in
 * the JS regex and its leading/trailing trim without a second pass. */
static void breach_slug(const char *name, char *out, size_t cap) {
  size_t n = 0;
  int pending = 0;
  const unsigned char *p = (const unsigned char *)name;
  if (cap == 0) return;
  while (*p) {
    int adv = 1;
    unsigned cp = utf8_next(p, &adv);
    p += adv;

    char one[2];
    const char *rep = NULL;
    if (cp < 0x80) {
      int ch = tolower((int)cp);
      if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_') {
        one[0] = (char)ch; one[1] = 0; rep = one;
      }
    } else {
      rep = nfkd_ascii(cp);
    }

    if (!rep) { if (n) pending = 1; continue; }
    /* '-' inside a table value is a separator MARKER, not a literal: it stands
     * for the combining mark (or fraction slash) the decomposition left behind. */
    for (const char *q = rep; *q; q++) {
      if (*q == '-') { if (n) pending = 1; continue; }
      if (pending && n + 1 < cap) out[n++] = '-';
      pending = 0;
      if (n + 1 < cap) out[n++] = *q;
    }
  }
  out[n] = 0;
}

/* "821.1k" | "6.6M" | "2B" | "126" → integer; -1 when the field does not match
 * /^([\d.]+)\s*([kMB]?)$/ (JS parseCount returns null there). Same IEEE-754
 * ops in the same order as parseFloat(x) * mult, so the rounding agrees. */
static long long parse_count(const char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
  const char *start = s;
  while ((*s >= '0' && *s <= '9') || *s == '.') s++;
  const char *digits_end = s;
  if (digits_end == start) return -1;

  while (*s == ' ' || *s == '\t') s++;              /* \s* */
  double mult = 1;
  if      (*s == 'k') { mult = 1e3; s++; }
  else if (*s == 'M') { mult = 1e6; s++; }
  else if (*s == 'B') { mult = 1e9; s++; }
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
  if (*s) return -1;                                /* anchored $ */

  char buf[64];
  size_t len = (size_t)(digits_end - start);
  if (len >= sizeof buf) return -1;
  memcpy(buf, start, len);
  buf[len] = 0;
  double v = strtod(buf, NULL);
  if (v < 0) return -1;
  return (long long)(v * mult + 0.5);               /* == JS Math.round, v >= 0 */
}

/* ── shared breach_meta upsert ─────────────────────────────────────────────
 * Both the JSON manifest and the seed TSV land here so the column list, the
 * conflict clause and the transaction live in exactly one place. */
typedef struct {
  const char *breach_id, *name, *title, *domain, *breach_date, *added_date;
  long long pwn_count; int has_pwn;
  const char *data_classes_json;
  int verified,  has_verified;
  int sensitive, has_sensitive;
  const char *source_id;
} bm_row;

static sqlite3_stmt *bm_prepare(db_handle *db) {
  static const char *SQL =
    "INSERT INTO breach_meta(breach_id,name,title,domain,breach_date,added_date,"
    "pwn_count,data_classes_json,verified,sensitive,source_id) "
    "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11) "
    "ON CONFLICT(breach_id) DO UPDATE SET name=excluded.name,title=excluded.title,"
    "domain=excluded.domain,breach_date=excluded.breach_date,"
    "added_date=excluded.added_date,pwn_count=excluded.pwn_count,"
    "data_classes_json=excluded.data_classes_json,verified=excluded.verified,"
    "sensitive=excluded.sensitive,source_id=excluded.source_id";
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h, SQL, -1, &st, NULL) != SQLITE_OK) {
    fprintf(stderr, "[breach_meta] prepare: %s\n", sqlite3_errmsg(db->h));
    return NULL;
  }
  return st;
}

/* Bind text, or SQL NULL when the pointer is NULL. */
static void bind_opt(sqlite3_stmt *st, int col, const char *v) {
  if (v) sqlite3_bind_text(st, col, v, -1, SQLITE_TRANSIENT);
  else   sqlite3_bind_null(st, col);
}

/* 1 on a written row, 0 otherwise. Resets the statement either way. */
static int bm_write(sqlite3_stmt *st, const bm_row *r) {
  bind_opt(st, 1,  r->breach_id);
  bind_opt(st, 2,  r->name);
  bind_opt(st, 3,  r->title);
  bind_opt(st, 4,  r->domain);
  bind_opt(st, 5,  r->breach_date);
  bind_opt(st, 6,  r->added_date);
  if (r->has_pwn) sqlite3_bind_int64(st, 7, r->pwn_count);
  else            sqlite3_bind_null(st, 7);
  bind_opt(st, 8,  r->data_classes_json);
  if (r->has_verified)  sqlite3_bind_int(st, 9,  r->verified  ? 1 : 0);
  else                  sqlite3_bind_null(st, 9);
  if (r->has_sensitive) sqlite3_bind_int(st, 10, r->sensitive ? 1 : 0);
  else                  sqlite3_bind_null(st, 10);
  bind_opt(st, 11, r->source_id);

  int ok = (sqlite3_step(st) == SQLITE_DONE);
  sqlite3_reset(st);
  sqlite3_clear_bindings(st);
  return ok ? 1 : 0;
}

/* 1 if the slug is already catalogued. */
static int bm_exists(db_handle *db, const char *breach_id) {
  return breach_meta_is_source(db, breach_id);
}

/* cJSON string field, or NULL when absent/non-string. */
static const char *json_str(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring) ? v->valuestring : NULL;
}

int breach_meta_load_manifest(db_handle *db, const char *path) {
  if (!db || !db->h) return -1;
  const char *p = (path && *path) ? path : corpus_path();

  FILE *f = fopen(p, "rb");
  if (!f) { fprintf(stderr, "[breach_meta] cannot open %s\n", p); return -1; }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0) { fclose(f); return -1; }
  char *buf = malloc((size_t)sz + 1);
  if (!buf) { fclose(f); return -1; }
  size_t rd = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[rd] = 0;

  cJSON *root = cJSON_Parse(buf);
  free(buf);
  if (!root) { fprintf(stderr, "[breach_meta] parse failed: %s\n", p); return -1; }
  cJSON *arr = cJSON_GetObjectItem(root, "breaches");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(root); return -1; }

  sqlite3_stmt *st = bm_prepare(db);
  if (!st) { cJSON_Delete(root); return -1; }

  sqlite3_exec(db->h, "BEGIN", NULL, NULL, NULL);
  int n = 0;
  cJSON *b = NULL;
  cJSON_ArrayForEach(b, arr) {
    const char *id = json_str(b, "id");
    if (!id || !id[0]) continue;

    bm_row r = {0};
    r.breach_id   = id;
    r.name        = json_str(b, "name");
    r.title       = json_str(b, "title");
    r.domain      = json_str(b, "domain");
    r.breach_date = json_str(b, "breach_date");
    r.added_date  = json_str(b, "added_date");
    r.source_id   = json_str(b, "source_id");

    cJSON *pc = cJSON_GetObjectItem(b, "pwn_count");
    if (cJSON_IsNumber(pc)) { r.has_pwn = 1; r.pwn_count = (long long)pc->valuedouble; }

    char *djs = NULL;
    cJSON *dc = cJSON_GetObjectItem(b, "data_classes");
    if (dc && !cJSON_IsNull(dc)) { djs = cJSON_PrintUnformatted(dc); r.data_classes_json = djs; }

    cJSON *ver = cJSON_GetObjectItem(b, "verified");
    if (cJSON_IsBool(ver)) { r.has_verified = 1; r.verified = cJSON_IsTrue(ver); }
    cJSON *sen = cJSON_GetObjectItem(b, "sensitive");
    if (cJSON_IsBool(sen)) { r.has_sensitive = 1; r.sensitive = cJSON_IsTrue(sen); }

    n += bm_write(st, &r);
    free(djs);
  }
  sqlite3_exec(db->h, "COMMIT", NULL, NULL, NULL);
  sqlite3_finalize(st);
  cJSON_Delete(root);
  fprintf(stderr, "[breach_meta] loaded %d breaches from %s\n", n, p);
  return n;
}

/* ── seed TSV ──────────────────────────────────────────────────────────────
 * One pass over the file, shared by the preview and the loader. `st` NULL =>
 * parse only, nothing is written. Returns rows_parsed, or -1 if unreadable.
 *
 * Column layout (3 leading '#' comment lines, then a header row):
 *   name <TAB> records <TAB> added <TAB> breach_date
 * The script destructures only the first four fields and skips any row whose
 * name is empty or the literal "name", so this does the same. Rows whose slug
 * comes out empty are additionally unstorable (breach_id is the primary key),
 * and are counted as skipped. */
static int seed_scan(db_handle *db, const char *path, sqlite3_stmt *st,
                     int limit, breach_seed_row **sample, int *n_sample,
                     breach_seed_stats *stats, int *out_written) {
  const char *p = (path && *path) ? path : breach_meta_seed_path();

  FILE *f = fopen(p, "rb");
  if (!f) { fprintf(stderr, "[breach_meta] cannot open %s\n", p); return -1; }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0) { fclose(f); return -1; }
  char *buf = malloc((size_t)sz + 1);
  if (!buf) { fclose(f); return -1; }
  size_t rd = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[rd] = 0;

  breach_seed_row *S = NULL;
  int ns = 0;
  if (sample && limit > 0) {
    S = calloc((size_t)limit, sizeof *S);
    if (!S) { free(buf); return -1; }
  }
  if (stats) {
    memset(stats, 0, sizeof *stats);
    snprintf(stats->format, sizeof stats->format, "%s", "tsv");
    snprintf(stats->path, sizeof stats->path, "%s", p);
  }

  int parsed = 0, written = 0;
  char *line = buf;
  while (line && *line) {
    char *nl = strpbrk(line, "\r\n");
    char *next = NULL;
    if (nl) {
      next = (nl[0] == '\r' && nl[1] == '\n') ? nl + 2 : nl + 1;
      *nl = 0;
    }
    char *cur = line;
    line = next;

    if (!*cur || *cur == '#') continue;
    if (stats) stats->rows_read++;

    /* split on tabs, in place; a 5th+ column stays glued to fld[3], so trim it
     * — the script's destructuring keeps only the 4th field. */
    char *fld[4] = { cur, NULL, NULL, NULL };
    int nf = 1;
    for (char *q = cur; *q && nf < 4; q++)
      if (*q == '\t') { *q = 0; fld[nf++] = q + 1; }
    if (nf == 4) { char *t = strchr(fld[3], '\t'); if (t) *t = 0; }

    const char *name = fld[0];
    if (!name[0] || strcmp(name, "name") == 0) {
      if (stats) stats->rows_skipped++;
      continue;
    }

    char slug[128];
    breach_slug(name, slug, sizeof slug);
    if (!slug[0]) {            /* e.g. an all-Cyrillic or Hangul name */
      if (stats) stats->rows_skipped++;
      continue;
    }

    parsed++;
    long long pwn      = fld[1] ? parse_count(fld[1]) : -1;
    const char *added  = fld[2] ? fld[2] : "";
    const char *bdate  = fld[3] ? fld[3] : "";

    if (stats) {
      if (db && bm_exists(db, slug)) stats->rows_existing++;
      else                           stats->rows_new++;
    }
    if (S && ns < limit) {
      breach_seed_row *r = &S[ns++];
      snprintf(r->breach_id,   sizeof r->breach_id,   "%s", slug);
      snprintf(r->name,        sizeof r->name,        "%s", name);
      snprintf(r->added_date,  sizeof r->added_date,  "%s", added);
      snprintf(r->breach_date, sizeof r->breach_date, "%s", bdate);
      r->pwn_count = pwn;
    }
    if (st) {
      bm_row w = {0};
      w.breach_id   = slug;
      w.name        = name;
      w.title       = name;               /* the script sets title := name */
      w.breach_date = bdate[0] ? bdate : NULL;
      w.added_date  = added[0] ? added : NULL;
      w.has_pwn     = pwn >= 0;
      w.pwn_count   = pwn;
      w.has_sensitive = 1; w.sensitive = 0;   /* script emits sensitive:false */
      w.source_id   = "hibp-corpus-seed";
      written += bm_write(st, &w);
    }
  }

  if (stats) stats->rows_parsed = parsed;
  if (out_written) *out_written = written;
  free(buf);
  if (sample) { *sample = S; if (n_sample) *n_sample = ns; }
  else free(S);
  return parsed;
}

int breach_meta_parse_seed(db_handle *db, const char *path, int limit,
                           breach_seed_row **sample, int *n_sample,
                           breach_seed_stats *stats) {
  if (sample)   *sample = NULL;
  if (n_sample) *n_sample = 0;
  if (limit < 0) limit = 0;
  return seed_scan(db, path, NULL, limit, sample, n_sample, stats, NULL);
}

int breach_meta_load_seed_tsv(db_handle *db, const char *path,
                              breach_seed_stats *stats) {
  if (!db || !db->h) return -1;
  sqlite3_stmt *st = bm_prepare(db);
  if (!st) return -1;

  sqlite3_exec(db->h, "BEGIN", NULL, NULL, NULL);
  int written = 0;
  int parsed = seed_scan(db, path, st, 0, NULL, NULL, stats, &written);
  sqlite3_exec(db->h, "COMMIT", NULL, NULL, NULL);
  sqlite3_finalize(st);

  if (parsed < 0) return -1;
  fprintf(stderr, "[breach_meta] loaded %d breaches from %s\n", written,
          (path && *path) ? path : breach_meta_seed_path());
  return written;
}

int breach_meta_is_source(db_handle *db, const char *id) {
  if (!db || !db->h || !id || !*id) return 0;
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT 1 FROM breach_meta WHERE breach_id=?1 LIMIT 1", -1, &st, NULL)
      != SQLITE_OK) return 0;
  sqlite3_bind_text(st, 1, id, -1, SQLITE_TRANSIENT);
  int found = (sqlite3_step(st) == SQLITE_ROW);
  sqlite3_finalize(st);
  return found;
}

void breach_meta_display(const breach_src_row *br, long long *count,
                         const char **fresh, char *desc, size_t desc_cap) {
  if (!br) return;
  if (count) *count = br->item_count > 0 ? br->item_count : br->pwn_count;
  if (fresh) *fresh = br->last_seen[0]  ? br->last_seen
                    : br->added_date[0] ? br->added_date : NULL;
  if (desc && desc_cap)
    snprintf(desc, desc_cap, "%lld accounts%s%s", br->pwn_count,
             br->breach_date[0] ? " \xc2\xb7 breached " : "",
             br->breach_date[0] ? br->breach_date : "");
}

breach_src_row *breach_meta_sources(db_handle *db, int *n) {
  if (n) *n = 0;
  if (!db || !db->h) return NULL;
  static const char *Q =
    "SELECT m.breach_id, COALESCE(m.name,m.title,m.breach_id), COALESCE(m.domain,''), "
    "COALESCE(m.breach_date,''), COALESCE(m.added_date,''), COALESCE(m.pwn_count,0), "
    "COALESCE(bi.c,0), COALESCE(bi.mx,'') "
    "FROM breach_meta m "
    "LEFT JOIN (SELECT source_id, COUNT(*) c, MAX(first_seen) mx "
    "           FROM breach_items GROUP BY source_id) bi "
    "ON bi.source_id = m.breach_id";
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db->h, Q, -1, &st, NULL) != SQLITE_OK) return NULL;

  int cap = 64, k = 0;
  breach_src_row *R = malloc((size_t)cap * sizeof *R);
  if (!R) { sqlite3_finalize(st); return NULL; }
  while (sqlite3_step(st) == SQLITE_ROW) {
    if (k == cap) { cap *= 2; R = realloc(R, (size_t)cap * sizeof *R); }
    breach_src_row *r = &R[k++];
    snprintf(r->breach_id,   sizeof r->breach_id,   "%s", (const char *)sqlite3_column_text(st, 0));
    snprintf(r->name,        sizeof r->name,        "%s", (const char *)sqlite3_column_text(st, 1));
    snprintf(r->domain,      sizeof r->domain,      "%s", (const char *)sqlite3_column_text(st, 2));
    snprintf(r->breach_date, sizeof r->breach_date, "%s", (const char *)sqlite3_column_text(st, 3));
    snprintf(r->added_date,  sizeof r->added_date,  "%s", (const char *)sqlite3_column_text(st, 4));
    r->pwn_count  = sqlite3_column_int64(st, 5);
    r->item_count = sqlite3_column_int64(st, 6);
    snprintf(r->last_seen,   sizeof r->last_seen,   "%s", (const char *)sqlite3_column_text(st, 7));
  }
  sqlite3_finalize(st);
  if (n) *n = k;
  if (k == 0) { free(R); return NULL; }
  return R;
}
