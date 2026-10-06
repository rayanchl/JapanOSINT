#include "maintenanceapi.h"
#include "url_override.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#define UA "JapanOSINT/1.0 (github.com/rayanchl/JapanOSINT)"

static cJSON *rate_num(long su,long fa){ long t=su+fa;
  if (t==0) return cJSON_CreateNull();
  double v=(double)su/(double)t; v=round(v*1000.0)/1000.0;
  return cJSON_CreateNumber(v); }

/* Add a column to `o`: text-or-null. */
static void col_text(cJSON *o, const char *k, sqlite3_stmt *s, int i){
  if (sqlite3_column_type(s,i)==SQLITE_NULL) cJSON_AddNullToObject(o,k);
  else cJSON_AddStringToObject(o,k,(const char*)sqlite3_column_text(s,i));
}
/* Add a column to `o`: integer-or-null. */
static void col_int(cJSON *o, const char *k, sqlite3_stmt *s, int i){
  if (sqlite3_column_type(s,i)==SQLITE_NULL) cJSON_AddNullToObject(o,k);
  else cJSON_AddNumberToObject(o,k,(double)sqlite3_column_int64(s,i));
}
/* Add a column to `o`: real-or-null (triage_confidence). */
static void col_real(cJSON *o, const char *k, sqlite3_stmt *s, int i){
  if (sqlite3_column_type(s,i)==SQLITE_NULL) cJSON_AddNullToObject(o,k);
  else cJSON_AddNumberToObject(o,k,sqlite3_column_double(s,i));
}
/* UTC ISO-8601 ms timestamp into `out` (>=40 bytes). */
static void iso_now(char *out, size_t cap){
  time_t now=time(NULL); struct tm g; gmtime_r(&now,&g);
  struct timespec sp; clock_gettime(CLOCK_REALTIME,&sp);
  /* Modulos, not defensiveness: %0Nd widths are minimums, so without them
   * -Wformat-truncation must assume 11-20 characters per field. They are
   * identity for every value gmtime_r/clock_gettime can return. */
  snprintf(out,cap,"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
    (unsigned)(g.tm_year+1900) % 10000u,(unsigned)(g.tm_mon+1) % 100u,
    (unsigned)g.tm_mday % 100u,(unsigned)g.tm_hour % 100u,
    (unsigned)g.tm_min % 100u,(unsigned)g.tm_sec % 100u,
    (unsigned)(sp.tv_nsec/1000000) % 1000u);
}
/* Build a {"error":"code"} body (malloc'd). */
static char *err_json(const char *code){
  cJSON *o=cJSON_CreateObject(); cJSON_AddStringToObject(o,"error",code);
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}

/* ── paged lists ───────────────────────────────────────────────────────────
 *
 * Every list the digest and the per-source view return used to be a bare
 * `ORDER BY … LIMIT 50/100/30/20/30` with nothing in the response saying a
 * LIMIT had been applied, so the admin page had to mirror those numbers in a
 * client constant and guess "a full list means there may be more". Worse, the
 * three "verified" buckets (awaiting_apply, awaiting_pr, auto_dismissed) were
 * carved out of the newest 50 verified repairs: a staged fix older than the
 * 50th verified row in the window was never offered for review at all.
 *
 * Now each list is one row in ML below — a COUNT(*) and a SELECT over the SAME
 * predicate — and every response that carries a list also carries its page
 * block, {limit, offset, count, total, has_more}, in the shape the other
 * offset-paged routes (entityapi.c, miscapi.c) use. `total` is a measured
 * COUNT(*) or null when the count itself failed, never a guess. The digest and
 * the per-source view keep their arrays (and the sizes they always served, so
 * a client that reads only the arrays — the iOS app — sees exactly what it saw
 * before, plus each verified bucket in full up to its own limit); the next
 * pages come from
 *     GET /api/admin/maintenance/lists/:name?hours=&limit=&offset=
 *     GET /api/admin/maintenance/source/:id/:list?limit=&offset=
 * which run the very same row of ML. */

enum { MLB_NONE = 0, MLB_WINDOW, MLB_SOURCE };

typedef struct {
  const char *name;
  int bind;            /* ?1: MLB_WINDOW "-N hours", MLB_SOURCE source id   */
  int dflt;            /* rows the digest/detail serve; 0 = every row       */
  const char *count_sql;
  const char *select_sql;    /* ends LIMIT ?2 OFFSET ?3                     */
  void (*row)(cJSON *o, sqlite3_stmt *s);
} mlist;

/* `patch`, `gate` and `model` are what the reviewer is approving. The admin
 * page's RepairCard renders the URL swap from row.patch and offers "Approve"
 * on a verified url_swap — and the digest's query once did not select patch
 * (nor gate or model), so the card showed an Approve button over an EMPTY
 * diff: a live URL override applied on the strength of a change nobody was
 * shown. Same columns as the per-source view's repairs. */
#define ML_REPAIR_COLS \
  "id,anomaly_id,source_id,status,action,triage_class,pr_url,created_at," \
  "patch,gate,model"
