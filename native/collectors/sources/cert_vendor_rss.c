/* cert_vendor_rss.c — vendor, project and distribution security advisory feeds.
 *
 * Fourteen keyless RSS/Atom PSIRT streams, one source_def each, served by one
 * table-driven run() dispatching on ctx->source_id.
 *
 *   cisco-psirt-advisories     https://sec.cloudapps.cisco.com/security/center/psirtrss20/CiscoSecurityAdvisory.xml
 *   paloalto-psirt-advisories  https://security.paloaltonetworks.com/rss.xml
 *   fortiguard-ir-advisories   https://filestore.fortinet.com/fortiguard/rss/ir.xml
 *   splunk-security-advisories https://advisory.splunk.com/feed.xml
 *   elastic-security-announcements https://discuss.elastic.co/c/announcements/security-announcements.rss
 *   jenkins-security-advisories https://www.jenkins.io/security/advisories/rss.xml
 *   gitlab-security-releases   https://about.gitlab.com/security-releases.xml
 *   drupal-security-advisories https://www.drupal.org/security/rss.xml
 *   nodejs-vulnerability-releases https://nodejs.org/en/feed/vulnerability.xml
 *   chrome-releases-feed       https://chromereleases.googleblog.com/feeds/posts/default
 *   ubuntu-security-notices    https://ubuntu.com/security/notices/rss.xml
 *   gentoo-glsa-feed           https://security.gentoo.org/glsa/feed.rss
 *   amazon-linux-alas          https://alas.aws.amazon.com/AL2/alas.rss
 *   cxsecurity-wlb-feed        https://cxsecurity.com/wlb/rss/all/
 *
 * Emits (all fetched, nothing constructed): title, body/summary from
 * <description>/<content>, link, guid/id as remote_key, and the publication
 * date normalised to ISO-8601 UTC by lib/rss_atom.c. The advisory identifier
 * (cisco-sa-*, FG-IR-*, SVD-*, ESA-*, SA-CORE-*, USN-*, GLSA, ALAS2*) is
 * already inside the fetched title/link; it is NOT re-derived into a synthetic
 * field, and no lookup URL is ever constructed (R1).
 *
 * Traps handled / noted:
 *   - Splunk and Jenkins wrap title/description in CDATA with surrounding
 *     newlines; lib/rss_atom.c trims before and after unwrapping CDATA, so the
 *     stored title is clean.
 *   - amazon-linux-alas serves the FULL AL2 archive (3,689 items, ~1.9 MB) on
 *     every poll. rss_collect caps a run at JO_RSS_MAX_ITEMS (500 by default)
 *     and feeds are newest-first, so this keeps the fresh end instead of
 *     re-emitting a decade of advisories every 12 h.
 *   - chrome-releases-feed is a Blogger Atom feed covering every release
 *     channel; the CVE list lives inside <content type="html"> of the Stable
 *     Channel entries, which is stored verbatim in `body`. Nothing is filtered
 *     out and nothing is invented for the non-security entries.
 *   - gitlab-security-releases is Atom; the per-release CVE table is inside the
 *     escaped <content>, kept verbatim in `body`.
 *   - nodejs-vulnerability-releases has no <description>: title + link only,
 *     which is exactly what the feed carries.
 *
 * R2: no geometry — a vulnerability is not a place, and none of these feeds
 * publishes a coordinate.
 * R3: an upstream that shipped no advisory today is not broken. A successful
 * fetch returns 0 even at zero rows; only a failed fetch/parse returns -1.
 *
 * Keyless. Licence: stated per source_def where the publisher states one. */
#include "../../source.h"
#include "../../lib/rss_atom.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>

typedef struct {
  const char *id;
  const char *url;
  const char *tags;
} vendor_feed;

