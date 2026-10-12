/* core/source_card.c — see source_card.h for why one function builds every
 * source's card. */
#include "source_card.h"
#include "service_aliases.h"
#include "source_registry.h"
#include "../lib/vecindex.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A sentence shared verbatim by at least this many sources' descriptions is
 * template boilerplate (source_card.h). Measured 2026-10-12 over the 18,883
 * registered sources: 97 distinct sentences reach 25, and they are generator
 * prose and lab notes — "Second hop: the record behind each list hit is
 * fetched …" (336 rows), "UNPROBED" (139), "Fetched during discovery and
 * again at manifest time with the same item count" (127), "Shift_JIS" (66),
 * "Pagination follows links.next" (56). Each says the same thing about every
 * row that carries it, so it can only pull those rows towards each other. */
#define SC_FREQ_MIN 25

/* ---- kind ---------------------------------------------------------------- */

source_kind source_kind_of(const source_def *d) {
  if (!d) return SC_KIND_MANUAL;
  if (d->collector && d->collector[0] == '_') return SC_KIND_POD;
  /* The same rule as osint_dispatch.c's is_entity_pivot and service_vec.c's
   * old svec_is_pivot: if it moves, all of them move together. */
  if (d->collector && !strcmp(d->collector, "osint") && d->update_interval_sec == 0)
    return SC_KIND_PIVOT;
  if (d->update_interval_sec > 0) return SC_KIND_SCHEDULED;
  return SC_KIND_MANUAL;
}

static const char *const KIND_NAMES[SC_NKINDS] = {
  "pivot", "scheduled", "pod", "manual" };

const char *source_kind_name(source_kind k) {
  return (k >= 0 && k < SC_NKINDS) ? KIND_NAMES[k] : "manual";
}

int source_kind_parse(const char *s) {
  for (int k = 0; s && k < SC_NKINDS; k++)
    if (!strcmp(s, KIND_NAMES[k])) return k;
  return -1;
}

/* ---- noise --------------------------------------------------------------- */

/* Phrases that mark a statement about the day a row was authored or probed.
 * Matched case-insensitively. Deliberately specific: "measured 20" and not
 * "measured", because "traffic volumes measured by image recognition" is a
 * description and "MEASURED 2026-10-05 over …" is a lab note. "as of
 * 2025-04-01" is NOT here: on this registry it is a dataset's own snapshot
 * date ("fire hydrants (as of 2025-04-01)"), which is what the data is. */
static const char *const NOISE[] = {
  "probe time", "at probe", "at authoring", "authoring time", "when probed",
  "probed on", "probed 20", "measured 20", "measured on 20", "verified 20",
  "verified on 20", NULL };

static const char *ci_find(const char *s, size_t n, const char *needle) {
  size_t m = strlen(needle);
  for (size_t i = 0; i + m <= n; i++) {
    size_t j = 0;
    while (j < m && tolower((unsigned char)s[i + j]) == needle[j]) j++;
    if (j == m) return s + i;
  }
  return NULL;
}

/* Offset of the first noise marker in s[0..n), or -1. */
static long noise_at(const char *s, size_t n) {
  long best = -1;
  for (int i = 0; NOISE[i]; i++) {
    const char *p = ci_find(s, n, NOISE[i]);
    if (p && (best < 0 || p - s < best)) best = (long)(p - s);
  }
  return best;
}

/* A marker this close to the start of a sentence makes the whole sentence a
 * lab note ("At probe time (2026-09-14) returnCountOnly reported …",
 * "Transport note, MEASURED 2026-10-06: …"); dropping one clause of it would
 * leave a fragment that reads as a claim. */
#define SC_LEAD_NOTE 24

typedef struct { const char *s; size_t n; int sep, joiner, gone; } sc_tok;

/* Separator at p? Returns its byte length (0 = none); *joiner says whether it
 * joins clauses (removable with one) or is structural (a paren). */
