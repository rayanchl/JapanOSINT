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
  /* live runtime URL-swap overrides (the C apply mechanism for merged repairs) */
  cJSON *ovr=cJSON_CreateArray();
  if (sqlite3_prepare_v2(h,
    "SELECT source_id,old_url,new_url,anomaly_id,created_at FROM collector_url_overrides "
    "ORDER BY created_at DESC LIMIT 100",-1,&s,NULL)==SQLITE_OK){
    while (sqlite3_step(s)==SQLITE_ROW){
      cJSON *c=cJSON_CreateObject();
      cJSON_AddStringToObject(c,"source_id",(const char*)sqlite3_column_text(s,0));
      cJSON_AddStringToObject(c,"old_url",(const char*)sqlite3_column_text(s,1));
      cJSON_AddStringToObject(c,"new_url",(const char*)sqlite3_column_text(s,2));
      if (sqlite3_column_type(s,3)==SQLITE_NULL) cJSON_AddNullToObject(c,"anomaly_id");
      else cJSON_AddNumberToObject(c,"anomaly_id",(double)sqlite3_column_int64(s,3));
      cJSON_AddStringToObject(c,"created_at",(const char*)sqlite3_column_text(s,4));
      cJSON_AddItemToArray(ovr,c);
    }
  }
  sqlite3_finalize(s);
  cJSON_AddItemToObject(o,"url_overrides",ovr);
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

