/* lib/linecolor.c — port of _lineColor.js computeLineColor. See header. */
#include "linecolor.h"
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>

static const char *NAMED[][2] = {
  {"red","#e53935"},{"blue","#1e88e5"},{"green","#43a047"},{"yellow","#fdd835"},
  {"orange","#fb8c00"},{"purple","#8e24aa"},{"pink","#ec407a"},{"brown","#6d4c41"},
  {"grey","#757575"},{"gray","#757575"},{"silver","#bdbdbd"},{"black","#212121"},
  {"white","#eeeeee"},{"cyan","#00acc1"},{"magenta","#d81b60"},{"lime","#c0ca33"},
  {"teal","#00897b"},{"navy","#1a237e"},{"maroon","#b71c1c"},{"olive","#827717"},
};
static const char *PALETTE[24] = {
  "#e53935","#1e88e5","#43a047","#fb8c00","#8e24aa","#00acc1","#d81b60","#3949ab",
  "#f4511e","#7cb342","#5e35b1","#00897b","#fdd835","#6d4c41","#c0ca33","#1e6091",
  "#ad1457","#2e7d32","#ef6c00","#4527a0","#ff7043","#546e7a","#c2185b","#00838f",
};

static int is_hex(char c){ return (c>='0'&&c<='9')||(c>='a'&&c<='f'); }

/* normalizeHex: ^#([0-9a-f]{3}|[0-9a-f]{6})$ (ci); #rgb → #rrggbb; lower. */
static int normalize_hex(const char *s, char *out8) {
  char l[8]; size_t n = 0;
  for (const char *p = s; *p && n < 7; p++) l[n++] = (char)tolower((unsigned char)*p);
  l[n] = 0;
  if (l[0] != '#') return 0;
  size_t hl = n - 1;
  if (hl != 3 && hl != 6) return 0;
  for (size_t i = 1; i < n; i++) if (!is_hex(l[i])) return 0;
  if (hl == 3) {
    snprintf(out8, 8, "#%c%c%c%c%c%c", l[1],l[1],l[2],l[2],l[3],l[3]);
  } else {
    memcpy(out8, l, 7); out8[7] = 0;
  }
  return 1;
}

/* tags[k] as a trimmed string or NULL (only non-empty). */
static const char *tag_str(cJSON *tags, const char *k) {
  cJSON *v = cJSON_GetObjectItem(tags, k);
  if (!v || !cJSON_IsString(v)) return NULL;
  const char *s = v->valuestring;
  while (*s==' '||*s=='\t'||*s=='\n'||*s=='\r') s++;
  return *s ? s : NULL;
}

/* lowercased+trailing-trimmed copy of tags[k] into buf; 1 if non-empty. */
static int tag_lower(cJSON *tags, const char *k, char *buf, size_t n) {
  const char *s = tag_str(tags, k);
  if (!s) return 0;
  size_t L = 0;
  for (; s[L] && L < n - 1; L++) buf[L] = (char)tolower((unsigned char)s[L]);
  while (L && (buf[L-1]==' '||buf[L-1]=='\t'||buf[L-1]=='\n'||buf[L-1]=='\r')) L--;
  buf[L] = 0;
  return L > 0;
}

static int parse_colour(cJSON *tags, char *out8) {
  cJSON *cv = cJSON_GetObjectItem(tags, "colour");
  if (!cv || !cJSON_IsString(cv)) return 0;
  const char *raw = cv->valuestring;
  while (*raw==' '||*raw=='\t'||*raw=='\n'||*raw=='\r') raw++;
  if (!*raw) return 0;
  if (raw[0] == '#') return normalize_hex(raw, out8);
  char low[64]; size_t i = 0;
  for (; raw[i] && i < sizeof low - 1; i++) low[i] = (char)tolower((unsigned char)raw[i]);
  low[i] = 0;
  while (i && (low[i-1]==' '||low[i-1]=='\t')) low[--i] = 0;
  for (size_t j = 0; j < sizeof NAMED / sizeof NAMED[0]; j++)
    if (strcmp(low, NAMED[j][0]) == 0) {
      snprintf(out8, 8, "%s", NAMED[j][1]); return 1;
    }
  return 0;
}

/* FNV-1a 32-bit over the bytes (== JS algorithm, UTF-8 input). */
static uint32_t fnv1a(const char *s) {
  uint32_t h = 0x811c9dc5u;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    h ^= *p;
    h = h + ((h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24));
  }
  return h;
}

int line_color(cJSON *tags, char *out8) {
  if (!tags) return 0;
  if (parse_colour(tags, out8)) return 1;

  char agency[256] = {0}, line[256] = {0};
  int ha = tag_lower(tags, "operator", agency, sizeof agency);
  if (!ha) ha = tag_lower(tags, "network", agency, sizeof agency);
  int hl = tag_lower(tags, "line", line, sizeof line);
  if (!hl) hl = tag_lower(tags, "ref", line, sizeof line);
  if (!ha && !hl) return 0;

  char key[560];
  snprintf(key, sizeof key, "agency=%s|line=%s", ha ? agency : "", hl ? line : "");
  snprintf(out8, 8, "%s", PALETTE[fnv1a(key) % 24]);
  return 1;
}
