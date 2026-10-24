/* core/prompt_rank.c — see prompt_rank.h. */
#include "prompt_rank.h"
#include "qembed.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* An open-addressing table text-hash -> vector, so a record embedded for one
 * prompt view is not sent again for the next view of the same run. */
typedef struct { unsigned long long h; float *v; } pr_slot;

struct prompt_rank {
  char *query;
  int state;                 /* 1 active, 0 off (failed)                   */
  float *q;                  /* query vector, NULL until first use          */
  int dim;
  pr_slot *tab;
  int cap, used;
  long sent;
};

static int rerank_max(void) {
  const char *e = getenv("JO_PROMPT_RERANK_MAX");
  int v = (e && *e) ? atoi(e) : 0;
  return v > 0 ? v : PROMPT_RERANK_MAX_DEFAULT;
}

prompt_rank *prompt_rank_new(const char *query) {
  if (!qembed_enabled() || !query || !*query) return NULL;   /* inert */
  prompt_rank *pr = calloc(1, sizeof *pr);
  if (!pr) return NULL;
  pr->query = strdup(query);
  if (!pr->query) { free(pr); return NULL; }
  pr->state = 1;
  return pr;
}

void prompt_rank_free(prompt_rank *pr) {
  if (!pr) return;
  for (int i = 0; i < pr->cap; i++) free(pr->tab[i].v);
  free(pr->tab);
  free(pr->q);
  free(pr->query);
  free(pr);
}

int prompt_rank_active(const prompt_rank *pr) { return pr && pr->state == 1; }
long prompt_rank_sent(const prompt_rank *pr) { return pr ? pr->sent : 0; }

/* 0 is the empty-slot marker, so a text whose hash IS 0 is stored as 1. */
static unsigned long long key_of(const char *t) {
  unsigned long long h = vecindex_hash(t);
  return h ? h : 1;
}

static pr_slot *tab_find(prompt_rank *pr, unsigned long long h) {
  if (!pr->cap) return NULL;
  for (int i = (int)(h % (unsigned)pr->cap), k = 0; k < pr->cap;
       k++, i = (i + 1) % pr->cap) {
    if (pr->tab[i].h == h) return &pr->tab[i];
    if (pr->tab[i].h == 0) return NULL;
  }
  return NULL;
}

static int tab_put(prompt_rank *pr, unsigned long long h, float *v) {
  if ((pr->used + 1) * 2 > pr->cap) {
    int nc = pr->cap ? pr->cap * 2 : 256;
    pr_slot *nt = calloc((size_t)nc, sizeof *nt);
    if (!nt) return -1;
    for (int i = 0; i < pr->cap; i++) {
      if (!pr->tab[i].h) continue;
      int j = (int)(pr->tab[i].h % (unsigned)nc);
      while (nt[j].h) j = (j + 1) % nc;
      nt[j] = pr->tab[i];
    }
    free(pr->tab);
    pr->tab = nt; pr->cap = nc;
  }
  int j = (int)(h % (unsigned)pr->cap);
  while (pr->tab[j].h && pr->tab[j].h != h) j = (j + 1) % pr->cap;
  if (pr->tab[j].h == h) { free(v); return 0; }
  pr->tab[j].h = h; pr->tab[j].v = v;
  pr->used++;
  return 0;
}

/* Fields a record is described by, in the order they are joined. These are
 * the names the engines emit (hpengine title/summary, VJSON name/description,
 * the corpus lookup's excerpt); anything else falls back to the JSON. */
static const char *const TEXT_KEYS[] = {
  "title", "name", "summary", "description", "excerpt", "snippet", "label",
  "value", "text", "body", NULL
};

char *prompt_rank_record_text(const cJSON *rec) {
  if (cJSON_IsString(rec)) return strdup(rec->valuestring ? rec->valuestring : "");
  size_t cap = 1024, len = 0;
  char *b = malloc(cap);
  if (!b) return NULL;
  b[0] = 0;
  if (cJSON_IsObject(rec)) {
    for (int i = 0; TEXT_KEYS[i]; i++) {
      const cJSON *f = cJSON_GetObjectItem(rec, TEXT_KEYS[i]);
      if (!cJSON_IsString(f) || !f->valuestring[0]) continue;
      size_t fl = strlen(f->valuestring);
      if (len + fl + 2 > cap) {
        while (len + fl + 2 > cap) cap *= 2;
        char *nb = realloc(b, cap);
        if (!nb) { free(b); return NULL; }
        b = nb;
      }
      if (len) b[len++] = ' ';
      memcpy(b + len, f->valuestring, fl + 1);
      len += fl;
    }
  }
  if (len) return b;
  free(b);
  char *j = cJSON_PrintUnformatted(rec);
  return j ? j : strdup("");
}

