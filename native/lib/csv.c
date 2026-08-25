/* lib/csv.c — port of _liveHelpers.js parseCsv + decodeShiftJis. */
#include "csv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iconv.h>
#include <errno.h>

cJSON *csv_parse(const char *text, int headers) {
  return csv_parse_d(text, headers, ',');
}

/* Cells in one line, honouring RFC 4180 quoting. Used only to decide whether a
 * banner line is secretly the header, so it has to agree with the real parser
 * about what a delimiter is — an embedded delimiter inside quotes is not one. */
static int csv_field_count(const char *line, size_t len, char delim) {
  int n = 1, in_q = 0;
  for (size_t i = 0; i < len; i++) {
    char c = line[i];
    if (in_q) {
      if (c == '"') { if (i + 1 < len && line[i + 1] == '"') i++; else in_q = 0; }
    } else if (c == '"') in_q = 1;
    else if (c == delim) n++;
  }
  return n;
}

/* Strip a LEADING comment banner, and recover the header out of it if that is
 * where the publisher put the column names. See csv.h for why this exists.
 * Returns a malloc'd replacement, or NULL to mean "nothing to strip, use the
 * original text". */
static char *csv_strip_banner(const char *text, char delim, const char *prefix,
                              int headers) {
  size_t plen = strlen(prefix);
  const char *p = text;
  const char *last_c = NULL; size_t last_clen = 0;   /* last banner line */
  int skipped = 0;

  for (;;) {
    const char *eol = strchr(p, '\n');
    size_t linelen = eol ? (size_t)(eol - p) : strlen(p);
    size_t body = linelen;
    while (body && (p[body-1] == '\r')) body--;
    const char *t = p;
    size_t tlen = body;
    while (tlen && (*t == ' ' || *t == '\t')) { t++; tlen--; }
    if (tlen == 0) {                       /* blank line inside the banner */
      if (!eol) break;
      skipped = 1; p = eol + 1; continue;
    }
    if (tlen < plen || strncmp(t, prefix, plen) != 0) break;   /* first real line */
    last_c = t; last_clen = tlen;
    skipped = 1;
    if (!eol) { p = t + tlen; break; }
    p = eol + 1;
  }
  if (!skipped) return NULL;               /* no banner — leave the text alone */

  /* Is the last banner line the column-name row? URLhaus writes its header as
   * `# id,dateadded,url,url_status,…`, so dropping the whole banner would take
   * the column names with it and promote the first real record into their
   * place — losing that record AND naming every column after its values. The
   * test is arity: the line, with the prefix peeled off, has to split into
   * exactly as many cells as the first data row, and into more than one. */
  const char *data = p;
  const char *deol = strchr(data, '\n');
  size_t dlen = deol ? (size_t)(deol - data) : strlen(data);
  while (dlen && data[dlen-1] == '\r') dlen--;
  const char *hdr = NULL; size_t hlen = 0;
  if (headers && last_c && dlen) {
    const char *h = last_c + plen;
    size_t hl = last_clen - plen;
    while (hl && (*h == ' ' || *h == '\t')) { h++; hl--; }
    while (hl && (h[hl-1] == ' ' || h[hl-1] == '\t')) hl--;
    int want = csv_field_count(data, dlen, delim);
    if (hl && want > 1 && csv_field_count(h, hl, delim) == want) {
      hdr = h; hlen = hl;
    }
  }

  size_t rest = strlen(data);
  char *out = malloc(hlen + 1 + rest + 1);
  if (!out) return NULL;
  size_t w = 0;
  if (hdr) { memcpy(out, hdr, hlen); w = hlen; out[w++] = '\n'; }
  memcpy(out + w, data, rest);
  out[w + rest] = 0;
  return out;
}

cJSON *csv_parse_dc(const char *text, int headers, char delim,
                    const char *comment) {
  if (!text || !comment || !*comment) return csv_parse_d(text, headers, delim);
  char *stripped = csv_strip_banner(text, delim, comment, headers);
  cJSON *r = csv_parse_d(stripped ? stripped : text, headers, delim);
  free(stripped);
  return r;
}

cJSON *csv_parse_d(const char *text, int headers, char delim) {
  const int trim = (delim != ',');   /* see the note in csv.h */
  cJSON *rows = cJSON_CreateArray();
  if (!text || !*text) return rows;

  cJSON *cur = cJSON_CreateArray();
  size_t cap = 64, fl = 0;
  char *field = malloc(cap);
  int in_q = 0, have = 0;        /* have: any field/cell started on this row */
  /* An unchecked malloc here wrote through NULL on the very first cell, and
   * the realloc below did the same on any field past 64 bytes while leaking
   * the old buffer. Out of memory has to stop the parse, not corrupt it. */
  if (!field) { cJSON_Delete(cur); return rows; }
  field[0] = 0;

#define PUSH_FIELD() do { field[fl] = 0; \
    const char *v_ = field; \
    if (trim) { while (*v_ == ' ' || *v_ == '\t') v_++; \
                size_t e_ = strlen(v_); \
                while (e_ && (v_[e_-1] == ' ' || v_[e_-1] == '\t')) e_--; \
                field[(v_ - field) + e_] = 0; } \
    cJSON_AddItemToArray(cur, cJSON_CreateString(v_)); fl = 0; have = 1; \
  } while (0)
