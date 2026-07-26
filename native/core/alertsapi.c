#include "alertsapi.h"
#include "alert_deliver.h"
#include "breach_adapter.h"
#include "alert_eval.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <openssl/rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void uuid4(char out[37]) {
  unsigned char b[16]; RAND_bytes(b, 16);
  b[6] = (b[6] & 0x0F) | 0x40; b[8] = (b[8] & 0x3F) | 0x80;
  snprintf(out, 37,
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
    b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
}
static const char *ctext(sqlite3_stmt *s, int i) {
  return sqlite3_column_type(s, i) == SQLITE_NULL
           ? NULL : (const char *)sqlite3_column_text(s, i);
}
static char *err(int *st, int code, const char *msg) {
  *st = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", msg);
  char *j = cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}
static cJSON *safe_json(const char *s, int as_array) {
  if (s && *s) { cJSON *j = cJSON_Parse(s); if (j) return j; }
  return as_array ? cJSON_CreateArray() : cJSON_CreateObject();
}

/* {data:{decoded rule}} — webhook secrets masked, never echoed. */
static cJSON *decode_row(sqlite3_stmt *s) {
  /* cols: id,name,enabled,predicate_json,channels_json,dedup_window_sec,
   *       storm_cap_per_hour,muted_until,created_at,updated_at */
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "id", (const char *)sqlite3_column_text(s,0));
  cJSON_AddStringToObject(o, "name", (const char *)sqlite3_column_text(s,1));
  cJSON_AddBoolToObject(o, "enabled", sqlite3_column_int(s,2) != 0);
  cJSON_AddItemToObject(o, "predicate", safe_json(ctext(s,3), 0));
  cJSON *ch = safe_json(ctext(s,4), 1);
  if (cJSON_IsArray(ch)) {
    cJSON *c;
    cJSON_ArrayForEach(c, ch) {
      cJSON *ty = cJSON_GetObjectItem(c, "type");
      cJSON *se = cJSON_GetObjectItem(c, "secret");
      if (ty && cJSON_IsString(ty) && strcmp(ty->valuestring,"webhook")==0
          && se && cJSON_IsString(se) && se->valuestring[0]) {
        cJSON_ReplaceItemInObject(c, "secret", cJSON_CreateString("••••"));
      }
    }
  }
  cJSON_AddItemToObject(o, "channels", ch);
  cJSON_AddItemToObject(o, "dedup_window_sec",
                        cJSON_CreateNumber(sqlite3_column_int64(s,5)));
  cJSON_AddItemToObject(o, "storm_cap_per_hour",
                        cJSON_CreateNumber(sqlite3_column_int64(s,6)));
  const char *mu = ctext(s,7);
  cJSON_AddItemToObject(o, "muted_until",
                        mu ? cJSON_CreateString(mu) : cJSON_CreateNull());
  cJSON_AddStringToObject(o, "created_at", (const char *)sqlite3_column_text(s,8));
  cJSON_AddStringToObject(o, "updated_at", (const char *)sqlite3_column_text(s,9));
  return o;
}

static const char *DECODE_COLS =
  "SELECT id,name,enabled,predicate_json,channels_json,dedup_window_sec,"
  "storm_cap_per_hour,muted_until,created_at,updated_at FROM alert_rules "
  "WHERE id=?1 AND tenant_id=?2";

static char *one_rule(db_handle *db, const char *tid, const char *id,
                      int code, int *st) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, DECODE_COLS, -1, &s, NULL) != SQLITE_OK)
    return err(st, 500, "server_error");
  sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(s,2,tid,-1,SQLITE_TRANSIENT);
  char *out;
  if (sqlite3_step(s) == SQLITE_ROW) {
    cJSON *w = cJSON_CreateObject();
    cJSON_AddItemToObject(w, "data", decode_row(s));
    out = cJSON_PrintUnformatted(w); cJSON_Delete(w); *st = code;
  } else {
    out = err(st, 404, "Not found");
  }
  sqlite3_finalize(s);
  return out;
}

/* validateRule → fills *name,*pred,*chans (owned by caller via root),
 * enabled, dedup, storm. Returns NULL ok, else an error message. */
