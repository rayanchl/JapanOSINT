/* core/feeddisc.c — see feeddisc.h. */
#include "core/feeddisc.h"
#include "core/httpclient.h"
#include "lib/hpengine.h"
#include "lib/htmlparse.h"
#include "lib/jocore.h"
#include "lib/seenset.h"
#include "source.h"

#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── pure parsers ───────────────────────────────────────────────────────── */

static void decode_amp(char *s) {
  char *w = s;
  for (const char *r = s; *r;) {
    if (!strncmp(r, "&amp;", 5)) { *w++ = '&'; r += 5; }
    else if (!strncmp(r, "&#38;", 5)) { *w++ = '&'; r += 5; }
    else *w++ = *r++;
  }
  *w = 0;
}

/* scheme://host of `url` into out (no trailing slash). */
static int url_origin(const char *url, char *out, size_t n) {
  const char *p = strstr(url, "://");
  if (!p) return 0;
  const char *h = p + 3;
  size_t len = (size_t)(h - url) + strcspn(h, "/?#");
  if (len + 1 > n) return 0;
  memcpy(out, url, len); out[len] = 0;
  return 1;
}

/* RFC 3986 5.2.4, on the path part of an absolute URL, in place: "/./" -> "/",
 * "/x/../" -> "/". "./rss/rss.xml" otherwise registers as a second URL. */
static void remove_dot_segments(char *url, size_t n) {
  char *p = strstr(url, "://");
  if (!p) return;
  p = strchr(p + 3, '/');
  if (!p) return;
  char *q = p + strcspn(p, "?#");
  char tail[2048];
  snprintf(tail, sizeof tail, "%s", q);
  *q = 0;
  char out[2048] = "";
  size_t ol = 0;
  for (char *seg = p; *seg;) {
    if (!strncmp(seg, "/./", 3)) { seg += 2; continue; }
    if (!strcmp(seg, "/.")) { seg[1] = 0; continue; }
    if (!strncmp(seg, "/../", 4) || !strcmp(seg, "/..")) {
      while (ol > 0 && out[ol - 1] != '/') ol--;
      if (ol > 0) ol--;
      out[ol] = 0;
      seg += 3;
      if (!*seg) { out[ol++] = '/'; out[ol] = 0; }
      continue;
    }
    char *nx = strchr(seg + 1, '/');
    size_t l = nx ? (size_t)(nx - seg) : strlen(seg);
    if (ol + l + 1 >= sizeof out) break;
    memcpy(out + ol, seg, l); ol += l; out[ol] = 0;
    seg += l;
  }
  snprintf(p, n - (size_t)(p - url), "%s%s", out[0] ? out : "/", tail);
}

static void resolve_raw(const char *base, const char *href, char *out, size_t n);
static void resolve(const char *base, const char *href, char *out, size_t n) {
  resolve_raw(base, href, out, n);
  if (out[0]) remove_dot_segments(out, n);
}

/* A URL that does not fit is not a URL: clear it rather than keep a prefix. */
#define FIT(expr) do { int w_ = (expr); if (w_ < 0 || (size_t)w_ >= n) out[0] = 0; } while (0)
static void resolve_raw(const char *base, const char *href, char *out, size_t n) {
  out[0] = 0;
  if (!strncasecmp(href, "http://", 7) || !strncasecmp(href, "https://", 8)) {
    FIT(snprintf(out, n, "%s", href)); return;
  }
  if (!strncmp(href, "//", 2)) {
    const char *sch = strncasecmp(base, "http://", 7) ? "https:" : "http:";
    FIT(snprintf(out, n, "%s%s", sch, href)); return;
  }
  char origin[1024];
  if (!url_origin(base, origin, sizeof origin)) return;
  if (href[0] == '/') { FIT(snprintf(out, n, "%s%s", origin, href)); return; }
  if (href[0] == '?') {
    size_t bl = strcspn(base, "?#");
    FIT(snprintf(out, n, "%.*s%s", (int)bl, base, href)); return;
  }
  /* relative to the base's directory */
  const char *path = base + strlen(origin);
  size_t pl = strcspn(path, "?#");
  const char *slash = NULL;
  for (size_t i = 0; i < pl; i++) if (path[i] == '/') slash = path + i;
  int dl = slash ? (int)(slash - path) + 1 : 0;
  FIT(snprintf(out, n, "%s%.*s%s%s", origin, dl, path, dl ? "" : "/", href));
}
#undef FIT

