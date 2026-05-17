#include "geoproxyapi.h"
#include "httpclient.h"
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

typedef struct { char key[256]; char *val; time_t exp; } cent;
static cent CACHE[256]; static int ncache;
static const char *cache_get(const char *k){
  time_t now=time(NULL);
  for (int i=0;i<ncache;i++) if (!strcmp(CACHE[i].key,k)){
    if (CACHE[i].exp<now) return NULL; return CACHE[i].val; }
  return NULL;
}
static void cache_put(const char *k,const char *v,int ttl_s){
  for (int i=0;i<ncache;i++) if (!strcmp(CACHE[i].key,k)){
    free(CACHE[i].val); CACHE[i].val=strdup(v); CACHE[i].exp=time(NULL)+ttl_s; return; }
  if (ncache<256){ snprintf(CACHE[ncache].key,sizeof CACHE[ncache].key,"%s",k);
    CACHE[ncache].val=strdup(v); CACHE[ncache].exp=time(NULL)+ttl_s; ncache++; }
}
static void urlenc(const char *s,char *o,size_t n){
  size_t j=0; for (;*s&&j+4<n;s++){
    if (isalnum((unsigned char)*s)||strchr("-_.~",*s)) o[j++]=*s;
    else { snprintf(o+j,4,"%%%02X",(unsigned char)*s); j+=3; } }
  o[j]=0;
}
static cJSON *http_json(const char *method,const char *url,const char *body){
  http_client *hc=http_client_new(); if(!hc) return NULL;
  const char *hdrs[]={"User-Agent: " UA,"Accept: application/json",
                      body?"Content-Type: application/json":NULL,NULL};
  http_response r={0};
  int rc=http_request(hc,method,url,hdrs,body,body?strlen(body):0,8000,1,&r);
  cJSON *j=NULL;
  if (rc==0 && r.status>=200 && r.status<300 && r.body) j=cJSON_Parse(r.body);
  http_response_free(&r); http_client_free(hc);
  return j;
}
static double jnum(cJSON *o,const char *k){ cJSON *v=cJSON_GetObjectItem(o,k);
  if (cJSON_IsNumber(v)) return v->valuedouble;
  if (cJSON_IsString(v)) return atof(v->valuestring); return NAN; }