static const char *validate_rule(cJSON *b, const char **name, int *enabled,
    cJSON **predicate, cJSON **channels, long *dedup, long *storm) {
  if (!b || !cJSON_IsObject(b)) return "body required";
  cJSON *jn = cJSON_GetObjectItem(b, "name");
  const char *nm = (jn && cJSON_IsString(jn)) ? jn->valuestring : "";
  /* trim */
  while (*nm == ' ' || *nm == '\t' || *nm == '\n' || *nm == '\r') nm++;
  size_t nl = strlen(nm);
  while (nl && (nm[nl-1]==' '||nm[nl-1]=='\t'||nm[nl-1]=='\n'||nm[nl-1]=='\r')) nl--;
  if (nl == 0) return "name required";
  if (nl > 200) return "name too long";
  static char nbuf[256];
  snprintf(nbuf, sizeof nbuf, "%.*s", (int)nl, nm);
  *name = nbuf;

  cJSON *pred = cJSON_GetObjectItem(b, "predicate");
  if (!pred || cJSON_IsNull(pred)) pred = NULL;
  if (pred && (!cJSON_IsObject(pred))) return "predicate must be an object";
  if (pred) {
    cJSON *x;
    if ((x=cJSON_GetObjectItem(pred,"q")) && !cJSON_IsNull(x) && !cJSON_IsString(x))
      return "predicate.q must be string";
    if ((x=cJSON_GetObjectItem(pred,"source_ids")) && !cJSON_IsArray(x))
      return "predicate.source_ids must be array";
    if ((x=cJSON_GetObjectItem(pred,"tags_any")) && !cJSON_IsArray(x))
      return "predicate.tags_any must be array";
    if ((x=cJSON_GetObjectItem(pred,"tags_all")) && !cJSON_IsArray(x))
      return "predicate.tags_all must be array";
    /* ── item 9: at most one spatial term ──────────────────────────
     * Two spatial terms in one predicate is an authoring mistake, not a
     * conjunction — rejecting it here beats failing closed silently at
     * evaluation time, when nobody is watching. */
    cJSON *sb = cJSON_GetObjectItem(pred,"bbox");
    cJSON *sp = cJSON_GetObjectItem(pred,"polygon");
    cJSON *sc = cJSON_GetObjectItem(pred,"circle");
    cJSON *sa = cJSON_GetObjectItem(pred,"aoi_id");
    if (sb && cJSON_IsNull(sb)) sb = NULL;
    if (sp && cJSON_IsNull(sp)) sp = NULL;
    if (sc && cJSON_IsNull(sc)) sc = NULL;
    if (sa && cJSON_IsNull(sa)) sa = NULL;
    if ((!!sb + !!sp + !!sc + !!sa) > 1)
      return "predicate may carry only one of bbox/polygon/circle/aoi_id";
    if (sb && (!cJSON_IsArray(sb) || cJSON_GetArraySize(sb) != 4))
      return "predicate.bbox must be [w,s,e,n]";
    if (sp) {
      char *gj = cJSON_PrintUnformatted(sp);
      const char *why = alert_eval_shape_check("polygon", gj, 0,0,0,0);
      free(gj);
      if (why) return why;          /* static string, safe to return */
    }
    if (sc) {
      char *gj = cJSON_PrintUnformatted(sc);
      const char *why = alert_eval_shape_check("circle", gj, 0,0,0,0);
      free(gj);
      if (why) return why;
    }
    if (sa && (!cJSON_IsString(sa) || !sa->valuestring[0]))
      return "predicate.aoi_id must be a non-empty string";
    /* aoi_id existence is deliberately NOT checked here: a 404-on-save would
     * make a rule un-editable the moment its AOI is deleted. Existence is a
     * matching question, and the evaluator fails closed on an unresolvable
     * id; aoiapi.c refuses (409) to delete an AOI a rule still references. */
    /* ── item 21: entity terms ─────────────────────────────────────── */
    if ((x=cJSON_GetObjectItem(pred,"entity_ids")) && !cJSON_IsNull(x) &&
        !cJSON_IsArray(x))
      return "predicate.entity_ids must be array";
    if ((x=cJSON_GetObjectItem(pred,"entity_types")) && !cJSON_IsNull(x) &&
        !cJSON_IsArray(x))
      return "predicate.entity_types must be array";
    /* Query authoring mode: "fts" (default) matches the FTS predicate.q;
     * "llm" carries a natural-language query (nl_query) driving the agentic
     * search pipeline. Both round-trip opaquely in predicate_json. */
    if ((x=cJSON_GetObjectItem(pred,"mode")) && !cJSON_IsNull(x) &&
        (!cJSON_IsString(x) ||
         (strcmp(x->valuestring,"fts")!=0 && strcmp(x->valuestring,"llm")!=0)))
      return "predicate.mode must be \"fts\" or \"llm\"";
    if ((x=cJSON_GetObjectItem(pred,"nl_query")) && !cJSON_IsNull(x) && !cJSON_IsString(x))
      return "predicate.nl_query must be string";
  }
  *predicate = pred ? cJSON_Duplicate(pred,1) : cJSON_CreateObject();

  cJSON *chans = cJSON_GetObjectItem(b, "channels");
  if (!chans || cJSON_IsNull(chans)) chans = NULL;
  if (!chans || !cJSON_IsArray(chans) || cJSON_GetArraySize(chans) == 0) {
    cJSON_Delete(*predicate); return "at least one channel required";
  }
  cJSON *ch;
  cJSON_ArrayForEach(ch, chans) {
    cJSON *ty = ch ? cJSON_GetObjectItem(ch,"type") : NULL;
    const char *tys = (ty && cJSON_IsString(ty)) ? ty->valuestring : NULL;
    if (!tys || (strcmp(tys,"email")!=0 && strcmp(tys,"webhook")!=0)) {
      cJSON_Delete(*predicate);
      static char eb[64]; snprintf(eb,sizeof eb,"invalid channel type: %s",
        tys?tys:"undefined"); return eb;
    }
    cJSON *tg = cJSON_GetObjectItem(ch,"target");
    if (!tg || !cJSON_IsString(tg)) {
      cJSON_Delete(*predicate);
      static char eb[64]; snprintf(eb,sizeof eb,"channel %s missing target",tys);
      return eb;
    }
    if (strcmp(tys,"email")==0) {
      /* /^[^\s@]+@[^\s@]+\.[^\s@]+$/ */
      const char *t = tg->valuestring; const char *at = NULL, *dot = NULL;
      int bad = 0;
      for (const char *p=t; *p; p++) {
        if (*p==' '||*p=='\t'||*p=='\n'||*p=='\r') { bad=1; break; }
        if (*p=='@') { if (at) { bad=1; break; } at=p; }
        else if (*p=='.' && at) dot=p;
      }
      if (bad || !at || at==t || !dot || dot<=at+1 || !dot[1]) {
        cJSON_Delete(*predicate); return "email target is not a valid address";
      }
    } else { /* webhook */
      const char *t = tg->valuestring;
      if (strncmp(t,"http://",7)!=0 && strncmp(t,"https://",8)!=0) {
        cJSON_Delete(*predicate); return "webhook target must be http(s) URL";
      }
      cJSON *se = cJSON_GetObjectItem(ch,"secret");
      if (!se || !cJSON_IsString(se) || strlen(se->valuestring) < 16) {
        cJSON_Delete(*predicate);
        return "webhook secret required (≥16 chars)";
      }
    }
  }
  *channels = cJSON_Duplicate(chans,1);

  cJSON *jd = cJSON_GetObjectItem(b,"dedup_window_sec");
  cJSON *js = cJSON_GetObjectItem(b,"storm_cap_per_hour");
  double dd = (jd && cJSON_IsNumber(jd)) ? jd->valuedouble
            : (jd && !cJSON_IsNull(jd) && !cJSON_IsObject(jd)) ? NAN : 3600;
  double ss = (js && cJSON_IsNumber(js)) ? js->valuedouble
            : (js && !cJSON_IsNull(js) && !cJSON_IsObject(js)) ? NAN : 100;
  if (!(dd >= 0) || !isfinite(dd)) { cJSON_Delete(*predicate); cJSON_Delete(*channels); return "dedup_window_sec must be ≥0"; }
  if (!(ss >= 1) || !isfinite(ss)) { cJSON_Delete(*predicate); cJSON_Delete(*channels); return "storm_cap_per_hour must be ≥1"; }
  *dedup = (long)floor(dd);
  *storm = (long)floor(ss);

  cJSON *je = cJSON_GetObjectItem(b,"enabled");
  *enabled = !(je && cJSON_IsBool(je) && !cJSON_IsTrue(je)); /* !==false */
  return NULL;
}