int feeddisc_links(const char *html, const char *page_url, feeddisc_link_cb cb, void *ud) {
  if (!html || !page_url) return 0;
  char base[1024];
  snprintf(base, sizeof base, "%s", page_url);
  const char *b = jo_strcasestr(html, "<base ");
  if (b) {
    char bh[1024];
    if (html_attr(b, "href", bh, sizeof bh) && (!strncasecmp(bh, "http", 4)))
      snprintf(base, sizeof base, "%s", bh);
  }
  int n = 0;
  for (const char *p = html; (p = jo_strcasestr(p, "<link")); p += 5) {
    char d = p[5];
    if (d != ' ' && d != '\t' && d != '\n' && d != '\r' && d != '/') continue;
    const char *end = strchr(p, '>');
    if (!end) break;
    size_t tl = (size_t)(end - p) + 1;
    char *tag = malloc(tl + 1);
    if (!tag) break;
    memcpy(tag, p, tl); tag[tl] = 0;
    char rel[128] = "", type[128] = "", href[2048] = "", title[512] = "";
    html_attr(tag, "rel", rel, sizeof rel);
    html_attr(tag, "type", type, sizeof type);
    html_attr(tag, "href", href, sizeof href);
    html_attr(tag, "title", title, sizeof title);
    free(tag);
    if (!jo_strcasestr(rel, "alternate")) continue;
    if (!jo_strcasestr(type, "rss+xml") && !jo_strcasestr(type, "atom+xml") &&
        !jo_strcasestr(type, "rdf+xml")) continue;
    decode_amp(href);
    char *h = href;
    while (isspace((unsigned char)*h)) h++;
    if (!*h) continue;
    if (jo_strcasestr(h, "comments") || jo_strcasestr(h, "/feed/atom")) continue;
    char abs[2048];
    resolve(base, h, abs, sizeof abs);
    if (!abs[0]) continue;
    decode_amp(title);
    cb(abs, title, ud);
    n++;
  }
  return n;
}

static int elem_count(const char *body, const char *name) {
  int c = 0;
  size_t nl = strlen(name);
  for (const char *p = body; (p = jo_strcasestr(p, name)); p += nl) {
    char d = p[nl];
    if (d == ' ' || d == '>' || d == '\t' || d == '\n' || d == '\r' || d == '/') c++;
  }
  return c;
}

int feeddisc_count_items(const char *body) {
  return body ? elem_count(body, "<item") + elem_count(body, "<entry") : 0;
}

int feeddisc_is_feed(const char *body) {
  if (!body) return 0;
  char head[4097];
  snprintf(head, sizeof head, "%s", body);
  return jo_strcasestr(head, "<rss") || jo_strcasestr(head, "<feed") ||
         jo_strcasestr(head, "<rdf:RDF") != NULL;
}

/* Phrases that do not occur in a public body's own feed. */
static const char *const STRONG[] = {
  "nhà cái", "game bài", "đổi thưởng", "quay hũ", "nổ hũ", "xổ số", "cá cược",
  "bắn cá", "soi kèo", "slot gacor", "gacor", "maxwin", "togel", "situs judi",
  "situs slot", "judi slot", "judi online", "sbobet", "8xbet", "sunwin", "new88",
  "online casino", "casino online", "crypto casino", "bitcoin casino",
  "no deposit bonus", "no-deposit bonus", "free spins", "pokies", "สล็อต",
  "บาคาร่า", "viagra", "cialis", "kamagra", "payday loan", "essay writing service",
  NULL };
/* Generic words: legitimate in a lottery regulator's feed, so several are needed. */
static const char *const WEAK[] = {
  "casino", "casinos", "slot", "slots", "betting", "jackpot", "poker",
  "baccarat", "roulette", "sportsbook", NULL };

