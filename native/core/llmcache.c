/* core/llmcache.c — see llmcache.h. */
#include "llmcache.h"
#include "../lib/vecindex.h"        /* vecindex_served_model: GET /v1/models */
#include "../third_party/sqlite3.h"
#include <openssl/evp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

long llmcache_ttl(void) {
  const char *e = getenv("JO_LLM_CACHE_TTL_SEC");
  long v = (e && *e) ? atol(e) : 0;
  return v > 0 ? v : 0;
}

/* One remembered answer per base URL is enough: the suggest path talks to one
 * server. A different URL simply replaces it. */
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static char   g_url[512], g_model[256];
static time_t g_at;

int llmcache_model(llm_client *c, char *out, size_t cap) {
  if (!c || !c->base_url || !out || cap == 0) return -1;
  time_t now = time(NULL);
  pthread_mutex_lock(&g_mu);
  if (g_model[0] && !strcmp(g_url, c->base_url) &&
      now - g_at < LLMCACHE_MODEL_TTL_SEC) {
    snprintf(out, cap, "%s", g_model);
    pthread_mutex_unlock(&g_mu);
    return 0;
  }
  pthread_mutex_unlock(&g_mu);
  char m[256];
  if (vecindex_served_model(NULL, c->base_url, m, sizeof m) != 0 || !m[0])
    return -1;
  pthread_mutex_lock(&g_mu);
  snprintf(g_url, sizeof g_url, "%s", c->base_url);
  snprintf(g_model, sizeof g_model, "%s", m);
  g_at = now;
  pthread_mutex_unlock(&g_mu);
  snprintf(out, cap, "%s", m);
  return 0;
}

void llmcache_key(const char *prompt, const char *params, char out[65]) {
  unsigned char d[EVP_MAX_MD_SIZE];
  unsigned int dl = 0;
  out[0] = 0;
  EVP_MD_CTX *x = EVP_MD_CTX_new();
  if (!x) return;
  int ok = EVP_DigestInit_ex(x, EVP_sha256(), NULL) == 1 &&
           (!prompt || EVP_DigestUpdate(x, prompt, strlen(prompt)) == 1) &&
           EVP_DigestUpdate(x, "\n", 1) == 1 &&
           (!params || EVP_DigestUpdate(x, params, strlen(params)) == 1) &&
           EVP_DigestFinal_ex(x, d, &dl) == 1 && dl == 32;
  EVP_MD_CTX_free(x);
  if (!ok) return;                 /* out stays "": callers then do not cache */
  for (unsigned i = 0; i < dl; i++) snprintf(out + i * 2, 3, "%02x", d[i]);
}

/* The prompt builders fence untrusted text with an id that is RANDOM per call
 * (core/prompts.c fence_token), announced in the security rule as
 * "-----BEGIN <NAME> <id>-----". Two builds of the same prompt therefore
 * differ only in that id, and keyed raw they would never match. The id
 * carries no meaning for the answer, so it is masked before hashing; every
 * other byte still counts. */
#define FENCE_ANNOUNCE "-----BEGIN <NAME> "
#define FENCE_ID_LEN   16

void llmcache_key_prompt(const char *prompt, const char *params, char out[65]) {
  const char *a = prompt ? strstr(prompt, FENCE_ANNOUNCE) : NULL;
  if (!a || strlen(a) < sizeof FENCE_ANNOUNCE - 1 + FENCE_ID_LEN) {
    llmcache_key(prompt, params, out);
    return;
  }
  char id[FENCE_ID_LEN + 1];
  memcpy(id, a + sizeof FENCE_ANNOUNCE - 1, FENCE_ID_LEN);
  id[FENCE_ID_LEN] = 0;
  char *m = strdup(prompt);
  if (!m) { out[0] = 0; return; }
  for (char *p = strstr(m, id); p; p = strstr(p + FENCE_ID_LEN, id))
    memset(p, '#', FENCE_ID_LEN);
  llmcache_key(m, params, out);
  free(m);
}

static int ensure_table(db_handle *db) {
  return sqlite3_exec(db->h,
    "CREATE TABLE IF NOT EXISTS " LLMCACHE_TABLE " ("
    " model TEXT NOT NULL, key TEXT NOT NULL, response TEXT NOT NULL,"
    " created_at INTEGER NOT NULL, PRIMARY KEY (model, key))",
    NULL, NULL, NULL) == SQLITE_OK ? 0 : -1;
}

char *llmcache_get(db_handle *db, const char *model, const char *key) {
  long ttl = llmcache_ttl();
  if (!ttl || !db || !db->h || !model || !*model || !key || !*key) return NULL;
  if (ensure_table(db) != 0) return NULL;
  long long cutoff = (long long)time(NULL) - ttl;
  sqlite3_stmt *s = NULL;
  char *out = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT response, created_at FROM " LLMCACHE_TABLE
        " WHERE model=?1 AND key=?2", -1, &s, NULL) != SQLITE_OK) return NULL;
  sqlite3_bind_text(s, 1, model, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 2, key, -1, SQLITE_STATIC);
  int stale = 0;
  if (sqlite3_step(s) == SQLITE_ROW) {
    if (sqlite3_column_int64(s, 1) >= cutoff) {
      const char *r = (const char *)sqlite3_column_text(s, 0);
      out = r ? strdup(r) : NULL;
    } else stale = 1;
  }
  sqlite3_finalize(s);
  if (stale &&
      sqlite3_prepare_v2(db->h, "DELETE FROM " LLMCACHE_TABLE
                         " WHERE created_at < ?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_int64(s, 1, cutoff);
    sqlite3_step(s);
    sqlite3_finalize(s);
  }
  return out;
}

void llmcache_put(db_handle *db, const char *model, const char *key,
                  const char *response) {
  if (!llmcache_ttl() || !db || !db->h || !model || !*model || !key || !*key ||
      !response || !*response) return;
  if (ensure_table(db) != 0) return;
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "INSERT OR REPLACE INTO " LLMCACHE_TABLE
        " (model, key, response, created_at) VALUES (?1, ?2, ?3, ?4)",
        -1, &s, NULL) != SQLITE_OK) return;
  sqlite3_bind_text(s, 1, model, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 2, key, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 3, response, -1, SQLITE_STATIC);
  sqlite3_bind_int64(s, 4, (long long)time(NULL));
  if (sqlite3_step(s) != SQLITE_DONE)
    fprintf(stderr, "[llmcache] store failed: %s\n", sqlite3_errmsg(db->h));
  sqlite3_finalize(s);
}
