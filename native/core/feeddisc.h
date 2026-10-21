/* core/feeddisc.h — RSS/Atom feed autodiscovery (./bin/japanosint --discover-feeds).
 *
 * Asks each site's homepage which feeds it declares (<link rel="alternate"
 * type="application/rss+xml|atom+xml|rdf+xml">) instead of probing paths we can
 * guess, then fetches every declared feed through the engine's own HTTP client
 * and classifies it. Batch 50 found 961 Japanese government feeds this way where
 * probing /rss/ and /feed/ found 27.
 *
 * The parsers are pure so tests/unit/test_feeddisc.c can pin them offline. */
#ifndef JO_FEEDDISC_H
#define JO_FEEDDISC_H

#include <stddef.h>

typedef void (*feeddisc_link_cb)(const char *abs_url, const char *title, void *ud);

/* Every feed a page declares, resolved against `page_url` (or the page's own
 * <base href>), with &amp; decoded and WordPress comment feeds skipped. Returns
 * the number reported. A relative href resolves against the URL ASKED FOR, not
 * a redirect target, so a homepage that now redirects to another domain does not
 * hand us that domain's feed. */
int feeddisc_links(const char *html, const char *page_url, feeddisc_link_cb cb, void *ud);

/* <item>/<entry> elements in a feed body — not RSS 1.0's <items> container. */
int feeddisc_count_items(const char *body);

/* 1 when the first 4 KB carry an <rss>, <feed> or <rdf:RDF> root. */
int feeddisc_is_feed(const char *body);

/* Reads the item titles of a feed and decides whether its content contradicts
 * its publisher: unambiguous gambling/pharma spam phrases, or several generic
 * gambling words, or Vietnamese text on a source with no reason to publish it
 * (`vi_ok` = the source is Vietnamese). Writes the evidence (matched phrase or
 * counter) into `why`. Returns 1 for a suspected domain takeover. A flag is a
 * question for a human — "casino" is inside "Liceo Pascasino". */
int feeddisc_takeover_suspect(const char *body, int vi_ok, char *why, size_t why_n);

/* The CLI mode: sites from `path` (one URL or bare host per line, '#' comments),
 * `jobs` worker threads, one TSV row per declared feed on stdout. */
int feeddisc_main(const char *path, int jobs);

#endif
