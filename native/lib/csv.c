/* lib/csv.c — port of _liveHelpers.js parseCsv + decodeShiftJis. */
#include "csv.h"
#include <stdlib.h>
#include <string.h>
#include <iconv.h>
#include <errno.h>

cJSON *csv_parse(const char *text, int headers) {
  cJSON *rows = cJSON_CreateArray();
  if (!text || !*text) return rows;

  cJSON *cur = cJSON_CreateArray();
  size_t cap = 64, fl = 0;
  char *field = malloc(cap);
  int in_q = 0, have = 0;        /* have: any field/cell started on this row */
  field[0] = 0;

#define PUSH_FIELD() do { field[fl] = 0; \
    cJSON_AddItemToArray(cur, cJSON_CreateString(field)); fl = 0; have = 1; \
  } while (0)
#define PUSH_CHAR(c) do { if (fl + 1 >= cap) { cap *= 2; \
    field = realloc(field, cap); } field[fl++] = (c); } while (0)

  for (const char *p = text; *p; p++) {
    char ch = *p;
    if (in_q) {
      if (ch == '"') {
        if (p[1] == '"') { PUSH_CHAR('"'); p++; }
        else in_q = 0;
      } else PUSH_CHAR(ch);
    } else {
      if (ch == '"') in_q = 1;
      else if (ch == ',') { PUSH_FIELD(); }
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
    field[fl] = 0;
    cJSON_AddItemToArray(cur, cJSON_CreateString(field));
    cJSON_AddItemToArray(rows, cur);
  } else {
    cJSON_Delete(cur);
  }
  free(field);

  if (!headers) return rows;

  int nrows = cJSON_GetArraySize(rows);
  if (nrows == 0) { cJSON_Delete(rows); return cJSON_CreateArray(); }
  cJSON *head = cJSON_GetArrayItem(rows, 0);
  int nh = cJSON_GetArraySize(head);
  char **names = calloc(nh, sizeof(char *));
  for (int i = 0; i < nh; i++) {
    const char *h = cJSON_GetArrayItem(head, i)->valuestring;
    while (*h == ' ' || *h == '\t') h++;        /* trim() */
    char *t = strdup(h);
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
    char *c = malloc(len + 1); memcpy(c, buf, len); c[len] = 0; return c;
  }
  size_t in_left = len, out_cap = len * 3 + 1, out_left = out_cap;
  char *out = malloc(out_cap);
  char *in_p = (char *)buf, *out_p = out;
  int failed = 0;
  while (in_left > 0) {
    size_t r = iconv(cd, &in_p, &in_left, &out_p, &out_left);
    if (r == (size_t)-1) {
      if (errno == E2BIG) {
        size_t used = (size_t)(out_p - out);
        out_cap *= 2; out = realloc(out, out_cap);
        out_p = out + used; out_left = out_cap - used;
        continue;
      }
      failed = 1; break;          /* invalid/incomplete → JS catch → utf8 */
    }
  }
  iconv_close(cd);
  if (failed) {
    free(out);
    char *c = malloc(len + 1); memcpy(c, buf, len); c[len] = 0; return c;
  }
  *out_p = 0;
  return out;
}
