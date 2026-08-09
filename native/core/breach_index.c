#include "breach_index.h"
#include "breach_store.h"
#include "entitystore.h"
#include "breach_monitor.h"
#include "../lib/bigfile.h"
#include "../lib/bloom.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

#ifndef JO_REPO_ROOT
#define JO_REPO_ROOT "/Users/rayan/JapanOSINT"
#endif

/* ── type helpers ─────────────────────────────────────────────────────── */
static const char *TYPE_NAME[] = { "email", "username", "phone", "password", "auto" };

breach_type breach_type_parse(const char *s) {
  if (!s) return BT_AUTO;
  for (int i = 0; i < 4; i++) if (strcasecmp(s, TYPE_NAME[i]) == 0) return (breach_type)i;
  return BT_AUTO;
}
const char *breach_type_name(breach_type t) {
  return (t >= 0 && t <= 4) ? TYPE_NAME[t] : "auto";
}

static const char *root_dir(void) {
  const char *e = getenv("JO_BREACH_DIR");
  return (e && *e) ? e : JO_REPO_ROOT "/data/breach";
}

/* ── SHA-1 uppercase hex ──────────────────────────────────────────────── */
static void sha1_hex(const char *s, size_t n, char out[41]) {
  unsigned char h[SHA_DIGEST_LENGTH];
  SHA1((const unsigned char *)s, n, h);
  static const char *H = "0123456789ABCDEF";
  for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
    out[i * 2] = H[h[i] >> 4]; out[i * 2 + 1] = H[h[i] & 15];
  }
  out[40] = 0;
}

/* ── identifier normalization ─────────────────────────────────────────── */
static char *norm(breach_type t, const char *in) {
  if (!in) return NULL;
  while (*in == ' ' || *in == '\t') in++;
  size_t L = strlen(in);
  while (L && (in[L - 1] == ' ' || in[L - 1] == '\t')) L--;
  if (!L) return NULL;
  char *o = malloc(L + 1);
  if (!o) return NULL;
  memcpy(o, in, L); o[L] = 0;
  if (t == BT_EMAIL || t == BT_USERNAME) {
    for (size_t i = 0; i < L; i++) o[i] = (char)tolower((unsigned char)o[i]);
  } else if (t == BT_PHONE) {
    char *w = o; int plus = (o[0] == '+');
    if (plus) *w++ = '+';
    for (size_t i = 0; i < L; i++) if (isdigit((unsigned char)o[i])) *w++ = o[i];
    *w = 0;
    if (!o[plus ? 1 : 0]) { free(o); return NULL; }
  }
  /* BT_PASSWORD: exact bytes (never trimmed beyond surrounding blanks above). */
  return o;
}

static breach_type detect(const char *id) {
  if (strchr(id, '@')) return BT_EMAIL;
  const char *p = id; if (*p == '+') p++;
  int digits = 1, count = 0;
  for (; *p; p++) { if (!isdigit((unsigned char)*p)) { digits = 0; break; } count++; }
  if (digits && count >= 7 && count <= 15) return BT_PHONE;
  return BT_USERNAME;
}