#define PUSH_CHAR(c) do { if (fl + 1 >= cap) { size_t nc_ = cap * 2; \
    char *nf_ = realloc(field, nc_); if (!nf_) break; field = nf_; cap = nc_; } \
    field[fl++] = (c); } while (0)

  for (const char *p = text; *p; p++) {
    char ch = *p;
    if (in_q) {
      if (ch == '"') {
        if (p[1] == '"') { PUSH_CHAR('"'); p++; }
        else in_q = 0;
      } else PUSH_CHAR(ch);
    } else {
      if (ch == '"') in_q = 1;
      else if (ch == delim) { PUSH_FIELD(); }
      else if (ch == '\n') {
        PUSH_FIELD();
        cJSON_AddItemToArray(rows, cur);
        cur = cJSON_CreateArray();
        have = 0;
      } else if (ch == '\r') { /* ignore CR */ }
      else PUSH_CHAR(ch);
    }
  }
  /* JS: if (field.length>0 || cur.length>0) { cur.push(field); rows.push } */
  if (fl > 0 || cJSON_GetArraySize(cur) > 0 || have) {
    PUSH_FIELD();
    cJSON_AddItemToArray(rows, cur);
  } else {
    cJSON_Delete(cur);
  }
  free(field);

  if (!headers) return rows;

  int nrows = cJSON_GetArraySize(rows);
  if (nrows == 0) { cJSON_Delete(rows); return cJSON_CreateArray(); }
  cJSON *head = cJSON_GetArrayItem(rows, 0);  /* exhaustive-ok: header row, not a record */
  int nh = cJSON_GetArraySize(head);
  char **names = calloc((size_t)nh + 1, sizeof(char *));
  if (!names) return rows;                      /* keep the positional rows */
  for (int i = 0; i < nh; i++) {
    const char *h = cJSON_GetArrayItem(head, i)->valuestring;
    while (*h == ' ' || *h == '\t') h++;        /* trim() */
    char *t = strdup(h);
    if (!t) { nh = i; break; }
    size_t L = strlen(t);
    while (L && (t[L-1]==' '||t[L-1]=='\t'||t[L-1]=='\n'||t[L-1]=='\r')) t[--L]=0;
    names[i] = t;
  }
  cJSON *out = cJSON_CreateArray();
  for (int r = 1; r < nrows; r++) {
    cJSON *row = cJSON_GetArrayItem(rows, r);
    cJSON *o = cJSON_CreateObject();
    for (int i = 0; i < nh; i++) {
      cJSON *cell = cJSON_GetArrayItem(row, i);
      cJSON_AddStringToObject(o, names[i],
                              cell && cell->valuestring ? cell->valuestring : "");
    }
    /* Ragged rows: a data row with MORE cells than the header had was cut off
     * at nh and the surplus vanished — a silent discard of real bytes we
     * already paid a request for (docs/SOURCE_EXHAUSTIVENESS.md). There is no
     * name for those columns, so they keep the positional one the headerless
     * mode already uses; nothing is invented and nothing is dropped. */
    int ncells = cJSON_GetArraySize(row);
    for (int i = nh; i < ncells; i++) {
      cJSON *cell = cJSON_GetArrayItem(row, i);
      char key[24];
      snprintf(key, sizeof key, "col%d", i);
      cJSON_AddStringToObject(o, key,
                              cell && cell->valuestring ? cell->valuestring : "");
    }
    cJSON_AddItemToArray(out, o);
  }
  for (int i = 0; i < nh; i++) free(names[i]);
  free(names);
  cJSON_Delete(rows);
  return out;
}

int csv_is_utf8(const char *s, size_t n) {
  const unsigned char *p = (const unsigned char *)s;
  for (size_t i = 0; i < n; ) {
    unsigned char c = p[i];
    size_t need;
    if (c < 0x80) { i++; continue; }
    else if ((c & 0xE0) == 0xC0) need = 1;
    else if ((c & 0xF0) == 0xE0) need = 2;
    else if ((c & 0xF8) == 0xF0) need = 3;
    else return 0;
    if (i + need >= n) return 0;          /* truncated sequence */
    for (size_t k = 1; k <= need; k++)
      if ((p[i + k] & 0xC0) != 0x80) return 0;
    i += need + 1;
  }
  return 1;
}

char *csv_decode_sjis(const char *buf, size_t len) {
  if (!buf) return NULL;
  iconv_t cd = iconv_open("UTF-8", "SHIFT_JIS");
  if (cd == (iconv_t)-1) {
    char *c = malloc(len + 1);
    if (!c) return NULL;
    memcpy(c, buf, len); c[len] = 0; return c;
  }
  size_t in_left = len, out_cap = len * 3 + 1, out_left = out_cap;
  char *out = malloc(out_cap);
  if (!out) { iconv_close(cd); return NULL; }
  char *in_p = (char *)buf, *out_p = out;
  int failed = 0;
  while (in_left > 0) {
    size_t r = iconv(cd, &in_p, &in_left, &out_p, &out_left);
    if (r == (size_t)-1) {
      if (errno == E2BIG) {
        size_t used = (size_t)(out_p - out);
        char *no = realloc(out, out_cap * 2);
        if (!no) { failed = 1; break; }
        out = no; out_cap *= 2;
        out_p = out + used; out_left = out_cap - used;
        continue;
      }
      failed = 1; break;          /* invalid/incomplete → JS catch → utf8 */
    }
  }
  iconv_close(cd);
  if (failed) {
    free(out);
    char *c = malloc(len + 1);
    if (!c) return NULL;
    memcpy(c, buf, len); c[len] = 0; return c;
  }
  *out_p = 0;
  return out;
}
