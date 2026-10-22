/* core/source_settings.c — see source_settings.h for why the key set is
 * closed and why `enabled` is separate from the value. */
#include "source_settings.h"
#include "source_interval.h"
#include "cadence.h"
#include "scheduler.h"
#include "../source.h"
#include "../third_party/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* THE WHITELIST. One row per knob: the key an API caller and the UI both name,
 * the bounds a value must fall inside, the default the resolver returns when
 * the setting is absent or disabled, and a one-line explanation the settings
 * page shows so an operator is not guessing what they are changing.
 *
 * `lo` is a real floor, not decoration: a body ceiling below the 64 MB default
 * would be a setting that makes a source fetch LESS while reading as an
 * allowance, so the minimum is the default itself. Lowering the global bound is
 * $JO_HTTP_MAX_BODY's job and it still wins over anything here. */
typedef struct {
  const char *key;
  long lo, hi, def;
  const char *unit;
  const char *label;
  const char *help;
} srcset_key;

#define MB (1024L * 1024L)

static const srcset_key KEYS[] = {
  { SRCSET_BODY_CEILING, 64 * MB, 512 * MB, 64 * MB, "bytes",
    "Large response allowance",
    "How large a single response body this source may hold in memory. The "
    "default 64 MB is a security bound against a compressed body that "
    "inflates to many gigabytes, so raise it only for a source whose "
    "complete answer is one known-large file — ods-public-geonames needs "
    "~111 MB for the 171,176-record export it cannot page. The ceiling is "
    "still enforced on the decompressed stream, and an explicit "
    "$JO_HTTP_MAX_BODY caps this." },
  { SRCSET_INTERVAL, SI_MIN_SEC, SI_MAX_SEC, 0, "seconds",
    "Collection interval",
    "How often this source runs, set for this source alone. It outranks the "
    "system-wide cadence policy (the Sources page) and the interval the "
    "source declares or measures from its own items — specific beats "
    "general. Off, the policy decides; the number is kept. Takes effect in "
    "the running scheduler without a restart. Not offered for an entity "
    "pivot, which runs when asked." },
};
static const int NKEYS = (int)(sizeof KEYS / sizeof KEYS[0]);

/* The interval key is a FACADE over core/source_interval.c: that module owns
 * the row, the cache the scheduler reads every tick and the generation that
 * re-plans it. Storing a copy here would be two truths about one cadence. */
static int delegated(const srcset_key *k) {
  return k && strcmp(k->key, SRCSET_INTERVAL) == 0;
}

static const srcset_key *find_key(const char *key) {
  if (!key) return NULL;
  for (int i = 0; i < NKEYS; i++)
    if (strcmp(KEYS[i].key, key) == 0) return &KEYS[i];
  return NULL;
}

/* strtol with the whole string required to be the number, so "64x" and "" are
 * refused rather than read as 64 and 0. */
static int parse_long(const char *s, long *out) {
  if (!s || !*s) return -1;
  char *end = NULL;
  long v = strtol(s, &end, 10);
  if (!end || *end) return -1;
  *out = v;
  return 0;
}

long source_settings_int(db_handle *db, const char *source_id,
                         const char *key, long def) {
  const srcset_key *k = find_key(key);
  if (!k || !db || !db->h || !source_id || !*source_id) return def;
  if (delegated(k)) { long ov = source_interval_for(source_id); return ov > 0 ? ov : def; }

  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h,
        "SELECT value FROM source_settings "
        "WHERE source_id=?1 AND key=?2 AND enabled=1", -1, &s, NULL)
      != SQLITE_OK)
    return def;                          /* an older DB without the table */
  sqlite3_bind_text(s, 1, source_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, key, -1, SQLITE_TRANSIENT);

  long out = def;
  if (sqlite3_step(s) == SQLITE_ROW) {
    const char *v = (const char *)sqlite3_column_text(s, 0);
    long parsed = 0;
    /* Re-validated on the way OUT, not only where it was written. A row can
     * arrive from an older build, a restored backup or a hand-edited database,
     * and this is the one place every read passes through — the same reasoning
     * url_override_reload() documents for its own rows. */
    if (parse_long(v, &parsed) == 0 && parsed >= k->lo && parsed <= k->hi)
      out = parsed;
    else
      fprintf(stderr, "[source-settings] %s/%s: stored value %s is outside "
                      "[%ld, %ld]; using the default %ld\n",
              source_id, key, v ? v : "(null)", k->lo, k->hi, def);
  }
  sqlite3_finalize(s);
  return out;
}