static void row_repair(cJSON *r, sqlite3_stmt *s){
  col_int(r,"id",s,0); col_int(r,"anomaly_id",s,1); col_text(r,"source_id",s,2);
  col_text(r,"status",s,3); col_text(r,"action",s,4);
  col_text(r,"triage_class",s,5); col_text(r,"pr_url",s,6);
  col_text(r,"created_at",s,7); col_text(r,"patch",s,8);
  col_text(r,"gate",s,9); col_text(r,"model",s,10);
}
static void row_quarantined(cJSON *c, sqlite3_stmt *s){
  col_text(c,"source_id",s,0); col_text(c,"name",s,1); col_text(c,"category",s,2);
  col_text(c,"since",s,3); col_text(c,"until",s,4);
  cJSON_AddBoolToObject(c,"active",sqlite3_column_int(s,6)!=0);
  col_text(c,"reason",s,5);
}
static void row_override(cJSON *c, sqlite3_stmt *s){
  col_text(c,"source_id",s,0); col_text(c,"old_url",s,1); col_text(c,"new_url",s,2);
  col_int(c,"anomaly_id",s,3); col_text(c,"created_at",s,4);
}
static void row_worst(cJSON *c, sqlite3_stmt *s){
  long su=(long)sqlite3_column_int64(s,1), fa=(long)sqlite3_column_int64(s,2);
  col_text(c,"source_id",s,0);
  cJSON_AddNumberToObject(c,"success",(double)su);
  cJSON_AddNumberToObject(c,"fail",(double)fa);
  cJSON_AddItemToObject(c,"success_rate",rate_num(su,fa));
}
static void row_run(cJSON *r, sqlite3_stmt *s){
  col_int(r,"id",s,0); col_text(r,"timestamp",s,1); col_text(r,"status",s,2);
  col_int(r,"records_fetched",s,3); col_int(r,"duration_ms",s,4);
  col_text(r,"error",s,5);
}
static void row_anomaly(cJSON *r, sqlite3_stmt *s){
  col_int(r,"id",s,0); col_int(r,"fetch_log_id",s,1); col_text(r,"verdict",s,2);
  col_text(r,"reason",s,3); col_text(r,"evidence",s,4);
  col_int(r,"escalation_level",s,5); col_text(r,"created_at",s,6);
  col_text(r,"resolved_at",s,7); col_text(r,"resolution",s,8);
  col_text(r,"triage_class",s,9); col_real(r,"triage_confidence",s,10);
  col_text(r,"triage_evidence",s,11); col_text(r,"triage_suggested_fix",s,12);
  col_text(r,"triaged_at",s,13); col_text(r,"triage_model",s,14);
}
static void row_source_repair(cJSON *r, sqlite3_stmt *s){
  col_int(r,"id",s,0); col_int(r,"anomaly_id",s,1); col_text(r,"status",s,2);
  col_text(r,"action",s,3); col_text(r,"patch",s,4); col_text(r,"gate",s,5);
  col_text(r,"model",s,6); col_text(r,"pr_url",s,7);
  col_text(r,"triage_class",s,8); col_text(r,"created_at",s,9);
}

/* A repair bucket inside the window: one predicate, used by both statements. */
#define ML_WIN "created_at>=datetime('now',?1)"
#define ML_REPAIRS(pred) \
  "SELECT COUNT(*) FROM collector_repair WHERE " pred " AND " ML_WIN, \
  "SELECT " ML_REPAIR_COLS " FROM collector_repair WHERE " pred " AND " ML_WIN \
  " ORDER BY created_at DESC,id DESC LIMIT ?2 OFFSET ?3"
/* worst_sources excludes the await_recovery bookkeeping rows, as it always
 * has, and counts GROUPS: its total is the number of sources, not repairs. */
#define ML_NA "(action IS NULL OR action <> 'await_recovery')"
#define ML_WORST_FROM \
  " FROM collector_repair WHERE " ML_WIN " AND " ML_NA " GROUP BY source_id " \
  "HAVING (SUM(status IN ('verified','merged'))+SUM(status IN ('rejected','error')))>0"

/* Digest lists. Order is the order they appear in the digest's `pages`. */
static const mlist ML_DIGEST[] = {
  { "needs_human",    MLB_WINDOW, 50, ML_REPAIRS("status='needs_human'"), row_repair },
  { "auto_fixed",     MLB_WINDOW, 50, ML_REPAIRS("status='merged'"), row_repair },
  /* A verified url_swap without a PR is applied from this page; with one it
   * waits on the PR. pr_url IS NOT NULL is the old `cJSON_IsString(pr_url)`. */
  { "awaiting_apply", MLB_WINDOW, 50,
    ML_REPAIRS("status='verified' AND action='url_swap' AND pr_url IS NULL"), row_repair },
  { "awaiting_pr",    MLB_WINDOW, 50,
    ML_REPAIRS("status='verified' AND action='url_swap' AND pr_url IS NOT NULL"), row_repair },
  { "auto_dismissed", MLB_WINDOW, 50,
    ML_REPAIRS("status='verified' AND action='auto_dismiss'"), row_repair },
  /* Served whole by the digest, as it always was — capping it there now would
   * slice it silently for any client that does not read `pages`. */
  { "quarantined",    MLB_NONE,    0,
    "SELECT COUNT(*) FROM sources WHERE quarantined_until IS NOT NULL",
    "SELECT id,name,category,quarantined_at,quarantined_until,quarantine_reason,"
    "(quarantined_until>datetime('now')) FROM sources "
    "WHERE quarantined_until IS NOT NULL ORDER BY quarantined_at DESC,id "
    "LIMIT ?2 OFFSET ?3", row_quarantined },
  { "url_overrides",  MLB_NONE,  100,
    "SELECT COUNT(*) FROM collector_url_overrides",
    "SELECT source_id,old_url,new_url,anomaly_id,created_at "
    "FROM collector_url_overrides ORDER BY created_at DESC,source_id "
    "LIMIT ?2 OFFSET ?3", row_override },
  { "worst_sources",  MLB_WINDOW, 50,
    "SELECT COUNT(*) FROM (SELECT source_id" ML_WORST_FROM ")",
    "SELECT source_id,SUM(status IN ('verified','merged')),"
    "SUM(status IN ('rejected','error'))" ML_WORST_FROM
    " ORDER BY 3 DESC,2 DESC,source_id LIMIT ?2 OFFSET ?3", row_worst },
};

