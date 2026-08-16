/* cert_national_rss.c — national / governmental CERT-CSIRT advisory feeds.
 *
 * Nine keyless RSS/Atom advisory streams, one source_def each, all served by
 * one table-driven run() that dispatches on ctx->source_id.
 *
 *   bsi-cert-bund-advisories   https://wid.cert-bund.de/content/public/securityAdvisory/rss
 *   dnsc-romania-advisories    https://dnsc.ro/feed
 *   incibe-cert-avisos         https://www.incibe.es/incibe-cert/alerta-temprana/avisos/feed
 *   cert-hr-advisories         https://www.cert.hr/feed/
 *   govcert-bg-advisories      https://www.govcert.bg/feed/
 *   hkcert-security-bulletin   https://www.hkcert.org/getrss/security-bulletin
 *   cert-eu-security-advisories https://cert.europa.eu/publications/security-advisories-rss
 *   certcc-vulnerability-notes https://www.kb.cert.org/vuls/atomfeed/
 *   kaspersky-ics-cert         https://ics-cert.kaspersky.com/feed/
 *
 * Emits (all straight from the feed, nothing synthesized): title, body and
 * summary from <description>/<summary>, link, guid/id as remote_key, and
 * pubDate/published normalised to ISO-8601 by lib/rss_atom.c.
 *
 * Traps handled / noted:
 *   - HKCERT: the feed lives at /getrss/security-bulletin; /rss/security-bulletin
 *     404s (recorded in the probe notes), so the path is pinned here.
 *   - BSI: severity is carried in <category> (niedrig/mittel/hoch/kritisch) and
 *     the WID-SEC-YYYY-NNNN id in the ?name= query of <link>; both stay in the
 *     stored title/link rather than being re-derived into invented fields.
 *   - CERT/CC is Atom and puts the whole note body in <summary type="html">;
 *     rss_collect already prefers <summary> for Atom entries.
 *
 * R2: no geometry. A CERT is not the location of the vulnerability it reports,
 * and none of these feeds publishes a coordinate.
 * R3: a national CERT that published nothing today is not broken — a successful
 * fetch returns 0 even at zero rows; only a failed fetch/parse returns -1.
 *
 * Keyless. Licence: see each source_def .license (only BSI states terms). */
#include "source.h"
#include "lib/rss_atom.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>

typedef struct {
  const char *id;
  const char *url;
  const char *lang;
  const char *tags;
} cert_feed;

static const cert_feed CERT_FEEDS[] = {
  { "bsi-cert-bund-advisories",
    "https://wid.cert-bund.de/content/public/securityAdvisory/rss", "de",
    "[\"advisory\",\"cert\",\"cyber\",\"de\"]" },
  { "dnsc-romania-advisories",
    "https://dnsc.ro/feed", "ro",
    "[\"advisory\",\"cert\",\"cyber\",\"ro\"]" },
  { "incibe-cert-avisos",
    "https://www.incibe.es/incibe-cert/alerta-temprana/avisos/feed", "es",
    "[\"advisory\",\"cert\",\"cyber\",\"es\"]" },
  { "cert-hr-advisories",
    "https://www.cert.hr/feed/", "hr",
    "[\"advisory\",\"cert\",\"cyber\",\"hr\"]" },
  { "govcert-bg-advisories",
    "https://www.govcert.bg/feed/", "bg",
    "[\"advisory\",\"cert\",\"cyber\",\"bg\"]" },
  { "hkcert-security-bulletin",
    "https://www.hkcert.org/getrss/security-bulletin", "en",
    "[\"advisory\",\"cert\",\"cyber\",\"hk\"]" },
  { "cert-eu-security-advisories",
    "https://cert.europa.eu/publications/security-advisories-rss", "en",
    "[\"advisory\",\"cert\",\"cyber\",\"eu\"]" },
  { "certcc-vulnerability-notes",
    "https://www.kb.cert.org/vuls/atomfeed/", "en",
    "[\"advisory\",\"cert\",\"cyber\",\"vulnerability-note\"]" },
  { "kaspersky-ics-cert",
    "https://ics-cert.kaspersky.com/feed/", "en",
    "[\"advisory\",\"cert\",\"cyber\",\"ics\",\"ot\"]" },
};