static size_t sep_at(const char *p, int *joiner) {
  unsigned char c = (unsigned char)*p;
  /* An ASCII joiner only when a space follows: "2,421", "https://" and
   * "10:30" are one token, not two clauses. */
  if ((c == ',' || c == ';' || c == ':') &&
      (p[1] == 0 || isspace((unsigned char)p[1]))) { *joiner = 1; return 1; }
  if (c == '(' || c == ')') { *joiner = 0; return 1; }
  if (c == 0xE3 && (unsigned char)p[1] == 0x80 && (unsigned char)p[2] == 0x81) {
    *joiner = 1; return 3;                         /* 、 */
  }
  if (c == 0xEF && (unsigned char)p[1] == 0xBC) {
    unsigned char d = (unsigned char)p[2];
    if (d == 0x9B) { *joiner = 1; return 3; }      /* ； */
    if (d == 0x88 || d == 0x89) { *joiner = 0; return 3; }   /* （ ） */
  }
  return 0;
}

static int has_word(const char *s, size_t n) {
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (isalnum(c) || c >= 0x80) return 1;
  }
  return 0;
}

/* Append s[0..n) to out, with every run of whitespace collapsed to one space
 * and the small artefacts clause removal leaves ("()", " ,", "( ")
 * tidied. Returns the new length. */
static size_t tidy_append(char *out, size_t len, const char *s, size_t n) {
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (isspace((unsigned char)c)) {
      if (len && out[len - 1] != ' ' && out[len - 1] != '(') out[len++] = ' ';
      continue;
    }
    if ((c == ',' || c == ';' || c == ':' || c == ')') && len && out[len - 1] == ' ')
      len--;
    if (c == ')' && len && out[len - 1] == '(') { len--; continue; }   /* "()" */
    if ((c == ',' || c == ';' || c == ':') && (len == 0 || out[len - 1] == '('))
      continue;                                    /* a leading joiner */
    out[len++] = c;
  }
  return len;
}

/* Clean one sentence s[0..n) into out (capacity >= n + 1). Returns the
 * cleaned length; 0 = nothing worth keeping. */
static size_t clean_sentence(const char *s, size_t n, char *out) {
  long at = noise_at(s, n);
  if (at < 0) {
    size_t len = tidy_append(out, 0, s, n);
    while (len && out[len - 1] == ' ') len--;
    out[len] = 0;
    return has_word(out, len) ? len : 0;
  }
  {
    size_t lead = 0;
    while (lead < n && isspace((unsigned char)s[lead])) lead++;
    if ((size_t)at < lead + SC_LEAD_NOTE) { out[0] = 0; return 0; }
  }
  /* Tokenise into clauses and separators. */
  sc_tok tk[96];
  int nt = 0;
  size_t i = 0;
  while (i < n && nt < 95) {
    int joiner = 0;
    size_t sl = sep_at(s + i, &joiner);
    if (sl) {
      tk[nt++] = (sc_tok){ s + i, sl, 1, joiner, 0 };
      i += sl;
      continue;
    }
    size_t j = i;
    while (j < n) { int jj; if (sep_at(s + j, &jj)) break; j++; }
    tk[nt++] = (sc_tok){ s + i, j - i, 0, 0, 0 };
    i = j;
  }
  if (i < n) tk[nt++] = (sc_tok){ s + i, n - i, 0, 0, 0 };   /* overflow: verbatim */
  for (int t = 0; t < nt; t++) {
    if (tk[t].sep || noise_at(tk[t].s, tk[t].n) < 0) continue;
    tk[t].gone = 1;
    /* One joiner goes with it: the one before, else the one after. */
    if (t > 0 && tk[t - 1].sep && tk[t - 1].joiner && !tk[t - 1].gone)
      tk[t - 1].gone = 1;
    else if (t + 1 < nt && tk[t + 1].sep && tk[t + 1].joiner)
      tk[t + 1].gone = 1;
  }
  size_t len = 0;
  for (int t = 0; t < nt; t++)
    if (!tk[t].gone) len = tidy_append(out, len, tk[t].s, tk[t].n);
  while (len && (out[len - 1] == ' ' || out[len - 1] == ',' ||
                 out[len - 1] == ';' || out[len - 1] == ':')) len--;
  out[len] = 0;
  return has_word(out, len) ? len : 0;
}

/* One sentence of a description: [s, s+n) plus the terminator it ended with
 * ("" at the end of the text). */
typedef struct { const char *s; size_t n; const char *term; } sc_sent;

