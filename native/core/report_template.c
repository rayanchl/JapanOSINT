/* core/report_template.c — see report_template.h. */
#include "report_template.h"
#include "reportapi.h"           /* report_template: the reader's buffers */
#include "audit.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The four fields, each bounded by the reader's buffer (reportapi.h) less the
 * NUL — so a saved value is never cut when a report loads it. */
#define RT_FIELD(name, multiline) \
  { #name, sizeof(((report_template *)0)->name) - 1, multiline }
typedef struct { const char *key; size_t max; int multiline; } rt_field;
static const rt_field FIELDS[] = {
  RT_FIELD(classification, 0),
  RT_FIELD(header_note,    0),
  RT_FIELD(footer_md,      1),
  RT_FIELD(logo_url,       0),
};
#define NFIELDS ((int)(sizeof FIELDS / sizeof FIELDS[0]))
#define F_LOGO 3

static void rt_migrate(db_handle *db) {
  /* schema.sql's declaration, for a database created without it. */
  sqlite3_exec(db->h,
    "CREATE TABLE IF NOT EXISTS tenant_report_templates ("
    " tenant_id TEXT PRIMARY KEY REFERENCES tenants(id) ON DELETE CASCADE,"
    " classification TEXT, header_note TEXT, footer_md TEXT, logo_url TEXT,"
    " updated_at TEXT NOT NULL DEFAULT (datetime('now')))", NULL, NULL, NULL);
}

static int can_write(const tenant_ctx *t) {
  return !strcmp(t->role, "owner") || !strcmp(t->role, "admin");
}

static char *err_body(const char *error, const char *detail) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", error);
  if (detail) cJSON_AddStringToObject(o, "detail", detail);
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}

/* vals[i] = malloc'd copy of the stored field or NULL; returns 1 if a row. */
static int load_row(db_handle *db, const char *tenant_id, char *vals[NFIELDS],
                    char *updated_at, size_t ua_n) {
  for (int i = 0; i < NFIELDS; i++) vals[i] = NULL;
  if (updated_at && ua_n) updated_at[0] = 0;
  sqlite3_stmt *s; int found = 0;
  if (sqlite3_prepare_v2(db->h,
        "SELECT classification, header_note, footer_md, logo_url, updated_at "
        "FROM tenant_report_templates WHERE tenant_id=?1", -1, &s, NULL)
      != SQLITE_OK) return 0;
  sqlite3_bind_text(s, 1, tenant_id, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) {
    found = 1;
    for (int i = 0; i < NFIELDS; i++) {
      const char *v = (const char *)sqlite3_column_text(s, i);
      if (v && *v) vals[i] = strdup(v);
    }
    const char *ua = (const char *)sqlite3_column_text(s, 4);
    if (ua && updated_at) snprintf(updated_at, ua_n, "%s", ua);
  }
  sqlite3_finalize(s);
  return found;
}

static cJSON *template_json(char *vals[NFIELDS], int present,
                            const char *updated_at) {
  cJSON *o = cJSON_CreateObject();
  for (int i = 0; i < NFIELDS; i++)
    cJSON_AddStringToObject(o, FIELDS[i].key, vals[i] ? vals[i] : "");
  cJSON_AddBoolToObject(o, "present", present);
  if (present && updated_at && *updated_at)
    cJSON_AddStringToObject(o, "updated_at", updated_at);
  else cJSON_AddNullToObject(o, "updated_at");
  return o;
}

static void add_meta(cJSON *root, const tenant_ctx *t) {
  cJSON *m = cJSON_AddObjectToObject(root, "meta");
  cJSON_AddStringToObject(m, "tenant_id", t->tenant_id);
  cJSON_AddStringToObject(m, "role", t->role);
  cJSON_AddBoolToObject(m, "can_write", can_write(t));
  cJSON *lim = cJSON_AddObjectToObject(m, "limits");
  for (int i = 0; i < NFIELDS; i++)
    cJSON_AddNumberToObject(lim, FIELDS[i].key, (double)FIELDS[i].max);
  cJSON_AddStringToObject(m, "logo_url_rule",
    "https:// only; never fetched by the server (the HTML report's reader "
    "loads it)");
}

static void free_vals(char *vals[NFIELDS]) {
  for (int i = 0; i < NFIELDS; i++) { free(vals[i]); vals[i] = NULL; }
}

char *report_template_api_get(db_handle *db, const tenant_ctx *t, int *status) {
  rt_migrate(db);
  char *vals[NFIELDS]; char ua[40];
  int present = load_row(db, t->tenant_id, vals, ua, sizeof ua);
  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "data", template_json(vals, present, ua));
  add_meta(root, t);
  free_vals(vals);
  char *s = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  *status = 200;
  return s;
}

/* NULL when `v` is acceptable for field `f`, else the reason. */
static const char *check_field(const rt_field *f, int idx, const char *v,
                               char *why, size_t wn) {
  size_t n = strlen(v);
  if (n > f->max) {
    snprintf(why, wn, "%s is %zu bytes; at most %zu are kept by the report "
             "renderer", f->key, n, f->max);
    return why;
  }
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)v[i];
    if (c == 0x7f || (c < 0x20 && !(f->multiline && (c == '\n' || c == '\t' ||
                                                     c == '\r')))) {
      snprintf(why, wn, "%s carries a control character at byte %zu%s", f->key,
               i, f->multiline ? "" : " (it is one line)");
      return why;
    }
  }
  if (idx == F_LOGO && n) {
    /* https only, a host after the scheme, and reportapi safe_url()'s
     * character rule (no space, quotes or angle brackets), or the renderer
     * would drop it without saying so. */
    if (strncmp(v, "https://", 8) || n == 8 || v[8] == '/') {
      snprintf(why, wn, "logo_url must be an https:// URL with a host");
      return why;
    }
    for (size_t i = 0; i < n; i++) {
      unsigned char c = (unsigned char)v[i];
      if (c < 0x21 || c == '"' || c == '\'' || c == '<' || c == '>') {
        snprintf(why, wn, "logo_url carries a character a report cannot "
                 "emit in a URL (byte %zu)", i);
        return why;
      }
    }
  }
  return NULL;
}