static int run(const source_ctx *c, intel_sink *s) {
  if (!c || !c->source_id) return -1;
  for (size_t i = 0; i < sizeof CERT_FEEDS / sizeof CERT_FEEDS[0]; i++) {
    if (strcmp(c->source_id, CERT_FEEDS[i].id) != 0) continue;
    int n = rss_collect(c, s, CERT_FEEDS[i].url, CERT_FEEDS[i].lang,
                        CERT_FEEDS[i].tags);
    if (n < 0) {
      fprintf(stderr, "[%s] fetch/parse failed\n", c->source_id);
      return -1;                       /* the fetch actually failed (R3) */
    }
    fprintf(stderr, "[%s] emitted %d\n", c->source_id, n);
    return 0;                          /* fetched fine; 0 rows is honest (R3) */
  }
  fprintf(stderr, "[cert_national_rss] unknown source id %s\n", c->source_id);
  return -1;
}

static const source_def cert_bsi_def = {
  .id = "bsi-cert-bund-advisories", .collector = "cyber",
  .name = "BSI CERT-Bund WID Security Advisories (Germany)",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://wid.cert-bund.de/content/public/securityAdvisory/rss",
  .description = "Germany's BSI Warn- und Informationsdienst advisory feed; "
                 "each item carries a WID-SEC advisory id in the link and a "
                 "German severity band (niedrig/mittel/hoch/kritisch).",
  .license = "BSI provides content 'as is' for informational purposes (per "
             "their CSAF issuing-authority statement).",
  .free_tier = 1 };
REGISTER_SOURCE(cert_bsi_def)

static const source_def cert_dnsc_ro_def = {
  .id = "dnsc-romania-advisories", .collector = "cyber",
  .name = "DNSC Romania - National Cyber Security Directorate",
  .update_interval_sec = 10800, .run = run,
  .category = "cyber", .type = "api", .url = "https://dnsc.ro/feed",
  .description = "Romania's national CERT (DNSC) alert feed: critical-"
                 "vulnerability alerts and weekly cyber situation reports.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_dnsc_ro_def)

static const source_def cert_incibe_def = {
  .id = "incibe-cert-avisos", .collector = "cyber",
  .name = "INCIBE-CERT Early Warning Advisories (Spain)",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://www.incibe.es/incibe-cert/alerta-temprana/avisos/feed",
  .description = "Spain's INCIBE-CERT product-vulnerability advisories, one "
                 "per affected product, naming vendor, product and impact.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_incibe_def)

static const source_def cert_hr_def = {
  .id = "cert-hr-advisories", .collector = "cyber",
  .name = "CERT.hr - Croatian National CERT",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "api", .url = "https://www.cert.hr/feed/",
  .description = "Croatia's national CERT publication feed: advisories, "
                 "campaign warnings and national cyber-security notices.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_hr_def)

static const source_def cert_govcert_bg_def = {
  .id = "govcert-bg-advisories", .collector = "cyber",
  .name = "CERT Bulgaria (GovCERT.bg)",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "api", .url = "https://www.govcert.bg/feed/",
  .description = "Bulgaria's government CERT advisory feed: vulnerability "
                 "warnings and incident notices.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_govcert_bg_def)

static const source_def cert_hkcert_def = {
  .id = "hkcert-security-bulletin", .collector = "cyber",
  .name = "HKCERT Security Bulletin (Hong Kong)",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://www.hkcert.org/getrss/security-bulletin",
  .description = "Hong Kong CERT Coordination Centre security bulletins: "
                 "per-product vulnerability notices with risk rating and "
                 "affected-version detail.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_hkcert_def)

static const source_def cert_eu_adv_def = {
  .id = "cert-eu-security-advisories", .collector = "cyber",
  .name = "CERT-EU Security Advisories",
  .update_interval_sec = 7200, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://cert.europa.eu/publications/security-advisories-rss",
  .description = "CERT-EU's numbered YYYY-NNN security advisories for EU "
                 "institutions (distinct from their threat-intelligence feed).",
  .free_tier = 1 };
REGISTER_SOURCE(cert_eu_adv_def)

static const source_def cert_certcc_def = {
  .id = "certcc-vulnerability-notes", .collector = "cyber",
  .name = "CERT/CC Vulnerability Notes (VU#)",
  .update_interval_sec = 7200, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://www.kb.cert.org/vuls/atomfeed/",
  .description = "CMU/SEI CERT Coordination Center vulnerability notes: "
                 "multi-vendor coordinated disclosures with affected-vendor "
                 "lists and remediation, often ahead of NVD enrichment.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_certcc_def)

static const source_def cert_kaspersky_ics_def = {
  .id = "kaspersky-ics-cert", .collector = "cyber",
  .name = "Kaspersky ICS-CERT",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://ics-cert.kaspersky.com/feed/",
  .description = "Kaspersky ICS-CERT publications: OT/ICS vulnerability "
                 "advisories, industrial incident statistics and sector threat "
                 "landscapes; non-US industrial telemetry alongside CISA ICS.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_kaspersky_ics_def)
