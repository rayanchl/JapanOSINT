/* Verified-live jp_research sources (15), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"
#include "lib/jocore.h"
#include "lib/rss_atom.h"

/* J-STAGE WebAPI pages on start (1-based) and count (at most 1,000 — count=2000
 * answers WARN_002), and states the whole match set in
 * <opensearch:totalResults>. The four rows below were VRSS rows reading
 * `start=1&count=10`: the first TEN results, every run — text=graphene matched
 * 9,309 articles (measured 2026-10-02). VRSS is a single fetch, so they are
 * hand-written now: the total is read once (count=1), then pages are walked to
 * it. A page is 500 entries, not the API's 1,000, because rss_collect keeps at
 * most $JO_RSS_MAX_ITEMS (default 500) items of any one fetch: 1,000-entry pages
 * stored 4,809 of graphene's 9,309 with nine of its notices. At most
 * JO_PAGE_MAX pages (default 20) per run; when that stops a walk short of the
 * total, the shortfall is a collector-truncation-notice. */
static int jstage_count(void) {
  const char *e = getenv("JO_RSS_MAX_ITEMS");
  int v = e ? atoi(e) : 0;
  return (v > 0 && v < 500) ? v : 500;     /* never past what rss_collect keeps */
}

static long jstage_total(const source_ctx *c, const char *base) {
  char url[640];
  snprintf(url, sizeof url, "%s&start=1&count=1", base);  /* exhaustive-ok: reads only <opensearch:totalResults>; jstage_walk reads every record */
  char *body = feed_get_text(c->http, url, 30000);
  if (!body) return -1;
  long total = -1;
  const char *t = strstr(body, "<opensearch:totalResults>");
  if (t) total = strtol(t + strlen("<opensearch:totalResults>"), NULL, 10);
  free(body);
  return total;
}

static int jstage_walk(const source_ctx *c, intel_sink *s, const char *id,
                       const char *base, const char *lang, const char *tags) {
  /* Only the article search (service=3) pages. The journal list (service=1)
   * and volume list (service=2) ignore start/count and return the whole list
   * every time (587 and 258 entries at count=1 and count=1000 alike, measured
   * 2026-10-02), so a walk would re-read the same document; they are one
   * fetch, and rss_collect discloses its own item ceiling if a list exceeds it. */
  if (!strstr(base, "service=3")) {
    int n = rss_collect(c, s, base, lang, tags);
    if (n >= 0) fprintf(stderr, "[%s] emitted %d (unpaged list)\n", id, n);
    return n;
  }
  const char *pm = getenv("JO_PAGE_MAX");
  int page_max = pm && atoi(pm) > 0 ? atoi(pm) : 20;
  long total = jstage_total(c, base);
  const int count = jstage_count();
  int emitted = 0, pages = 0, stopped = 0;
  long start = 1;
  for (;;) {
    char url[640];
    snprintf(url, sizeof url, "%s&start=%ld&count=%d", base, start, count);
    int n = rss_collect(c, s, url, lang, tags);
    if (n < 0) {
      if (pages == 0) return -1;
      stopped = 1;                              /* a later page failed */
      break;
    }
    pages++;
    emitted += n;
    start += count;
    if (total >= 0 ? start > total : n < count) break;
    if (pages >= page_max) { stopped = 1; break; }
  }
  if (stopped) {
    char why[160];
    snprintf(why, sizeof why, "the walk stopped after %d page(s) of %d at "
             "start=%ld", pages, count, start);
    jo_trunc_notice(s, id, base, emitted, total, why,
                    "raise JO_PAGE_MAX (pages of 500 per run), or re-run");
  }
  fprintf(stderr, "[%s] emitted %d across %d page(s), totalResults %ld\n",
          id, emitted, pages, total);
  return emitted;
}