/* ── at-rest secret crypto (AES-256-GCM, HKDF from SECRETS_MASTER_KEY) ──── */
static int master_key(unsigned char out[32]) {
  const char *raw = getenv("SECRETS_MASTER_KEY");
  if (!raw || !*raw) return 0;
  size_t L = strlen(raw);
  int hex = (L == 64);
  for (size_t i = 0; hex && i < L; i++) if (!isxdigit((unsigned char)raw[i])) hex = 0;
  if (hex) {
    for (int i = 0; i < 32; i++) { unsigned x; sscanf(raw + 2 * i, "%2x", &x); out[i] = (unsigned char)x; }
    return 1;
  }
  SHA256((const unsigned char *)raw, L, out);
  return 1;
}
static int derive_key(unsigned char out[32]) {
  unsigned char mk[32]; if (!master_key(mk)) return 0;
  EVP_PKEY_CTX *c = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
  if (!c) return 0;
  size_t ol = 32; int ok = 0;
  if (EVP_PKEY_derive_init(c) == 1 &&
      EVP_PKEY_CTX_set_hkdf_md(c, EVP_sha256()) == 1 &&
      EVP_PKEY_CTX_set1_hkdf_key(c, mk, 32) == 1 &&
      EVP_PKEY_CTX_set1_hkdf_salt(c, (const unsigned char *)"breach_index", 12) == 1 &&
      EVP_PKEY_CTX_add1_hkdf_info(c, (const unsigned char *)"JapanOSINT.breach_index.v1", 26) == 1 &&
      EVP_PKEY_derive(c, out, &ol) == 1 && ol == 32)
    ok = 1;
  EVP_PKEY_CTX_free(c);
  return ok;
}
static char *tohex(const unsigned char *b, int n) {
  char *o = malloc((size_t)n * 2 + 1); if (!o) return NULL;
  static const char *H = "0123456789abcdef";
  for (int i = 0; i < n; i++) { o[i * 2] = H[b[i] >> 4]; o[i * 2 + 1] = H[b[i] & 15]; }
  o[n * 2] = 0; return o;
}
static int fromhex(const char *h, unsigned char *out, int max) {
  int n = 0;
  for (; h[0] && h[1] && n < max; h += 2) {
    unsigned x; if (sscanf(h, "%2x", &x) != 1) return -1; out[n++] = (unsigned char)x;
  }
  return n;
}
/* blob = [12 nonce][16 tag][ct] -> hex string; NULL if no master key set (fail closed). */
static char *enc_secret(const char *pt) {
  if (!pt || !*pt) return NULL;
  unsigned char key[32]; if (!derive_key(key)) return NULL;
  unsigned char nonce[12]; if (RAND_bytes(nonce, 12) != 1) return NULL;
  int ptl = (int)strlen(pt);
  unsigned char *blob = malloc((size_t)28 + ptl + 16); if (!blob) return NULL;
  memcpy(blob, nonce, 12);
  EVP_CIPHER_CTX *x = EVP_CIPHER_CTX_new();
  int len, ok = 0, ctl = 0;
  if (x && EVP_EncryptInit_ex(x, EVP_aes_256_gcm(), NULL, key, nonce) == 1 &&
      EVP_EncryptUpdate(x, blob + 28, &len, (const unsigned char *)pt, ptl) == 1) {
    ctl = len;
    if (EVP_EncryptFinal_ex(x, blob + 28 + ctl, &len) == 1) {
      ctl += len;
      if (EVP_CIPHER_CTX_ctrl(x, EVP_CTRL_GCM_GET_TAG, 16, blob + 12) == 1) ok = 1;
    }
  }
  if (x) EVP_CIPHER_CTX_free(x);
  char *hex = ok ? tohex(blob, 28 + ctl) : NULL;
  free(blob);
  return hex;
}
static char *dec_secret(const char *hex) {
  if (!hex || !*hex || hex[0] == '-') return NULL;
  unsigned char blob[16384];
  int bl = fromhex(hex, blob, sizeof blob);
  if (bl < 28) return NULL;
  unsigned char key[32]; if (!derive_key(key)) return NULL;
  int ctl = bl - 28;
  char *pt = malloc((size_t)ctl + 1); if (!pt) return NULL;
  EVP_CIPHER_CTX *x = EVP_CIPHER_CTX_new();
  int len, ok = 0, pl = 0;
  if (x && EVP_DecryptInit_ex(x, EVP_aes_256_gcm(), NULL, key, blob) == 1 &&
      EVP_DecryptUpdate(x, (unsigned char *)pt, &len, blob + 28, ctl) == 1) {
    pl = len;
    EVP_CIPHER_CTX_ctrl(x, EVP_CTRL_GCM_SET_TAG, 16, blob + 12);
    if (EVP_DecryptFinal_ex(x, (unsigned char *)pt + pl, &len) == 1) { pl += len; pt[pl] = 0; ok = 1; }
  }
  if (x) EVP_CIPHER_CTX_free(x);
  if (!ok) { free(pt); return NULL; }
  return pt;
}

/* ── shard paths ──────────────────────────────────────────────────────── */
static void ensure_type_dir(breach_type t) {
  char p[1024];
  snprintf(p, sizeof p, "%s", root_dir()); mkdir(p, 0755);
  snprintf(p, sizeof p, "%s/%s", root_dir(), breach_type_name(t)); mkdir(p, 0755);
}
static void shard_path(breach_type t, const char *hash, char *out, size_t n) {
  snprintf(out, n, "%s/%s/%c%c.idx", root_dir(), breach_type_name(t),
           (char)tolower((unsigned char)hash[0]), (char)tolower((unsigned char)hash[1]));
}

void breach_index_keyid(breach_type t, const char *value, char *out, size_t n) {
  if (t == BT_AUTO) t = detect(value ? value : "");
  char *nv = norm(t, value ? value : "");
  char hash[41];
  sha1_hex(nv ? nv : "", nv ? strlen(nv) : 0, hash);
  free(nv);
  snprintf(out, n, "%s:%.10s", breach_type_name(t), hash);
}