char *report_template_api_put(db_handle *db, const tenant_ctx *t,
                              const char *body, size_t body_len, int *status) {
  if (!can_write(t)) {
    *status = 403;
    return err_body("forbidden", "editing the report template requires the "
                    "workspace owner or admin role");
  }
  cJSON *jb = (body && body_len) ? cJSON_ParseWithLength(body, body_len) : NULL;
  if (!cJSON_IsObject(jb)) {
    cJSON_Delete(jb);
    *status = 400;
    return err_body("bad_request", "body must be a JSON object of "
                    "classification / header_note / footer_md / logo_url");
  }
  /* The key set is closed: an unknown key is refused by name, never ignored —
   * a typo would otherwise read as a saved setting. */
  for (cJSON *it = jb->child; it; it = it->next) {
    int known = 0;
    for (int i = 0; i < NFIELDS; i++)
      if (it->string && !strcmp(it->string, FIELDS[i].key)) known = 1;
    if (!known) {
      char det[160];
      snprintf(det, sizeof det, "unknown field '%.100s'", it->string ? it->string : "");
      cJSON_Delete(jb);
      *status = 400;
      return err_body("unknown_field", det);
    }
    if (!cJSON_IsString(it) && !cJSON_IsNull(it)) {
      char det[160];
      snprintf(det, sizeof det, "%.100s must be a string or null", it->string);
      cJSON_Delete(jb);
      *status = 400;
      return err_body("bad_request", det);
    }
  }

  rt_migrate(db);
  char *old[NFIELDS], *nv[NFIELDS];
  int had = load_row(db, t->tenant_id, old, NULL, 0);
  /* Absent field = keep; string = replace; null or "" = clear. */
  for (int i = 0; i < NFIELDS; i++) {
    const cJSON *jv = cJSON_GetObjectItem(jb, FIELDS[i].key);
    if (!jv) { nv[i] = old[i] ? strdup(old[i]) : NULL; continue; }
    const char *v = cJSON_IsString(jv) ? jv->valuestring : NULL;
    char why[200];
    if (v && *v && check_field(&FIELDS[i], i, v, why, sizeof why)) {
      for (int k = 0; k < i; k++) free(nv[k]);
      free_vals(old);
      cJSON_Delete(jb);
      *status = 400;
      return err_body("invalid_field", why);
    }
    nv[i] = (v && *v) ? strdup(v) : NULL;
  }
  cJSON_Delete(jb);

  int any = 0;
  for (int i = 0; i < NFIELDS; i++) if (nv[i]) any = 1;
  sqlite3_stmt *s = NULL; int rc;
  if (any) {
    rc = sqlite3_prepare_v2(db->h,
      "INSERT INTO tenant_report_templates(tenant_id, classification, "
      "header_note, footer_md, logo_url, updated_at) "
      "VALUES(?1, ?2, ?3, ?4, ?5, datetime('now')) "
      "ON CONFLICT(tenant_id) DO UPDATE SET classification=excluded.classification, "
      "header_note=excluded.header_note, footer_md=excluded.footer_md, "
      "logo_url=excluded.logo_url, updated_at=excluded.updated_at",
      -1, &s, NULL);
  } else {
    rc = sqlite3_prepare_v2(db->h,
      "DELETE FROM tenant_report_templates WHERE tenant_id=?1", -1, &s, NULL);
  }
  if (rc == SQLITE_OK) {
    sqlite3_bind_text(s, 1, t->tenant_id, -1, SQLITE_TRANSIENT);
    if (any)
      for (int i = 0; i < NFIELDS; i++) {
        if (nv[i]) sqlite3_bind_text(s, 2 + i, nv[i], -1, SQLITE_TRANSIENT);
        else       sqlite3_bind_null(s, 2 + i);
      }
    rc = sqlite3_step(s) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
  }
  sqlite3_finalize(s);
  if (rc != SQLITE_OK) {
    free_vals(old); free_vals(nv);
    *status = 500;
    return err_body("server_error", NULL);
  }

  /* Audit: which fields changed and their lengths — and the classification
   * and logo values themselves, which are short labels, not content. */
  cJSON *ap = cJSON_CreateObject();
  cJSON *ch = cJSON_AddArrayToObject(ap, "changed");
  for (int i = 0; i < NFIELDS; i++) {
    const char *a = old[i] ? old[i] : "", *b = nv[i] ? nv[i] : "";
    if (strcmp(a, b)) cJSON_AddItemToArray(ch, cJSON_CreateString(FIELDS[i].key));
  }
  cJSON_AddStringToObject(ap, "classification", nv[0] ? nv[0] : "");
  cJSON_AddStringToObject(ap, "logo_url", nv[F_LOGO] ? nv[F_LOGO] : "");
  cJSON_AddNumberToObject(ap, "footer_md_bytes", nv[2] ? (double)strlen(nv[2]) : 0);
  cJSON_AddBoolToObject(ap, "had_template", had);
  cJSON_AddBoolToObject(ap, "has_template", any);
  char *pj = cJSON_PrintUnformatted(ap);
  cJSON_Delete(ap);
  audit_write(db, t->tenant_id, t->user_id, "report_template.put",
              "tenant_report_templates", pj ? pj : "{}");
  free(pj);
  free_vals(old); free_vals(nv);
  return report_template_api_get(db, t, status);
}