#define VJSTAGE(SYM, ID, NAME, NAMEJA, COLL, CAT, BASE, LANG, TAGS, IVAL, DESC) \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                   \
    return jstage_walk(c, s, ID, BASE, LANG, TAGS) < 0 ? -1 : 0; }              \
  static const source_def SYM = {                                             \
    .id = ID, .collector = COLL, .name = NAME, .name_ja = NAMEJA,              \
    .update_interval_sec = IVAL, .run = run_##SYM,                             \
    .category = CAT, .type = "web_request", .url = BASE,                       \
    .description = DESC, .layer = NULL, .free_tier = 1 };                      \
  REGISTER_SOURCE(SYM)

VJSON(jp_ihr_hegemony_countries, "jp-ihr-hegemony-countries", "IIJ IHR country-level AS hegemony", "IIJ IHR country-level AS hegemony",
  "jp_research", "research",
  "https://ihr.iijlab.net/ihr/api/hegemony/countries/?country=JP&af=4",
  "results",
  "en", "[\"jp\",\"research\",\"batch16\",\"high-penetrancy\"]", 86400,
  "For a country, every transit AS ranked by hegemony score (dependency weight) with timebin, asn, asn_name, weight, weightscheme, transitonly flag. 1,488 rows for JP. Shows which foreign carriers a national internet depends on.");

VJSTAGE(jstage_articles_by_issn, "jstage-articles-by-issn", "J-STAGE article search by ISSN (JP)", "J-STAGE article search by ISSN (JP)",
  "jp_research", "research",
  "https://api.jstage.jst.go.jp/searchapi/do?service=3&issn=1347-4715",
  "ja", "[\"jp\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "J-STAGE WebAPI service=3: articles for a journal ISSN as Atom+PRISM - article titles in Japanese and English, authors in both scripts, volume/issue/pages, DOI, publication date and the J-STAGE full-text url. J-STAGE was entirely absent from the tree. Returns XML/Atom, status code in <result><status>.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSTAGE(jstage_articles_text_search, "jstage-articles-text-search", "J-STAGE full-text article search (JP)", "J-STAGE full-text article search (JP)",
  "jp_research", "research",
  "https://api.jstage.jst.go.jp/searchapi/do?service=3&text=graphene",
  "ja", "[\"jp\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "service=3 with text= searches the article body across all Japanese society journals on J-STAGE. Requires at least one of material/article/author/affil/keyword/abst/text/issn/cdjournal (otherwise ERR_012).  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSTAGE(jstage_journals, "jstage-journals", "J-STAGE journal directory (JP)", "J-STAGE journal directory (JP)",
  "jp_research", "research",
  "https://api.jstage.jst.go.jp/searchapi/do?service=1&pubyearfrom=2024",
  "ja", "[\"jp\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "939KB directory of Japanese scholarly journals with cdjournal code, ISSN, publisher society name in JA and EN, and coverage years. The cdjournal codes are the keys for the volumes and article endpoints above.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");

VJSTAGE(jstage_volumes_by_journal, "jstage-volumes-by-journal", "J-STAGE volumes and issues of a journal (JP)", "J-STAGE volumes and issues of a journal (JP)",
  "jp_research", "research",
  "https://api.jstage.jst.go.jp/searchapi/do?service=2&cdjournal=jsme1958",
  "ja", "[\"jp\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "377KB: every volume and issue of a Japanese journal with dates and issue-level urls - the journal->issue->article drilldown. Requires cdjournal or issn (ERR_011 otherwise).  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");


VJSON(global_ooni_measurements_jp, "global-ooni-measurements-jp", "OONI measurements list (country filtered)", "OONI measurements list (country filtered)",
  "jp_research", "research",
  "https://api.ooni.io/api/v1/measurements?limit=3&probe_cc=JP",
  "results",
  "en", "[\"jp\",\"research\",\"batch16\",\"high-penetrancy\",\"detail-hop\"]", 86400,
  "Measurement rows with anomaly/confirmed/failure flags, input URL, probe_asn, probe_cc, report_id, blocking scores (general/global/country/isp/local) and, critically, measurement_url — the raw_measurement link used as the detail hop.  A per-record detail endpoint was verified for this source; see docs/verified-sources-batch16.md. This collector fetches the list endpoint only.");
