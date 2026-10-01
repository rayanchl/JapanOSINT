/* core/url_override.c — see url_override.h. Append-only singly-linked list so
 * that pointers handed out by url_override_apply() never dangle across a reload
 * (the list only grows; nodes are never freed). A mutex serializes traversal
 * against the rare prepend. Removal tombstones a node (sets `dead`) for the
 * same reason: a caller may still be holding the string we handed it. */
#include "url_override.h"
#include "hostgate.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct ov { char *old_url, *new_url; int dead; struct ov *next; } ov;

static ov *g_head = NULL;
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;

/* true if (old,new) is already in the list (caller holds g_mu). A tombstoned
 * entry counts as present and is revived, so a reload after a removal that the
 * operator has since undone in the table works without a restart. */
static int already_present(const char *old_url, const char *new_url) {
  for (ov *p = g_head; p; p = p->next)
    if (strcmp(p->old_url, old_url) == 0 && strcmp(p->new_url, new_url) == 0) {
      p->dead = 0;
      return 1;
    }
  return 0;
}

int url_override_validate(const char *old_url, const char *new_url) {
  if (!old_url || !*old_url || !new_url || !*new_url) return HG_URL_BAD_HOST;
  /* The old side is checked too: a rewrite keyed on a URL we would never
   * fetch is at best dead weight, and at worst a way to smuggle an internal
   * address past a reviewer who only looked at the destination.
   *
   * But WITHOUT the DNS half. `old_url` is a match key, never a destination,
   * and the overwhelmingly common reason an override exists is that the old
   * host stopped resolving — so resolving it as an admission test refused
   * precisely the repairs that were doing their job, on every reload, with
   * nothing but a log line to show for it. The literal-address and
   * metadata-hostname rules still apply here; only `new_url`, which we
   * actually fetch, has to resolve. */
  int rc = hostgate_url_check_strict_nodns(old_url);
  if (rc != HG_URL_OK) return rc;
  return hostgate_url_check_strict(new_url);
}

void url_override_reload(db_handle *db) {
  if (!db || !db->h) return;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT old_url,new_url FROM collector_url_overrides "
        "WHERE old_url IS NOT NULL AND new_url IS NOT NULL", -1, &s, NULL)
      != SQLITE_OK)
    return;
  int added = 0, refused = 0;
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *o = (const char *)sqlite3_column_text(s, 0);
    const char *n = (const char *)sqlite3_column_text(s, 1);
    if (!o || !*o || !n || !*n) continue;
    /* Validated on the way IN, not only where the row is written: rows can
     * arrive from an older build, a manual edit, or a restored backup, and
     * this is the one place every one of them passes through. getaddrinfo()
     * is called outside g_mu — this is boot/repair-time, not the fetch path,
     * but holding a mutex across DNS is still a stall waiting to happen. */
    int vr = url_override_validate(o, n);
    if (vr != HG_URL_OK) {
      fprintf(stderr, "[url-override] refused %s -> %s: %s\n",
              o, n, hostgate_url_reason(vr));
      refused++;
      continue;
    }
    pthread_mutex_lock(&g_mu);
    if (already_present(o, n)) { pthread_mutex_unlock(&g_mu); continue; }
    ov *node = calloc(1, sizeof *node);
    if (!node) { pthread_mutex_unlock(&g_mu); continue; }
    node->old_url = strdup(o);
    node->new_url = strdup(n);
    if (!node->old_url || !node->new_url) {
      free(node->old_url); free(node->new_url); free(node);
      pthread_mutex_unlock(&g_mu); continue;
    }
    node->next = g_head;
    g_head = node;
    added++;
    pthread_mutex_unlock(&g_mu);
  }
  sqlite3_finalize(s);
  if (added || refused)
    fprintf(stderr, "[url-override] loaded %d override(s), refused %d\n",
            added, refused);
}

const char *url_override_apply(const char *url) {
  if (!url) return url;
  const char *r = url;
  pthread_mutex_lock(&g_mu);
  for (ov *p = g_head; p; p = p->next)
    if (!p->dead && strcmp(p->old_url, url) == 0) { r = p->new_url; break; }
  pthread_mutex_unlock(&g_mu);
  return r;
}

int url_override_remove(db_handle *db, const char *old_url) {
  if (!old_url || !*old_url) return 0;
  int n = 0;
  pthread_mutex_lock(&g_mu);
  for (ov *p = g_head; p; p = p->next)
    if (!p->dead && strcmp(p->old_url, old_url) == 0) { p->dead = 1; n++; }
  pthread_mutex_unlock(&g_mu);
  if (db && db->h) {
    /* `s` must be initialised and the finalize kept INSIDE the success branch:
     * sqlite3_prepare_v2 leaves the out-param untouched on failure, so the
     * previous shape finalized an uninitialised pointer whenever the prepare
     * failed. The step is also checked now — a DELETE that silently fails
     * leaves the row on disk while the in-memory list says it is gone, so the
     * override reappears on the next reload. */
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db->h,
          "DELETE FROM collector_url_overrides WHERE old_url=?1",
          -1, &s, NULL) == SQLITE_OK) {
      sqlite3_bind_text(s, 1, old_url, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(s) != SQLITE_DONE)
        fprintf(stderr, "[url-override] delete failed for %s: %s\n",
                old_url, sqlite3_errmsg(db->h));
      sqlite3_finalize(s);
    } else {
      fprintf(stderr, "[url-override] delete prepare failed: %s\n",
              sqlite3_errmsg(db->h));
    }
  }
  if (n) fprintf(stderr, "[url-override] removed %d override(s) for %s\n",
                 n, old_url);
  return n;
}

int url_override_reset(db_handle *db) {
  int n = 0;
  pthread_mutex_lock(&g_mu);
  for (ov *p = g_head; p; p = p->next) if (!p->dead) { p->dead = 1; n++; }
  pthread_mutex_unlock(&g_mu);
  if (db && db->h)
    sqlite3_exec(db->h, "DELETE FROM collector_url_overrides", 0, 0, 0);
  if (n) fprintf(stderr, "[url-override] reset %d override(s)\n", n);
  return n;
}
