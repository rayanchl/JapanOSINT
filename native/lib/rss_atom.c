#include "rss_atom.h"
#include "../core/httpclient.h"
#include "../third_party/cJSON.h"
#include <openssl/sha.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* --- tiny tolerant XML helpers (sufficient for RSS/RDF/Atom feeds) --- */

static char *dup_n(const char *s, size_t n) {
  char *r = malloc(n + 1); if (!r) return NULL;
  memcpy(r, s, n); r[n] = 0; return r;
}

/* Find first <tag ...>...</tag> inside [from,end); returns inner text
 * (malloc'd, CDATA-stripped, a few entities decoded) or NULL. *after set to
 * end of close tag. Case-insensitive tag match, namespace-agnostic suffix. */
static char *tag_text(const char *from, const char *end, const char *tag,
                      const char **after) {
  size_t tl = strlen(tag);
  for (const char *p = from; p && p < end; p++) {
    if (*p != '<') continue;
    const char *q = p + 1;
    if (q < end && (*q == '/' || *q == '!' || *q == '?')) continue;
    /* match tag possibly with ns prefix: <(...:)?tag[ >/] */
    const char *nameend = q;
    while (nameend < end && *nameend != ' ' && *nameend != '>' &&
           *nameend != '/' && *nameend != '\t' && *nameend != '\n') nameend++;
    size_t namelen = (size_t)(nameend - q);
    const char *base = q;
    for (const char *c = q; c < nameend; c++) if (*c == ':') base = c + 1;
    size_t baselen = (size_t)(nameend - base);
    if (baselen != tl || strncasecmp(base, tag, tl) != 0) {
      (void)namelen; continue;
    }
    const char *gt = memchr(q, '>', (size_t)(end - q));
    if (!gt) return NULL;
    if (gt[-1] == '/') { if (after) *after = gt + 1; return dup_n("", 0); }
    const char *content = gt + 1;
    /* find matching close </...tag> */
    char close[64]; snprintf(close, sizeof close, "</");
    const char *cl = content;
    while (cl < end) {
      const char *lt = memchr(cl, '<', (size_t)(end - cl));
      if (!lt) return NULL;
      if (lt + 1 < end && lt[1] == '/') {
        const char *cn = lt + 2, *ce = cn;
        while (ce < end && *ce != '>') ce++;
        const char *cb = cn;
        for (const char *c = cn; c < ce; c++) if (*c == ':') cb = c + 1;
        if ((size_t)(ce - cb) == tl && strncasecmp(cb, tag, tl) == 0) {
          size_t len = (size_t)(lt - content);
          char *raw = dup_n(content, len);
          if (after) *after = ce + 1;
          /* Trim BEFORE looking for the CDATA wrapper: publishers commonly
           * pretty-print `<title>\n  <![CDATA[ ... ]]>\n</title>`, and matching
           * the wrapper at offset 0 missed every one of those, persisting the
           * literal "<![CDATA[ ... ]]>" as the title (and leaving links
           * unparseable). */
          char *s = raw;
          while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
          if (!strncmp(s, "<![CDATA[", 9)) {
            char *e2 = strstr(s, "]]>");
            if (e2) { *e2 = 0; memmove(s, s + 9, strlen(s + 9) + 1); }
          }
          /* trim ws again — the CDATA payload has its own padding */
          while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
          size_t L = strlen(s);
          while (L && (s[L-1]==' '||s[L-1]=='\n'||s[L-1]=='\r'||s[L-1]=='\t')) s[--L]=0;
          /* decode minimal entities */
          char *o = s;
          for (char *r = s; *r; ) {
            if (!strncmp(r,"&amp;",5)){*o++='&';r+=5;}
            else if(!strncmp(r,"&lt;",4)){*o++='<';r+=4;}
            else if(!strncmp(r,"&gt;",4)){*o++='>';r+=4;}
            else if(!strncmp(r,"&quot;",6)){*o++='"';r+=6;}
            else if(!strncmp(r,"&#39;",5)||!strncmp(r,"&apos;",6)){*o++='\'';r+=(r[2]=='3')?5:6;}
            else *o++=*r++;
          }
          *o=0;
          char *res = strdup(s); free(raw); return res;
        }
      }
      cl = lt + 1;
    }
    return NULL;
  }
  return NULL;
}