char *alertsapi(db_handle *db, const char *tid, const char *uid,
                const char *method, const char *id, const char *action,
                const char *body, int ev_limit, int *st) {
  int is_get = strcmp(method,"GET")==0, is_post = strcmp(method,"POST")==0,
      is_patch = strcmp(method,"PATCH")==0, is_del = strcmp(method,"DELETE")==0;
  cJSON *jb = (body && *body) ? cJSON_Parse(body) : NULL;

  /* collection: GET list / POST create */
  if (!id[0]) {
    if (is_get) {
      sqlite3_stmt *s;
      sqlite3_prepare_v2(db->h,
        "SELECT id,name,enabled,predicate_json,channels_json,"
        "dedup_window_sec,storm_cap_per_hour,muted_until,created_at,updated_at "
        "FROM alert_rules WHERE tenant_id=?1 ORDER BY created_at DESC",
        -1, &s, NULL);
      sqlite3_bind_text(s,1,tid,-1,SQLITE_TRANSIENT);
      cJSON *arr = cJSON_CreateArray();
      while (sqlite3_step(s)==SQLITE_ROW) cJSON_AddItemToArray(arr, decode_row(s));
      sqlite3_finalize(s);
      cJSON *w = cJSON_CreateObject(); cJSON_AddItemToObject(w,"data",arr);
      char *o = cJSON_PrintUnformatted(w); cJSON_Delete(w);
      if (jb) cJSON_Delete(jb); *st=200; return o;
    }
    if (is_post) {
      const char *nm; int en; cJSON *pred=NULL,*chans=NULL; long dd,ss;
      const char *e = validate_rule(jb,&nm,&en,&pred,&chans,&dd,&ss);
      if (e) { if (jb) cJSON_Delete(jb); return err(st,400,e); }
      char nid[37]; uuid4(nid);
      char *pj=cJSON_PrintUnformatted(pred), *cj=cJSON_PrintUnformatted(chans);
      cJSON_Delete(pred); cJSON_Delete(chans);
      sqlite3_stmt *s;
      sqlite3_prepare_v2(db->h,
        "INSERT INTO alert_rules (id,tenant_id,name,enabled,predicate_json,"
        "channels_json,dedup_window_sec,storm_cap_per_hour,created_by) "
        "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9)", -1, &s, NULL);
      sqlite3_bind_text(s,1,nid,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,2,tid,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,3,nm,-1,SQLITE_TRANSIENT);
      sqlite3_bind_int (s,4,en?1:0);
      sqlite3_bind_text(s,5,pj,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,6,cj,-1,SQLITE_TRANSIENT);
      sqlite3_bind_int64(s,7,dd);
      sqlite3_bind_int64(s,8,ss);
      if (uid && *uid) sqlite3_bind_text(s,9,uid,-1,SQLITE_TRANSIENT);
      else sqlite3_bind_null(s,9);
      int rc = sqlite3_step(s); sqlite3_finalize(s);
      free(pj); free(cj);
      if (jb) cJSON_Delete(jb);
      if (rc != SQLITE_DONE) return err(st,500,"server_error");
      return one_rule(db,tid,nid,201,st);
    }
    if (jb) cJSON_Delete(jb);
    return err(st,404,"not_found");
  }

  /* item-level */
  if (!action[0]) {
    if (is_get) { if(jb)cJSON_Delete(jb); return one_rule(db,tid,id,200,st); }
    if (is_del) {
      sqlite3_stmt *s;
      sqlite3_exec(db->h,"BEGIN",0,0,0);
      sqlite3_prepare_v2(db->h,"DELETE FROM alert_events WHERE rule_id=?1 AND tenant_id=?2",-1,&s,NULL);
      sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT); sqlite3_bind_text(s,2,tid,-1,SQLITE_TRANSIENT);
      sqlite3_step(s); sqlite3_finalize(s);
      sqlite3_prepare_v2(db->h,"DELETE FROM alert_rules WHERE id=?1 AND tenant_id=?2",-1,&s,NULL);
      sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT); sqlite3_bind_text(s,2,tid,-1,SQLITE_TRANSIENT);
      sqlite3_step(s); sqlite3_finalize(s);
      sqlite3_exec(db->h,"COMMIT",0,0,0);
      if (jb) cJSON_Delete(jb);
      *st = 204; return NULL;
    }
    if (is_patch) {
      sqlite3_stmt *s;
      sqlite3_prepare_v2(db->h,
        "SELECT name,enabled,predicate_json,channels_json,dedup_window_sec,"
        "storm_cap_per_hour FROM alert_rules WHERE id=?1 AND tenant_id=?2",
        -1,&s,NULL);
      sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,2,tid,-1,SQLITE_TRANSIENT);
      if (sqlite3_step(s)!=SQLITE_ROW) { sqlite3_finalize(s); if(jb)cJSON_Delete(jb); return err(st,404,"Not found"); }
      cJSON *merged = cJSON_CreateObject();
      cJSON *bn = jb?cJSON_GetObjectItem(jb,"name"):NULL;
      cJSON_AddStringToObject(merged,"name",
        (bn&&cJSON_IsString(bn))?bn->valuestring:(const char*)sqlite3_column_text(s,0));
      cJSON *be = jb?cJSON_GetObjectItem(jb,"enabled"):NULL;
      cJSON_AddBoolToObject(merged,"enabled",
        (be&&cJSON_IsBool(be))?cJSON_IsTrue(be):(sqlite3_column_int(s,1)!=0));
      cJSON *bp = jb?cJSON_GetObjectItem(jb,"predicate"):NULL;
      cJSON_AddItemToObject(merged,"predicate",
        (bp&&!cJSON_IsNull(bp))?cJSON_Duplicate(bp,1):safe_json(ctext(s,2),0));
      cJSON *bc = jb?cJSON_GetObjectItem(jb,"channels"):NULL;
      cJSON_AddItemToObject(merged,"channels",
        (bc&&!cJSON_IsNull(bc))?cJSON_Duplicate(bc,1):safe_json(ctext(s,3),1));
      cJSON *bd = jb?cJSON_GetObjectItem(jb,"dedup_window_sec"):NULL;
      cJSON_AddItemToObject(merged,"dedup_window_sec",
        (bd&&!cJSON_IsNull(bd))?cJSON_Duplicate(bd,1):cJSON_CreateNumber(sqlite3_column_int64(s,4)));
      cJSON *bs = jb?cJSON_GetObjectItem(jb,"storm_cap_per_hour"):NULL;
      cJSON_AddItemToObject(merged,"storm_cap_per_hour",
        (bs&&!cJSON_IsNull(bs))?cJSON_Duplicate(bs,1):cJSON_CreateNumber(sqlite3_column_int64(s,5)));
      sqlite3_finalize(s);

      const char *nm; int en; cJSON *pred=NULL,*chans=NULL; long dd,ssv;
      const char *e = validate_rule(merged,&nm,&en,&pred,&chans,&dd,&ssv);
      if (e) { cJSON_Delete(merged); if(jb)cJSON_Delete(jb); return err(st,400,e); }
      char *pj=cJSON_PrintUnformatted(pred), *cj=cJSON_PrintUnformatted(chans);
      cJSON_Delete(pred); cJSON_Delete(chans); cJSON_Delete(merged);
      sqlite3_prepare_v2(db->h,
        "UPDATE alert_rules SET name=?1,enabled=?2,predicate_json=?3,"
        "channels_json=?4,dedup_window_sec=?5,storm_cap_per_hour=?6,"
        "updated_at=datetime('now') WHERE id=?7 AND tenant_id=?8",-1,&s,NULL);
      sqlite3_bind_text(s,1,nm,-1,SQLITE_TRANSIENT);
      sqlite3_bind_int(s,2,en?1:0);
      sqlite3_bind_text(s,3,pj,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,4,cj,-1,SQLITE_TRANSIENT);
      sqlite3_bind_int64(s,5,dd); sqlite3_bind_int64(s,6,ssv);
      sqlite3_bind_text(s,7,id,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,8,tid,-1,SQLITE_TRANSIENT);
      sqlite3_step(s); sqlite3_finalize(s); free(pj); free(cj);
      if (jb) cJSON_Delete(jb);
      return one_rule(db,tid,id,200,st);
    }
    if (jb) cJSON_Delete(jb);
    return err(st,404,"not_found");
  }

  /* sub-actions */
  if (is_post && strcmp(action,"mute")==0) {
    cJSON *d = jb?cJSON_GetObjectItem(jb,"duration_sec"):NULL;
    char expr[64];
    if (!d || cJSON_IsNull(d) || (cJSON_IsString(d) && strcmp(d->valuestring,"forever")==0)) {
      if (d && cJSON_IsString(d) && strcmp(d->valuestring,"forever")!=0) {
        if (jb) cJSON_Delete(jb);
        return err(st,400,"duration_sec must be positive number or \"forever\"");
      }
      /* forever (string) or explicit null. Missing (undefined) → invalid. */
      if (!d && !(jb && cJSON_HasObjectItem(jb,"duration_sec"))) {
        if (jb) cJSON_Delete(jb);
        return err(st,400,"duration_sec must be positive number or \"forever\"");
      }
      snprintf(expr,sizeof expr,"datetime('now', '+36500 days')");
    } else if (cJSON_IsNumber(d) && d->valuedouble > 0) {
      snprintf(expr,sizeof expr,"datetime('now', '+%ld seconds')",
               (long)floor(d->valuedouble));
    } else {
      if (jb) cJSON_Delete(jb);
      return err(st,400,"duration_sec must be positive number or \"forever\"");
    }
    char sql[256];
    snprintf(sql,sizeof sql,
      "UPDATE alert_rules SET muted_until=%s, updated_at=datetime('now') "
      "WHERE id=?1 AND tenant_id=?2", expr);
    sqlite3_stmt *s; sqlite3_prepare_v2(db->h,sql,-1,&s,NULL);
    sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,tid,-1,SQLITE_TRANSIENT);
    sqlite3_step(s); int ch=sqlite3_changes(db->h); sqlite3_finalize(s);
    if (jb) cJSON_Delete(jb);
    if (ch==0) return err(st,404,"Not found");
    *st=200; return strdup("{\"ok\":true}");
  }
  if (is_post && strcmp(action,"unmute")==0) {
    sqlite3_stmt *s;
    sqlite3_prepare_v2(db->h,
      "UPDATE alert_rules SET muted_until=NULL, updated_at=datetime('now') "
      "WHERE id=?1 AND tenant_id=?2",-1,&s,NULL);
    sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,tid,-1,SQLITE_TRANSIENT);
    sqlite3_step(s); int ch=sqlite3_changes(db->h); sqlite3_finalize(s);
    if (jb) cJSON_Delete(jb);
    if (ch==0) return err(st,404,"Not found");
    *st=200; return strdup("{\"ok\":true}");
  }
  if (is_get && strcmp(action,"events")==0) {
    int lim = ev_limit>0?ev_limit:100; if(lim<1)lim=1; if(lim>500)lim=500;
    sqlite3_stmt *s;
    sqlite3_prepare_v2(db->h,
      "SELECT e.id,e.item_uid,e.matched_at,e.delivered_channels_json,"
      "e.suppressed,e.reason,i.title,i.source_id,i.link "
      "FROM alert_events e LEFT JOIN intel_items i ON i.uid=e.item_uid "
      "WHERE e.rule_id=?1 AND e.tenant_id=?2 ORDER BY e.matched_at DESC LIMIT ?3",
      -1,&s,NULL);
    sqlite3_bind_text(s,1,id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,tid,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(s,3,lim);
    cJSON *arr=cJSON_CreateArray();
    while (sqlite3_step(s)==SQLITE_ROW) {
      cJSON *r=cJSON_CreateObject();
      cJSON_AddStringToObject(r,"id",(const char*)sqlite3_column_text(s,0));
      cJSON_AddStringToObject(r,"item_uid",(const char*)sqlite3_column_text(s,1));
      cJSON_AddStringToObject(r,"matched_at",(const char*)sqlite3_column_text(s,2));
      cJSON_AddItemToObject(r,"suppressed",
        cJSON_CreateNumber(sqlite3_column_int64(s,4)));
      const char *rs=ctext(s,5);
      cJSON_AddItemToObject(r,"reason", rs?cJSON_CreateString(rs):cJSON_CreateNull());
      cJSON_AddItemToObject(r,"delivered_channels", safe_json(ctext(s,3),1));
      const char *t=ctext(s,6),*sr=ctext(s,7),*lk=ctext(s,8);
      cJSON_AddItemToObject(r,"item_title", t?cJSON_CreateString(t):cJSON_CreateNull());
      cJSON_AddItemToObject(r,"item_source_id", sr?cJSON_CreateString(sr):cJSON_CreateNull());
      cJSON_AddItemToObject(r,"item_link", lk?cJSON_CreateString(lk):cJSON_CreateNull());
      cJSON_AddItemToArray(arr,r);
    }
    sqlite3_finalize(s);
    cJSON *w=cJSON_CreateObject(); cJSON_AddItemToObject(w,"data",arr);
    char *o=cJSON_PrintUnformatted(w); cJSON_Delete(w);
    if (jb) cJSON_Delete(jb); *st=200; return o;
  }
  if (is_post && strcmp(action,"test")==0) {
    if (jb) cJSON_Delete(jb);
    /* P0.3 — real dispatch. Synthesizes an event and pushes it through the
     * rule's configured channels immediately, so an operator can verify a
     * webhook BEFORE trusting it with real intel. Writes ledger rows under a
     * synthetic event id but never an alert_events row, so a test can't
     * pollute the inbox or be picked up again by the delivery worker.
     * Returns 404 internally when the rule isn't this tenant's. */
    return alert_deliver_test(db, tid, id, st);
  }

  if (jb) cJSON_Delete(jb);
  return err(st,404,"not_found");
}

