#include "dbexplorerapi.h"
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

static const char *ALLOWED[] = { "intel_items","sources","fetch_log",
  "cameras","transport_stations","transport_lines" };
static int allowed_tbl(const char *n) {
  for (size_t i=0;i<sizeof ALLOWED/sizeof *ALLOWED;i++)
    if (strcmp(ALLOWED[i],n)==0) return 1;
  return 0;
}
/* `*ok` (optional) reports whether the PRAGMA scan really reached the end.
 * It matters: dbexplorer_table() derives the `q` filter from this list, so a
 * short read there does not just shorten the schema view — it drops WHERE
 * clauses and answers a filtered request with UNFILTERED rows. */
static cJSON *cols_of(sqlite3 *h, const char *name, int *ok) {
  if (ok) *ok=0;
  char sql[128]; snprintf(sql,sizeof sql,"PRAGMA table_info(\"%s\")",name);
  sqlite3_stmt *s=NULL; cJSON *a=cJSON_CreateArray();
  if (sqlite3_prepare_v2(h,sql,-1,&s,NULL)==SQLITE_OK) {
    /* `while (step() == ROW)` cannot tell DONE from IOERR/CORRUPT/BUSY/
     * INTERRUPT, so the loop ends the same way whether the schema was fully
     * read or the read died halfway. */
    int scan_rc;
    while ((scan_rc=sqlite3_step(s))==SQLITE_ROW) {
      cJSON *c=cJSON_CreateObject();
      cJSON_AddStringToObject(c,"name",(const char*)sqlite3_column_text(s,1));
      const char *ty=(const char*)sqlite3_column_text(s,2);
      cJSON_AddStringToObject(c,"type",(ty&&*ty)?ty:"TEXT");
      cJSON_AddItemToArray(a,c);
    }
    if (ok) *ok = (scan_rc==SQLITE_DONE);
  }
  sqlite3_finalize(s);
  return a;
}

/* Append to a growing heap string. Returns 0 on OOM (buffer left usable but
 * short — every caller treats that as a hard failure, never as "good enough"). */
