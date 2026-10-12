/* core/relevance.c — see relevance.h. */
#include "relevance.h"
#include "qembed.h"
#include "../lib/jpnorm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Append up to `room` bytes of `s` (and a separating space) to b[*len]. */
static void cat_bounded(char *b, size_t *len, size_t cap, const char *s) {
  if (!s || !*s || *len + 2 >= cap) return;
  if (*len) b[(*len)++] = ' ';
  size_t sl = strlen(s), room = cap - 1 - *len;
  if (sl > room) sl = room;
  memcpy(b + *len, s, sl);
  *len += sl;
  b[*len] = 0;
}

/* Keys that ECHO THE REQUEST rather than describe the record. hpengine writes
 * the entity into every record's properties as "query" and the request URL as
 * "endpoint"; hand-written services put {"query": entity} in their bodies.
 * Left in, every record of a filter-ignoring pivot would "carry the entity"
 * and the check would clear exactly the rows it exists to catch. */
static const char *const ECHO_KEYS[] = {
  "query", "endpoint", "entity", "entity_type", "request_url", "search_term",
  "searched", "q", NULL
};

/* `json` with the echo keys removed (malloc'd), or NULL when it is not a JSON
 * object (the caller then uses it verbatim). */
static char *strip_echo(const char *json) {
  if (!json || *json != '{') return NULL;
  cJSON *o = cJSON_Parse(json);
  if (!cJSON_IsObject(o)) { cJSON_Delete(o); return NULL; }
  for (int i = 0; ECHO_KEYS[i]; i++) cJSON_DeleteItemFromObject(o, ECHO_KEYS[i]);
  char *out = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return out;
}

void relev_sample_add(relev_sample *s, const intel_item *it) {
  if (!s || !it || s->n >= RELEV_SAMPLE_MAX) return;
  if (!qembed_enabled()) return;      /* inert: no verdict is possible anyway */
  char raw[RELEV_TEXT_MAX];
  size_t len = 0;
  raw[0] = 0;
  char *body  = strip_echo(it->body);
  char *props = strip_echo(it->properties_json);
  cat_bounded(raw, &len, sizeof raw, it->title);
  cat_bounded(raw, &len, sizeof raw, it->summary);
  cat_bounded(raw, &len, sizeof raw, body ? body : it->body);
  cat_bounded(raw, &len, sizeof raw, props ? props : it->properties_json);
  free(body); free(props);
  char lab[1024];
  size_t ll = 0;
  lab[0] = 0;
  cat_bounded(lab, &ll, sizeof lab, it->title);
  if (!it->summary || !it->title || strcmp(it->summary, it->title))
    cat_bounded(lab, &ll, sizeof lab, it->summary);
  if (!ll) cat_bounded(lab, &ll, sizeof lab, it->body);
  /* A cut may split a UTF-8 sequence; the sanitiser repairs the tail. */
  char *clean = vecindex_utf8_sanitize(raw, NULL);
  s->text[s->n]  = clean ? jpnorm_fold_dup(clean) : NULL;
  free(clean);
  s->label[s->n] = vecindex_utf8_sanitize(lab, NULL);
  if (!s->text[s->n] || !s->label[s->n]) {
    free(s->text[s->n]); free(s->label[s->n]);
    s->text[s->n] = s->label[s->n] = NULL;
    return;
  }
  s->n++;
}

void relev_sample_free(relev_sample *s) {
  if (!s) return;
  for (int i = 0; i < s->n; i++) { free(s->text[i]); free(s->label[i]); }
  s->n = 0;
}

/* Does folded `text` carry folded entity `fe` — whole, or every word of it
 * (words shorter than 2 bytes do not count; they match anything)? */
static int carries(const char *text, const char *fe) {
  if (strstr(text, fe)) return 1;
  char *dup = strdup(fe);
  if (!dup) return 0;
  int words = 0, all = 1;
  char *save = NULL;
  for (char *w = strtok_r(dup, " ", &save); w; w = strtok_r(NULL, " ", &save)) {
    if (strlen(w) < 2) continue;
    words++;
    if (!strstr(text, w)) { all = 0; break; }
  }
  free(dup);
  return words > 1 && all;
}

static double sim_min(void) {
  const char *e = getenv("JO_RELEVANCE_SIM_MIN");
  double v = (e && *e) ? atof(e) : 0;
  return v > 0 && v < 1 ? v : RELEV_SIM_MIN;
}

cJSON *relev_check(const relev_sample *s, const char *entity, long records) {
  const char *off = getenv("JO_RELEVANCE_CHECK");
  if (off && !strcmp(off, "0")) return NULL;
  if (!s || !entity || s->n < RELEV_MIN_RECORDS) return NULL;
  if (!qembed_enabled()) return NULL;          /* inert: see relevance.h */
  char *fe = jpnorm_fold_dup(entity);
  if (!fe) return NULL;
  if (strlen(fe) < RELEV_MIN_ENTITY) { free(fe); return NULL; }

  int lex = 0;
  for (int i = 0; i < s->n; i++) if (carries(s->text[i], fe)) lex++;
  free(fe);
  if (lex >= RELEV_LEXICAL_PASS * s->n) return NULL;     /* most carry it */

  /* Most lack it: ask the embedding server whether they are about it anyway.
   * One request: the entity first, then each label. */
  const char **texts = malloc((size_t)(s->n + 1) * sizeof *texts);
  if (!texts) return NULL;
  texts[0] = entity;
  for (int i = 0; i < s->n; i++) texts[i + 1] = s->label[i];
  float *v = NULL; int d = 0; llm_status st;
  int rc = qembed_texts(texts, s->n + 1, &v, &d, &st);
  free(texts);
  if (rc != 0) {
    /* No second opinion, no accusation: a lexical miss alone is not proof. */
    fprintf(stderr, "[relevance] embedding failed (%s); no verdict\n",
            llm_status_code(st));
    return NULL;
  }
  int related = 0;
  double thr = sim_min(), best = -1;
  for (int i = 0; i < s->n; i++) {
    double c = vec_cosine(v, v + (size_t)(i + 1) * (size_t)d, d);
    if (c > best) best = c;
    if (c >= thr) related++;
  }
  free(v);
  int about = 0;          /* records that carry the entity OR are near it */
  about = related > lex ? related : lex;
  if (about > RELEV_SUSPECT_MAX * s->n) return NULL;

  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "verdict", "relevance-suspect");
  cJSON_AddNumberToObject(o, "sampled", s->n);
  cJSON_AddNumberToObject(o, "of_records", (double)records);
  cJSON_AddNumberToObject(o, "carrying_entity", lex);
  cJSON_AddNumberToObject(o, "semantically_related", related);
  cJSON_AddNumberToObject(o, "similarity_threshold", thr);
  cJSON_AddNumberToObject(o, "best_similarity", (double)(long)(best * 1000) / 1000.0);
  cJSON_AddStringToObject(o, "method", "lexical+embedding");
  char note[400];
  snprintf(note, sizeof note,
    "of the first %d records this service returned, %d contain the entity and "
    "%d are semantically related to it; the upstream may have ignored the "
    "entity filter and returned an unfiltered collection (house rule 4d). "
    "Treat these records as NOT established to be about the entity.",
    s->n, lex, related);
  cJSON_AddStringToObject(o, "note", note);
  return o;
}