/* Per-source lists (GET /api/admin/maintenance/source/:id[/:list]). */
static const mlist ML_SOURCE_LISTS[] = {
  { "fetch_log", MLB_SOURCE, 30,
    "SELECT COUNT(*) FROM fetch_log WHERE source_id=?1",
    "SELECT id,timestamp,status,records_fetched,duration_ms,error "
    "FROM fetch_log WHERE source_id=?1 ORDER BY timestamp DESC,id DESC "
    "LIMIT ?2 OFFSET ?3", row_run },
  { "anomalies", MLB_SOURCE, 20,
    "SELECT COUNT(*) FROM collector_anomaly WHERE source_id=?1",
    "SELECT id,fetch_log_id,verdict,reason,evidence,escalation_level,created_at,"
    "resolved_at,resolution,triage_class,triage_confidence,triage_evidence,"
    "triage_suggested_fix,triaged_at,triage_model "
    "FROM collector_anomaly WHERE source_id=?1 "
    "ORDER BY created_at DESC,id DESC LIMIT ?2 OFFSET ?3", row_anomaly },
  { "repairs", MLB_SOURCE, 30,
    "SELECT COUNT(*) FROM collector_repair WHERE source_id=?1",
    "SELECT id,anomaly_id,status,action,patch,gate,model,pr_url,triage_class,created_at "
    "FROM collector_repair WHERE source_id=?1 "
    "ORDER BY created_at DESC,id DESC LIMIT ?2 OFFSET ?3", row_source_repair },
};

#define ML_N(a) ((int)(sizeof(a)/sizeof((a)[0])))
#define ML_LIMIT_MAX 200

static const mlist *ml_find(const mlist *set, int n, const char *name){
  if (!name) return NULL;
  for (int i=0;i<n;i++) if (!strcmp(set[i].name,name)) return &set[i];
  return NULL;
}

/* One page of list `L`. `limit` <= 0 reads every row. Returns the row array
 * and sets *page_out to {limit,offset,count,total,has_more}; both owned by the
 * caller. `arg` is the ?1 value (window or source id; ignored for MLB_NONE).
 *
 * has_more is measured, not inferred: the SELECT asks for limit+1 rows, and a
 * row past the limit is the proof that more exist. It is also true when the
 * measured total exceeds what this page reached — which is what catches a
 * scan that ended early on an error, since `while (step()==ROW)` cannot tell
 * SQLITE_DONE from a failure. */
