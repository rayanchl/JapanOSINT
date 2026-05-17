#include "maintenanceapi.h"
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

static cJSON *recent_repairs(sqlite3 *h, const char *status, int hours) {
  cJSON *a=cJSON_CreateArray(); sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
    "SELECT id,anomaly_id,source_id,status,action,triage_class,pr_url,created_at "
    "FROM collector_repair WHERE status=?1 AND created_at>=datetime('now',?2) "
    "ORDER BY created_at DESC LIMIT 50",-1,&s,NULL)==SQLITE_OK){
    char win[32]; snprintf(win,sizeof win,"-%d hours",hours);
    sqlite3_bind_text(s,1,status,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,win,-1,SQLITE_TRANSIENT);
    static const char *K[]={"id","anomaly_id","source_id","status","action",
      "triage_class","pr_url","created_at"};
    while (sqlite3_step(s)==SQLITE_ROW){
      cJSON *r=cJSON_CreateObject();
      for (int i=0;i<8;i++){
        if (sqlite3_column_type(s,i)==SQLITE_NULL) cJSON_AddNullToObject(r,K[i]);
        else if (i==0||i==1) cJSON_AddNumberToObject(r,K[i],(double)sqlite3_column_int64(s,i));
        else cJSON_AddStringToObject(r,K[i],(const char*)sqlite3_column_text(s,i));
      }
      cJSON_AddItemToArray(a,r);
    }
  }
  sqlite3_finalize(s);
  return a;
}
static cJSON *rate_num(long su,long fa){ long t=su+fa;
  if (t==0) return cJSON_CreateNull();
  double v=(double)su/(double)t; v=round(v*1000.0)/1000.0;
  return cJSON_CreateNumber(v); }