static int abbrev_before(const char *start, const char *dot) {
  /* "e.g." "i.e." "etc." "vs." "U.S." "No." — the dot is not an end. */
  const char *w = dot;
  while (w > start && !isspace((unsigned char)w[-1])) w--;
  size_t wl = (size_t)(dot - w);
  if (wl == 0) return 0;
  static const char *const AB[] = { "e.g", "i.e", "etc", "vs", "No", "approx",
                                    "incl", "cf", "St", NULL };
  for (int i = 0; AB[i]; i++)
    if (wl == strlen(AB[i]) && !strncmp(w, AB[i], wl)) return 1;
  /* U.S / U.K: single capitals separated by dots */
  if (wl >= 1 && isupper((unsigned char)dot[-1]) &&
      (wl == 1 || dot[-2] == '.')) return 1;
  return 0;
}

/* Split `t` into sentences. Returns the count written into out[0..cap). */
static int split_sentences(const char *t, sc_sent *out, int cap) {
  int n = 0;
  const char *s = t, *p = t;
  while (*p && n < cap) {
    const char *term = NULL;
    size_t tl = 0;
    unsigned char c = (unsigned char)*p;
    if ((c == '.' || c == '!' || c == '?') &&
        (p[1] == 0 || isspace((unsigned char)p[1])) &&
        !(c == '.' && abbrev_before(s, p))) { term = c == '.' ? "." : c == '!' ? "!" : "?"; tl = 1; }
    else if (c == 0xE3 && (unsigned char)p[1] == 0x80 && (unsigned char)p[2] == 0x82) {
      term = "\xE3\x80\x82"; tl = 3;               /* 。 */
    } else if (c == '\n') { term = ""; tl = 1; }
    if (term) {
      out[n++] = (sc_sent){ s, (size_t)(p - s), term };
      p += tl;
      while (*p && isspace((unsigned char)*p)) p++;
      s = p;
      continue;
    }
    p++;
  }
  if (*s && n < cap) out[n++] = (sc_sent){ s, strlen(s), "" };
  return n;
}

#define SC_MAX_SENT 64

/* Cleaned sentences of `desc`: out[i] malloc'd text, term[i] its terminator.
 * Returns the count. */
static int clean_sentences(const char *desc, char **out, const char **term) {
  if (!desc || !*desc) return 0;
  sc_sent ss[SC_MAX_SENT];
  int ns = split_sentences(desc, ss, SC_MAX_SENT), k = 0;
  for (int i = 0; i < ns; i++) {
    char *buf = malloc(ss[i].n + 1);
    if (!buf) continue;
    size_t len = clean_sentence(ss[i].s, ss[i].n, buf);
    if (!len) { free(buf); continue; }
    /* the same sentence twice in one description says nothing twice */
    int dup = 0;
    for (int j = 0; j < k && !dup; j++) dup = !strcmp(out[j], buf);
    if (dup) { free(buf); continue; }
    out[k] = buf; term[k] = ss[i].term; k++;
  }
  return k;
}

char *source_card_clean(const char *desc) {
  char *sent[SC_MAX_SENT];
  const char *term[SC_MAX_SENT];
  int n = clean_sentences(desc, sent, term);
  size_t cap = 1;
  for (int i = 0; i < n; i++) cap += strlen(sent[i]) + 5;
  char *o = malloc(cap);
  if (!o) { for (int i = 0; i < n; i++) free(sent[i]); return NULL; }
  size_t len = 0;
  o[0] = 0;
  for (int i = 0; i < n; i++) {
    len += (size_t)snprintf(o + len, cap - len, "%s%s%s", len ? " " : "",
                            sent[i], term[i][0] ? term[i] : ".");
    free(sent[i]);
  }
  return o;
}

/* ---- the corpus ---------------------------------------------------------- */

/* Open-addressed count of sentence hashes. */
typedef struct { unsigned long long *k; int *c; size_t cap; } sc_counts;

static int *count_slot(sc_counts *t, unsigned long long h) {
  size_t i = (size_t)(h ^ (h >> 29)) & (t->cap - 1);
  while (t->k[i] && t->k[i] != h) i = (i + 1) & (t->cap - 1);
  t->k[i] = h ? h : 1;
  return &t->c[i];
}

static source_card *g_cards;
static int g_ncards;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;

