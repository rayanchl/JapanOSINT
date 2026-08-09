/* port of server/src/collectors/ipaAlertsRss.js — multi-feed, one sourceId.
 * uid PK dedups across the 3 feeds (== Node concat+map). */
#include "../../source.h"
#include "../../lib/rss_atom.h"
#include <stdio.h>
static const char *FEEDS[] = {
  "https://www.ipa.go.jp/security/alert-rss.rdf",
  "https://www.ipa.go.jp/security/announce/alert.rss",
  "https://www.ipa.go.jp/security/security-alert.rss", 0 };
static int run(const source_ctx *c, intel_sink *s) {
  int tot = 0, fetched = 0;
  for (int i = 0; FEEDS[i]; i++) {
    int n = rss_collect(c, s, FEEDS[i], "ja", "[\"advisory\",\"ipa\",\"cyber\"]");
    if (n >= 0) { fetched++; tot += n; }
  }
  fprintf(stderr, "[ipa-alerts] emitted %d (%d/3 feeds fetched)\n", tot, fetched);
  /* STATUS code, not a row count: a quiet day on all three feeds is an honest
   * empty. Only a total fetch failure (every feed down) is an error. */
  return fetched > 0 ? 0 : -1; }
static const source_def ipa_alerts_def = {
  .id="ipa-alerts", .collector="cyber", .name="IPA advisories",
  .name_ja="IPA 注意喚起",  .update_interval_sec=3600, .run=run };
REGISTER_SOURCE(ipa_alerts_def)