static int sb_add(char **buf, size_t *len, size_t *cap, const char *s) {
  size_t n=strlen(s);
  if (*len+n+1 > *cap) {
    size_t nc = *cap ? *cap : 256;
    while (nc < *len+n+1) nc*=2;
    char *nb=realloc(*buf,nc);
    if (!nb) return 0;
    *buf=nb; *cap=nc;
  }
  memcpy(*buf+*len,s,n+1);
  *len+=n;
  return 1;
}
char *dbexplorer_tables(db_handle *db) {
  cJSON *arr=cJSON_CreateArray();
  for (size_t i=0;i<sizeof ALLOWED/sizeof *ALLOWED;i++) {
    const char *name=ALLOWED[i];
    sqlite3_stmt *s; int exists=0;
    if (sqlite3_prepare_v2(db->h,
      "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1",-1,&s,NULL)==SQLITE_OK){
      sqlite3_bind_text(s,1,name,-1,SQLITE_TRANSIENT);
      exists=sqlite3_step(s)==SQLITE_ROW;
    }
    sqlite3_finalize(s);
    if (!exists) continue;
    long cnt=0; char q[64]; snprintf(q,sizeof q,"SELECT COUNT(*) FROM \"%s\"",name);
    if (sqlite3_prepare_v2(db->h,q,-1,&s,NULL)==SQLITE_OK && sqlite3_step(s)==SQLITE_ROW)
      cnt=sqlite3_column_int64(s,0);
    sqlite3_finalize(s);
    cJSON *o=cJSON_CreateObject();
    cJSON_AddStringToObject(o,"name",name);
    cJSON_AddNumberToObject(o,"row_count",(double)cnt);
    cJSON_AddItemToObject(o,"columns",cols_of(db->h,name,NULL));
    cJSON_AddItemToArray(arr,o);
  }
  char *j=cJSON_PrintUnformatted(arr); cJSON_Delete(arr); return j;
}
char *dbexplorer_table(db_handle *db, const char *name, int limit, int offset,
                  const char *q, const char *orderBy, const char *orderDir) {
  if (!allowed_tbl(name)) return NULL;            /* → 400 */
  if (limit<1) limit=50; if (limit>200) limit=200;
  if (offset<0) offset=0;
  int cols_ok=0;
  cJSON *cols=cols_of(db->h,name,&cols_ok);
  /* Every failure below lands here instead of silently degrading. `complete`
   * is the field that separates "this table really has no matching rows" from
   * "we could not read it": the old code left `total` and `rows` at their
   * initialised 0 / [] on a prepare or step failure and returned 200, which
   * renders as an empty table. */
  char err[256]=""; int complete=1;
  #define FAILED(...) do { if (!err[0]) snprintf(err,sizeof err,__VA_ARGS__); \
                           complete=0; } while (0)
  if (!cols_ok) FAILED("schema read failed: %s", sqlite3_errmsg(db->h));

  /* text columns + name validation for orderBy */
  int ob_ok=0;
  cJSON *cc;
  cJSON_ArrayForEach(cc,cols)
    if (orderBy&&*orderBy&&strcmp(cJSON_GetObjectItem(cc,"name")->valuestring,orderBy)==0) ob_ok=1;
  const char *dir = (orderDir && strcasecmp(orderDir,"DESC")==0) ? "DESC" : "ASC";

  /* The WHERE clause is one `"col" LIKE ?` per TEXT column, so its length is a
   * property of the table, not a constant. It used to be built with strncat
   * into `char where[512]`: on a wide table that truncated mid-fragment, the
   * resulting SQL failed to prepare, and both queries below fell through
   * leaving total=0 / rows=[]. Worse, the bind count was recovered by counting
   * "LIKE ?" in that same truncated buffer, so the placeholders and the binds
   * could disagree. Grow the buffer instead (no truncation is possible) and
   * count the placeholders as they are emitted. */
  char *where=NULL; size_t wlen=0, wcap=0;
  int hasq = q && *q, nlike=0;
  if (hasq) {
    cJSON_ArrayForEach(cc,cols) {
      const char *ty=cJSON_GetObjectItem(cc,"type")->valuestring;
      if (ty && (strcasestr(ty,"TEXT")||strcasestr(ty,"CHAR")||strcasestr(ty,"CLOB"))) {
        if (!sb_add(&where,&wlen,&wcap, nlike?" OR \"":"WHERE \"") ||
            !sb_add(&where,&wlen,&wcap, cJSON_GetObjectItem(cc,"name")->valuestring) ||
            !sb_add(&where,&wlen,&wcap, "\" LIKE ?")) {
          /* A half-built WHERE would answer a filtered request with a NARROWER
           * filter than asked for, and look like a legitimate result set. */
          FAILED("out of memory building the search filter");
          nlike=0; break;
        }
        nlike++;
      }
    }
    /* Either no text columns, or the build failed: run unfiltered ONLY in the
     * first case — a dropped filter is otherwise reported, never applied. */
    if (!nlike) hasq=0;
  }
  char *like=NULL;
  if (hasq) {
    size_t ln=strlen(q)+3;                 /* '%' + q + '%' + NUL */
    like=malloc(ln);
    if (!like) { FAILED("out of memory"); hasq=0; nlike=0; }
    else snprintf(like,ln,"%%%s%%",q);
  }
  const char *wclause = hasq ? where : "";

  /* count */
  long total=0; int total_known=0;
  size_t qsz = strlen(name)+wlen+128;
  char *cq=malloc(qsz);
  sqlite3_stmt *s=NULL;
  if (!cq) FAILED("out of memory");
  else {
    snprintf(cq,qsz,"SELECT COUNT(*) FROM \"%s\" %s",name,wclause);
    if (sqlite3_prepare_v2(db->h,cq,-1,&s,NULL)==SQLITE_OK){
      for (int i=0;i<nlike;i++) sqlite3_bind_text(s,i+1,like,-1,SQLITE_TRANSIENT);
      if (sqlite3_step(s)==SQLITE_ROW) { total=sqlite3_column_int64(s,0); total_known=1; }
      else FAILED("count failed: %s", sqlite3_errmsg(db->h));
    } else FAILED("count prepare failed: %s", sqlite3_errmsg(db->h));
    sqlite3_finalize(s); s=NULL;
  }
  free(cq);

  char order[128]=""; if (ob_ok) snprintf(order,sizeof order,"ORDER BY \"%s\" %s",orderBy,dir);
  size_t ssz = strlen(name)+wlen+sizeof order+64;
  char *sql=malloc(ssz);
  cJSON *rows=cJSON_CreateArray();
  if (!sql) FAILED("out of memory");
  else {
    snprintf(sql,ssz,"SELECT * FROM \"%s\" %s %s LIMIT ? OFFSET ?",
      name,wclause,order);
    if (sqlite3_prepare_v2(db->h,sql,-1,&s,NULL)==SQLITE_OK){
      int bi=1;
      for (int i=0;i<nlike;i++) sqlite3_bind_text(s,bi++,like,-1,SQLITE_TRANSIENT);
      sqlite3_bind_int(s,bi++,limit); sqlite3_bind_int(s,bi++,offset);
      int ncol=sqlite3_column_count(s);
      int scan_rc;
      while ((scan_rc=sqlite3_step(s))==SQLITE_ROW){
        cJSON *r=cJSON_CreateObject();
        for (int i=0;i<ncol;i++){
          const char *cn=sqlite3_column_name(s,i);
          switch (sqlite3_column_type(s,i)){
            case SQLITE_NULL: cJSON_AddNullToObject(r,cn); break;
            case SQLITE_INTEGER: cJSON_AddNumberToObject(r,cn,(double)sqlite3_column_int64(s,i)); break;
            case SQLITE_FLOAT: cJSON_AddNumberToObject(r,cn,sqlite3_column_double(s,i)); break;
            default: cJSON_AddStringToObject(r,cn,(const char*)sqlite3_column_text(s,i)); break;
          }
        }
        cJSON_AddItemToArray(rows,r);
      }
      /* `while (step() == ROW)` cannot tell DONE from IOERR/CORRUPT/BUSY/
       * INTERRUPT: a page that died after 3 of its 50 rows is a 3-row page
       * that reads as the whole page. */
      if (scan_rc!=SQLITE_DONE)
        FAILED("row read failed after %d row(s): %s",
               cJSON_GetArraySize(rows), sqlite3_errmsg(db->h));
    } else FAILED("row prepare failed: %s", sqlite3_errmsg(db->h));
    sqlite3_finalize(s); s=NULL;
  }
  free(sql); free(where); free(like);
  #undef FAILED

  if (!complete)
    fprintf(stderr,"[dbexplorer] %s: %s\n",name,err);

  cJSON *o=cJSON_CreateObject();
  cJSON_AddStringToObject(o,"name",name);
  cJSON_AddItemToObject(o,"columns",cols);
  cJSON_AddItemToObject(o,"rows",rows);
  /* `total` stays a plain number even when it is not known — the existing iOS
   * DBPage decodes it as a non-optional Int and a null would fail the whole
   * response, error field included. `total_known`/`complete`/`error` carry the
   * truth alongside it; a reader must not take total==0 as "empty table"
   * without checking them. */
  cJSON_AddNumberToObject(o,"total",(double)total);
  cJSON_AddBoolToObject(o,"total_known",total_known);
  cJSON_AddBoolToObject(o,"complete",complete);
  if (!complete) cJSON_AddStringToObject(o,"error",err);
  cJSON_AddNumberToObject(o,"limit",limit);
  cJSON_AddNumberToObject(o,"offset",offset);
  cJSON_AddItemToObject(o,"orderBy",ob_ok?cJSON_CreateString(orderBy):cJSON_CreateNull());
  cJSON_AddStringToObject(o,"orderDir",dir);
  cJSON_AddStringToObject(o,"q",hasq?q:"");
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}