/* ── /api/alert-events — the cross-rule notification inbox (item 11) ────────
 * /api/alerts/:id/events answers "what did THIS rule match"; an inbox has to
 * answer "what is waiting for ME", which is tenant-wide, newest-first and
 * stateful. Suppressed events are excluded by default: a storm-capped row
 * exists so the storm is diagnosable, not so it can bury the inbox it was
 * capping. read_at is added by db.c's ensure_column() boot migration. */
char *alerteventsapi(db_handle *db, const char *tid, const char *uid,
                     const char *method, const char *seg,
                     const char *qs, int *st) {
  int is_get = strcmp(method,"GET")==0, is_post = strcmp(method,"POST")==0;

  /* tiny query-string reader: qs is "a=1&b=2" (already URL-decoded enough for
   * our integer/flag params; no string values are read here). */
  int qflag = 0, qlimit = 0;
  if (qs && *qs) {
    const char *p = strstr(qs, "unread=");
    if (p) qflag = atoi(p + 7);
    p = strstr(qs, "limit=");
    if (p) qlimit = atoi(p + 6);
  }

  if (is_get && strcmp(seg, "unread-count") == 0) {
    sqlite3_stmt *s; long n = 0;
    if (sqlite3_prepare_v2(db->h,
          "SELECT COUNT(*) FROM alert_events "
          "WHERE tenant_id=?1 AND suppressed=0 AND read_at IS NULL",
          -1,&s,NULL) == SQLITE_OK) {
      sqlite3_bind_text(s,1,tid,-1,SQLITE_TRANSIENT);
      if (sqlite3_step(s)==SQLITE_ROW) n = sqlite3_column_int64(s,0);
    }
    sqlite3_finalize(s);
    char *o = malloc(64);
    if (!o) return err(st,500,"server_error");
    snprintf(o,64,"{\"unread\":%ld}",n);
    *st=200; return o;
  }

  if (is_post && strcmp(seg, "read-all") == 0) {
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h,
          "UPDATE alert_events SET read_at=datetime('now') "
          "WHERE tenant_id=?1 AND read_at IS NULL",-1,&s,NULL) != SQLITE_OK)
      return err(st,500,"server_error");
    sqlite3_bind_text(s,1,tid,-1,SQLITE_TRANSIENT);
    sqlite3_step(s); int ch = sqlite3_changes(db->h); sqlite3_finalize(s);
    char *o = malloc(64);
    if (!o) return err(st,500,"server_error");
    snprintf(o,64,"{\"ok\":true,\"marked\":%d}",ch);
    *st=200; return o;
  }

  if (is_post && seg[0]) {           /* POST /api/alert-events/:id/read */
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h,
          "UPDATE alert_events SET read_at=datetime('now') "
          "WHERE id=?1 AND tenant_id=?2",-1,&s,NULL) != SQLITE_OK)
      return err(st,500,"server_error");
    sqlite3_bind_text(s,1,seg,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,tid,-1,SQLITE_TRANSIENT);
    sqlite3_step(s); int ch = sqlite3_changes(db->h); sqlite3_finalize(s);
    if (ch==0) return err(st,404,"Not found");
    *st=200; return strdup("{\"ok\":true}");
  }

  if (is_get && !seg[0]) {
    int lim = qlimit>0?qlimit:100; if(lim<1)lim=1; if(lim>500)lim=500;
    const char *sql = qflag
      ? "SELECT e.id,e.rule_id,r.name,e.item_uid,e.matched_at,e.read_at,"
        "e.delivered_channels_json,i.title,i.source_id,i.link "
        "FROM alert_events e "
        "LEFT JOIN alert_rules r ON r.id=e.rule_id "
        "LEFT JOIN intel_items i ON i.uid=e.item_uid "
        "WHERE e.tenant_id=?1 AND e.suppressed=0 AND e.read_at IS NULL "
        "ORDER BY e.matched_at DESC LIMIT ?2"
      : "SELECT e.id,e.rule_id,r.name,e.item_uid,e.matched_at,e.read_at,"
        "e.delivered_channels_json,i.title,i.source_id,i.link "
        "FROM alert_events e "
        "LEFT JOIN alert_rules r ON r.id=e.rule_id "
        "LEFT JOIN intel_items i ON i.uid=e.item_uid "
        "WHERE e.tenant_id=?1 AND e.suppressed=0 "
        "ORDER BY e.matched_at DESC LIMIT ?2";
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) != SQLITE_OK)
      return err(st,500,"server_error");
    sqlite3_bind_text(s,1,tid,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(s,2,lim);
    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(s)==SQLITE_ROW) {
      cJSON *r = cJSON_CreateObject();
      cJSON_AddStringToObject(r,"id",(const char*)sqlite3_column_text(s,0));
      cJSON_AddStringToObject(r,"rule_id",(const char*)sqlite3_column_text(s,1));
      const char *rn = ctext(s,2);
      cJSON_AddItemToObject(r,"rule_name", rn?cJSON_CreateString(rn):cJSON_CreateNull());
      cJSON_AddStringToObject(r,"item_uid",(const char*)sqlite3_column_text(s,3));
      cJSON_AddStringToObject(r,"matched_at",(const char*)sqlite3_column_text(s,4));
      const char *ra = ctext(s,5);
      cJSON_AddItemToObject(r,"read_at", ra?cJSON_CreateString(ra):cJSON_CreateNull());
      cJSON_AddBoolToObject(r,"unread", ra ? 0 : 1);
      cJSON_AddItemToObject(r,"delivered_channels", safe_json(ctext(s,6),1));
      const char *t=ctext(s,7),*sr=ctext(s,8),*lk=ctext(s,9);
      const char *uid = (const char *)sqlite3_column_text(s,3);
      /* Breach-monitor hits (roadmap 24) carry a synthetic "breach:<keyid>"
       * uid that has no intel_items row, so the LEFT JOIN above yields NULL
       * previews and the inbox would show a bare uid. Resolve those through
       * the breach adapter, which is the same redacted path the delivered
       * webhook payload uses — the inbox and the webhook must not disagree
       * about what an alert was. Only breach uids take this per-row lookup;
       * ordinary intel rows keep the single-query join. */
      if (!t && uid && strncmp(uid, "breach:", 7) == 0) {
        char *bj = breach_adapter_item_by_uid(db, uid);
        if (bj) {
          cJSON *bo = cJSON_Parse(bj);
          cJSON *bd = bo ? cJSON_GetObjectItem(bo, "data") : NULL;
          if (bd) {
            cJSON *bt = cJSON_GetObjectItem(bd, "title");
            cJSON *bs = cJSON_GetObjectItem(bd, "source_id");
            cJSON *bl = cJSON_GetObjectItem(bd, "link");
            cJSON_AddItemToObject(r,"item_title",
              (bt && cJSON_IsString(bt)) ? cJSON_CreateString(bt->valuestring) : cJSON_CreateNull());
            cJSON_AddItemToObject(r,"item_source_id",
              (bs && cJSON_IsString(bs)) ? cJSON_CreateString(bs->valuestring) : cJSON_CreateNull());
            cJSON_AddItemToObject(r,"item_link",
              (bl && cJSON_IsString(bl)) ? cJSON_CreateString(bl->valuestring) : cJSON_CreateNull());
          }
          if (bo) cJSON_Delete(bo);
          free(bj);
        }
      }
      if (!cJSON_GetObjectItem(r,"item_title")) {
        cJSON_AddItemToObject(r,"item_title", t?cJSON_CreateString(t):cJSON_CreateNull());
        cJSON_AddItemToObject(r,"item_source_id", sr?cJSON_CreateString(sr):cJSON_CreateNull());
        cJSON_AddItemToObject(r,"item_link", lk?cJSON_CreateString(lk):cJSON_CreateNull());
      }
      cJSON_AddItemToArray(arr,r);
    }
    sqlite3_finalize(s);
    cJSON *w = cJSON_CreateObject(); cJSON_AddItemToObject(w,"data",arr);
    char *o = cJSON_PrintUnformatted(w); cJSON_Delete(w);
    *st=200; return o;
  }

  return err(st,404,"not_found");
}
