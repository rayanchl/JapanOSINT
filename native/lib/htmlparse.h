/* lib/htmlparse.h — text/HTML/XML scan primitives for the "HTML_SCRAPE"
 * family. Survey of the 18 collectors: they use fetchText + REGEX/string
 * parsing (tag-content extract, tag strip, <tr>/<td>/<row> block iteration,
 * href attr) — NOT cheerio DOM selectors. So this is a tiny purpose-built
 * scanner, not a cheerio port. Pair with feed_get_text() (feedlib) and
 * csv_decode_sjis() (csv.h) for the Shift_JIS pages. */
#ifndef JO_HTMLPARSE_H
#define JO_HTMLPARSE_H
#include <stddef.h>

/* JS `s.replace(/<[^>]+>/g,' ').replace(/\s+/g,' ').trim()`.
 * Returns a malloc'd string (caller frees); "" if in is NULL. */
char *html_strip(const char *in);

/* First `<tag ...>INNER</tag>` (case-insensitive, attribute/namespace
 * tolerant; INNER = bytes up to the matching `</tag>`, covers both JS
 * `[^<]*` and `[\s\S]*?` capture intents). Copies raw INNER into out
 * (caller may html_strip it). Returns 1 if found, else 0 (out[0]=0). */
int html_tag(const char *s, const char *tag, char *out, size_t n);

/* Iterate `<tag ...>...</tag>` blocks. Pass cursor=NULL-able start; on a
 * match sets *inner (pointer INTO s, not NUL-terminated) + *inner_len and
 * returns the position just past `</tag>` (feed back as `from`); returns
 * NULL when no more. Case-insensitive, attribute tolerant. */
const char *html_block(const char *from, const char *tag,
                       const char **inner, int *inner_len);

/* First `attr="..."` (or `attr='...'`) value within the first tag of `s`
 * (or anywhere in s). Copies value into out. Returns 1/0. */
int html_attr(const char *s, const char *attr, char *out, size_t n);

/* ── anchors ───────────────────────────────────────────────────────────────
 * THE one `<a href="…">text</a>` scanner in the tree. It used to exist twice:
 * jo_emit_anchors() in collectors/sources/_jp_osint.inc (the registry sweeps)
 * and hp_run_html() in lib/hpengine.c (the HP_HTML rows) each had their own
 * copy, so a fix — e.g. replacing the fixed 64-slot dedupe ring that silently
 * dropped a long listing's tail — had to be made in both. Now both call this.
 *
 * Callers keep their own policy (which hrefs to accept, what to emit); this
 * only finds anchors. */
typedef struct {
  const char *href;      /* into the caller's buffer, NOT NUL-terminated */
  size_t      href_len;
  char        text[512]; /* inner text, tags stripped, trimmed            */
  size_t      text_len;
} html_anchor;

/* Next anchor at/after `from`. Returns where to resume, or NULL when done.
 * Anchors whose markup is malformed (no closing quote / no </a>) are skipped,
 * not silently ending the scan. */
const char *html_anchor_next(const char *from, html_anchor *out);

/* Dedupe of hrefs already emitted is just the generic growable seen-set
 * (lib/seenset.h) — one implementation for the whole tree. */
#include "seenset.h"
typedef seen_set html_seen;
#define html_seen_add(s, href) seen_add((s), (href))
#define html_seen_free(s)      seen_free((s))

#endif