/* ── bounded open-handle cache for scatter writes ─────────────────────── */
#define WC_MAX 200
typedef struct { char path[1024]; FILE *f; unsigned long long lru; } wc_ent;
typedef struct { wc_ent e[WC_MAX]; int n; unsigned long long clock; } wcache;

/* NULL on failure, leaving the cache in a consistent state. Getting that wrong
 * is not theoretical: the slot used to be claimed (n++) or vacated (fclose)
 * BEFORE the fopen, so a failed open left either a NULL handle or a freed one
 * sitting in the table — which wc_closeall() then fclose()d, and which the
 * strcmp above would hand back to a later caller. */
static FILE *wc_get(wcache *c, const char *path) {
  for (int i = 0; i < c->n; i++)
    if (c->e[i].f && strcmp(c->e[i].path, path) == 0) {
      c->e[i].lru = ++c->clock; return c->e[i].f;
    }
  int slot = -1;
  /* A slot vacated by an earlier FAILED fopen is reused before anything is
   * evicted. Without this the table stayed full of one empty entry whose lru
   * was still the smallest, so the next call re-picked that same slot and ran
   * fclose(NULL) on it — deterministic the moment a disk fills mid-ingest.
   * (Bumping lru on the vacated slot would hide the crash but leak the free
   * slot; reusing it is the actual repair.) */
  for (int i = 0; i < c->n; i++) if (!c->e[i].f) { slot = i; break; }
  if (slot < 0 && c->n < WC_MAX) slot = c->n;
  if (slot < 0) {
    slot = 0;
    for (int i = 1; i < c->n; i++) if (c->e[i].lru < c->e[slot].lru) slot = i;
    fclose(c->e[slot].f);                 /* non-NULL: every slot is occupied */
    c->e[slot].f = NULL;                  /* vacate before we risk failing */
    c->e[slot].path[0] = 0;
  }
  FILE *f = fopen(path, "ab");
  if (!f) return NULL;                    /* slot stays empty / n unchanged */
  snprintf(c->e[slot].path, sizeof c->e[slot].path, "%s", path);
  c->e[slot].f = f; c->e[slot].lru = ++c->clock;
  if (slot == c->n) c->n++;               /* commit the new slot only on success */
  return f;
}
static void wc_closeall(wcache *c) {
  for (int i = 0; i < c->n; i++)
    if (c->e[i].f) { fclose(c->e[i].f); c->e[i].f = NULL; }
  c->n = 0;
}

/* Rows the input can plausibly contain, from its size on disk. Deliberately an
 * OVER-estimate (a short average line length) — oversizing the dedup filter
 * costs memory, undersizing it costs data. */
static unsigned long long estimate_rows(const char *path, breach_type t) {
  struct stat sb;
  if (!path || stat(path, &sb) != 0 || sb.st_size <= 0) return 50000000ULL;
  /* "SHA1HEX:count\n" is ~48 B; identity rows are shorter and more variable. */
  unsigned long long avg = (t == BT_PASSWORD) ? 40ULL : 24ULL;
  unsigned long long n = (unsigned long long)sb.st_size / avg;
  return n < 1000000ULL ? 1000000ULL : n;
}

/* The dedup filter for `t`, sized for `expect` insertions.
 *
 * A bloom hit makes the ingest SKIP the row, so a saturated filter silently
 * discards real data — this used to be hard-wired to 50M entries while the
 * password path targets the ~850M-row Pwned Passwords list, which pushes the
 * false-positive rate past 50% and drops the majority of the file. If the
 * filter restored from disk was built for materially fewer rows than this input
 * needs, rebuild it larger: losing the dedup history costs duplicate rows in a
 * shard, which is recoverable, whereas dropping rows is not. */
static bloom *bloom_for(breach_type t, unsigned long long expect) {
  char p[1024]; snprintf(p, sizeof p, "%s/%s.bloom", root_dir(), breach_type_name(t));
  bloom *b = bloom_load(p);
  if (b) {
    unsigned long long need = bloom_count(b) + expect;
    if (bloom_capacity(b) >= need) return b;
    fprintf(stderr,
            "[breach_index] %s.bloom sized for %llu but %llu needed "
            "(%llu already in it) — rebuilding, dedup history is reset\n",
            breach_type_name(t), bloom_capacity(b), need, bloom_count(b));
    bloom_free(b);
    expect = need;
  }
  return bloom_new(expect, 0.001);
}