/* ── geocode ─────────────────────────────────────────────────────────── */
static cJSON *fwd_nominatim(const char *q){
  char e[512],u[700]; urlenc(q,e,sizeof e);
  snprintf(u,sizeof u,"https://nominatim.openstreetmap.org/search?format=json&addressdetails=1&limit=5&countrycodes=jp&q=%s",e);
  cJSON *d=http_json("GET",u,NULL);
  if (!d||!cJSON_IsArray(d)||cJSON_GetArraySize(d)==0){ cJSON_Delete(d); return NULL; }
  cJSON *out=cJSON_CreateArray(),*r;
  cJSON_ArrayForEach(r,d){
    cJSON *h=cJSON_CreateObject();
    cJSON_AddNumberToObject(h,"lat",jnum(r,"lat"));
    cJSON_AddNumberToObject(h,"lon",jnum(r,"lon"));
    cJSON *dn=cJSON_GetObjectItem(r,"display_name");
    cJSON_AddItemToObject(h,"display_name",dn?cJSON_Duplicate(dn,1):cJSON_CreateNull());
    cJSON *ty=cJSON_GetObjectItem(r,"type"); cJSON *cl=cJSON_GetObjectItem(r,"class");
    cJSON_AddItemToObject(h,"type",(ty&&cJSON_IsString(ty))?cJSON_Duplicate(ty,1):
      (cl?cJSON_Duplicate(cl,1):cJSON_CreateNull()));
    cJSON_AddStringToObject(h,"source","nominatim");
    cJSON_AddItemToArray(out,h);
  }
  cJSON_Delete(d); return out;
}
static cJSON *join3(cJSON *o,const char*a,const char*b,const char*c){
  char buf[512]=""; const char*ks[3]={a,b,c}; int first=1;
  for (int i=0;i<3;i++){ cJSON *v=cJSON_GetObjectItem(o,ks[i]);
    if (v&&cJSON_IsString(v)&&v->valuestring[0]){
      if(!first) strncat(buf,", ",sizeof buf-strlen(buf)-1);
      strncat(buf,v->valuestring,sizeof buf-strlen(buf)-1); first=0; } }
  return cJSON_CreateString(buf);
}
static cJSON *fwd_photon(const char *q){
  char e[512],u[700]; urlenc(q,e,sizeof e);
  snprintf(u,sizeof u,"https://photon.komoot.io/api/?q=%s&limit=5&bbox=122,24,154,46",e);
  cJSON *d=http_json("GET",u,NULL); cJSON *f=d?cJSON_GetObjectItem(d,"features"):NULL;
  if (!f||!cJSON_IsArray(f)||cJSON_GetArraySize(f)==0){ cJSON_Delete(d); return NULL; }
  cJSON *out=cJSON_CreateArray(),*ft;
  cJSON_ArrayForEach(ft,f){
    cJSON *pr=cJSON_GetObjectItem(ft,"properties");
    cJSON *cc=pr?cJSON_GetObjectItem(pr,"countrycode"):NULL;
    if (!cc||!cJSON_IsString(cc)||strcasecmp(cc->valuestring,"JP")!=0) continue;
    cJSON *g=cJSON_GetObjectItem(ft,"geometry"),*co=g?cJSON_GetObjectItem(g,"coordinates"):NULL;
    if (!co||cJSON_GetArraySize(co)<2) continue;
    cJSON *h=cJSON_CreateObject();
    cJSON_AddNumberToObject(h,"lat",cJSON_GetArrayItem(co,1)->valuedouble);
    cJSON_AddNumberToObject(h,"lon",cJSON_GetArrayItem(co,0)->valuedouble);
    cJSON_AddItemToObject(h,"display_name",join3(pr,"name","city","state"));
    cJSON *ov=pr?cJSON_GetObjectItem(pr,"osm_value"):NULL;
    cJSON_AddItemToObject(h,"type",(ov&&cJSON_IsString(ov))?cJSON_Duplicate(ov,1):cJSON_CreateNull());
    cJSON_AddStringToObject(h,"source","photon");
    cJSON_AddItemToArray(out,h);
  }
  cJSON_Delete(d);
  if (cJSON_GetArraySize(out)==0){ cJSON_Delete(out); return NULL; }
  return out;
}
static cJSON *fwd_gsi(const char *q){
  char e[512],u[700]; urlenc(q,e,sizeof e);
  snprintf(u,sizeof u,"https://msearch.gsi.go.jp/address-search/AddressSearch?q=%s",e);
  cJSON *d=http_json("GET",u,NULL);
  if (!d||!cJSON_IsArray(d)||cJSON_GetArraySize(d)==0){ cJSON_Delete(d); return NULL; }
  cJSON *out=cJSON_CreateArray(),*r; int cnt=0;
  cJSON_ArrayForEach(r,d){ if (cnt++>=5) break;
    cJSON *g=cJSON_GetObjectItem(r,"geometry"),*co=g?cJSON_GetObjectItem(g,"coordinates"):NULL;
    if (!co||cJSON_GetArraySize(co)<2) continue;
    double lat=cJSON_GetArrayItem(co,1)->valuedouble, lon=cJSON_GetArrayItem(co,0)->valuedouble;
    if (!isfinite(lat)||!isfinite(lon)) continue;
    cJSON *pr=cJSON_GetObjectItem(r,"properties"),*ti=pr?cJSON_GetObjectItem(pr,"title"):NULL;
    cJSON *h=cJSON_CreateObject();
    cJSON_AddNumberToObject(h,"lat",lat); cJSON_AddNumberToObject(h,"lon",lon);
    cJSON_AddStringToObject(h,"display_name",(ti&&cJSON_IsString(ti))?ti->valuestring:q);
    cJSON_AddStringToObject(h,"type","address");
    cJSON_AddStringToObject(h,"source","gsi");
    cJSON_AddItemToArray(out,h);
  }
  cJSON_Delete(d);
  if (cJSON_GetArraySize(out)==0){ cJSON_Delete(out); return NULL; }
  return out;
}
/* returns cJSON array (hits, owned) + sets *prov; uses 24h cache */
static cJSON *fwd_one(const char *q,const char **prov){
  char ck[300]; snprintf(ck,sizeof ck,"fwd:%s",q);
  const char *c=cache_get(ck);
  if (c){ cJSON *h=cJSON_Parse(c);
    cJSON *f=cJSON_GetArrayItem(h,0); cJSON *sp=f?cJSON_GetObjectItem(f,"source"):NULL;
    *prov=(sp&&cJSON_IsString(sp))?sp->valuestring:NULL; return h; }
  cJSON *(*P[3])(const char*)={fwd_nominatim,fwd_photon,fwd_gsi};
  for (int i=0;i<3;i++){ cJSON *h=P[i](q);
    if (h&&cJSON_GetArraySize(h)>0){ char *t=cJSON_PrintUnformatted(h);
      cache_put(ck,t,86400); free(t);
      cJSON *f=cJSON_GetArrayItem(h,0);
      *prov=cJSON_GetObjectItem(f,"source")->valuestring; return h; }
    cJSON_Delete(h); }
  *prov=NULL; return cJSON_CreateArray();
}
char *geoproxy_geocode_forward(const char *q,const char *qAlt){
  const char *p1=NULL; cJSON *h1=fwd_one(q,&p1);
  cJSON *o=cJSON_CreateObject();
  if (!qAlt||!*qAlt||!strcmp(qAlt,q)){
    cJSON_AddItemToObject(o,"results",h1);
    cJSON_AddItemToObject(o,"provider",p1?cJSON_CreateString(p1):cJSON_CreateNull());
  } else {
    const char *p2=NULL; cJSON *h2=fwd_one(qAlt,&p2);
    cJSON *merged=cJSON_CreateArray(); char seen[64][32]; int ns=0; cJSON *it;
    cJSON_ArrayForEach(it,h1){
      char k[32]; snprintf(k,sizeof k,"%.5f,%.5f",
        cJSON_GetObjectItem(it,"lat")->valuedouble,cJSON_GetObjectItem(it,"lon")->valuedouble);
      int dup=0; for(int i=0;i<ns;i++) if(!strcmp(seen[i],k))dup=1;
      if(dup)continue; if(ns<64)snprintf(seen[ns++],32,"%s",k);
      cJSON *d=cJSON_Duplicate(it,1); cJSON_AddBoolToObject(d,"via_translation",0);
      cJSON_AddItemToArray(merged,d);
    }
    cJSON_ArrayForEach(it,h2){
      char k[32]; snprintf(k,sizeof k,"%.5f,%.5f",
        cJSON_GetObjectItem(it,"lat")->valuedouble,cJSON_GetObjectItem(it,"lon")->valuedouble);
      int dup=0; for(int i=0;i<ns;i++) if(!strcmp(seen[i],k))dup=1;
      if(dup)continue; if(ns<64)snprintf(seen[ns++],32,"%s",k);
      cJSON *d=cJSON_Duplicate(it,1); cJSON_AddBoolToObject(d,"via_translation",1);
      cJSON_AddStringToObject(d,"matched_alt",qAlt);
      cJSON_AddItemToArray(merged,d);
    }
    cJSON_Delete(h1); cJSON_Delete(h2);
    cJSON_AddItemToObject(o,"results",merged);
    cJSON_AddItemToObject(o,"provider",p1?cJSON_CreateString(p1):
      p2?cJSON_CreateString(p2):cJSON_CreateNull());
  }
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}
char *geoproxy_geocode_reverse(double lat,double lon){
  char ck[64]; snprintf(ck,sizeof ck,"rev2:%.5f,%.5f",lat,lon);
  const char *c=cache_get(ck);
  if (c){ cJSON *h=cJSON_Parse(c); cJSON_AddBoolToObject(h,"fromCache",1);
    char *j=cJSON_PrintUnformatted(h); cJSON_Delete(h); return j; }
  char u[256];
  /* Nominatim (ja then en) */
  snprintf(u,sizeof u,"https://nominatim.openstreetmap.org/reverse?format=json&addressdetails=1&zoom=18&lat=%.6f&lon=%.6f&accept-language=ja",lat,lon);
  cJSON *ja=http_json("GET",u,NULL);
  snprintf(u,sizeof u,"https://nominatim.openstreetmap.org/reverse?format=json&addressdetails=1&zoom=18&lat=%.6f&lon=%.6f&accept-language=en",lat,lon);
  cJSON *en=http_json("GET",u,NULL);
  cJSON *prim=ja?ja:en;
  cJSON *hit=NULL;
  if (prim && !cJSON_GetObjectItem(prim,"error")){
    hit=cJSON_CreateObject();
    cJSON_AddNumberToObject(hit,"lat",jnum(prim,"lat"));
    cJSON_AddNumberToObject(hit,"lon",jnum(prim,"lon"));
    cJSON *dja=ja?cJSON_GetObjectItem(ja,"display_name"):NULL;
    cJSON *den=en?cJSON_GetObjectItem(en,"display_name"):NULL;
    cJSON_AddItemToObject(hit,"display_name",
      (dja&&cJSON_IsString(dja))?cJSON_Duplicate(dja,1):
      (den&&cJSON_IsString(den))?cJSON_Duplicate(den,1):cJSON_CreateNull());
    cJSON_AddItemToObject(hit,"display_name_ja",(dja&&cJSON_IsString(dja))?cJSON_Duplicate(dja,1):cJSON_CreateNull());
    cJSON_AddItemToObject(hit,"display_name_en",(den&&cJSON_IsString(den))?cJSON_Duplicate(den,1):cJSON_CreateNull());
    cJSON *ad=cJSON_GetObjectItem(prim,"address");
    cJSON_AddItemToObject(hit,"address",ad?cJSON_Duplicate(ad,1):cJSON_CreateNull());
    cJSON_AddStringToObject(hit,"source","nominatim");
  }
  cJSON_Delete(ja); cJSON_Delete(en);
  if (!hit){ /* photon */
    snprintf(u,sizeof u,"https://photon.komoot.io/reverse?lat=%.6f&lon=%.6f",lat,lon);
    cJSON *d=http_json("GET",u,NULL); cJSON *fa=d?cJSON_GetObjectItem(d,"features"):NULL;
    cJSON *f=fa?cJSON_GetArrayItem(fa,0):NULL;
    if (f){ cJSON *g=cJSON_GetObjectItem(f,"geometry"),*co=g?cJSON_GetObjectItem(g,"coordinates"):NULL;
      cJSON *pr=cJSON_GetObjectItem(f,"properties");
      if (co&&cJSON_GetArraySize(co)>=2){ hit=cJSON_CreateObject();
        cJSON_AddNumberToObject(hit,"lat",cJSON_GetArrayItem(co,1)->valuedouble);
        cJSON_AddNumberToObject(hit,"lon",cJSON_GetArrayItem(co,0)->valuedouble);
        cJSON_AddItemToObject(hit,"display_name",join3(pr,"name","city","state"));
        cJSON_AddItemToObject(hit,"address",pr?cJSON_Duplicate(pr,1):cJSON_CreateNull());
        cJSON_AddStringToObject(hit,"source","photon"); } }
    cJSON_Delete(d);
  }
  if (!hit){ /* gsi */
    snprintf(u,sizeof u,"https://mreversegeocoder.gsi.go.jp/reverse-geocoder/LonLatToAddress?lat=%.6f&lon=%.6f",lat,lon);
    cJSON *d=http_json("GET",u,NULL); cJSON *rr=d?cJSON_GetObjectItem(d,"results"):NULL;
    if (rr){ cJSON *lv=cJSON_GetObjectItem(rr,"lv01Nm"),*mc=cJSON_GetObjectItem(rr,"muniCd");
      hit=cJSON_CreateObject();
      cJSON_AddNumberToObject(hit,"lat",lat); cJSON_AddNumberToObject(hit,"lon",lon);
      cJSON_AddItemToObject(hit,"display_name",
        (lv&&cJSON_IsString(lv))?cJSON_Duplicate(lv,1):
        (mc&&cJSON_IsString(mc))?cJSON_Duplicate(mc,1):cJSON_CreateNull());
      cJSON *ad=cJSON_CreateObject();
      cJSON_AddItemToObject(ad,"muniCd",mc?cJSON_Duplicate(mc,1):cJSON_CreateNull());
      cJSON_AddItemToObject(ad,"lv01Nm",lv?cJSON_Duplicate(lv,1):cJSON_CreateNull());
      cJSON_AddItemToObject(hit,"address",ad);
      cJSON_AddStringToObject(hit,"source","gsi"); }
    cJSON_Delete(d);
  }
  if (hit){ cJSON *dn=cJSON_GetObjectItem(hit,"display_name");
    if (dn&&cJSON_IsString(dn)&&dn->valuestring[0]){
      char *t=cJSON_PrintUnformatted(hit); cache_put(ck,t,86400); free(t);
      char *j=cJSON_PrintUnformatted(hit); cJSON_Delete(hit); return j; }
    cJSON_Delete(hit);
  }
  cJSON *o=cJSON_CreateObject();
  cJSON_AddNumberToObject(o,"lat",lat); cJSON_AddNumberToObject(o,"lon",lon);
  cJSON_AddNullToObject(o,"display_name"); cJSON_AddNullToObject(o,"source");
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}