static const vendor_feed VENDOR_FEEDS[] = {
  { "cisco-psirt-advisories",
    "https://sec.cloudapps.cisco.com/security/center/psirtrss20/CiscoSecurityAdvisory.xml",
    "[\"advisory\",\"psirt\",\"cyber\",\"cisco\"]" },
  { "paloalto-psirt-advisories",
    "https://security.paloaltonetworks.com/rss.xml",
    "[\"advisory\",\"psirt\",\"cyber\",\"palo-alto\"]" },
  { "fortiguard-ir-advisories",
    "https://filestore.fortinet.com/fortiguard/rss/ir.xml",
    "[\"advisory\",\"psirt\",\"cyber\",\"fortinet\"]" },
  { "splunk-security-advisories",
    "https://advisory.splunk.com/feed.xml",
    "[\"advisory\",\"psirt\",\"cyber\",\"splunk\"]" },
  { "elastic-security-announcements",
    "https://discuss.elastic.co/c/announcements/security-announcements.rss",
    "[\"advisory\",\"psirt\",\"cyber\",\"elastic\"]" },
  { "jenkins-security-advisories",
    "https://www.jenkins.io/security/advisories/rss.xml",
    "[\"advisory\",\"cyber\",\"jenkins\",\"ci-cd\"]" },
  { "gitlab-security-releases",
    "https://about.gitlab.com/security-releases.xml",
    "[\"advisory\",\"cyber\",\"gitlab\",\"ci-cd\"]" },
  { "drupal-security-advisories",
    "https://www.drupal.org/security/rss.xml",
    "[\"advisory\",\"cyber\",\"drupal\",\"cms\"]" },
  { "nodejs-vulnerability-releases",
    "https://nodejs.org/en/feed/vulnerability.xml",
    "[\"advisory\",\"cyber\",\"nodejs\"]" },
  { "chrome-releases-feed",
    "https://chromereleases.googleblog.com/feeds/posts/default",
    "[\"advisory\",\"cyber\",\"chrome\",\"browser\"]" },
  { "ubuntu-security-notices",
    "https://ubuntu.com/security/notices/rss.xml",
    "[\"advisory\",\"cyber\",\"ubuntu\",\"linux-distro\"]" },
  { "gentoo-glsa-feed",
    "https://security.gentoo.org/glsa/feed.rss",
    "[\"advisory\",\"cyber\",\"gentoo\",\"linux-distro\"]" },
  { "amazon-linux-alas",
    "https://alas.aws.amazon.com/AL2/alas.rss",
    "[\"advisory\",\"cyber\",\"amazon-linux\",\"linux-distro\"]" },
  { "cxsecurity-wlb-feed",
    "https://cxsecurity.com/wlb/rss/all/",
    "[\"advisory\",\"exploit\",\"cyber\",\"bugtraq\"]" },
};

static int run(const source_ctx *c, intel_sink *s) {
  if (!c || !c->source_id) return -1;
  for (size_t i = 0; i < sizeof VENDOR_FEEDS / sizeof VENDOR_FEEDS[0]; i++) {
    if (strcmp(c->source_id, VENDOR_FEEDS[i].id) != 0) continue;
    int n = rss_collect(c, s, VENDOR_FEEDS[i].url, "en", VENDOR_FEEDS[i].tags);
    if (n < 0) {
      fprintf(stderr, "[%s] fetch/parse failed\n", c->source_id);
      return -1;                       /* the fetch actually failed (R3) */
    }
    fprintf(stderr, "[%s] emitted %d\n", c->source_id, n);
    return 0;                          /* fetched fine; 0 rows is honest (R3) */
  }
  fprintf(stderr, "[cert_vendor_rss] unknown source id %s\n", c->source_id);
  return -1;
}

