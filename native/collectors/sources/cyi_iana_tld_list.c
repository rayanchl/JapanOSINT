/* collectors/sources/cyi_iana_tld_list.c
 * IANA delegated TLD list — every TLD currently in the root zone.
 * Endpoint: https://data.iana.org/TLD/tlds-alpha-by-domain.txt      (keyless)
 * parse_notes: "One label per line, uppercase, single '# Version' comment."
 * The version line is parsed (not emitted as a record) and its version string
 * is carried onto every row, which is what makes a later diff — a newly
 * delegated or retired gTLD — visible without inventing state here.
 * Emits: TLD label (lowercased for the title, raw label kept in properties)
 * plus the list version. No coordinates -> has_geo 0 (R2).
 * Licence: IANA public data.
 */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://data.iana.org/TLD/tlds-alpha-by-domain.txt"

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, CYI_URL, 20000);
  if (!body) { fprintf(stderr, "[iana-tld-list] fetch failed\n"); return -1; }

  char version[128] = "";
  int n = 0;
  char *cur = body, *line;
  while ((line = jo_next_line(&cur)) != NULL) {
    if (!line[0]) continue;
    if (line[0] == '#') {
      const char *v = strstr(line, "Version");
      if (v && !version[0]) snprintf(version, sizeof version, "%s", line + 1);
      continue;
    }
    /* labels are A-Z0-9- (and XN--… punycode); anything else is not a TLD */
    int ok = 1;
    for (const char *q = line; *q; q++)
      if (!(isalnum((unsigned char)*q) || *q == '-')) { ok = 0; break; }
    if (!ok || strlen(line) < 2 || strlen(line) > 63) continue;

    char lower[80];
    size_t i = 0;
    for (; line[i] && i + 1 < sizeof lower; i++)
      lower[i] = (char)tolower((unsigned char)line[i]);
    lower[i] = '\0';

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "tld", lower);
    cJSON_AddStringToObject(p, "label", line);
    if (version[0]) cJSON_AddStringToObject(p, "list_version", version);
    cJSON_AddBoolToObject(p, "punycode", strncmp(lower, "xn--", 4) == 0);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    /* `lower` is itself an 80-byte buffer, so ".%s" needs 82 to be safe.
     * Truncation here would only cost a display character, but the title is
     * also what an analyst searches on — a silently clipped TLD is a row you
     * cannot find again. */
    char title[82];
    snprintf(title, sizeof title, ".%s", lower);

    intel_item it = {0};
    it.remote_key      = lower;
    it.title           = title;
    it.summary         = version[0] ? version : "IANA root zone delegation";
    it.link            = CYI_URL;
    it.lang            = "en";
    it.record_type     = "iana-tld";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"dns\",\"tld\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[iana-tld-list] emitted %d TLDs (%s)\n", n, version[0] ? version : "no version line");
  return 0;
}

static const source_def cyi_iana_tld_list_def = {
  .id = "iana-tld-list", .collector = "cyber",
  .name = "IANA delegated TLD list",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "Every TLD currently delegated in the root zone, versioned daily — new gTLD delegations are where abuse registrations land first.",
  .license = "IANA public data.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_iana_tld_list_def)