/* ── plateauCatalog (GraphQL proxy, 24h cache) ───────────────────────── */
static long g_plateau_fetched;
static char *plat_pick(cJSON *items,int lod){
  /* CESIUM3DTILES by lod preference: preferred,1,2,0,any */
  int order[5]={lod,1,2,0,-1}; cJSON *it;
  for (int k=0;k<5;k++){
    cJSON_ArrayForEach(it,items){
      cJSON *fmt=cJSON_GetObjectItem(it,"format"),*l=cJSON_GetObjectItem(it,"lod");
      if (fmt&&cJSON_IsString(fmt)&&!strcmp(fmt->valuestring,"CESIUM3DTILES")){
        if (order[k]==-1) return cJSON_PrintUnformatted(it);
        if (l&&cJSON_IsNumber(l)&&(int)l->valuedouble==order[k]) return cJSON_PrintUnformatted(it);
      }
    }
  }
  return NULL;
}
char *geoproxy_plateau_tilesets(int lod,int *status){
  char ck[16]="plateau:ds";
  const char *cached=cache_get(ck);
  cJSON *datasets=NULL;
  if (cached) datasets=cJSON_Parse(cached);
  if (!datasets){
    const char *Q="{\"query\":\"query NationwideBuildingTilesets { datasets(input: { includeTypes: [\\\"bldg\\\"] }) { __typename ... on PlateauDataset { id name year prefectureCode cityCode prefecture { name } city { name } items { id format url lod texture } } } }\"}";
    cJSON *resp=http_json("POST","https://api.plateau.reearth.io/datacatalog/graphql",Q);
    cJSON *dat=resp?cJSON_GetObjectItem(resp,"data"):NULL;
    cJSON *ds=dat?cJSON_GetObjectItem(dat,"datasets"):NULL;
    if (!ds||!cJSON_IsArray(ds)){ cJSON_Delete(resp); *status=502;
      return strdup("{\"error\":\"catalog_unavailable\"}"); }
    datasets=cJSON_Duplicate(ds,1); cJSON_Delete(resp);
    char *t=cJSON_PrintUnformatted(datasets); cache_put(ck,t,86400); free(t);
    g_plateau_fetched=(long)time(NULL)*1000;
  }
  cJSON *out=cJSON_CreateArray(),*ds;
  cJSON_ArrayForEach(ds,datasets){
    cJSON *items=cJSON_GetObjectItem(ds,"items");
    if (!items||!cJSON_IsArray(items)) continue;
    char *itj=plat_pick(items,lod); if (!itj) continue;
    cJSON *item=cJSON_Parse(itj); free(itj);
    cJSON *url=item?cJSON_GetObjectItem(item,"url"):NULL;
    if (!url||!cJSON_IsString(url)||!url->valuestring[0]){ cJSON_Delete(item); continue; }
    cJSON *city=cJSON_GetObjectItem(ds,"city"),*pref=cJSON_GetObjectItem(ds,"prefecture");
    cJSON *o=cJSON_CreateObject();
    cJSON *idv=cJSON_GetObjectItem(ds,"id");
    cJSON_AddItemToObject(o,"id",idv?cJSON_Duplicate(idv,1):cJSON_CreateNull());
    cJSON *cn=city?cJSON_GetObjectItem(city,"name"):NULL;
    cJSON_AddItemToObject(o,"city",(cn&&cJSON_IsString(cn))?cJSON_Duplicate(cn,1):cJSON_CreateNull());
    cJSON *pn=pref?cJSON_GetObjectItem(pref,"name"):NULL;
    cJSON_AddItemToObject(o,"prefecture",(pn&&cJSON_IsString(pn))?cJSON_Duplicate(pn,1):cJSON_CreateNull());
    cJSON *ccode=cJSON_GetObjectItem(ds,"cityCode"),*pcode=cJSON_GetObjectItem(ds,"prefectureCode"),
          *yr=cJSON_GetObjectItem(ds,"year"),*ld=cJSON_GetObjectItem(item,"lod");
    cJSON_AddItemToObject(o,"cityCode",ccode?cJSON_Duplicate(ccode,1):cJSON_CreateNull());
    cJSON_AddItemToObject(o,"prefectureCode",pcode?cJSON_Duplicate(pcode,1):cJSON_CreateNull());
    cJSON_AddItemToObject(o,"year",yr?cJSON_Duplicate(yr,1):cJSON_CreateNull());
    cJSON_AddItemToObject(o,"lod",ld?cJSON_Duplicate(ld,1):cJSON_CreateNull());
    cJSON_AddItemToObject(o,"tilesetUrl",cJSON_Duplicate(url,1));
    cJSON_AddItemToArray(out,o);
    cJSON_Delete(item);
  }
  cJSON_Delete(datasets);
  cJSON *o=cJSON_CreateObject();
  cJSON_AddItemToObject(o,"tilesets",out);
  cJSON_AddNumberToObject(o,"count",cJSON_GetArraySize(out));
  cJSON_AddNumberToObject(o,"preferredLod",lod);
  cJSON_AddNumberToObject(o,"fetchedAt",(double)g_plateau_fetched);
  *status=200;
  char *j=cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}