static int is_word_char(unsigned char c) { return isalnum(c) || c >= 0x80; }

/* whole-word, case-insensitive occurrences of `w` in `s` */
static int word_hits(const char *s, const char *w) {
  int c = 0;
  size_t wl = strlen(w);
  for (const char *p = s; (p = jo_strcasestr(p, w)); p += wl) {
    int left = (p == s) || !is_word_char((unsigned char)p[-1]);
    int right = !is_word_char((unsigned char)p[wl]);
    if (left && right) c++;
  }
  return c;
}

/* Vietnamese-only letters: not the â/ê/ô/ă French and Romanian use, nor the đ
 * of Serbian, Croatian and Bosnian (Blic.rs and a Bosnian ministry both read as
 * "Vietnamese" when it counted). */
static int vietnamese_letters(const char *s) {
  static const char *const V[] = {
    "ơ","ư","ạ","ả","ấ","ầ","ẩ","ẫ","ậ","ắ","ằ","ẳ","ẵ","ặ","ẹ","ẻ","ẽ","ế",
    "ề","ể","ễ","ệ","ỉ","ị","ọ","ỏ","ố","ồ","ổ","ỗ","ộ","ớ","ờ","ở","ỡ","ợ","ụ",
    "ủ","ứ","ừ","ử","ữ","ự","ỳ","ỵ","ỷ","ỹ", NULL };
  int c = 0;
  for (int i = 0; V[i]; i++)
    for (const char *p = s; (p = strstr(p, V[i])); p += strlen(V[i])) c++;
  return c;
}

/* The titles of the first 15 items, joined; the channel's own title is skipped. */
static char *item_titles(const char *body) {
  size_t cap = 8192, len = 0;
  char *out = malloc(cap);
  if (!out) return NULL;
  out[0] = 0;
  int k = 0;
  for (const char *p = body; (p = jo_strcasestr(p, "<title")) && k < 16; p += 6) {
    const char *gt = strchr(p, '>');
    if (!gt) break;
    const char *close = jo_strcasestr(gt, "</title>");
    if (!close) break;
    if (k++ == 0) { p = close; continue; }
    size_t tl = (size_t)(close - gt - 1);
    if (tl > 600) tl = 600;
    if (len + tl + 4 >= cap) break;
    memcpy(out + len, gt + 1, tl); len += tl;
    memcpy(out + len, " | ", 3); len += 3;
    out[len] = 0;
    p = close;
  }
  return out;
}

/* A Vietnamese phrase: it carries non-ASCII letters, and they are not Thai. */
static int vietnamese_phrase(const char *s) {
  int nonascii = 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) if (*p >= 0x80) nonascii = 1;
  return nonascii && !strstr(s, "\xe0\xb8") && !strstr(s, "\xe0\xb9");
}

int feeddisc_takeover_suspect(const char *body, int vi_ok, char *why, size_t why_n) {
  if (why && why_n) why[0] = 0;
  if (!body) return 0;
  char *t = item_titles(body);
  if (!t) return 0;
  int hit = 0;
  for (int i = 0; STRONG[i] && !hit; i++) {
    int multi = strchr(STRONG[i], ' ') != NULL || (unsigned char)STRONG[i][0] >= 0x80;
    /* On a Vietnamese source the Vietnamese phrases are ordinary news words:
     * Tuoi Tre and VTC publish lottery results (xổ số) every day. */
    if (vi_ok && vietnamese_phrase(STRONG[i])) continue;
    int found = multi ? jo_strcasestr(t, STRONG[i]) != NULL : word_hits(t, STRONG[i]) > 0;
    if (found) { hit = 1; if (why) snprintf(why, why_n, "phrase \"%s\"", STRONG[i]); }
  }
  if (!hit) {
    int weak = 0;
    for (int i = 0; WEAK[i]; i++) weak += word_hits(t, WEAK[i]);
    if (weak >= 3) { hit = 1; if (why) snprintf(why, why_n, "%d gambling words", weak); }
  }
  if (!hit && !vi_ok) {
    int v = vietnamese_letters(t);
    if (v >= 8) { hit = 1; if (why) snprintf(why, why_n, "%d Vietnamese letters", v); }
  }
  free(t);
  return hit;
}