int source_settings_put(db_handle *db, const char *source_id, const char *key,
                        const char *value, int enabled, const char *who) {
  const srcset_key *k = find_key(key);
  if (!k) return -1;
  if (!db || !db->h || !source_id || !*source_id) return -3;

  long v = 0;
  if (parse_long(value, &v) != 0 || v < k->lo || v > k->hi) return -2;

  if (delegated(k)) {
    /* source_interval_set validates the id against the REGISTRY (a setting for
     * an id nobody registers is -3 here too), refuses an entity pivot (no
     * schedule to set: -2, the value cannot apply) and the same bounds. */
    const char *err = NULL;
    if (source_interval_set(db, source_id, v, enabled, who, &err) == 0) return 0;
    if (err && strstr(err, "no registered source")) return -3;
    if (err && (strstr(err, "no schedule") || strstr(err, "under 10 s") || strstr(err, "longer than a year"))) return -2;
    return -4;
  }

  /* The source must exist. Storing a setting for an id nobody registers is a
   * knob that can never apply, which reads as configured and is not. */
  sqlite3_stmt *s = NULL;
  if (sqlite3_prepare_v2(db->h, "SELECT 1 FROM sources WHERE id=?1", -1, &s,
                         NULL) != SQLITE_OK)
    return -4;
  sqlite3_bind_text(s, 1, source_id, -1, SQLITE_TRANSIENT);
  int have = (sqlite3_step(s) == SQLITE_ROW);
  sqlite3_finalize(s);
  if (!have) return -3;

  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO source_settings(source_id,key,value,enabled,updated_at,"
        "updated_by) VALUES(?1,?2,?3,?4,datetime('now'),?5) "
        "ON CONFLICT(source_id,key) DO UPDATE SET value=excluded.value,"
        "enabled=excluded.enabled,updated_at=excluded.updated_at,"
        "updated_by=excluded.updated_by", -1, &s, NULL) != SQLITE_OK)
    return -4;
  sqlite3_bind_text(s, 1, source_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, key, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(s, 3, (sqlite3_int64)v);
  sqlite3_bind_int(s, 4, enabled ? 1 : 0);
  if (who && *who) sqlite3_bind_text(s, 5, who, -1, SQLITE_TRANSIENT);
  else sqlite3_bind_null(s, 5);
  int rc = sqlite3_step(s);
  sqlite3_finalize(s);
  if (rc != SQLITE_DONE) return -4;

  fprintf(stderr, "[source-settings] %s/%s = %ld (%s) by %s\n",
          source_id, key, v, enabled ? "enabled" : "disabled",
          who && *who ? who : "unknown");
  return 0;
}

