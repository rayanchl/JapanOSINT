/* lib/rss_atom.h — RSS 2.0 / RDF(RSS1) / Atom → intel, port of
 * intelHelpers.parseFeed + feedItemToIntel + intelUid. One toolkit; each RSS
 * collector becomes a ~10-line source.c (mirrors JS createRssCollector). */
#ifndef JO_RSS_ATOM_H
#define JO_RSS_ATOM_H
#include "../source.h"

/* Fetch `url`, parse entries, emit one intel row each via `sink`.
 * uid precedence = guid → link → sha1(title|pubDate) (== intelUid).
 * Returns #emitted, or -1 on fetch/parse failure. */
int rss_collect(const source_ctx *ctx, intel_sink *sink,
                const char *url, const char *lang, const char *tags_json);

#endif
