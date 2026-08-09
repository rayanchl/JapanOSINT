/* JPCERT/CC alerts — RSS sibling of ipa_alerts.c. Replaces the de-duped
 * Node `jpcert-alerts` (Node keeps `jpcert-alerts-rss`; C had no rss variant,
 * so this restores JPCERT coverage at the canonical id). uid PK dedups. */
#include "../../source.h"
#include "../../lib/rss_atom.h"
#include <stdio.h>
static const char *FEEDS[] = {
  "https://www.jpcert.or.jp/rss/jpcert.rdf",
  "https://www.jpcert.or.jp/rss/jpcert-all.rdf", 0 };
/* Contract R3: rc is a STATUS, not a row count, and an honest empty is not a
 * failure. `return tot > 0 ? 0 : -1` reported success as failure on any day
 * JPCERT published nothing — core/scheduler.c logs a non-zero rc as
 * status=error and feeds anomaly_detect(), so a quiet day quarantined a
 * perfectly healthy source. rss_collect returns -1 on a fetch/parse failure
 * and >=0 on success, so "did any feed parse?" is the real health signal. */
static int run(const source_ctx *c, intel_sink *s) {
  int tot = 0, ok = 0;
  for (int i = 0; FEEDS[i]; i++) {
    int n = rss_collect(c, s, FEEDS[i], "ja", "[\"advisory\",\"jpcert\",\"cyber\"]");
    if (n >= 0) { ok = 1; tot += n; }
  }
  fprintf(stderr, "[jpcert-alerts-rss] emitted %d\n", tot);
  return ok ? 0 : -1; }
static const source_def jpcert_alerts_rss_def = {
  .id="jpcert-alerts-rss", .collector="cyber", .name="JPCERT/CC alerts RSS",
  .name_ja="JPCERT/CC 注意喚起 RSS", .update_interval_sec=3600, .run=run };
REGISTER_SOURCE(jpcert_alerts_rss_def)