static void build_cards(void) {
  const source_def **all = registry_all();
  int n = registry_count();
  if (n <= 0) return;
  source_card *cards = calloc((size_t)n, sizeof *cards);
  char ***sents = calloc((size_t)n, sizeof *sents);
  const char ***terms = calloc((size_t)n, sizeof *terms);
  int *nsent = calloc((size_t)n, sizeof *nsent);
  sc_counts cnt = { NULL, NULL, 0 };
  size_t want = 1;
  while (want < (size_t)n * 16) want <<= 1;
  cnt.cap = want;
  cnt.k = calloc(want, sizeof *cnt.k);
  cnt.c = calloc(want, sizeof *cnt.c);
  if (!cards || !sents || !terms || !nsent || !cnt.k || !cnt.c) goto out;

  /* Pass 1: clean every description and count each distinct sentence once
   * per source. */
  for (int i = 0; i < n; i++) {
    const source_def *d = all[i];
    const src_meta *m = d && d->id ? src_meta_get(d->id) : NULL;
    const char *desc = (m && m->description) ? m->description
                       : (d ? d->description : NULL);
    sents[i] = calloc(SC_MAX_SENT, sizeof **sents);
    terms[i] = calloc(SC_MAX_SENT, sizeof **terms);
    if (!sents[i] || !terms[i]) continue;
    nsent[i] = clean_sentences(desc, sents[i], terms[i]);
    for (int j = 0; j < nsent[i]; j++) {
      unsigned long long h = vecindex_hash(sents[i][j]);
      int seen_here = 0;
      for (int q = 0; q < j && !seen_here; q++) seen_here = !strcmp(sents[i][q], sents[i][j]);
      if (!seen_here) (*count_slot(&cnt, h))++;
    }
  }

  /* Pass 2: the card. */
  for (int i = 0; i < n; i++) {
    const source_def *d = all[i];
    if (!d || !d->id) continue;
    const src_meta *m = src_meta_get(d->id);
    const char *nm = (m && m->name && *m->name) ? m->name
                     : (d->name && *d->name ? d->name : NULL);
    const char *alias = service_alias_for(d->id);
    int keep_all = 1;               /* every sentence frequent: keep them all */
    for (int j = 0; j < nsent[i]; j++)
      if (*count_slot(&cnt, vecindex_hash(sents[i][j])) < SC_FREQ_MIN) keep_all = 0;
    size_t cap = strlen(d->id) + (nm ? strlen(nm) : 0) + 32 +
                 (alias ? strlen(alias) + 24 : 0);
    for (int j = 0; j < nsent[i]; j++) cap += strlen(sents[i][j]) + 5;
    char *t = malloc(cap);
    if (!t) continue;
    size_t len = (size_t)snprintf(t, cap, "%s", d->id);
    if (nm && strcmp(nm, d->id)) {
      size_t nl = strlen(nm);
      while (nl && (nm[nl - 1] == '.' || nm[nl - 1] == ' ')) nl--;
      len += (size_t)snprintf(t + len, cap - len, ": %.*s.", (int)nl, nm);
    } else {
      len += (size_t)snprintf(t + len, cap - len, ".");
    }
    for (int j = 0; j < nsent[i]; j++) {
      if (!keep_all && *count_slot(&cnt, vecindex_hash(sents[i][j])) >= SC_FREQ_MIN)
        continue;
      len += (size_t)snprintf(t + len, cap - len, " %s%s", sents[i][j],
                              terms[i][j][0] ? terms[i][j] : ".");
    }
    if (alias) len += (size_t)snprintf(t + len, cap - len, "\nAlso asked as: %s.", alias);
    cards[i].def = d;
    cards[i].kind = source_kind_of(d);
    cards[i].category = (m && m->category) ? m->category
                        : (d->category ? d->category : "investigation");
    cards[i].text = t;
    cards[i].hash = vecindex_hash(t);
  }
  g_cards = cards;
  g_ncards = n;
  cards = NULL;
out:
  for (int i = 0; sents && i < n; i++) {
    for (int j = 0; j < nsent[i]; j++) free(sents[i][j]);
    free(sents[i]);
    if (terms) free((void *)terms[i]);
  }
  free(sents); free((void *)terms); free(nsent);
  free(cnt.k); free(cnt.c);
  free(cards);
}

int source_cards_all(const source_card **out) {
  pthread_once(&g_once, build_cards);
  if (out) *out = g_cards;
  return g_cards ? g_ncards : 0;
}

const source_card *source_card_get(const char *id) {
  const source_card *c = NULL;
  int n = source_cards_all(&c);
  int i = id ? registry_index(id) : -1;
  if (!c || i < 0 || i >= n || !c[i].text) return NULL;
  return &c[i];
}