/* 1 = seen before, caller skips the row.
 *
 * Returns 0 when the filter is absent (allocation failed) or already past its
 * design capacity: at that point a "hit" is more likely to be a false positive
 * than a real duplicate, and skipping would destroy data. Writing a duplicate
 * into a shard is recoverable; dropping a breach record is not. */
static int dedup_seen(bloom *b, const void *key, size_t len) {
  if (!b) return 0;
  if (bloom_maybe(b, key, len) && !bloom_saturated(b)) return 1;
  bloom_add(b, key, len);
  return 0;
}

/* ── ingest ───────────────────────────────────────────────────────────── */
int breach_index_ingest(const char *source_id, const char *path, breach_type type,
                        unsigned long long *rows_in, unsigned long long *rows_new,
                        db_handle *db, int materialize, int dry_run) {
  bigfile *bf = bigfile_open(path);
  if (!bf) return -1;
  const unsigned long long expect = estimate_rows(path, type);
  wcache wc = {0};
  /* Optional eager intel materialization into breach_items (dedicated store).
   * Never opened for a dry run — a projection persists nothing. */
  breach_store *store = (db && materialize && !dry_run) ? breach_store_open(db) : NULL;
  bloom *blooms[4] = {0, 0, 0, 0};
  int touched[4] = {0, 0, 0, 0};
  unsigned long long ri = 0, rn = 0;
  const char *line; size_t len;

  while ((line = bigfile_next(bf, &len))) {
    if (!len) continue;
    ri++;

    if (type == BT_PASSWORD) {
      /* Pwned Passwords bulk: "SHA1HEX:count" (already hashed). */
      const char *colon = strchr(line, ':');
      size_t hlen = colon ? (size_t)(colon - line) : len;
      if (hlen != 40) continue;
      char hash[41];
      for (size_t i = 0; i < 40; i++) hash[i] = (char)toupper((unsigned char)line[i]);
      hash[40] = 0;
      long long count = colon ? atoll(colon + 1) : 1;
      if (!blooms[BT_PASSWORD]) blooms[BT_PASSWORD] = bloom_for(BT_PASSWORD, expect);
      if (dedup_seen(blooms[BT_PASSWORD], hash, 40)) continue;
      touched[BT_PASSWORD] = 1;
      if (!dry_run) {
        ensure_type_dir(BT_PASSWORD);
        char sp[1024]; shard_path(BT_PASSWORD, hash, sp, sizeof sp);
        FILE *f = wc_get(&wc, sp);
        if (f) { fprintf(f, "%s\t%lld\n", hash, count); rn++; }
      } else rn++;
      if (store) {
        char keyid[32]; snprintf(keyid, sizeof keyid, "password:%.10s", hash);
        breach_store_put(store, keyid, "password", NULL /* hash-only */,
                         source_id, hash, 0, count);
      }
      continue;
    }

    /* identity dumps: "identifier[:secret]" or "identifier[\tsecret]". */
    char *copy = NULL; const char *ident = line, *secret = NULL;
    char *delim = strpbrk(line, ":\t");
    if (delim) {
      size_t il = (size_t)(delim - line);
      copy = malloc(il + 1);
      if (!copy) continue;
      memcpy(copy, line, il); copy[il] = 0;
      ident = copy; secret = delim + 1;
    }
    breach_type ct = (type == BT_AUTO) ? detect(ident) : type;
    char *nv = norm(ct, ident);
    free(copy);
    if (!nv) continue;
    char hash[41]; sha1_hex(nv, strlen(nv), hash);
    /* Keep the normalized cleartext only when materializing (it becomes the
     * searchable breach_items.value); the offline path frees it immediately. */
    char *val = store ? nv : NULL;
    if (!val) free(nv);

    if (!blooms[ct]) blooms[ct] = bloom_for(ct, expect);
    char dkey[160]; snprintf(dkey, sizeof dkey, "%s|%.100s", hash, source_id ? source_id : "");
    if (dedup_seen(blooms[ct], dkey, strlen(dkey))) { free(val); continue; }
    touched[ct] = 1;

    char *enc = (!dry_run && secret && *secret) ? enc_secret(secret) : NULL;
    if (!dry_run) {
      ensure_type_dir(ct);
      char sp[1024]; shard_path(ct, hash, sp, sizeof sp);
      FILE *f = wc_get(&wc, sp);
      if (f) { fprintf(f, "%s\t%s\t%s\n", hash, source_id ? source_id : "?", enc ? enc : "-"); rn++; }
    } else rn++;
    if (store) {
      /* Per-(identifier, breach) keyid so each breach is a distinct source and
       * `WHERE source_id=?` / COUNT(*) GROUP BY source_id are exact. (Passwords
       * keep a global keyid above — their count is a cross-breach prevalence.) */
      char keyid[128];
      snprintf(keyid, sizeof keyid, "%s:%.10s|%.80s", breach_type_name(ct), hash,
               source_id ? source_id : "?");
      breach_store_put(store, keyid, breach_type_name(ct), val, source_id, hash,
                       enc ? 1 : 0, 1);

      /* Phase 3 — full entity materialization (deterministic, no LLM). Every
       * identity record's identifier becomes an entity + a mention keyed on the
       * synthetic item uid "breach:"+keyid, so breach items get entity chips and
       * "entity → its breaches" works. Passwords are never entities. */
      char uid[144];
      snprintf(uid, sizeof uid, "breach:%s", keyid);
      char *eid = es_upsert_entity(db, breach_type_name(ct), val);
      if (eid) {
        es_add_mention(db, eid, uid, source_id, val, "breach", 0.99, "breach-ingest");
        free(eid);
      }
    }
    free(val);
    free(enc);
  }

  if (store) breach_store_finish(store);
  /* Roadmap 24 — raise alert_events for any standing breach monitor this
   * ingest just satisfied. `if (store)` is the right guard: it already means
   * db && materialize && !dry_run, so a dry run stays side-effect free. */
  if (store) breach_monitor_scan_new(db, source_id, NULL);
  wc_closeall(&wc);
  for (int t = 0; t < 4; t++) {
    if (!dry_run && touched[t] && blooms[t]) {
      char p[1024]; snprintf(p, sizeof p, "%s/%s.bloom", root_dir(), breach_type_name((breach_type)t));
      bloom_save(blooms[t], p);
    }
    if (blooms[t]) bloom_free(blooms[t]);
  }
  bigfile_close(bf);
  if (rows_in) *rows_in = ri;
  if (rows_new) *rows_new = rn;
  return 0;
}

