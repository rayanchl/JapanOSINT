/* Bonneville Power Administration balancing authority load and generation.
 * Endpoint: https://transmission.bpa.gov/business/operations/Wind/baltwg.txt
 * Keyless. Tab-separated: a ~11-line prose preamble, then the header row
 * "Date/Time\tLoad\tVER\tHydro\tFossil/Biomass\tNuclear" and 5-minute rows.
 * Emits one row per metric of the newest COMPLETE observation:
 *   value (UNIT: MW — SCADA 5-minute readings), metric name, Pacific-local
 *   timestamp.
 *
 * THE AUDIT'S SIGNATURE BUG, quoted from the source survey: "THE FILE IS PADDED
 * TO THE END OF THE 7-DAY WINDOW WITH FUTURE ROWS THAT HAVE THE TIMESTAMP AND
 * FIVE EMPTY FIELDS ('08/01/2026 23:55\t\t\t\t\t'). Split on tab and require a
 * non-empty Load token before emitting, or a naive index-based parse will slide
 * and read the minute of the timestamp as megawatts."
 * Accordingly: split_tabs() below splits POSITIONALLY and NEVER collapses empty
 * fields (strtok does collapse them — that is what slid the column index), and
 * a row is only used when the Load token is non-empty and numeric.
 * Licence: US Department of Energy / BPA public data, no key. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "bpa-balancing-authority"
#define NCOL 6

static const char *COLS[NCOL] = { "Date/Time", "Load", "VER", "Hydro",
                                  "Fossil/Biomass", "Nuclear" };
static const char *METRIC[NCOL] = { NULL, "load", "ver_wind_solar", "hydro",
                                    "fossil_biomass", "nuclear" };

/* Positional split on TAB that never collapses empty fields. Writes up to
 * `max` pointers into `out`, NUL-terminating each field in place. */
static int split_tabs(char *s, char **out, int max) {
  int n = 0;
  out[n++] = s;
  for (char *p = s; *p && n < max; p++)
    if (*p == '\t') { *p = 0; out[n++] = p + 1; }
  return n;
}

static char *trim(char *s) {
  while (*s == ' ' || *s == '\r') s++;
  size_t l = strlen(s);
  while (l && (s[l-1] == ' ' || s[l-1] == '\r' || s[l-1] == '\n')) s[--l] = 0;
  return s;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *txt = feed_get_text(ctx->http,
    "https://transmission.bpa.gov/business/operations/Wind/baltwg.txt", 45000);
  if (!txt) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  /* Walk lines; remember the last line whose Load token is non-empty numeric. */
  char best[512]; best[0] = 0;
  int seen_header = 0;
  char *save = txt, *line;
  while ((line = save) && *line) {
    char *nl = strchr(line, '\n');
    if (nl) { *nl = 0; save = nl + 1; } else save = line + strlen(line);
    char *l = trim(line);
    if (!*l) continue;
    if (!seen_header) {
      if (strncmp(l, "Date/Time", 9) == 0) seen_header = 1;
      continue;                                    /* prose preamble */
    }
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s", l);
    char *f[NCOL];
    int nf = split_tabs(tmp, f, NCOL);
    if (nf < 2) continue;
    char *load = trim(f[1]);
    if (!*load) continue;                          /* FUTURE padding row */
    char *e = NULL;
    (void)strtod(load, &e);
    if (e == load) continue;                       /* not a number */
    snprintf(best, sizeof best, "%s", l);
  }
  free(txt);

  if (!seen_header) { fprintf(stderr, "[" SRC "] header row not found\n"); return -1; }
  if (!best[0]) { fprintf(stderr, "[" SRC "] no complete row\n"); return 0; }

  char *f[NCOL];
  int nf = split_tabs(best, f, NCOL);
  const char *when = trim(f[0]);

  int n = 0;
  for (int i = 1; i < NCOL && i < nf; i++) {
    char *tok = trim(f[i]);
    if (!*tok) continue;                     /* empty field -> no measurement */
    char *e = NULL;
    double mw = strtod(tok, &e);
    if (e == tok) continue;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "balancing_authority", "BPA");
    cJSON_AddStringToObject(p, "metric", METRIC[i]);
    cJSON_AddStringToObject(p, "column", COLS[i]);
    cJSON_AddNumberToObject(p, "value_mw", mw);
    cJSON_AddStringToObject(p, "unit", "MW");
    cJSON_AddStringToObject(p, "observed_at_local", when);
    cJSON_AddStringToObject(p, "timezone", "America/Los_Angeles (no offset in file)");
    cJSON_AddStringToObject(p, "resolution", "5min");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[128], title[224];
    snprintf(key, sizeof key, "%s|%s", when, METRIC[i]);
    snprintf(title, sizeof title, "BPA %s = %.0f MW at %s", METRIC[i], mw, when);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = "https://transmission.bpa.gov/business/operations/Wind/baltwg.txt";
    row.record_type     = "grid-balancing-authority";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"bpa\",\"usa\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_bpa_balancing_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "BPA balancing authority load and generation",
  .update_interval_sec = 300, .run = run,
  .category = "infrastructure", .type = "dataset",
  .url = "https://transmission.bpa.gov/business/operations/Wind/baltwg.txt",
  .description = "Five-minute SCADA load and generation-by-type (MW) for the Bonneville Power Administration balancing authority, US Pacific Northwest.",
  .license = "US Department of Energy (BPA) public data, no key, no stated restriction.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_bpa_balancing_def)