/* Atom <link href="..."/> */
static char *atom_link(const char *from, const char *end) {
  for (const char *p = from; p < end; p++) {
    if (strncasecmp(p, "<link", 5) != 0) continue;
    const char *gt = memchr(p, '>', (size_t)(end - p)); if (!gt) return NULL;
    const char *h = NULL;
    for (const char *c = p; c < gt - 4; c++)
      if (strncasecmp(c, "href=", 5) == 0) { h = c + 5; break; }
    if (!h) return NULL;
    char quote = *h; if (quote != '"' && quote != '\'') return NULL;
    const char *e2 = memchr(h + 1, quote, (size_t)(gt - h));
    return e2 ? dup_n(h + 1, (size_t)(e2 - h - 1)) : NULL;
  }
  return NULL;
}

/* Atom's <author> is a container: <author><name>X</name><uri>…</uri></author>.
 * tag_text() returns its raw inner XML, so author was persisted as the literal
 * "<name>/u/AutoModerator</name><uri>…</uri>". If the text still looks like
 * markup, pull the <name> (or <email>) out of it. Returns a fresh string. */
static char *unwrap_person(char *s) {
  if (!s || !strchr(s, '<')) return s;
  const char *end = s + strlen(s);
  char *inner = tag_text(s, end, "name", NULL);
  if (!inner || !*inner) { free(inner); inner = tag_text(s, end, "email", NULL); }
  if (inner && *inner) { free(s); return inner; }
  free(inner);
  return s;
}

/* --- RFC-822 -> ISO-8601 ---------------------------------------------------
 * Feeds date items as "Fri, 31 Jul 2026 16:16:00 +0900"; every other collector
 * writes ISO-8601. They share one `published_at` column that core/intelapi.c,
 * core/exportapi.c and core/simhash.c all ORDER BY as TEXT — and since 'F' > '2',
 * every RSS row sorted ahead of every ISO row regardless of date, with RSS rows
 * ordered among themselves by weekday name. Normalise on the way in.
 * Returns a malloc'd ISO-8601 UTC string, or NULL if the input isn't RFC-822
 * (already-ISO input is returned as a copy). */