/* ── lookup ───────────────────────────────────────────────────────────── */
int breach_index_lookup(breach_type type, const char *value, int reveal, cJSON **out) {
  if (out) *out = NULL;
  if (!value || !*value) return 0;
  if (type == BT_AUTO) type = detect(value);
  char *nv = norm(type, value);
  if (!nv) return 0;
  char hash[41]; sha1_hex(nv, strlen(nv), hash); free(nv);

  char sp[1024]; shard_path(type, hash, sp, sizeof sp);
  FILE *f = fopen(sp, "rb");

  cJSON *root = cJSON_CreateObject();
  cJSON *arr = cJSON_CreateArray();
  int matches = 0; long long pw_count = 0;

  if (f) {
    char ln[16384];
    while (fgets(ln, sizeof ln, f)) {
      if (strncmp(ln, hash, 40) != 0 || ln[40] != '\t') continue;
      matches++;
      if (type == BT_PASSWORD) {
        pw_count = atoll(ln + 41);
        /* A password hash is unique within its shard by construction (the
         * ingest dedups on the hash alone), so there is nothing further to
         * find. Without this the scan always ran to EOF — and after a bulk
         * ingest a shard is hundreds of MB, read line-by-line, on the single
         * mongoose event-loop thread. */
        break;
      } else {
        char *p = ln + 41;
        char *tab = strchr(p, '\t');
        char *breach = p, *enc = NULL;
        if (tab) { *tab = 0; enc = tab + 1; char *nl = strpbrk(enc, "\r\n"); if (nl) *nl = 0; }
        else { char *nl = strpbrk(breach, "\r\n"); if (nl) *nl = 0; }
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "breach", breach);
        if (reveal && enc && enc[0] != '-') {
          char *pt = dec_secret(enc);
          if (pt) { cJSON_AddStringToObject(o, "secret", pt); free(pt); }
        }
        cJSON_AddItemToArray(arr, o);
      }
    }
    fclose(f);
  }

  cJSON_AddBoolToObject(root, "found", matches > 0);
  cJSON_AddNumberToObject(root, "count", matches);
  cJSON_AddStringToObject(root, "type", breach_type_name(type));
  if (type == BT_PASSWORD) {
    cJSON_AddNumberToObject(root, "pwn_count", (double)pw_count);
    cJSON_AddBoolToObject(root, "compromised", pw_count > 0);
    cJSON_Delete(arr);
  } else {
    cJSON_AddItemToObject(root, "breaches", arr);
  }

  if (out) *out = root; else cJSON_Delete(root);
  return matches;
}