/* minimal 5-field cron -> next datetime after now (UTC). Supports a star,
 * a star-slash-step, and plain numbers in each field — enough for the 3
 * fixed jobs (0 star/2 star star star ; 15 star star star star ; 30 ...). */
static int fld_match(const char *f, int v) {
  if (!strcmp(f,"*")) return 1;
  if (!strncmp(f,"*/",2)) { int n=atoi(f+2); return n>0 && (v%n)==0; }
  return atoi(f)==v;
}
static void cron_next_iso(const char *cron, char *out, size_t n) {
  char c[64]; snprintf(c,sizeof c,"%s",cron);
  char *mi=strtok(c," "), *ho=strtok(NULL," "), *dm=strtok(NULL," "),
       *mo=strtok(NULL," "), *dw=strtok(NULL," ");
  if (!mi||!ho||!dm||!mo||!dw){ snprintf(out,n,"null"); return; }
  time_t t=time(NULL); t -= t%60; t += 60;       /* start next minute */
  for (int i=0;i<366*24*60;i++,t+=60){
    struct tm g; gmtime_r(&t,&g);
    if (fld_match(mi,g.tm_min)&&fld_match(ho,g.tm_hour)&&
        fld_match(dm,g.tm_mday)&&fld_match(mo,g.tm_mon+1)&&
        fld_match(dw,g.tm_wday)){
      strftime(out,n,"%Y-%m-%dT%H:%M:%S.000Z",&g); return;
    }
  }
  snprintf(out,n,"null");
}
char *dbexplorer_scheduler(db_handle *db) {
  const char *CR[3]={"0 */2 * * *","15 * * * *","30 * * * *"};
  const char *ID[3]={"source-probe","camera-discovery","transport-discovery"};
  const char *DE[3]={"Probe every source's health endpoint every 2 hours",
    "Re-run camera discovery hourly at :15","Re-run transport fan-out hourly at :30"};
  char lastp[64]={0}; sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,"SELECT MAX(timestamp) FROM fetch_log",-1,&s,NULL)==SQLITE_OK
      && sqlite3_step(s)==SQLITE_ROW && sqlite3_column_type(s,0)!=SQLITE_NULL)
    snprintf(lastp,sizeof lastp,"%s",(const char*)sqlite3_column_text(s,0));
  sqlite3_finalize(s);
  cJSON *jobs=cJSON_CreateArray();
  for (int i=0;i<3;i++){
    cJSON *j=cJSON_CreateObject();
    cJSON_AddStringToObject(j,"id",ID[i]);
    cJSON_AddStringToObject(j,"cron",CR[i]);
    cJSON_AddStringToObject(j,"description",DE[i]);
    /* only source-probe has a DB-derivable last_run; camera/transport last
     * run is in-process scheduler state, not tracked in C → null. */
    if (i==0 && lastp[0]) cJSON_AddStringToObject(j,"last_run",lastp);
    else cJSON_AddNullToObject(j,"last_run");
    char nx[40]; cron_next_iso(CR[i],nx,sizeof nx);
    if (!strcmp(nx,"null")) cJSON_AddNullToObject(j,"next_run");
    else cJSON_AddStringToObject(j,"next_run",nx);
    cJSON_AddItemToArray(jobs,j);
  }
  cJSON *src=cJSON_CreateArray();
  if (sqlite3_prepare_v2(db->h,
    "SELECT id,name,category,last_check,last_success,status,records_count,"
    "response_time_ms FROM sources ORDER BY id",-1,&s,NULL)==SQLITE_OK){
    while (sqlite3_step(s)==SQLITE_ROW){
      cJSON *r=cJSON_CreateObject();
      cJSON_AddStringToObject(r,"id",(const char*)sqlite3_column_text(s,0));
      cJSON_AddStringToObject(r,"name",(const char*)sqlite3_column_text(s,1));
      cJSON_AddStringToObject(r,"category",(const char*)sqlite3_column_text(s,2));
      for (int i=3;i<=4;i++){ const char*k=i==3?"last_check":"last_success";
        if (sqlite3_column_type(s,i)==SQLITE_NULL) cJSON_AddNullToObject(r,k);
        else cJSON_AddStringToObject(r,k,(const char*)sqlite3_column_text(s,i)); }
      cJSON_AddStringToObject(r,"status",(const char*)sqlite3_column_text(s,5));
      if (sqlite3_column_type(s,6)==SQLITE_NULL) cJSON_AddNullToObject(r,"records_count");
      else cJSON_AddNumberToObject(r,"records_count",(double)sqlite3_column_int64(s,6));
      if (sqlite3_column_type(s,7)==SQLITE_NULL) cJSON_AddNullToObject(r,"response_time_ms");
      else cJSON_AddNumberToObject(r,"response_time_ms",(double)sqlite3_column_int64(s,7));
      cJSON_AddItemToArray(src,r);
    }
  }
  sqlite3_finalize(s);
  cJSON *o=cJSON_CreateObject();
  cJSON_AddItemToObject(o,"jobs",jobs);
  cJSON_AddItemToObject(o,"sources",src);
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}

/* ── admin/maintenance (buildMaintenanceDigest) ──────────────────────── */
