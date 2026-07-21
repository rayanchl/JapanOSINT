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

#endif