/* ── the CLI mode ───────────────────────────────────────────────────────── */

typedef struct {
  char **sites; int nsites;
  int next;
  seen_set feeds;        /* declared feed URLs already handled this run */
  seen_set registered;   /* source_def.url of every registered source */
  pthread_mutex_t mu;
  int n_sites_ok, n_declared, n_pass, n_flag, n_reg, n_other;
} disc_run;

static void norm_url(const char *u, char *out, size_t n) {
  const char *p = strstr(u, "://");
  p = p ? p + 3 : u;
  if (!strncasecmp(p, "www.", 4)) p += 4;
  size_t i = 0;
  for (; *p && i + 1 < n; p++) out[i++] = (char)tolower((unsigned char)*p);
  while (i > 0 && out[i - 1] == '/') i--;
  out[i] = 0;
}

static void tsv_field(const char *s) {
  for (; s && *s; s++) putchar(*s == '\t' || *s == '\n' || *s == '\r' ? ' ' : *s);
}

typedef struct { disc_run *run; http_client *hc; const char *site; char page_title[512]; } disc_ctx;

static void on_link(const char *abs_url, const char *title, void *ud) {
  disc_ctx *c = ud;
  disc_run *R = c->run;
  char key[2048];
  norm_url(abs_url, key, sizeof key);
  pthread_mutex_lock(&R->mu);
  int fresh = seen_add(&R->feeds, key);
  int known = seen_has(&R->registered, key);
  if (fresh) R->n_declared++;
  pthread_mutex_unlock(&R->mu);
  if (!fresh) return;

  const char *verdict = "REGISTERED";
  int items = 0;
  char why[160] = "";
  if (!known) {
    const char *hdrs[] = { "Accept: application/rss+xml,application/atom+xml,*/*", NULL };
    http_response r = {0};
    int rc = http_request(c->hc, "GET", abs_url, hdrs, NULL, 0, 25000, 1, &r);
    if (rc != 0) verdict = "TRANSPORT";
    else if (r.status != 200) verdict = "HTTP_ERROR";
    else if (!r.body || !r.body_len) verdict = "EMPTY_BODY";
    else if (!feeddisc_is_feed(r.body)) verdict = "NOT_FEED";
    else if (!(items = feeddisc_count_items(r.body))) verdict = "EMPTY_FEED";
    else {
      char host[512] = "";
      url_origin(abs_url, host, sizeof host);
      int vi_ok = jo_strcasestr(host, ".vn") != NULL || jo_strcasestr(c->site, ".vn") != NULL;
      verdict = feeddisc_takeover_suspect(r.body, vi_ok, why, sizeof why) ? "TAKEOVER_SUSPECT" : "PASS";
    }
    if (!strcmp(verdict, "HTTP_ERROR")) snprintf(why, sizeof why, "HTTP %ld", r.status);
    http_response_free(&r);
  }

  pthread_mutex_lock(&R->mu);
  if (!strcmp(verdict, "PASS")) R->n_pass++;
  else if (!strcmp(verdict, "TAKEOVER_SUSPECT")) R->n_flag++;
  else if (!strcmp(verdict, "REGISTERED")) R->n_reg++;
  else R->n_other++;
  tsv_field(c->site); putchar('\t');
  tsv_field(abs_url); putchar('\t');
  tsv_field(title); putchar('\t');
  tsv_field(c->page_title); putchar('\t');
  printf("%s\t%d\t", verdict, items);
  tsv_field(why); putchar('\n');
  fflush(stdout);
  pthread_mutex_unlock(&R->mu);
}

