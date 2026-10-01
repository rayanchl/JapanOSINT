/* OECD Data Explorer SDMX-JSON — Quarterly National Accounts (DF_QNA).
 * Endpoint: https://sdmx.oecd.org/public/rest/data/OECD.SDD.NAD,DSD_NAMAIN1@DF_QNA,1.1/all
 *           ?format=jsondata&lastNObservations=1
 *
 * Source id OECD_DATASETS, previously an hpengine row in pivot/table/hp3_gov.c.
 * The answer is SDMX-JSON: data.dataSets[0].series is a MAP of 43,889 series
 * keyed by positional dimension indices ("0:3:1:…"), each carrying its latest
 * observation, and data.structure holds the dimension and attribute value
 * lists those indices point into (live 2026-09-15, 5.1 MB). A generic JSON row
 * cannot read that: with no array_path the engine auto-detected the only
 * arrays of objects it could find — structure.annotations, the Data Explorer's
 * LAYOUT_ROW / LAYOUT_COLUMN / NOT_DISPLAYED display hints, all with id "@SDMX"
 * — and stored 179 of those while discarding every series (242 emitted, 179
 * stored, 0 observations). od_sdmx_collect() (od_shared.inc, the ECB and
 * Bundesbank reader) joins each positional key against structure, decodes
 * every series and observation dimension and every series / observation
 * attribute, and keys each observation on its full series key + observation
 * key, so one record per series observation lands.
 * Keyless. Licence: OECD data are published under CC BY 4.0. */
#include "od_shared.inc"
#include "lib/jocore.h"

#define SID "OECD_DATASETS"
#define OECD_QNA_URL \
  "https://sdmx.oecd.org/public/rest/data/OECD.SDD.NAD,DSD_NAMAIN1@DF_QNA,1.1/all" \
  "?format=jsondata&lastNObservations=1"
/* A guard against a runaway document, far above the 43,889 series the flow
 * holds; reaching it is disclosed below rather than read as the whole flow. */
#define OECD_QNA_MAX_ROWS 1000000 /* exhaustive-ok: runaway-document guard far above the 43,889-series flow; reaching it files a truncation notice */

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = od_sdmx_collect(ctx, sink, OECD_QNA_URL, "economic-dataset",
                          "[\"oecd\",\"statistics\",\"economy\",\"policy\"]",
                          "https://data-explorer.oecd.org/", OECD_QNA_MAX_ROWS);
  if (n >= OECD_QNA_MAX_ROWS)
    jo_truncation_notice_ex(sink, SID, NULL, n, -1,
      "the SDMX reader stopped at its per-run observation guard",
      "raise OECD_QNA_MAX_ROWS in collectors/sources/od_oecd_qna.c", NULL);
  return od_rc(SID, n);
}

static const source_def od_oecd_qna_def = {
  .id = SID, .collector = "statistics",
  .name = "OECD — Quarterly National Accounts (SDMX)",
  .name_ja = "OECD 四半期国民経済計算",
  .update_interval_sec = 86400, .run = run,
  .category = "economy", .type = "api",
  .url = OECD_QNA_URL,
  .description = "OECD Quarterly National Accounts for member and partner economies: the latest observation of every DF_QNA series with its decoded dimensions (reference area, sector, transaction, adjustment, unit, price base, transformation) and attributes, from the keyless SDMX-JSON API",
  .license = "OECD data - CC BY 4.0",
  .free_tier = 1,
};
REGISTER_SOURCE(od_oecd_qna_def)