char *source_settings_json(db_handle *db, const char *source_id,
                           int can_write) {
  cJSON *root = cJSON_CreateObject();
  if (!root) return NULL;
  cJSON_AddBoolToObject(root, "can_write", can_write ? 1 : 0);
  cJSON *arr = cJSON_AddArrayToObject(root, "settings");

  const source_def *sd = (source_id && *source_id) ? registry_get(source_id) : NULL;
  for (int i = 0; i < NKEYS; i++) {
    const srcset_key *k = &KEYS[i];
    if (delegated(k)) {
      /* The interval knob exists only for a row that has a schedule, and its
       * "default" is not a constant: it is what runs WITHOUT an override —
       * the declared or measured interval under the system-wide policy
       * (sched_state_base + cadence_apply), which differs per source. */
      if (!sd || sd->update_interval_sec <= 0) continue;
      sched_state st = {0};
      if (db && db->h) sched_state_load_one(db, source_id, &st);
      long base   = cadence_apply(sched_state_base(&st, sd->update_interval_sec, sd->adaptive_interval));
      source_interval ov; source_interval_get(db, source_id, &ov);
      long eff = (ov.present && ov.enabled && ov.interval_sec > 0) ? ov.interval_sec : base;
      cJSON *o = cJSON_CreateObject();
      cJSON_AddStringToObject(o, "key", k->key);
      cJSON_AddStringToObject(o, "label", k->label);
      cJSON_AddStringToObject(o, "help", k->help);
      cJSON_AddStringToObject(o, "unit", k->unit);
      cJSON_AddNumberToObject(o, "min", (double)k->lo);
      cJSON_AddNumberToObject(o, "max", (double)k->hi);
      cJSON_AddNumberToObject(o, "default", (double)base);
      cJSON_AddStringToObject(o, "default_basis", strcmp(cadence_mode_for(sched_state_base(&st, sd->update_interval_sec, sd->adaptive_interval)), "declared")
                              ? "the system-wide policy" : (sd->adaptive_interval && st.freshness_interval > 0) ? "measured from its newest item" : "as declared");
      cJSON_AddNumberToObject(o, "value", (double)(ov.present ? ov.interval_sec : base));
      cJSON_AddBoolToObject(o, "enabled", ov.present && ov.enabled);
      cJSON_AddBoolToObject(o, "configured", ov.present);
      cJSON_AddNumberToObject(o, "effective", (double)eff);
      if (ov.present && ov.updated_at[0]) cJSON_AddStringToObject(o, "updated_at", ov.updated_at);
      if (ov.present && ov.updated_by[0]) cJSON_AddStringToObject(o, "updated_by", ov.updated_by);
      cJSON_AddItemToArray(arr, o);
      continue;
    }
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "key", k->key);
    cJSON_AddStringToObject(o, "label", k->label);
    cJSON_AddStringToObject(o, "help", k->help);
    cJSON_AddStringToObject(o, "unit", k->unit);
    cJSON_AddNumberToObject(o, "min", (double)k->lo);
    cJSON_AddNumberToObject(o, "max", (double)k->hi);
    cJSON_AddNumberToObject(o, "default", (double)k->def);

    /* The stored row, whether or not it is active — the UI needs the value an
     * operator tuned even while the knob is off, or a re-enable would show a
     * default they did not choose. */
    long value = k->def;
    int enabled = 0, present = 0;
    const char *updated_at = NULL, *updated_by = NULL;
    char ua[64] = {0}, ub[128] = {0};
    if (db && db->h && source_id && *source_id) {
      sqlite3_stmt *s = NULL;
      if (sqlite3_prepare_v2(db->h,
            "SELECT value,enabled,updated_at,updated_by FROM source_settings "
            "WHERE source_id=?1 AND key=?2", -1, &s, NULL) == SQLITE_OK) {
        sqlite3_bind_text(s, 1, source_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s, 2, k->key, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(s) == SQLITE_ROW) {
          present = 1;
          long parsed = 0;
          if (parse_long((const char *)sqlite3_column_text(s, 0), &parsed) == 0)
            value = parsed;
          enabled = sqlite3_column_int(s, 1);
          const char *t = (const char *)sqlite3_column_text(s, 2);
          const char *b = (const char *)sqlite3_column_text(s, 3);
          if (t) { snprintf(ua, sizeof ua, "%s", t); updated_at = ua; }
          if (b) { snprintf(ub, sizeof ub, "%s", b); updated_by = ub; }
        }
        sqlite3_finalize(s);
      }
    }
    cJSON_AddNumberToObject(o, "value", (double)value);
    cJSON_AddBoolToObject(o, "enabled", enabled ? 1 : 0);
    cJSON_AddBoolToObject(o, "configured", present ? 1 : 0);
    /* What the code will ACTUALLY use, so the page states the effect rather
     * than leaving the reader to combine value and enabled themselves. */
    cJSON_AddNumberToObject(o, "effective",
                            (double)(enabled && present ? value : k->def));
    if (updated_at) cJSON_AddStringToObject(o, "updated_at", updated_at);
    if (updated_by) cJSON_AddStringToObject(o, "updated_by", updated_by);
    cJSON_AddItemToArray(arr, o);
  }

  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return out;
}
