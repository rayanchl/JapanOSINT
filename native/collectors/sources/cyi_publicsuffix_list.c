/* collectors/sources/cyi_publicsuffix_list.c
 * Mozilla Public Suffix List — the registrable-domain boundary table.
 * Endpoint: https://publicsuffix.org/list/public_suffix_list.dat   (keyless)
 * parse_notes: "Lines beginning // are comments except the section markers.
 * Rules may start with '*.' (wildcard) or '!' (exception). Best used as a local
 * index other collectors query, not as a wall of 16k intel rows."
 * So: the whole file is parsed (rule totals per section are logged and carried
 * onto every row), but only the WILDCARD and EXCEPTION rules are materialised
 * as intel rows — those are the boundary special cases that actually change how
 * a hostname is reduced to a registration. Every emitted value is a literal
 * rule read out of the file. No coordinates -> has_geo 0 (R2).
 * Licence: MPL 2.0 (stated in the file header); redistribution allowed with
 * the licence notice.
 */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://publicsuffix.org/list/public_suffix_list.dat"

/* Non-destructive line scanner: returns the trimmed line start + length and
 * advances *p, or NULL at end of buffer. Empty lines come back with len 0. */
static const char *scan_line(const char **p, size_t *len) {
  const char *s = *p;
  if (!s || !*s) return NULL;
  const char *e = strchr(s, '\n');
  size_t l = e ? (size_t)(e - s) : strlen(s);
  *p = e ? e + 1 : s + l;
  while (l && (s[l - 1] == '\r' || s[l - 1] == ' ' || s[l - 1] == '\t')) l--;
  while (l && (*s == ' ' || *s == '\t')) { s++; l--; }
  *len = l;
  return s;
}

/* substring test bounded to a line (the buffer is not NUL-split) */
static int line_has(const char *s, size_t l, const char *needle) {
  size_t nl = strlen(needle);
  if (nl > l) return 0;
  for (size_t i = 0; i + nl <= l; i++)
    if (memcmp(s + i, needle, nl) == 0) return 1;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, CYI_URL, 40000);
  if (!body) { fprintf(stderr, "[publicsuffix-list] fetch failed\n"); return -1; }

  /* pass 1: total rule count (comments excluded) */
  int total = 0;
  const char *cur = body, *s;
  size_t l;
  while ((s = scan_line(&cur, &l)) != NULL) {
    if (l == 0 || (l >= 2 && s[0] == '/' && s[1] == '/')) continue;
    total++;
  }

  /* pass 2: section tracking + emit the boundary rules */
  const char *section = NULL;
  int icann = 0, priv = 0, n = 0;
  cur = body;
  while ((s = scan_line(&cur, &l)) != NULL) {
    if (l == 0) continue;
    if (l >= 2 && s[0] == '/' && s[1] == '/') {
      if (line_has(s, l, "===BEGIN ICANN DOMAINS===")) section = "ICANN";
      else if (line_has(s, l, "===BEGIN PRIVATE DOMAINS===")) section = "PRIVATE";
      else if (line_has(s, l, "===END ICANN DOMAINS===") ||
               line_has(s, l, "===END PRIVATE DOMAINS===")) section = NULL;
      continue;
    }
    if (section && section[0] == 'I') icann++;
    else if (section) priv++;

    int wildcard = (s[0] == '*');
    int exception = (s[0] == '!');
    if (!wildcard && !exception) continue;      /* index-only rule, not a row */
    if (l >= 200) continue;

    char rule[200];
    memcpy(rule, s, l);
    rule[l] = '\0';

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "rule", rule);
    cJSON_AddStringToObject(p, "kind", wildcard ? "wildcard" : "exception");
    if (section) cJSON_AddStringToObject(p, "section", section);
    cJSON_AddNumberToObject(p, "list_rule_count", total);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char summary[192];
    snprintf(summary, sizeof summary, "%s rule%s%s",
             wildcard ? "wildcard" : "exception",
             section ? " · " : "", section ? section : "");

    intel_item it = {0};
    it.remote_key      = rule;
    it.title           = rule;
    it.summary         = summary;
    it.link            = CYI_URL;
    it.lang            = "en";
    it.record_type     = "public-suffix-rule";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"dns\",\"public-suffix\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr,
          "[publicsuffix-list] %d rules parsed (ICANN %d / PRIVATE %d); emitted %d boundary rules\n",
          total, icann, priv, n);
  return 0;
}

static const source_def cyi_publicsuffix_list_def = {
  .id = "publicsuffix-list", .collector = "cyber",
  .name = "Mozilla Public Suffix List",
  .update_interval_sec = 604800, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "The registrable-domain boundary table; the wildcard and exception rules that decide whether evil.co.uk is a registration or a subdomain are emitted as rows.",
  .license = "MPL 2.0 (stated in the file header); redistribution allowed with the licence notice.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_publicsuffix_list_def)