/* ── per-source pipeline detail + operator actions ──────────────────────
 * The digest above is host-wide aggregate; these expose one source's full
 * detect→triage→repair chain and let the operator steer it. */

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
  snprintf(out,cap,"%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
    g.tm_year+1900,g.tm_mon+1,g.tm_mday,g.tm_hour,g.tm_min,g.tm_sec,
    sp.tv_nsec/1000000);
}
/* Build a {"error":"code"} body (malloc'd). */
static char *err_json(const char *code){
  cJSON *o=cJSON_CreateObject(); cJSON_AddStringToObject(o,"error",code);
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}

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

  cJSON *runs=cJSON_CreateArray();
  if (sqlite3_prepare_v2(h,
    "SELECT id,timestamp,status,records_fetched,duration_ms,error "
    "FROM fetch_log WHERE source_id=?1 ORDER BY timestamp DESC,id DESC LIMIT 30",
    -1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_text(s,1,source_id,-1,SQLITE_TRANSIENT);
    while (sqlite3_step(s)==SQLITE_ROW){
      cJSON *r=cJSON_CreateObject();
      col_int(r,"id",s,0); col_text(r,"timestamp",s,1); col_text(r,"status",s,2);
      col_int(r,"records_fetched",s,3); col_int(r,"duration_ms",s,4);
      col_text(r,"error",s,5);
      cJSON_AddItemToArray(runs,r);
    }
  }
  sqlite3_finalize(s);

  cJSON *anoms=cJSON_CreateArray();
  if (sqlite3_prepare_v2(h,
    "SELECT id,fetch_log_id,verdict,reason,evidence,escalation_level,created_at,"
    "resolved_at,resolution,triage_class,triage_confidence,triage_evidence,"
    "triage_suggested_fix,triaged_at,triage_model "
    "FROM collector_anomaly WHERE source_id=?1 "
    "ORDER BY created_at DESC,id DESC LIMIT 20",-1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_text(s,1,source_id,-1,SQLITE_TRANSIENT);
    while (sqlite3_step(s)==SQLITE_ROW){
      cJSON *r=cJSON_CreateObject();
      col_int(r,"id",s,0); col_int(r,"fetch_log_id",s,1); col_text(r,"verdict",s,2);
      col_text(r,"reason",s,3); col_text(r,"evidence",s,4);
      col_int(r,"escalation_level",s,5); col_text(r,"created_at",s,6);
      col_text(r,"resolved_at",s,7); col_text(r,"resolution",s,8);
      col_text(r,"triage_class",s,9); col_real(r,"triage_confidence",s,10);
      col_text(r,"triage_evidence",s,11); col_text(r,"triage_suggested_fix",s,12);
      col_text(r,"triaged_at",s,13); col_text(r,"triage_model",s,14);
      cJSON_AddItemToArray(anoms,r);
    }
  }
  sqlite3_finalize(s);

  cJSON *reps=cJSON_CreateArray();
  if (sqlite3_prepare_v2(h,
    "SELECT id,anomaly_id,status,action,patch,gate,model,pr_url,triage_class,created_at "
    "FROM collector_repair WHERE source_id=?1 "
    "ORDER BY created_at DESC,id DESC LIMIT 30",-1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_text(s,1,source_id,-1,SQLITE_TRANSIENT);
    while (sqlite3_step(s)==SQLITE_ROW){
      cJSON *r=cJSON_CreateObject();
      col_int(r,"id",s,0); col_int(r,"anomaly_id",s,1); col_text(r,"status",s,2);
      col_text(r,"action",s,3); col_text(r,"patch",s,4); col_text(r,"gate",s,5);
      col_text(r,"model",s,6); col_text(r,"pr_url",s,7);
      col_text(r,"triage_class",s,8); col_text(r,"created_at",s,9);
      cJSON_AddItemToArray(reps,r);
    }
  }
  sqlite3_finalize(s);

  char ts[40]; iso_now(ts,sizeof ts);
  cJSON *o=cJSON_CreateObject();
  cJSON_AddStringToObject(o,"generated_at",ts);
  cJSON_AddItemToObject(o,"source",src);
  cJSON_AddItemToObject(o,"fetch_log",runs);
  cJSON_AddItemToObject(o,"anomalies",anoms);
  cJSON_AddItemToObject(o,"repairs",reps);
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}

/* Resolve an anomaly with an operator-supplied resolution string. */
static void resolve_anomaly_op(sqlite3 *h, long anomaly_id, const char *res){
  sqlite3_stmt *u;
  if (sqlite3_prepare_v2(h,
    "UPDATE collector_anomaly SET resolved_at=datetime('now'),resolution=?2 "
    "WHERE id=?1",-1,&u,NULL)==SQLITE_OK){
    sqlite3_bind_int64(u,1,(sqlite3_int64)anomaly_id);
    sqlite3_bind_text(u,2,res,-1,SQLITE_TRANSIENT);
    sqlite3_step(u);
  }
  sqlite3_finalize(u);
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
      if (pj) cJSON_Delete(pj); free(patch);
      *status=422; return err_json("patch_missing_urls");
    }
    sqlite3_stmt *u;
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
      sqlite3_step(u);
    }
    sqlite3_finalize(u);
    url_override_reload(db);          /* activate the swap live, no restart */
    if (sqlite3_prepare_v2(h,
      "UPDATE collector_repair SET status='merged' WHERE id=?1",-1,&u,NULL)==SQLITE_OK){
      sqlite3_bind_int64(u,1,(sqlite3_int64)repair_id); sqlite3_step(u);
    }
    sqlite3_finalize(u);
    if (anomaly_id) resolve_anomaly_op(h,anomaly_id,"approved_by_operator");
    fprintf(stderr,"[repair] OPERATOR APPROVED %s: %s -> %s\n",source_id,old_url,new_url);
    cJSON *o=cJSON_CreateObject();
    cJSON_AddBoolToObject(o,"ok",1);
    cJSON_AddNumberToObject(o,"repair_id",(double)repair_id);
    cJSON_AddStringToObject(o,"status","merged");
    cJSON_AddStringToObject(o,"source_id",source_id);
    cJSON_AddStringToObject(o,"old_url",old_url);
    cJSON_AddStringToObject(o,"new_url",new_url);
    if (pj) cJSON_Delete(pj); free(patch);
    char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o);
    *status=200; return j;
  }

  /* reject */
  free(patch);
  if (!strcmp(rstatus,"merged")){ *status=409; return err_json("already_merged"); }
  sqlite3_stmt *u;
  if (sqlite3_prepare_v2(h,
    "UPDATE collector_repair SET status='rejected' WHERE id=?1",-1,&u,NULL)==SQLITE_OK){
    sqlite3_bind_int64(u,1,(sqlite3_int64)repair_id); sqlite3_step(u);
  }
  sqlite3_finalize(u);
  if (anomaly_id) resolve_anomaly_op(h,anomaly_id,"rejected_by_operator");
  fprintf(stderr,"[repair] OPERATOR REJECTED repair #%ld (%s)\n",repair_id,source_id);
  cJSON *o=cJSON_CreateObject();
  cJSON_AddBoolToObject(o,"ok",1);
  cJSON_AddNumberToObject(o,"repair_id",(double)repair_id);
  cJSON_AddStringToObject(o,"status","rejected");
  cJSON_AddStringToObject(o,"source_id",source_id);
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
  /* Keep quarantined_at as history; clearing `until` + reason lifts the lock. */
  if (sqlite3_prepare_v2(h,
    "UPDATE sources SET quarantined_until=NULL,quarantine_reason=NULL WHERE id=?1",
    -1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_text(s,1,source_id,-1,SQLITE_TRANSIENT); sqlite3_step(s);
  }
  sqlite3_finalize(s);
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
  if (sqlite3_prepare_v2(h,
    "UPDATE collector_anomaly SET triaged_at=NULL,triage_class=NULL,"
    "triage_confidence=NULL,triage_evidence=NULL,triage_suggested_fix=NULL,"
    "triage_model=NULL,resolved_at=NULL,resolution=NULL WHERE id=?1",
    -1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_int64(s,1,(sqlite3_int64)anomaly_id); sqlite3_step(s);
  }
  sqlite3_finalize(s);
  fprintf(stderr,"[repair] OPERATOR REQUEUED anomaly #%ld (%s)\n",anomaly_id,source_id);
  cJSON *o=cJSON_CreateObject();
  cJSON_AddBoolToObject(o,"ok",1);
  cJSON_AddNumberToObject(o,"anomaly_id",(double)anomaly_id);
  cJSON_AddStringToObject(o,"source_id",source_id);
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o);
  *status=200; return j;
}

/* ── tiny TTL JSON cache (single-threaded mongoose loop) ─────────────── */