char *maintenance_digest(db_handle *db, int hours) {
  if (hours<1) hours=24; if (hours>720) hours=720;
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
  const char *NA="(action IS NULL OR action <> 'await_recovery')";
  cJSON *byClass=cJSON_CreateArray();
  char q1[400];
  snprintf(q1,sizeof q1,
    "SELECT COALESCE(triage_class,'?'),"
    "SUM(status IN ('verified','merged')),SUM(status IN ('rejected','error')),"
    "SUM(status='needs_human') FROM collector_repair "
    "WHERE created_at>=datetime('now',?1) AND %s GROUP BY 1 "
    "ORDER BY (SUM(status IN ('verified','merged'))+SUM(status IN ('rejected','error'))) DESC",NA);
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
  cJSON *bySrc=cJSON_CreateArray();
  char q2[400];
  snprintf(q2,sizeof q2,
    "SELECT source_id,SUM(status IN ('verified','merged')),"
    "SUM(status IN ('rejected','error')) FROM collector_repair "
    "WHERE created_at>=datetime('now',?1) AND %s GROUP BY source_id "
    "HAVING (SUM(status IN ('verified','merged'))+SUM(status IN ('rejected','error')))>0 "
    "ORDER BY 3 DESC,2 DESC LIMIT 50",NA);
  if (sqlite3_prepare_v2(h,q2,-1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_text(s,1,win,-1,SQLITE_TRANSIENT);
    while (sqlite3_step(s)==SQLITE_ROW){
      long su=sqlite3_column_int64(s,1),fa=sqlite3_column_int64(s,2);
      cJSON *c=cJSON_CreateObject();
      cJSON_AddStringToObject(c,"source_id",(const char*)sqlite3_column_text(s,0));
      cJSON_AddNumberToObject(c,"success",(double)su);
      cJSON_AddNumberToObject(c,"fail",(double)fa);
      cJSON_AddItemToObject(c,"success_rate",rate_num(su,fa));
      cJSON_AddItemToArray(bySrc,c);
    }
  }
  sqlite3_finalize(s);
  cJSON *quar=cJSON_CreateArray();
  if (sqlite3_prepare_v2(h,
    "SELECT id,name,category,quarantined_at,quarantined_until,quarantine_reason,"
    "(quarantined_until>datetime('now')) FROM sources "
    "WHERE quarantined_until IS NOT NULL ORDER BY quarantined_at DESC",-1,&s,NULL)==SQLITE_OK){
    while (sqlite3_step(s)==SQLITE_ROW){
      cJSON *c=cJSON_CreateObject();
      cJSON_AddStringToObject(c,"source_id",(const char*)sqlite3_column_text(s,0));
      cJSON_AddStringToObject(c,"name",(const char*)sqlite3_column_text(s,1));
      cJSON_AddStringToObject(c,"category",(const char*)sqlite3_column_text(s,2));
      cJSON_AddItemToObject(c,"since",sqlite3_column_type(s,3)==SQLITE_NULL?cJSON_CreateNull():cJSON_CreateString((const char*)sqlite3_column_text(s,3)));
      cJSON_AddItemToObject(c,"until",sqlite3_column_type(s,4)==SQLITE_NULL?cJSON_CreateNull():cJSON_CreateString((const char*)sqlite3_column_text(s,4)));
      cJSON_AddBoolToObject(c,"active",sqlite3_column_int(s,6)!=0);
      cJSON_AddItemToObject(c,"reason",sqlite3_column_type(s,5)==SQLITE_NULL?cJSON_CreateNull():cJSON_CreateString((const char*)sqlite3_column_text(s,5)));
      cJSON_AddItemToArray(quar,c);
    }
  }
  sqlite3_finalize(s);
  cJSON *verifiedRows=recent_repairs(h,"verified",hours);
  cJSON *awaiting_pr=cJSON_CreateArray(), *awaiting_apply=cJSON_CreateArray(),
        *auto_dismissed=cJSON_CreateArray();
  cJSON *vr;
  cJSON_ArrayForEach(vr,verifiedRows){
    cJSON *act=cJSON_GetObjectItem(vr,"action");
    const char *as=cJSON_IsString(act)?act->valuestring:NULL;
    if (as && !strcmp(as,"url_swap")){
      cJSON *pr=cJSON_GetObjectItem(vr,"pr_url");
      cJSON_AddItemToArray((pr&&cJSON_IsString(pr))?awaiting_pr:awaiting_apply,
                           cJSON_Duplicate(vr,1));
    } else if (as && !strcmp(as,"auto_dismiss")){
      cJSON_AddItemToArray(auto_dismissed,cJSON_Duplicate(vr,1));
    }
  }
  cJSON_Delete(verifiedRows);

  char ts[40]; { time_t now=time(NULL); struct tm g; gmtime_r(&now,&g);
    struct timespec sp; clock_gettime(CLOCK_REALTIME,&sp);
    snprintf(ts,sizeof ts,"%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
      g.tm_year+1900,g.tm_mon+1,g.tm_mday,g.tm_hour,g.tm_min,g.tm_sec,
      sp.tv_nsec/1000000); }
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
  cJSON_AddItemToObject(o,"worst_sources",bySrc);
  cJSON_AddItemToObject(o,"quarantined",quar);
  cJSON_AddItemToObject(o,"auto_fixed",recent_repairs(h,"merged",hours));
  cJSON *aw=cJSON_CreateObject();
  cJSON_AddItemToObject(aw,"awaiting_pr",awaiting_pr);
  cJSON_AddItemToObject(aw,"awaiting_apply",awaiting_apply);
  cJSON_AddItemToObject(o,"awaiting_review",aw);
  cJSON_AddItemToObject(o,"auto_dismissed",auto_dismissed);
  cJSON_AddItemToObject(o,"needs_human",recent_repairs(h,"needs_human",hours));
  /* llmConcurrencySnapshot — C LLM runtime has no shared queue gauge; report
   * the configured limits with zeroed live counters (honest, stable shape). */
  cJSON *cc=cJSON_CreateObject();
  int hl=getenv("LLM_HEAVY_CONCURRENCY")?atoi(getenv("LLM_HEAVY_CONCURRENCY")):1;
  int ml=getenv("LLM_MID_CONCURRENCY")?atoi(getenv("LLM_MID_CONCURRENCY")):2;
  if (hl<1)hl=1; if (ml<1)ml=1;
  cJSON *hv=cJSON_CreateObject();
  cJSON_AddNumberToObject(hv,"limit",hl); cJSON_AddNumberToObject(hv,"inflight",0);
  cJSON_AddNumberToObject(hv,"waiting",0); cJSON_AddItemToObject(cc,"heavy",hv);
  cJSON *md=cJSON_CreateObject();
  cJSON_AddNumberToObject(md,"limit",ml); cJSON_AddNumberToObject(md,"inflight",0);
  cJSON_AddNumberToObject(md,"waiting",0); cJSON_AddItemToObject(cc,"mid",md);
  cJSON_AddItemToObject(o,"concurrency",cc);
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}

/* ── tiny TTL JSON cache (single-threaded mongoose loop) ─────────────── */