static const source_def cert_cisco_psirt_def = {
  .id = "cisco-psirt-advisories", .collector = "cyber",
  .name = "Cisco PSIRT Security Advisories",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://sec.cloudapps.cisco.com/security/center/psirtrss20/CiscoSecurityAdvisory.xml",
  .description = "Cisco's keyless PSIRT advisory feed, 50 most recent "
                 "advisories with the cisco-sa-* identifier in the link. The "
                 "openVuln REST API needs OAuth; this feed does not.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_cisco_psirt_def)

static const source_def cert_paloalto_def = {
  .id = "paloalto-psirt-advisories", .collector = "cyber",
  .name = "Palo Alto Networks Security Advisories",
  .update_interval_sec = 7200, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://security.paloaltonetworks.com/rss.xml",
  .description = "Palo Alto Networks PSIRT feed. Each item title carries the "
                 "CVE id, the affected product line and the severity band.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_paloalto_def)

static const source_def cert_fortiguard_def = {
  .id = "fortiguard-ir-advisories", .collector = "cyber",
  .name = "FortiGuard Labs PSIRT / IR Advisories",
  .update_interval_sec = 7200, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://filestore.fortinet.com/fortiguard/rss/ir.xml",
  .description = "Fortinet PSIRT advisory feed (FG-IR-* advisories). Fortinet "
                 "edge appliances are a recurring mass-exploitation target, so "
                 "this is a high-signal edge-device vulnerability channel.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_fortiguard_def)

static const source_def cert_splunk_def = {
  .id = "splunk-security-advisories", .collector = "cyber",
  .name = "Splunk Security Advisories (SVD)",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://advisory.splunk.com/feed.xml",
  .description = "Splunk PSIRT advisory feed with SVD advisory ids and the "
                 "affected-version lists carried in the description.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_splunk_def)

static const source_def cert_elastic_def = {
  .id = "elastic-security-announcements", .collector = "cyber",
  .name = "Elastic Security Announcements (ESA)",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://discuss.elastic.co/c/announcements/security-announcements.rss",
  .description = "Elastic's ESA-numbered security announcements for "
                 "Elasticsearch, Kibana, Beats and Logstash, with the fixed "
                 "version list in the title.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_elastic_def)

static const source_def cert_jenkins_def = {
  .id = "jenkins-security-advisories", .collector = "cyber",
  .name = "Jenkins Security Advisories",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://www.jenkins.io/security/advisories/rss.xml",
  .description = "Jenkins core and plugin security advisories. Plugin CVEs are "
                 "a large share of the CI/CD attack surface and frequently "
                 "absent from vendor feeds.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_jenkins_def)

static const source_def cert_gitlab_def = {
  .id = "gitlab-security-releases", .collector = "cyber",
  .name = "GitLab Patch and Security Releases",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://about.gitlab.com/security-releases.xml",
  .description = "GitLab security/patch release feed; each entry's content "
                 "lists the CVEs and severities fixed in that release train.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_gitlab_def)

static const source_def cert_drupal_def = {
  .id = "drupal-security-advisories", .collector = "cyber",
  .name = "Drupal Security Advisories (SA-CORE / SA-CONTRIB)",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://www.drupal.org/security/rss.xml",
  .description = "Drupal core and contributed-module advisories. The title "
                 "carries project, risk band, vulnerability class and SA id.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_drupal_def)

static const source_def cert_nodejs_def = {
  .id = "nodejs-vulnerability-releases", .collector = "cyber",
  .name = "Node.js Security Release Feed",
  .update_interval_sec = 43200, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://nodejs.org/en/feed/vulnerability.xml",
  .description = "Node.js project security-release announcements covering "
                 "every coordinated Node security release since 2016.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_nodejs_def)

static const source_def cert_chrome_def = {
  .id = "chrome-releases-feed", .collector = "cyber",
  .name = "Google Chrome Releases (security channel updates)",
  .update_interval_sec = 10800, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://chromereleases.googleblog.com/feeds/posts/default",
  .description = "Google's official Chrome/ChromeOS release channel, where "
                 "browser CVE fixes are disclosed; Stable Channel entries "
                 "enumerate the CVEs fixed and the researcher credits.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_chrome_def)

static const source_def cert_ubuntu_usn_def = {
  .id = "ubuntu-security-notices", .collector = "cyber",
  .name = "Ubuntu Security Notices (USN)",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://ubuntu.com/security/notices/rss.xml",
  .description = "Canonical's USN advisory feed. The ubuntu.com/security/*.json "
                 "APIs returned HTTP 503 at probe time; this RSS endpoint is "
                 "the working keyless path.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_ubuntu_usn_def)

static const source_def cert_gentoo_def = {
  .id = "gentoo-glsa-feed", .collector = "cyber",
  .name = "Gentoo Linux Security Advisories (GLSA)",
  .update_interval_sec = 43200, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://security.gentoo.org/glsa/feed.rss",
  .description = "Gentoo Security advisories. Gentoo tracks upstream "
                 "source-level fixes, so GLSAs often cover packages other "
                 "distributions still ship unpatched.",
  .license = "CC BY-SA 4.0 (Gentoo)",
  .free_tier = 1 };
REGISTER_SOURCE(cert_gentoo_def)

static const source_def cert_amazon_alas_def = {
  .id = "amazon-linux-alas", .collector = "cyber",
  .name = "Amazon Linux Security Advisories (ALAS)",
  .update_interval_sec = 43200, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://alas.aws.amazon.com/AL2/alas.rss",
  .description = "Amazon Linux 2 advisories with the severity band encoded in "
                 "the title; covers the AMI base image fleet running on AWS. "
                 "The document is the full archive, so a run takes the newest "
                 "window (JO_RSS_MAX_ITEMS, 500 by default).",
  .free_tier = 1 };
REGISTER_SOURCE(cert_amazon_alas_def)

static const source_def cert_cxsecurity_def = {
  .id = "cxsecurity-wlb-feed", .collector = "cyber",
  .name = "CXSecurity World Laboratory of Bugtraq (WLB)",
  .update_interval_sec = 10800, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://cxsecurity.com/wlb/rss/all/",
  .description = "CXSecurity's exploit/bugtraq archive feed: independently "
                 "posted exploits and advisories with affected version ranges "
                 "in the title; complements Exploit-DB.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_cxsecurity_def)