static cJSON *ml_page(sqlite3 *h, const mlist *L, const char *arg,
                      int limit, int offset, cJSON **page_out){
  if (offset<0) offset=0;
  long long total=-1;
  sqlite3_stmt *s=NULL;
  if (sqlite3_prepare_v2(h,L->count_sql,-1,&s,NULL)==SQLITE_OK){
    if (L->bind!=MLB_NONE) sqlite3_bind_text(s,1,arg?arg:"",-1,SQLITE_TRANSIENT);
    if (sqlite3_step(s)==SQLITE_ROW) total=sqlite3_column_int64(s,0);
  }
  sqlite3_finalize(s); s=NULL;

  cJSON *arr=cJSON_CreateArray();
  int count=0, more=0;
  if (sqlite3_prepare_v2(h,L->select_sql,-1,&s,NULL)==SQLITE_OK){
    if (L->bind!=MLB_NONE) sqlite3_bind_text(s,1,arg?arg:"",-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(s,2,limit>0 ? (sqlite3_int64)limit+1 : -1);
    sqlite3_bind_int64(s,3,(sqlite3_int64)offset);
    while (sqlite3_step(s)==SQLITE_ROW){
      if (limit>0 && count>=limit){ more=1; break; }
      cJSON *r=cJSON_CreateObject();
      L->row(r,s);
      cJSON_AddItemToArray(arr,r);
      count++;
    }
  }
  sqlite3_finalize(s);
  if (total>=0 && (long long)offset+count<total) more=1;

  cJSON *pg=cJSON_CreateObject();
  if (limit>0) cJSON_AddNumberToObject(pg,"limit",limit);
  else         cJSON_AddNullToObject(pg,"limit");          /* every row */
  cJSON_AddNumberToObject(pg,"offset",offset);
  cJSON_AddNumberToObject(pg,"count",count);
  if (total>=0) cJSON_AddNumberToObject(pg,"total",(double)total);
  else          cJSON_AddNullToObject(pg,"total");
  cJSON_AddBoolToObject(pg,"has_more",more);
  *page_out=pg;
  return arr;
}

/* Put list `L`'s first page into `o` under its own name and its page block
 * into `pages`. */
static void ml_embed(sqlite3 *h, const mlist *L, const char *arg,
                     cJSON *o, cJSON *pages){
  cJSON *pg=NULL;
  cJSON *arr=ml_page(h,L,arg,L->dflt,0,&pg);
  cJSON_AddItemToObject(o,L->name,arr);
  cJSON_AddItemToObject(pages,L->name,pg);
}

static int source_exists(sqlite3 *h, const char *source_id){
  sqlite3_stmt *s=NULL; int found=0;
  if (sqlite3_prepare_v2(h,"SELECT 1 FROM sources WHERE id=?1",-1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_text(s,1,source_id,-1,SQLITE_TRANSIENT);
    found = sqlite3_step(s)==SQLITE_ROW;
  }
  sqlite3_finalize(s);
  return found;
}

char *maintenance_list(db_handle *db, const char *name, const char *source_id,
                       int hours, int limit, int offset, int *status){
  if (!db || !db->h){ *status=500; return err_json("server_error"); }
  const mlist *L = source_id
    ? ml_find(ML_SOURCE_LISTS,ML_N(ML_SOURCE_LISTS),name)
    : ml_find(ML_DIGEST,ML_N(ML_DIGEST),name);
  if (!L){ *status=404; return err_json("unknown_list"); }
  if (source_id && !source_exists(db->h,source_id)){
    *status=404; return err_json("source_not_found");
  }
  if (hours<1) hours=24;
  if (hours>720) hours=720;
  if (limit<=0) limit = L->dflt>0 ? L->dflt : 50;
  if (limit>ML_LIMIT_MAX) limit=ML_LIMIT_MAX;
  if (offset<0) offset=0;
  char win[32]; snprintf(win,sizeof win,"-%d hours",hours);
  const char *arg = L->bind==MLB_SOURCE ? source_id : L->bind==MLB_WINDOW ? win : NULL;

  cJSON *pg=NULL;
  cJSON *arr=ml_page(db->h,L,arg,limit,offset,&pg);
  char ts[40]; iso_now(ts,sizeof ts);
  cJSON *o=cJSON_CreateObject();
  cJSON_AddItemToObject(o,"data",arr);
  cJSON_AddItemToObject(o,"page",pg);
  cJSON *mt=cJSON_CreateObject();
  cJSON_AddStringToObject(mt,"list",L->name);
  if (L->bind==MLB_WINDOW) cJSON_AddNumberToObject(mt,"window_hours",hours);
  if (source_id) cJSON_AddStringToObject(mt,"source_id",source_id);
  cJSON_AddStringToObject(mt,"generated_at",ts);
  cJSON_AddItemToObject(o,"meta",mt);
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o);
  *status=200;
  return j;
}

char *maintenance_digest(db_handle *db, int hours) {
  if (hours<1) hours=24;
  if (hours>720) hours=720;
  char win[32]; snprintf(win,sizeof win,"-%d hours",hours);
  sqlite3 *h=db->h; sqlite3_stmt *s;
  long verified=0,merged=0,rejected=0,needs=0,error=0;
  if (sqlite3_prepare_v2(h,"SELECT status,COUNT(*) FROM collector_repair "
    "WHERE created_at>=datetime('now',?1) GROUP BY status",-1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_text(s,1,win,-1,SQLITE_TRANSIENT);
    while (sqlite3_step(s)==SQLITE_ROW){
      const char *st=(const char*)sqlite3_column_text(s,0);
      long n=sqlite3_column_int64(s,1);
      if(!strcmp(st,"verified"))verified=n; else if(!strcmp(st,"merged"))merged=n;
      else if(!strcmp(st,"rejected"))rejected=n; else if(!strcmp(st,"needs_human"))needs=n;
      else if(!strcmp(st,"error"))error=n;
    }
  }
  sqlite3_finalize(s);
  cJSON *byClass=cJSON_CreateArray();
  char q1[400];
  snprintf(q1,sizeof q1,
    "SELECT COALESCE(triage_class,'?'),"
    "SUM(status IN ('verified','merged')),SUM(status IN ('rejected','error')),"
    "SUM(status='needs_human') FROM collector_repair "
    "WHERE created_at>=datetime('now',?1) AND %s GROUP BY 1 "
    "ORDER BY (SUM(status IN ('verified','merged'))+SUM(status IN ('rejected','error'))) DESC",ML_NA);
  if (sqlite3_prepare_v2(h,q1,-1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_text(s,1,win,-1,SQLITE_TRANSIENT);
    while (sqlite3_step(s)==SQLITE_ROW){
      long su=sqlite3_column_int64(s,1),fa=sqlite3_column_int64(s,2),nh=sqlite3_column_int64(s,3);
      cJSON *c=cJSON_CreateObject();
      cJSON_AddStringToObject(c,"class",(const char*)sqlite3_column_text(s,0));
      cJSON_AddNumberToObject(c,"success",(double)su);
      cJSON_AddNumberToObject(c,"fail",(double)fa);
      cJSON_AddNumberToObject(c,"needs_human",(double)nh);
      cJSON_AddItemToObject(c,"success_rate",rate_num(su,fa));
      cJSON_AddItemToArray(byClass,c);
    }
  }
  sqlite3_finalize(s);

  char ts[40]; iso_now(ts,sizeof ts);
  cJSON *o=cJSON_CreateObject();
  cJSON_AddStringToObject(o,"generated_at",ts);
  cJSON_AddNumberToObject(o,"window_hours",hours);
  cJSON *tot=cJSON_CreateObject();
  cJSON_AddNumberToObject(tot,"verified",(double)verified);
  cJSON_AddNumberToObject(tot,"merged",(double)merged);
  cJSON_AddNumberToObject(tot,"rejected",(double)rejected);
  cJSON_AddNumberToObject(tot,"needs_human",(double)needs);
  cJSON_AddNumberToObject(tot,"error",(double)error);
  cJSON_AddItemToObject(o,"totals",tot);
  cJSON_AddItemToObject(o,"success_by_class",byClass);

  /* Every list, first page, plus `pages` saying how much of each that is.
   * The three verified buckets are grouped under awaiting_review /
   * auto_dismissed exactly where the digest has always put them. */
  cJSON *pages=cJSON_CreateObject();
  cJSON *aw=cJSON_CreateObject();
  for (int i=0;i<ML_N(ML_DIGEST);i++){
    const mlist *L=&ML_DIGEST[i];
    const char *arg = L->bind==MLB_WINDOW ? win : NULL;
    int in_review = !strcmp(L->name,"awaiting_apply") || !strcmp(L->name,"awaiting_pr");
    ml_embed(h,L,arg,in_review?aw:o,pages);
  }
  cJSON_AddItemToObject(o,"awaiting_review",aw);
  cJSON_AddItemToObject(o,"pages",pages);

  /* llmConcurrencySnapshot — C LLM runtime has no shared queue gauge; report
   * the configured limits with zeroed live counters (honest, stable shape). */
  cJSON *cc=cJSON_CreateObject();
  int hl=getenv("LLM_HEAVY_CONCURRENCY")?atoi(getenv("LLM_HEAVY_CONCURRENCY")):1;
  int ml=getenv("LLM_MID_CONCURRENCY")?atoi(getenv("LLM_MID_CONCURRENCY")):2;
  if (hl<1)hl=1;
  if (ml<1)ml=1;
  cJSON *hv=cJSON_CreateObject();
  cJSON_AddNumberToObject(hv,"limit",hl); cJSON_AddNumberToObject(hv,"inflight",0);
  cJSON_AddNumberToObject(hv,"waiting",0); cJSON_AddItemToObject(cc,"heavy",hv);
  cJSON *md=cJSON_CreateObject();
  cJSON_AddNumberToObject(md,"limit",ml); cJSON_AddNumberToObject(md,"inflight",0);
  cJSON_AddNumberToObject(md,"waiting",0); cJSON_AddItemToObject(cc,"mid",md);
  cJSON_AddItemToObject(o,"concurrency",cc);
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}

/* ── per-source pipeline detail + operator actions ──────────────────────
 * The digest above is host-wide aggregate; these expose one source's full
 * detect→triage→repair chain and let the operator steer it. */

char *maintenance_source_detail(db_handle *db, const char *source_id){
  if (!db || !db->h || !source_id || !*source_id) return NULL;
  sqlite3 *h=db->h; sqlite3_stmt *s;
  cJSON *src=NULL;
  if (sqlite3_prepare_v2(h,
    "SELECT id,name,category,type,url,status,last_check,last_success,"
    "response_time_ms,records_count,error_message,"
    "quarantined_at,quarantined_until,quarantine_reason,"
    "(quarantined_until IS NOT NULL AND quarantined_until>datetime('now')) "
    "FROM sources WHERE id=?1",-1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_text(s,1,source_id,-1,SQLITE_TRANSIENT);
    if (sqlite3_step(s)==SQLITE_ROW){
      src=cJSON_CreateObject();
      col_text(src,"id",s,0); col_text(src,"name",s,1);
      col_text(src,"category",s,2); col_text(src,"type",s,3);
      col_text(src,"url",s,4); col_text(src,"status",s,5);
      col_text(src,"last_check",s,6); col_text(src,"last_success",s,7);
      col_int(src,"response_time_ms",s,8); col_int(src,"records_count",s,9);
      col_text(src,"error_message",s,10);
      cJSON *q=cJSON_CreateObject();
      col_text(q,"at",s,11); col_text(q,"until",s,12);
      col_text(q,"reason",s,13);
      cJSON_AddBoolToObject(q,"active",sqlite3_column_int(s,14)!=0);
      cJSON_AddItemToObject(src,"quarantine",q);
    }
  }
  sqlite3_finalize(s);
  if (!src) return NULL;  /* unknown source → caller replies 404 */

  char ts[40]; iso_now(ts,sizeof ts);
  cJSON *o=cJSON_CreateObject();
  cJSON_AddStringToObject(o,"generated_at",ts);
  cJSON_AddItemToObject(o,"source",src);
  cJSON *pages=cJSON_CreateObject();
  for (int i=0;i<ML_N(ML_SOURCE_LISTS);i++)
    ml_embed(h,&ML_SOURCE_LISTS[i],source_id,o,pages);
  cJSON_AddItemToObject(o,"pages",pages);
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}

/* Every write in this file is an operator action on live collector state, and
 * every handler below answers {"ok":true} plus an "OPERATOR ..." stderr line
 * on the strength of it. A discarded step result therefore does not degrade to
 * "nothing happened" — it degrades to "we told the operator it happened".
 * `need_change` additionally requires the UPDATE to have matched a row: on
 * these statements the target's existence was confirmed on this same
 * connection moments earlier, so zero changes means the row moved underneath
 * us, which is not a success either. Finalizes `u` in all cases. */
static int write_ok(sqlite3 *h, sqlite3_stmt *u, int need_change){
  if (!u) return 0;
  int rc = sqlite3_step(u);
  int changed = sqlite3_changes(h);
  sqlite3_finalize(u);
  return rc == SQLITE_DONE && (!need_change || changed > 0);
}

/* Resolve an anomaly with an operator-supplied resolution string. Returns 1
 * only if the row was really updated. */
static int resolve_anomaly_op(sqlite3 *h, long anomaly_id, const char *res){
  sqlite3_stmt *u=NULL;
  if (sqlite3_prepare_v2(h,
    "UPDATE collector_anomaly SET resolved_at=datetime('now'),resolution=?2 "
    "WHERE id=?1",-1,&u,NULL)!=SQLITE_OK){ sqlite3_finalize(u); return 0; }
  sqlite3_bind_int64(u,1,(sqlite3_int64)anomaly_id);
  sqlite3_bind_text(u,2,res,-1,SQLITE_TRANSIENT);
  return write_ok(h,u,1);
}

char *maintenance_repair_action(db_handle *db, long repair_id, int approve,
                                int *status){
  if (!db || !db->h){ *status=500; return err_json("server_error"); }
  sqlite3 *h=db->h; sqlite3_stmt *s;
  char rstatus[32]={0}, raction[32]={0}, source_id[128]={0};
  char *patch=NULL; long anomaly_id=0; int found=0;
  if (sqlite3_prepare_v2(h,
    "SELECT status,action,patch,source_id,anomaly_id FROM collector_repair "
    "WHERE id=?1",-1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_int64(s,1,(sqlite3_int64)repair_id);
    if (sqlite3_step(s)==SQLITE_ROW){
      found=1;
      snprintf(rstatus,sizeof rstatus,"%s",(const char*)sqlite3_column_text(s,0));
      if (sqlite3_column_type(s,1)!=SQLITE_NULL)
        snprintf(raction,sizeof raction,"%s",(const char*)sqlite3_column_text(s,1));
      if (sqlite3_column_type(s,2)!=SQLITE_NULL)
        patch=strdup((const char*)sqlite3_column_text(s,2));
      snprintf(source_id,sizeof source_id,"%s",(const char*)sqlite3_column_text(s,3));
      if (sqlite3_column_type(s,4)!=SQLITE_NULL)
        anomaly_id=(long)sqlite3_column_int64(s,4);
    }
  }
  sqlite3_finalize(s);
  if (!found){ free(patch); *status=404; return err_json("repair_not_found"); }

  if (approve){
    if (strcmp(rstatus,"verified") || strcmp(raction,"url_swap")){
      free(patch); *status=409; return err_json("not_approvable");
    }
    cJSON *pj=patch?cJSON_Parse(patch):NULL;
    cJSON *ou=pj?cJSON_GetObjectItem(pj,"old_url"):NULL;
    cJSON *nu=pj?cJSON_GetObjectItem(pj,"new_url"):NULL;
    const char *old_url=(ou&&cJSON_IsString(ou))?ou->valuestring:NULL;
    const char *new_url=(nu&&cJSON_IsString(nu))?nu->valuestring:NULL;
    if (!old_url||!*old_url||!new_url||!*new_url){
      if (pj) cJSON_Delete(pj);
      free(patch);
      *status=422; return err_json("patch_missing_urls");
    }
    /* The override INSERT is the whole point of the approval: it is what makes
     * the collector fetch a different URL. If it does not land there is no
     * swap, so nothing downstream may run and nothing may be reported as
     * approved. Bail before url_override_reload(). The repair row is left at
     * 'verified', so the operator can simply retry — the upsert is idempotent. */
    sqlite3_stmt *u=NULL;
    int ins_ok=0;
    if (sqlite3_prepare_v2(h,
      "INSERT INTO collector_url_overrides (source_id,old_url,new_url,anomaly_id,created_at)"
      " VALUES (?1,?2,?3,?4,datetime('now'))"
      " ON CONFLICT(source_id) DO UPDATE SET old_url=excluded.old_url,"
      " new_url=excluded.new_url, anomaly_id=excluded.anomaly_id,"
      " created_at=excluded.created_at",-1,&u,NULL)==SQLITE_OK){
      sqlite3_bind_text(u,1,source_id,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(u,2,old_url,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(u,3,new_url,-1,SQLITE_TRANSIENT);
      sqlite3_bind_int64(u,4,(sqlite3_int64)anomaly_id);
      ins_ok=write_ok(h,u,1);
    } else sqlite3_finalize(u);
    if (!ins_ok){
      fprintf(stderr,"[repair] APPROVAL FAILED %s: url override not written: %s\n",
              source_id,sqlite3_errmsg(h));
      if (pj) { cJSON_Delete(pj); } free(patch);
      *status=500; return err_json("url_override_write_failed");
    }
    url_override_reload(db);          /* activate the swap live, no restart */

    /* From here the swap IS live. A failure below leaves a real, partial
     * state — the fetch URL changed but the ledger did not record it — and
     * saying {"ok":true,"status":"merged"} would hide exactly that. Report it
     * instead; re-approving is safe (the repair row is still 'verified' and
     * the override upsert is idempotent). */
    u=NULL;
    int merged_ok=0;
    if (sqlite3_prepare_v2(h,
      "UPDATE collector_repair SET status='merged' WHERE id=?1",-1,&u,NULL)==SQLITE_OK){
      sqlite3_bind_int64(u,1,(sqlite3_int64)repair_id);
      merged_ok=write_ok(h,u,1);
    } else sqlite3_finalize(u);
    int anom_ok = !anomaly_id || resolve_anomaly_op(h,anomaly_id,"approved_by_operator");
    if (!merged_ok || !anom_ok){
      fprintf(stderr,"[repair] PARTIAL APPROVAL %s: %s -> %s is LIVE but "
                     "repair#%ld status=%s anomaly=%s (%s)\n",
              source_id,old_url,new_url,repair_id,
              merged_ok?"merged":"NOT-merged", anom_ok?"resolved":"NOT-resolved",
              sqlite3_errmsg(h));
      if (pj) { cJSON_Delete(pj); } free(patch);
      *status=500;
      return err_json(merged_ok ? "override_live_anomaly_not_resolved"
                                : "override_live_repair_not_marked_merged");
    }
    fprintf(stderr,"[repair] OPERATOR APPROVED %s: %s -> %s\n",source_id,old_url,new_url);
    cJSON *o=cJSON_CreateObject();
    cJSON_AddBoolToObject(o,"ok",1);
    cJSON_AddNumberToObject(o,"repair_id",(double)repair_id);
    cJSON_AddStringToObject(o,"status","merged");
    cJSON_AddStringToObject(o,"source_id",source_id);
    cJSON_AddStringToObject(o,"old_url",old_url);
    cJSON_AddStringToObject(o,"new_url",new_url);
    if (pj) cJSON_Delete(pj);
    free(patch);
    char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o);
    *status=200; return j;
  }

  /* reject */
  free(patch);
  if (!strcmp(rstatus,"merged")){ *status=409; return err_json("already_merged"); }
  sqlite3_stmt *u=NULL;
  int rej_ok=0;
  if (sqlite3_prepare_v2(h,
    "UPDATE collector_repair SET status='rejected' WHERE id=?1",-1,&u,NULL)==SQLITE_OK){
    sqlite3_bind_int64(u,1,(sqlite3_int64)repair_id);
    rej_ok=write_ok(h,u,1);
  } else sqlite3_finalize(u);
  /* A rejection that did not persist leaves the repair pending approval while
   * the operator has been told it is rejected — the next person sees a queue
   * item that was already decided. */
  if (!rej_ok){
    fprintf(stderr,"[repair] REJECT FAILED repair #%ld (%s): %s\n",
            repair_id,source_id,sqlite3_errmsg(h));
    *status=500; return err_json("repair_reject_write_failed");
  }
  int anom_rej = !anomaly_id || resolve_anomaly_op(h,anomaly_id,"rejected_by_operator");
  if (!anom_rej){
    fprintf(stderr,"[repair] repair #%ld REJECTED but anomaly #%ld not resolved: %s\n",
            repair_id,anomaly_id,sqlite3_errmsg(h));
    *status=500; return err_json("repair_rejected_anomaly_not_resolved");
  }
  fprintf(stderr,"[repair] OPERATOR REJECTED repair #%ld (%s)\n",repair_id,source_id);
  cJSON *o=cJSON_CreateObject();
  cJSON_AddBoolToObject(o,"ok",1);
  cJSON_AddNumberToObject(o,"repair_id",(double)repair_id);
  cJSON_AddStringToObject(o,"status","rejected");
  cJSON_AddStringToObject(o,"source_id",source_id);
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o);
  *status=200; return j;
}

/* DELETE /api/admin/repairs/:id — undo an APPLIED url_swap.
 *
 * Approval installs a runtime rewrite that steers every outbound fetch whose
 * URL matches `old_url` (httpclient.c routes all of them through
 * url_override_apply). url_override_remove()/url_override_reset() were written
 * for exactly this and had no caller, so a repair approved onto the wrong
 * endpoint could only be undone by editing the table and restarting the
 * process — while every collector on that URL kept fetching the wrong host.
 *
 * The override row is keyed on source_id and the map is keyed on old_url, so
 * BOTH are cleared here: url_override_remove() deletes the row and tombstones
 * the in-memory entries, and url_override_reload() re-reads what is left so
 * the map matches the table exactly. The repair row goes back to 'verified'
 * rather than being deleted — the ledger keeps saying this swap was proposed
 * and verified, and the operator can re-approve it if the revert was the
 * mistake. */
char *maintenance_repair_revert(db_handle *db, long repair_id, int *status){
  if (!db || !db->h){ *status=500; return err_json("server_error"); }
  sqlite3 *h=db->h; sqlite3_stmt *s;
  char rstatus[32]={0}, raction[32]={0}, source_id[128]={0};
  char *patch=NULL; int found=0;
  if (sqlite3_prepare_v2(h,
    "SELECT status,action,patch,source_id FROM collector_repair WHERE id=?1",
    -1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_int64(s,1,(sqlite3_int64)repair_id);
    if (sqlite3_step(s)==SQLITE_ROW){
      found=1;
      snprintf(rstatus,sizeof rstatus,"%s",(const char*)sqlite3_column_text(s,0));
      if (sqlite3_column_type(s,1)!=SQLITE_NULL)
        snprintf(raction,sizeof raction,"%s",(const char*)sqlite3_column_text(s,1));
      if (sqlite3_column_type(s,2)!=SQLITE_NULL)
        patch=strdup((const char*)sqlite3_column_text(s,2));
      snprintf(source_id,sizeof source_id,"%s",(const char*)sqlite3_column_text(s,3));
    }
  }
  sqlite3_finalize(s);
  if (!found){ free(patch); *status=404; return err_json("repair_not_found"); }

  /* Only an applied url_swap has anything to undo. Answering "ok" for a repair
   * that was never merged would tell the operator a live rewrite was removed
   * when none existed. */
  if (strcmp(rstatus,"merged") || strcmp(raction,"url_swap")){
    free(patch); *status=409; return err_json("not_revertable");
  }
  cJSON *pj=patch?cJSON_Parse(patch):NULL;
  cJSON *ou=pj?cJSON_GetObjectItem(pj,"old_url"):NULL;
  cJSON *nu=pj?cJSON_GetObjectItem(pj,"new_url"):NULL;
  const char *old_url=(ou&&cJSON_IsString(ou))?ou->valuestring:NULL;
  const char *new_url=(nu&&cJSON_IsString(nu))?nu->valuestring:NULL;
  if (!old_url||!*old_url){
    if (pj) { cJSON_Delete(pj); } free(patch);
    *status=422; return err_json("patch_missing_urls");
  }

  int removed = url_override_remove(db, old_url);

  /* The row is keyed on source_id (ON CONFLICT(source_id) DO UPDATE at approve
   * time), so a re-approval under a DIFFERENT old_url would leave a row that
   * the old_url delete above cannot see. Clear it by its own key too, then
   * reload so memory and table agree. */
  sqlite3_stmt *d=NULL;
  if (sqlite3_prepare_v2(h,
    "DELETE FROM collector_url_overrides WHERE source_id=?1",-1,&d,NULL)==SQLITE_OK){
    sqlite3_bind_text(d,1,source_id,-1,SQLITE_TRANSIENT);
    if (sqlite3_step(d)!=SQLITE_DONE)
      fprintf(stderr,"[repair] revert: override row for %s not deleted: %s\n",
              source_id,sqlite3_errmsg(h));
    sqlite3_finalize(d);
  } else sqlite3_finalize(d);
  url_override_reload(db);

  /* From here the swap is NOT live any more. If the ledger update fails the
   * state is real and partial — the fetch URL is back to the original but the
   * repair still reads 'merged' — so it is reported, not papered over. */
  sqlite3_stmt *u=NULL;
  int unmerged=0;
  if (sqlite3_prepare_v2(h,
    "UPDATE collector_repair SET status='verified' WHERE id=?1",-1,&u,NULL)==SQLITE_OK){
    sqlite3_bind_int64(u,1,(sqlite3_int64)repair_id);
    unmerged=write_ok(h,u,1);
  } else sqlite3_finalize(u);
  if (!unmerged){
    fprintf(stderr,"[repair] PARTIAL REVERT %s: override for %s is REMOVED but "
                   "repair#%ld is still marked merged (%s)\n",
            source_id,old_url,repair_id,sqlite3_errmsg(h));
    if (pj) { cJSON_Delete(pj); } free(patch);
    *status=500; return err_json("override_removed_repair_not_unmarked");
  }

  fprintf(stderr,"[repair] OPERATOR REVERTED %s: %s -> %s (%d live entr%s removed)\n",
          source_id,old_url,new_url?new_url:"?",removed,removed==1?"y":"ies");
  cJSON *o=cJSON_CreateObject();
  cJSON_AddBoolToObject(o,"ok",1);
  cJSON_AddNumberToObject(o,"repair_id",(double)repair_id);
  cJSON_AddStringToObject(o,"status","verified");
  cJSON_AddStringToObject(o,"source_id",source_id);
  cJSON_AddStringToObject(o,"old_url",old_url);
  cJSON_AddItemToObject(o,"new_url",
    new_url?cJSON_CreateString(new_url):cJSON_CreateNull());
  /* How many live map entries were actually tombstoned. 0 is a real answer —
   * the process may have been restarted since approval with the row already
   * gone — and it is not the same as "reverted 1", so it is reported. */
  cJSON_AddNumberToObject(o,"overrides_removed",(double)removed);
  if (pj) { cJSON_Delete(pj); } free(patch);
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o);
  *status=200; return j;
}

char *maintenance_unquarantine(db_handle *db, const char *source_id, int *status){
  if (!db || !db->h || !source_id || !*source_id){
    *status=400; return err_json("bad_request");
  }
  sqlite3 *h=db->h; sqlite3_stmt *s; int found=0;
  if (sqlite3_prepare_v2(h,"SELECT 1 FROM sources WHERE id=?1",-1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_text(s,1,source_id,-1,SQLITE_TRANSIENT);
    if (sqlite3_step(s)==SQLITE_ROW) found=1;
  }
  sqlite3_finalize(s);
  if (!found){ *status=404; return err_json("source_not_found"); }
  /* Keep quarantined_at as history; clearing `until` + reason lifts the lock.
   * If it does not land the source stays quarantined while the operator has
   * been told it is running again — the collector's silence then looks like
   * "no data upstream". */
  s=NULL;
  int cleared=0;
  if (sqlite3_prepare_v2(h,
    "UPDATE sources SET quarantined_until=NULL,quarantine_reason=NULL WHERE id=?1",
    -1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_text(s,1,source_id,-1,SQLITE_TRANSIENT);
    cleared=write_ok(h,s,1);
  } else sqlite3_finalize(s);
  if (!cleared){
    fprintf(stderr,"[repair] UNQUARANTINE FAILED %s: %s\n",source_id,sqlite3_errmsg(h));
    *status=500; return err_json("unquarantine_write_failed");
  }
  fprintf(stderr,"[repair] OPERATOR CLEARED quarantine %s\n",source_id);
  cJSON *o=cJSON_CreateObject();
  cJSON_AddBoolToObject(o,"ok",1);
  cJSON_AddStringToObject(o,"source_id",source_id);
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o);
  *status=200; return j;
}

char *maintenance_requeue_anomaly(db_handle *db, long anomaly_id, int *status){
  if (!db || !db->h){ *status=500; return err_json("server_error"); }
  sqlite3 *h=db->h; sqlite3_stmt *s; int found=0; char source_id[128]={0};
  if (sqlite3_prepare_v2(h,
    "SELECT source_id FROM collector_anomaly WHERE id=?1",-1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_int64(s,1,(sqlite3_int64)anomaly_id);
    if (sqlite3_step(s)==SQLITE_ROW){
      found=1;
      snprintf(source_id,sizeof source_id,"%s",(const char*)sqlite3_column_text(s,0));
    }
  }
  sqlite3_finalize(s);
  if (!found){ *status=404; return err_json("anomaly_not_found"); }
  /* Reset triage + resolution so it re-enters the untriaged queue (the triage
   * worker re-picks it on its next cycle when LLM_ENABLED). */
  s=NULL;
  int requeued=0;
  if (sqlite3_prepare_v2(h,
    "UPDATE collector_anomaly SET triaged_at=NULL,triage_class=NULL,"
    "triage_confidence=NULL,triage_evidence=NULL,triage_suggested_fix=NULL,"
    "triage_model=NULL,resolved_at=NULL,resolution=NULL WHERE id=?1",
    -1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_int64(s,1,(sqlite3_int64)anomaly_id);
    requeued=write_ok(h,s,1);
  } else sqlite3_finalize(s);
  /* A requeue that did not persist means the anomaly never re-enters the
   * triage queue — and {"ok":true} is the operator's only feedback, so the
   * item would just sit there looking handled. */
  if (!requeued){
    fprintf(stderr,"[repair] REQUEUE FAILED anomaly #%ld (%s): %s\n",
            anomaly_id,source_id,sqlite3_errmsg(h));
    *status=500; return err_json("requeue_write_failed");
  }
  fprintf(stderr,"[repair] OPERATOR REQUEUED anomaly #%ld (%s)\n",anomaly_id,source_id);
  cJSON *o=cJSON_CreateObject();
  cJSON_AddBoolToObject(o,"ok",1);
  cJSON_AddNumberToObject(o,"anomaly_id",(double)anomaly_id);
  cJSON_AddStringToObject(o,"source_id",source_id);
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o);
  *status=200; return j;
}

/* ── tiny TTL JSON cache (single-threaded mongoose loop) ─────────────── */