static const char *const RFC822_MON[12] = {
  "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

static int tz_offset_minutes(const char *tz) {
  if (!tz || !*tz) return 0;
  if (*tz == '+' || *tz == '-') {
    int sign = (*tz == '-') ? -1 : 1;
    int hh = 0, mm = 0;
    if (sscanf(tz + 1, "%2d%2d", &hh, &mm) >= 1) return sign * (hh * 60 + mm);
    return 0;
  }
  /* the obsolete alphabetic zones RFC 822 still permits */
  if (!strcasecmp(tz, "GMT") || !strcasecmp(tz, "UT") || !strcasecmp(tz, "Z")) return 0;
  if (!strcasecmp(tz, "EDT")) return -4 * 60;
  if (!strcasecmp(tz, "EST") || !strcasecmp(tz, "CDT")) return -5 * 60;
  if (!strcasecmp(tz, "CST") || !strcasecmp(tz, "MDT")) return -6 * 60;
  if (!strcasecmp(tz, "MST") || !strcasecmp(tz, "PDT")) return -7 * 60;
  if (!strcasecmp(tz, "PST")) return -8 * 60;
  if (!strcasecmp(tz, "JST")) return  9 * 60;
  return 0;
}

/* days since 1970-01-01 for a civil date (Howard Hinnant's algorithm) */
static long days_from_civil(int y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

static char *rfc822_to_iso(const char *in) {
  if (!in || !*in) return NULL;
  while (*in == ' ' || *in == '\t') in++;
  /* Already ISO-8601 (starts with 4 digits then '-')? Keep as-is. */
  if (strlen(in) >= 5 && isdigit((unsigned char)in[0]) && isdigit((unsigned char)in[1]) &&
      isdigit((unsigned char)in[2]) && isdigit((unsigned char)in[3]) && in[4] == '-')
    return strdup(in);

  const char *p = in;
  const char *comma = strchr(p, ',');       /* skip the optional "Fri, " */
  if (comma && comma - p <= 4) p = comma + 1;
  while (*p == ' ') p++;

  int day = 0, year = 0, hh = 0, mi = 0, ss = 0;
  char mon[4] = {0}, tz[8] = {0};
  int got = sscanf(p, "%2d %3s %4d %2d:%2d:%2d %7s",
                   &day, mon, &year, &hh, &mi, &ss, tz);
  if (got < 5) {                             /* seconds are optional in RFC 822 */
    ss = 0;
    got = sscanf(p, "%2d %3s %4d %2d:%2d %7s", &day, mon, &year, &hh, &mi, tz);
    if (got < 5) return NULL;
  }
  int m = 0;
  for (int i = 0; i < 12; i++) if (!strcasecmp(mon, RFC822_MON[i])) { m = i + 1; break; }
  if (!m || day < 1 || day > 31 || year < 1000) return NULL;
  if (year < 100) year += (year < 70) ? 2000 : 1900;   /* 2-digit years */

  /* shift to UTC */
  long total = (long)hh * 60 + mi - tz_offset_minutes(tz);
  long dayadj = 0;
  while (total < 0)     { total += 1440; dayadj -= 1; }
  while (total >= 1440) { total -= 1440; dayadj += 1; }

  long days = days_from_civil(year, m, day) + dayadj;
  /* back to a civil date */
  long z = days + 719468;
  long era = (z >= 0 ? z : z - 146096) / 146097;
  unsigned doe = (unsigned)(z - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  long y2 = (long)yoe + era * 400;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  unsigned d2 = doy - (153 * mp + 2) / 5 + 1;
  unsigned m2 = mp + (mp < 10 ? 3 : -9);
  y2 += (m2 <= 2);

  char *out = malloc(32);
  if (!out) return NULL;
  snprintf(out, 32, "%04ld-%02u-%02uT%02ld:%02ld:%02dZ",
           y2, m2, d2, total / 60, total % 60, ss);
  return out;
}

/* --- charset normalisation ------------------------------------------------
 * Feeds still ship legacy encodings with no usable header: folha-br serves
 * ISO-8859-1, so "Fundação" arrives as bytes that are not valid UTF-8 and land
 * in `title` verbatim — the row then fails to decode on every read, which is
 * the DB_ERROR verdict. Fixing the 240-byte truncation stopped us CREATING bad
 * UTF-8; this stops us STORING what upstream already sent. */
static int is_utf8(const char *s, size_t n) {
  const unsigned char *p = (const unsigned char *)s;
  for (size_t i = 0; i < n; ) {
    unsigned char c = p[i];
    size_t need;
    if (c < 0x80) { i++; continue; }
    else if ((c & 0xE0) == 0xC0) need = 1;
    else if ((c & 0xF0) == 0xE0) need = 2;
    else if ((c & 0xF8) == 0xF0) need = 3;
    else return 0;
    if (i + need >= n) return 0;
    for (size_t k = 1; k <= need; k++)
      if ((p[i + k] & 0xC0) != 0x80) return 0;
    i += need + 1;
  }
  return 1;
}

/* Latin-1 → UTF-8. Every byte maps to a code point, so this cannot fail; it is
 * the correct fallback for a Western feed that is not valid UTF-8. */
static char *latin1_to_utf8(const char *s, size_t n, size_t *out_len) {
  char *o = malloc(n * 2 + 1);
  if (!o) return NULL;
  size_t j = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) o[j++] = (char)c;
    else { o[j++] = (char)(0xC0 | (c >> 6)); o[j++] = (char)(0x80 | (c & 0x3F)); }
  }
  o[j] = 0;
  if (out_len) *out_len = j;
  return o;
}

static char *sha1_20(const char *a, const char *b) {
  unsigned char d[20]; SHA_CTX c; SHA1_Init(&c);
  if (a) { SHA1_Update(&c, a, strlen(a)); SHA1_Update(&c, "|", 1); }
  if (b) { SHA1_Update(&c, b, strlen(b)); SHA1_Update(&c, "|", 1); }
  SHA1_Final(d, &c);
  char *h = malloc(41);
  for (int i = 0; i < 20; i++) sprintf(h + i*2, "%02x", d[i]);
  h[40] = 0; h[20] = 0;            /* intelHashKey slices to 20 hex chars */
  return h;
}

int rss_collect(const source_ctx *ctx, intel_sink *sink,
                const char *url, const char *lang, const char *tags_json) {
  http_response r = {0};
  /* A bare token identifies the bot but gives a publisher nothing to contact,
   * so a descriptive UA with a contact URL is the right default, and spoofing
   * a browser to evade a publisher's block is not something we do.
   *
   * CORRECTION (2026-08-09, measured). This comment used to claim the contact
   * URL was "the standard remedy" for ReliefWeb's 406. It is not, and for
   * ReliefWeb it actively makes things worse. Measured against
   * https://reliefweb.int/updates/rss.xml, one request each, spaced:
   *
   *   curl/8.5.0                                    -> 200
   *   feedbot/1.0                                   -> 200
   *   JapanOSINT/1.0                                -> 406
   *   JapanOSINT/1.0 (contact: repo issues)         -> 406
   *   JapanOSINT/1.0 +https://example.org           -> 406
   *   feedbot/1.0 (+https://github.com/RCorp/OSINTsaas) -> 406
   *
   * The discriminator is the substring "osint", not the shape of the UA: the
   * last line carries no "japanosint" token and is still refused, purely
   * because the contact URL names the repo. So the remedy as written adds a
   * SECOND matching token rather than removing one.
   *
   * This is left as-is deliberately. Honest identification is the policy, and
   * the alternatives are to drop the identifying token (defeating the point of
   * the UA) or to spoof (refused). ~10 humanitarian feeds — reliefweb.int,
   * unocha.org, data.humdata.org — are unreachable as a result and are
   * recorded as such rather than quietly worked around. Asking those
   * publishers for an allowlist entry is the real fix. */
  const char *hdrs[] = { "Accept: application/rss+xml,*/*",
                         "User-Agent: JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; "
                         "feed collector; contact via repo issues)", NULL };
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 8000, 2, &r);
  if (rc != 0 || r.status < 200 || r.status >= 300 || !r.body) {
    http_response_free(&r); return -1;
  }
  /* Normalise the body to UTF-8 before a single field is extracted, so no
   * downstream copy can carry undecodable bytes into the DB or its FTS mirror. */
  char *conv = NULL;
  size_t body_len = r.body_len;
  if (r.body && body_len && !is_utf8(r.body, body_len)) {
    size_t cl = 0;
    conv = latin1_to_utf8(r.body, body_len, &cl);
    if (conv) {
      fprintf(stderr, "[rss] %s body was not UTF-8; transcoded from Latin-1 "
                      "(%zu -> %zu bytes)\n", ctx->source_id, body_len, cl);
      body_len = cl;
    }
  }
  const char *xml = conv ? conv : r.body;
  const char *xend = xml + body_len;
  int n = 0;
  const char *cur = xml;
  /* Some feeds serve their whole archive rather than a window — msrc-blog ships
   * 4,995 items (3,539 rows, 10 s) on every poll, which is a duration_outlier
   * every hour for data that changed by a handful of entries. Cap the per-run
   * item count; feeds are newest-first, so this keeps the fresh end. */
  const char *cap_env = getenv("JO_RSS_MAX_ITEMS");
  int max_items = cap_env ? atoi(cap_env) : 500;
  if (max_items <= 0) max_items = 500;
  for (;;) {
    if (n >= max_items) {
      fprintf(stderr, "[rss] %s capped at %d items\n", ctx->source_id, max_items);
      break;
    }
    /* next <item ...>/<entry ...> block. Require a delimiter after the name
     * so the RDF <items> table-of-contents (Seq) is NOT matched. */
    const char *open = NULL; int atom = 0;
    for (const char *p = cur; (p = strchr(p, '<')) && p < xend; p++) {
      int isi = (strncasecmp(p, "<item", 5) == 0);
      int ise = (strncasecmp(p, "<entry", 6) == 0);
      if (!isi && !ise) continue;
      char d = p[isi ? 5 : 6];
      if (d == ' ' || d == '>' || d == '\t' || d == '\n' ||
          d == '\r' || d == '/') { open = p; atom = ise; break; }
    }
    if (!open || open >= xend) break;
    const char *it = NULL; size_t itlen = 0; const char *blkend = NULL;
    const char *closeTag = atom ? "</entry>" : "</item>";
    const char *cl = strcasestr(open, closeTag);
    if (!cl) break;
    it = open; blkend = cl; itlen = (size_t)(blkend - it);
    const char *a;
    char *title = tag_text(it, it + itlen, "title", &a);
    char *desc  = tag_text(it, it + itlen, atom ? "summary" : "description", &a);
    if (!desc) desc = tag_text(it, it + itlen, "content", &a);
    char *link  = atom ? atom_link(it, it + itlen)
                       : tag_text(it, it + itlen, "link", &a);
    char *pub   = tag_text(it, it + itlen, "pubDate", &a);
    if (!pub) pub = tag_text(it, it + itlen, "date", &a);     /* dc:date */
    if (!pub) pub = tag_text(it, it + itlen, "published", &a);
    if (!pub) pub = tag_text(it, it + itlen, "updated", &a);
    char *guid  = tag_text(it, it + itlen, "guid", &a);
    if (!guid) guid = tag_text(it, it + itlen, "id", &a);
    char *author= tag_text(it, it + itlen, "author", &a);
    author = unwrap_person(author);   /* Atom <author><name>…</name></author> */

    /* uid precedence: guid → link → sha1(title|pubDate) (== intelUid) */
    char *rk = NULL;
    if (guid && *guid) rk = strdup(guid);
    else if (link && *link) rk = strdup(link);
    else if (title && *title) rk = sha1_20(title, pub);
    if (rk) {
      /* Heap, not char[512]: Google News guids reach ~1.9 KB, and snprintf
       * truncation produced syntactically invalid JSON (cut mid-base64, no
       * closing quote/brace) that no consumer could cJSON_Parse. */
      char *props = NULL;
      if (guid && *guid) {
        cJSON *pj = cJSON_CreateObject();
        cJSON_AddStringToObject(pj, "guid", guid);
        props = cJSON_PrintUnformatted(pj);
        cJSON_Delete(pj);
      }
      /* Truncate on a CHARACTER boundary. The old strncpy(summ, desc, 240) cut
       * at 240 BYTES, splitting multi-byte UTF-8 and persisting invalid text
       * into `summary` and its FTS mirror — every non-Latin feed eventually
       * tripped "Could not decode to UTF-8" on read. */
      char summ[256] = {0};
      if (desc) {
        size_t L = strlen(desc);
        if (L > 240) {
          L = 240;
          while (L > 0 && ((unsigned char)desc[L] & 0xC0) == 0x80) L--;
        }
        memcpy(summ, desc, L); summ[L] = 0;
      }
      char *pub_iso = pub ? rfc822_to_iso(pub) : NULL;
      intel_item item = {0};
      item.remote_key = rk;
      item.title = title; item.body = desc;
      item.summary = desc ? summ : NULL;
      item.link = link; item.author = author;
      item.lang = lang ? lang : "ja";
      /* normalised to ISO-8601 UTC so this column sorts as one format */
      item.published_at = pub_iso ? pub_iso : pub;
      item.record_type = "article";
      item.properties_json = props ? props : "{}";
      item.tags_json = tags_json;
      if (sink->emit(sink, &item) >= 0) n++;
      free(pub_iso);
      free(props);
      free(rk);
    }
    free(title); free(desc); free(link); free(pub); free(guid); free(author);
    cur = blkend + strlen(closeTag);
  }
  http_response_free(&r);
  free(conv);
  fprintf(stderr, "[rss] %s emitted %d\n", ctx->source_id, n);
  return n;
}
