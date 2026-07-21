/* JPCERT/CC alerts — RSS sibling of ipa_alerts.c. Replaces the de-duped
 * Node `jpcert-alerts` (Node keeps `jpcert-alerts-rss`; C had no rss variant,
 * so this restores JPCERT coverage at the canonical id). uid PK dedups. */
#include "../../source.h"
#include "../../lib/rss_atom.h"
static const char *FEEDS[] = {
  "https://www.jpcert.or.jp/rss/jpcert.rdf",
  "https://www.jpcert.or.jp/rss/jpcert-all.rdf", 0 };
static int run(const source_ctx *c, intel_sink *s) {
  int tot = 0;
  for (int i = 0; FEEDS[i]; i++) {
    int n = rss_collect(c, s, FEEDS[i], "ja", "[\"advisory\",\"jpcert\",\"cyber\"]");
    if (n > 0) tot += n;
  }
  return tot > 0 ? 0 : -1; }
static const source_def jpcert_alerts_rss_def = {
  .id="jpcert-alerts-rss", .collector="cyber", .name="JPCERT/CC alerts RSS",
  .name_ja="JPCERT/CC 注意喚起 RSS", .update_interval_sec=3600, .run=run };
REGISTER_SOURCE(jpcert_alerts_rss_def)