/* Embed every text in texts[0..n) not already cached. 0 / -1. */
static int ensure_vectors(prompt_rank *pr, char **texts, int n) {
  const char **todo = malloc((size_t)(n + 1) * sizeof *todo);
  unsigned long long *th = malloc((size_t)(n + 1) * sizeof *th);
  if (!todo || !th) { free(todo); free(th); return -1; }
  int m = 0;
  if (!pr->q) { todo[m] = pr->query; th[m] = 0; m++; }
  for (int i = 0; i < n; i++) {
    unsigned long long h = key_of(texts[i]);
    if (tab_find(pr, h)) continue;
    int dup = 0;
    for (int k = 0; k < m && !dup; k++) dup = (th[k] == h);
    if (dup) continue;
    todo[m] = texts[i]; th[m] = h; m++;
  }
  int rc = 0;
  if (m > 0) {
    float *v = NULL; int d = 0; llm_status st;
    if (qembed_texts(todo, m, &v, &d, &st) != 0 || (pr->dim && d != pr->dim)) {
      fprintf(stderr, "[prompt-rank] embedding %d text(s) failed (%s); "
                      "records shown positionally for the rest of this run\n",
              m, llm_status_code(st));
      free(v);
      rc = -1;
    } else {
      pr->sent += m;
      pr->dim = d;
      for (int k = 0; k < m && rc == 0; k++) {
        float *one = malloc((size_t)d * sizeof *one);
        if (!one) { rc = -1; break; }
        memcpy(one, v + (size_t)k * (size_t)d, (size_t)d * sizeof *one);
        if (th[k] == 0 && !pr->q) pr->q = one;
        else if (tab_put(pr, th[k], one) != 0) { free(one); rc = -1; }
      }
      free(v);
    }
  }
  free(todo); free(th);
  return rc;
}

typedef struct { int idx; double score; } pr_score;

static int by_score(const void *a, const void *b) {
  const pr_score *x = a, *y = b;
  if (x->score > y->score) return -1;
  if (x->score < y->score) return 1;
  return x->idx - y->idx;            /* ties keep the upstream's order */
}

int prompt_rank_select(prompt_rank *pr, cJSON *records, int keep, int *ranked_out) {
  if (ranked_out) *ranked_out = 0;
  if (!prompt_rank_active(pr) || !cJSON_IsArray(records) || keep <= 0) return 0;
  int total = cJSON_GetArraySize(records);
  if (total <= keep) return 0;
  int m = total < rerank_max() ? total : rerank_max();

  char **texts = calloc((size_t)m, sizeof *texts);
  pr_score *sc = calloc((size_t)m, sizeof *sc);
  if (!texts || !sc) { free(texts); free(sc); return 0; }
  int ok = 1;
  for (int i = 0; i < m && ok; i++) {
    texts[i] = prompt_rank_record_text(cJSON_GetArrayItem(records, i));
    if (!texts[i]) ok = 0;
  }
  if (ok && ensure_vectors(pr, texts, m) != 0) { pr->state = 0; ok = 0; }
  for (int i = 0; i < m && ok; i++) {
    pr_slot *s = tab_find(pr, key_of(texts[i]));
    if (!s || !pr->q) { ok = 0; break; }
    sc[i].idx = i;
    sc[i].score = vec_cosine(pr->q, s->v, pr->dim);
  }
  for (int i = 0; i < m; i++) free(texts[i]);
  free(texts);
  if (!ok) { free(sc); return 0; }

  qsort(sc, (size_t)m, sizeof *sc, by_score);
  /* Detach the chosen records in score order, drop the rest, re-attach. */
  cJSON **chosen = calloc((size_t)keep, sizeof *chosen);
  if (!chosen) { free(sc); return 0; }
  cJSON **all = calloc((size_t)total, sizeof *all);
  if (!all) { free(chosen); free(sc); return 0; }
  int i = 0;
  cJSON *it;
  cJSON_ArrayForEach(it, records) all[i++] = it;
  for (int k = 0; k < keep; k++) chosen[k] = all[sc[k].idx];
  for (int k = 0; k < keep; k++)
    cJSON_DetachItemViaPointer(records, chosen[k]);
  while (cJSON_GetArraySize(records) > 0) cJSON_DeleteItemFromArray(records, 0);
  for (int k = 0; k < keep; k++) cJSON_AddItemToArray(records, chosen[k]);
  free(all); free(chosen); free(sc);
  if (ranked_out) *ranked_out = m;
  return 1;
}
