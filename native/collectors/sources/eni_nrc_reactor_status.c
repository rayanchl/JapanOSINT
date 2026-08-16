/* US NRC power reactor status — daily percent of rated electrical power.
 * Endpoint: https://www.nrc.gov/reading-rm/doc-collections/event-status/
 *           reactor-status/powerreactorstatusforlast365days.txt       (keyless)
 * Pipe-delimited with a single header line "ReportDt|Unit|Power",
 * ~1.3 MB / 365 days x ~93 units. Dates are US m/d/yyyy with a constant
 * '12:00:00 AM' time component.
 * Emits one row per reactor unit for the MOST RECENT ReportDt in the file.
 *
 * UNIT TRAP, quoted from the source survey: "Power is an INTEGER PERCENT
 * (0-100) of rated electrical output, NOT megawatts - mislabelling it as MW
 * would be exactly the unit error the audit found." The field is therefore
 * emitted as power_percent_of_rated with unit "% of rated electrical output",
 * and there is deliberately no MW field anywhere in this collector.
 * ZERO IS REAL: 0 means the unit is shut down (refuelling outage or scram) and
 * must not be dropped as "missing" — only a non-numeric token is dropped.
 * COLUMN SAFETY: split_pipe() is positional and never collapses empty fields.
 * Licence: US Nuclear Regulatory Commission, public domain. */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "nrc-reactor-status"

static int split_pipe(char *s, char **out, int max) {
  int n = 0;
  out[n++] = s;
  for (char *p = s; *p && n < max; p++)
    if (*p == '|') { *p = 0; out[n++] = p + 1; }
  return n;
}
static char *trimp(char *s) {
  while (*s == ' ' || *s == '\r') s++;
  size_t l = strlen(s);
  while (l && (s[l-1] == ' ' || s[l-1] == '\r' || s[l-1] == '\n')) s[--l] = 0;
  return s;
}
/* m/d/yyyy -> a comparable yyyymmdd integer (no strptime dependency) */
static long day_key(const char *d) {
  int m = 0, dd = 0, y = 0;
  if (sscanf(d, "%d/%d/%d", &m, &dd, &y) != 3) return -1;
  if (y < 1900 || m < 1 || m > 12 || dd < 1 || dd > 31) return -1;
  return (long)y * 10000 + m * 100 + dd;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *txt = feed_get_text(ctx->http,
    "https://www.nrc.gov/reading-rm/doc-collections/event-status/"
    "reactor-status/powerreactorstatusforlast365days.txt", 90000);
  if (!txt) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  /* pass 1: find the newest ReportDt actually present (non-destructive) */
  long newest = -1;
  for (const char *line = txt; line && *line; ) {
    const char *nl = strchr(line, '\n');
    size_t len = nl ? (size_t)(nl - line) : strlen(line);
    if (len && strncmp(line, "ReportDt", 8) != 0) {
      char buf[256];
      snprintf(buf, sizeof buf, "%.*s", (int)(len < 255 ? len : 255), line);
      char *f[4];
      if (split_pipe(buf, f, 4) >= 3) {
        long k = day_key(trimp(f[0]));
        if (k > newest) newest = k;
      }
    }
    line = nl ? nl + 1 : NULL;
  }
  if (newest < 0) { free(txt); fprintf(stderr, "[" SRC "] no parseable rows\n"); return -1; }

  /* pass 2: emit every unit reported on that day */
  int n = 0;
  for (const char *line = txt; line && *line; ) {
    const char *nl = strchr(line, '\n');
    size_t len = nl ? (size_t)(nl - line) : strlen(line);
    const char *next = nl ? nl + 1 : NULL;
    if (!len || strncmp(line, "ReportDt", 8) == 0) { line = next; continue; }
    char buf[256];
    snprintf(buf, sizeof buf, "%.*s", (int)(len < 255 ? len : 255), line);
    line = next;
    char *f[4];
    if (split_pipe(buf, f, 4) < 3) continue;
    char *dt = trimp(f[0]), *unit = trimp(f[1]), *pw = trimp(f[2]);
    if (day_key(dt) != newest) continue;
    if (!*unit || !*pw) continue;
    char *e = NULL;
    double pct = strtod(pw, &e);
    if (e == pw) continue;              /* non-numeric -> no row (0 IS kept) */

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "reactor_unit", unit);
    cJSON_AddNumberToObject(p, "power_percent_of_rated", pct);
    cJSON_AddStringToObject(p, "unit", "% of rated electrical output");
    cJSON_AddStringToObject(p, "unit_warning",
      "this is a PERCENT of rated output, not megawatts");
    cJSON_AddStringToObject(p, "report_date", dt);
    cJSON_AddBoolToObject(p, "shut_down", pct == 0.0);
    cJSON_AddStringToObject(p, "country", "US");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[192], title[256];
    snprintf(key, sizeof key, "%s|%s", unit, dt);
    snprintf(title, sizeof title, "%s: %.0f%% of rated power (%s)",
             unit, pct, dt);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = "https://www.nrc.gov/reading-rm/doc-collections/event-status/reactor-status/";
    row.record_type     = "nuclear-reactor-status";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"nuclear\",\"usa\",\"reactor\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  free(txt);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_nrc_reactor_status_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "US NRC power reactor status",
  .update_interval_sec = 86400, .run = run,
  .category = "infrastructure", .type = "dataset",
  .url = "https://www.nrc.gov/reading-rm/doc-collections/event-status/reactor-status/powerreactorstatusforlast365days.txt",
  .description = "Daily percent-of-rated-power for all US commercial nuclear reactors; refuelling outages, scrams and derates are directly visible.",
  .license = "US Nuclear Regulatory Commission, public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_nrc_reactor_status_def)
