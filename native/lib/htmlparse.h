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

/* First `attr="..."` (or `attr='...'`, or unquoted `attr=…`) value within the
 * first tag of `s` (or anywhere in s). The name must sit on a boundary (buffer
 * start, whitespace or right after '<'), so `data-src=` does NOT answer a
 * lookup for "src". Copies value into out. Returns 1/0. */
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
#define html_seen_has(s, href) seen_has((s), (href))
#define html_seen_free(s)      seen_free((s))

/* ── the strongest label for each link on a page ──────────────────────────
 * A listing links one item more than once — an image or icon anchor, then
 * the headline — and the record is one record whichever anchor is read
 * first. Its LABEL is not: emitting at the first anchor made an icon's weak
 * label (its alt text, or a neighbour's caption borrowed for it) the title,
 * and the real link text that followed was discarded as a duplicate. Gifu
 * Shimbun's article list titled `/articles/-/409812` "9月26日 10:00" — the
 * PREVIOUS card's timestamp — instead of "岐阜新聞・中学3年模試".
 *
 * So a page's anchors are offered here first and emitted after: one entry
 * per key (the resolved link) in first-appearance order. Its label is the
 * strongest one offered — the anchor's own text over its image's alt over a
 * label borrowed from beside it — with two qualifications, both measured on
 * a replay of the 1,127 HTML rows (2026-10-06):
 *
 *   - a stronger label does not replace one that already CONTAINS it
 *     (whitespace ignored, a trailing ellipsis stripped): an image's alt of
 *     "嬉野市嬉野町　不動山　上　国道34号(75k880)" keeps its place over the
 *     link text "不動山　上", and a full headline in an alt over the same
 *     headline cut to "…ウイル..." in the text. The longer label says
 *     everything the shorter one does;
 *   - between equal strengths the first still wins, so a link whose anchors
 *     are all text keeps exactly the title it always had.
 *
 * Nothing is dropped. Every key offered is returned, and every OTHER distinct
 * label the page itself gave the link (text or alt) is kept in `others` — a
 * logo's "愛知労働局" behind a "ホーム" link is a fact about that link, and the
 * caller records it beside the title rather than discarding it. A BORROWED
 * label that lost is not kept: it was the engine's guess from neighbouring
 * text, and the Gifu one belonged to another article. */
enum {
  HTML_LABEL_CONTEXT = 1,   /* borrowed: the anchor's attributes or nearby text */
  HTML_LABEL_IMG     = 2,   /* the anchor's image's alt / title               */
  HTML_LABEL_TEXT    = 3    /* the anchor's own text                          */
};
typedef struct {
  char  *key, *label;
  int    strength;
  char **others;            /* the other distinct labels, in the order seen */
  int    nothers, cothers;
} html_label_ent;
/* `v` in first-offer order; `h` an open-addressing index into it. {0}-init. */
typedef struct { html_label_ent *v; int n, cap; int *h; int hcap; } html_label_set;

/* 1 = a new key, recorded with this label; 0 = a key already offered (its
 * label replaced only per the rules above, the loser kept in `others`);
 * -1 = out of memory, nothing recorded — the caller must still use the anchor. */
int  html_label_offer(html_label_set *s, const char *key, const char *label,
                      int strength);
/* Does label `a` already say everything `b` says? (The containment rule.) */
int  html_label_covers(const char *a, const char *b);
void html_label_free(html_label_set *s);

#endif