static void *worker(void *arg) {
  disc_run *R = arg;
  http_client *hc = http_client_new();
  for (;;) {
    pthread_mutex_lock(&R->mu);
    int i = R->next < R->nsites ? R->next++ : -1;
    pthread_mutex_unlock(&R->mu);
    if (i < 0) break;
    disc_ctx c = { .run = R, .hc = hc, .site = R->sites[i] };
    const char *hdrs[] = { "Accept: text/html,application/xhtml+xml,*/*", NULL };
    http_response r = {0};
    if (http_request(hc, "GET", c.site, hdrs, NULL, 0, 20000, 1, &r) == 0 &&
        r.status == 200 && r.body) {
      const char *tb = NULL; int tl = 0;
      if (html_block(r.body, "title", &tb, &tl) && tb && tl > 0) {
        char raw[512];
        snprintf(raw, sizeof raw, "%.*s", tl > 500 ? 500 : tl, tb);
        char *s = html_strip(raw);
        snprintf(c.page_title, sizeof c.page_title, "%s", s ? s : raw);
        free(s);
      }
      pthread_mutex_lock(&R->mu); R->n_sites_ok++; pthread_mutex_unlock(&R->mu);
      feeddisc_links(r.body, c.site, on_link, &c);
    }
    http_response_free(&r);
  }
  http_client_free(hc);
  return NULL;
}

int feeddisc_main(const char *path, int jobs) {
  FILE *f = fopen(path, "r");
  if (!f) { perror(path); return 2; }
  disc_run R = {0};
  pthread_mutex_init(&R.mu, NULL);
  int cap = 0;
  char line[2048];
  seen_set sites_seen = {0};
  while (fgets(line, sizeof line, f)) {
    char *s = line;
    while (isspace((unsigned char)*s)) s++;
    s[strcspn(s, "\t\r\n ")] = 0;
    if (!*s || *s == '#') continue;
    char url[2100];
    if (strstr(s, "://")) snprintf(url, sizeof url, "%s", s);
    else snprintf(url, sizeof url, "https://%s/", s);
    char key[2100];
    norm_url(url, key, sizeof key);
    if (!seen_add(&sites_seen, key)) continue;
    if (R.nsites == cap) {
      cap = cap ? cap * 2 : 1024;
      char **nv = realloc(R.sites, (size_t)cap * sizeof *nv);
      if (!nv) break;
      R.sites = nv;
    }
    R.sites[R.nsites++] = strdup(url);
  }
  fclose(f);
  seen_free(&sites_seen);

  const source_def **all = registry_all();
  /* source_def.url is the human-facing portal for an hpengine row; the URL it
   * actually fetches is the row's own .url, so both are "registered". */
  for (int k = 0; k < registry_count(); k++) {
    char key[2048];
    if (all[k]->url) { norm_url(all[k]->url, key, sizeof key); seen_add(&R.registered, key); }
    const hp_source *hs = hp_source_get(all[k]->id);
    if (hs && hs->url && !strchr(hs->url, '{')) {
      norm_url(hs->url, key, sizeof key);
      seen_add(&R.registered, key);
    }
  }

  if (jobs < 1) jobs = 1;
  if (jobs > 128) jobs = 128;
  fprintf(stderr, "[discover] %d sites, %d workers; %d registered source URLs known\n",
          R.nsites, jobs, R.registered.n);
  printf("site\tfeed_url\tfeed_title\tpage_title\tverdict\titems\tevidence\n");
  pthread_t *t = calloc((size_t)jobs, sizeof *t);
  for (int i = 0; i < jobs; i++) pthread_create(&t[i], NULL, worker, &R);
  for (int i = 0; i < jobs; i++) pthread_join(t[i], NULL);
  free(t);

  fprintf(stderr, "[discover] %d of %d homepages answered; %d feeds declared: %d PASS, "
          "%d TAKEOVER_SUSPECT (read them), %d already registered, %d other\n",
          R.n_sites_ok, R.nsites, R.n_declared, R.n_pass, R.n_flag, R.n_reg, R.n_other);
  for (int i = 0; i < R.nsites; i++) free(R.sites[i]);
  free(R.sites);
  seen_free(&R.feeds);
  seen_free(&R.registered);
  pthread_mutex_destroy(&R.mu);
  return 0;
}
