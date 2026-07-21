/* core/url_override.c — see url_override.h. Append-only singly-linked list so
 * that pointers handed out by url_override_apply() never dangle across a reload
 * (the list only grows; nodes are never freed). A mutex serializes traversal
 * against the rare prepend. */
#include "url_override.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct ov { char *old_url, *new_url; struct ov *next; } ov;

static ov *g_head = NULL;
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;

/* true if (old,new) is already in the list (caller holds g_mu). */
static int already_present(const char *old_url, const char *new_url) {
  for (ov *p = g_head; p; p = p->next)
    if (strcmp(p->old_url, old_url) == 0 && strcmp(p->new_url, new_url) == 0)
      return 1;
  return 0;
}

void url_override_reload(db_handle *db) {
  if (!db || !db->h) return;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT old_url,new_url FROM collector_url_overrides "
        "WHERE old_url IS NOT NULL AND new_url IS NOT NULL", -1, &s, NULL)
      != SQLITE_OK)
    return;
  int added = 0;
  pthread_mutex_lock(&g_mu);
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *o = (const char *)sqlite3_column_text(s, 0);
    const char *n = (const char *)sqlite3_column_text(s, 1);
    if (!o || !*o || !n || !*n) continue;
    if (already_present(o, n)) continue;
    ov *node = calloc(1, sizeof *node);
    if (!node) continue;
    node->old_url = strdup(o);
    node->new_url = strdup(n);
    if (!node->old_url || !node->new_url) {
      free(node->old_url); free(node->new_url); free(node); continue;
    }
    node->next = g_head;
    g_head = node;
    added++;
  }
  pthread_mutex_unlock(&g_mu);
  sqlite3_finalize(s);
  if (added)
    fprintf(stderr, "[url-override] loaded %d override(s)\n", added);
}

const char *url_override_apply(const char *url) {
  if (!url) return url;
  const char *r = url;
  pthread_mutex_lock(&g_mu);
  for (ov *p = g_head; p; p = p->next)
    if (strcmp(p->old_url, url) == 0) { r = p->new_url; break; }
  pthread_mutex_unlock(&g_mu);
  return r;
}
