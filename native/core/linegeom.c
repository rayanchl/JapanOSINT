/* core/linegeom.c — faithful port of transportStore.js stitchAndSmoothLines
 * + chaikinSmooth + simplifyPolyline + perpDistDeg + endpointKey. */
#include "linegeom.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

typedef struct { double *xy; int n; cJSON *props; } frag;   /* n = #points */

/* "%.5f,%.5f" endpoint key (== JS endpointKey). */
static void epkey(double x, double y, char *o, size_t n) {
  snprintf(o, n, "%.5f,%.5f", x, y);
}

/* ── chaikinOnce: interior vertex -> 1/4 & 3/4 pts; ends preserved ── */
static double *chaikin_once(const double *c, int n, int *outn) {
  if (n < 3) { double *r = malloc(sizeof(double) * 2 * n);
    memcpy(r, c, sizeof(double) * 2 * n); *outn = n; return r; }
  int m = 2 + 2 * (n - 1);
  double *o = malloc(sizeof(double) * 2 * m);
  int w = 0;
  o[w*2] = c[0]; o[w*2+1] = c[1]; w++;
  for (int i = 0; i < n - 1; i++) {
    double ax = c[i*2], ay = c[i*2+1], bx = c[(i+1)*2], by = c[(i+1)*2+1];
    o[w*2] = ax + 0.25*(bx-ax); o[w*2+1] = ay + 0.25*(by-ay); w++;
    o[w*2] = ax + 0.75*(bx-ax); o[w*2+1] = ay + 0.75*(by-ay); w++;
  }
  o[w*2] = c[(n-1)*2]; o[w*2+1] = c[(n-1)*2+1]; w++;
  *outn = w;
  return o;
}

static double perp_dist(double px,double py,double ax,double ay,double bx,double by){
  double dx=bx-ax, dy=by-ay, l2=dx*dx+dy*dy;
  if (l2==0){ double ex=px-ax,ey=py-ay; return sqrt(ex*ex+ey*ey); }
  double t=((px-ax)*dx+(py-ay)*dy)/l2;
  if (t<0) t=0; else if (t>1) t=1;
  double cx=ax+t*dx, cy=ay+t*dy, ex=px-cx, ey=py-cy;
  return sqrt(ex*ex+ey*ey);
}

/* iterative RDP, eps in degrees (== simplifyPolyline). */
static double *simplify(const double *c, int n, double eps, int *outn) {
  if (n < 3) { double *r=malloc(sizeof(double)*2*n);
    memcpy(r,c,sizeof(double)*2*n); *outn=n; return r; }
  unsigned char *keep = calloc(n, 1);
  keep[0]=keep[n-1]=1;
  int *st = malloc(sizeof(int)*2*n); int sp=0;
  st[sp++]=0; st[sp++]=n-1;
  while (sp>0) {
    int hi=st[--sp], lo=st[--sp];
    double maxd=0; int idx=-1;
    for (int i=lo+1;i<hi;i++){
      double d=perp_dist(c[i*2],c[i*2+1],c[lo*2],c[lo*2+1],c[hi*2],c[hi*2+1]);
      if (d>maxd){ maxd=d; idx=i; }
    }
    if (maxd>eps && idx!=-1){ keep[idx]=1;
      st[sp++]=lo; st[sp++]=idx; st[sp++]=idx; st[sp++]=hi; }
  }
  free(st);
  double *o=malloc(sizeof(double)*2*n); int w=0;
  for (int i=0;i<n;i++) if (keep[i]){ o[w*2]=c[i*2]; o[w*2+1]=c[i*2+1]; w++; }
  free(keep);
  *outn=w;
  return o;
}

static double *chaikin_smooth(const double *c, int n, int *outn) {
  if (n < 3) { double *r=malloc(sizeof(double)*2*n);
    memcpy(r,c,sizeof(double)*2*n); *outn=n; return r; }
  double *cur=malloc(sizeof(double)*2*n); memcpy(cur,c,sizeof(double)*2*n);
  int cn=n;
  for (int it=0; it<4; it++){ int m; double *nx=chaikin_once(cur,cn,&m);
    free(cur); cur=nx; cn=m; }
  int sn; double *s=simplify(cur,cn,1e-6,&sn); free(cur);
  *outn=sn; return s;
}

/* ── string->slot open-addressing map: degree + incident frag idx list ── */
typedef struct { char *k; int deg; int *inc; int ninc; int cinc; } eslot;
typedef struct { eslot *s; int cap; } emap;
static unsigned ehash(const char *s){ unsigned h=2166136261u;
  for(;*s;s++){h^=(unsigned char)*s;h*=16777619u;} return h; }
static eslot *eget(emap *m, const char *k, int create){
  unsigned i=ehash(k)&(m->cap-1);
  while (m->s[i].k){ if(!strcmp(m->s[i].k,k)) return &m->s[i]; i=(i+1)&(m->cap-1); }
  if(!create) return NULL;
  m->s[i].k=strdup(k); m->s[i].deg=0; m->s[i].inc=NULL; m->s[i].ninc=0; m->s[i].cinc=0;
  return &m->s[i];
}
static void einc(eslot *e,int idx){ if(e->ninc==e->cinc){ e->cinc=e->cinc?e->cinc*2:4;
  e->inc=realloc(e->inc,sizeof(int)*e->cinc);} e->inc[e->ninc++]=idx; }
static int other_inc(eslot *e,int excl){ if(!e) return -1;
  for(int i=0;i<e->ninc;i++) if(e->inc[i]!=excl) return e->inc[i]; return -1; }

static cJSON *mkfeat(const double *xy, int n, cJSON *props){
  cJSON *f=cJSON_CreateObject();
  cJSON_AddStringToObject(f,"type","Feature");
  cJSON *g=cJSON_CreateObject();
  cJSON_AddStringToObject(g,"type","LineString");
  cJSON *cc=cJSON_CreateArray();
  for(int i=0;i<n;i++){ cJSON *p=cJSON_CreateArray();
    cJSON_AddItemToArray(p,cJSON_CreateNumber(xy[i*2]));
    cJSON_AddItemToArray(p,cJSON_CreateNumber(xy[i*2+1]));
    cJSON_AddItemToArray(cc,p); }
  cJSON_AddItemToObject(g,"coordinates",cc);
  cJSON_AddItemToObject(f,"geometry",g);
  cJSON_AddItemToObject(f,"properties", props?cJSON_Duplicate(props,1):cJSON_CreateObject());
  return f;
}

cJSON *linegeom_stitch_smooth(cJSON *in, const char *mode) {
  cJSON *out = cJSON_CreateArray();
  if (!in || !cJSON_IsArray(in)) return out;

  /* bus: passthrough untouched (== JS `if (mode==='bus') return features`). */
  if (mode && !strcmp(mode, "bus")) {
    cJSON *f; cJSON_ArrayForEach(f, in) cJSON_AddItemToArray(out, cJSON_Duplicate(f,1));
    return out;
  }

  int nf = cJSON_GetArraySize(in), nfrag = 0;
  frag *fr = calloc(nf > 0 ? nf : 1, sizeof(frag));
  cJSON *f;
  cJSON_ArrayForEach(f, in) {
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    cJSON *t = g ? cJSON_GetObjectItem(g, "type") : NULL;
    cJSON *c = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
    int n = (c && cJSON_IsArray(c)) ? cJSON_GetArraySize(c) : 0;
    if (t && cJSON_IsString(t) && !strcmp(t->valuestring,"LineString") && n>=2) {
      fr[nfrag].xy = malloc(sizeof(double)*2*n);
      for (int i=0;i<n;i++){ cJSON *pt=cJSON_GetArrayItem(c,i);
        fr[nfrag].xy[i*2]   = cJSON_GetArrayItem(pt,0)->valuedouble;
        fr[nfrag].xy[i*2+1] = cJSON_GetArrayItem(pt,1)->valuedouble; }
      fr[nfrag].n = n;
      fr[nfrag].props = cJSON_GetObjectItem(f,"properties");
      nfrag++;
    } else {
      cJSON_AddItemToArray(out, cJSON_Duplicate(f,1));   /* passthrough */
    }
  }
  if (nfrag == 0) { free(fr); return out; }

  int cap=16; while (cap < nfrag*4) cap<<=1;
  emap m = { calloc(cap,sizeof(eslot)), cap };
  char **headK=malloc(sizeof(char*)*nfrag), **tailK=malloc(sizeof(char*)*nfrag);
  char kb[64];
  for (int i=0;i<nfrag;i++){
    epkey(fr[i].xy[0],fr[i].xy[1],kb,sizeof kb); headK[i]=strdup(kb);
    epkey(fr[i].xy[(fr[i].n-1)*2],fr[i].xy[(fr[i].n-1)*2+1],kb,sizeof kb);
    tailK[i]=strdup(kb);
    eget(&m,headK[i],1)->deg++; eget(&m,tailK[i],1)->deg++;
  }
  for (int i=0;i<nfrag;i++){ einc(eget(&m,headK[i],1),i); einc(eget(&m,tailK[i],1),i); }

  unsigned char *vis = calloc(nfrag,1);
  for (int start=0; start<nfrag; start++){
    if (vis[start]) continue;
    vis[start]=1;
    int cn=fr[start].n, ccap=cn>4?cn:4;
    double *co=malloc(sizeof(double)*2*ccap);
    memcpy(co,fr[start].xy,sizeof(double)*2*cn);
    cJSON *base=fr[start].props;
    /* extend forward (tail) */
    char *tailKey=strdup(tailK[start]); int tailFrom=start;
    for(;;){ eslot *e=eget(&m,tailKey,0);
      if(!e||e->deg!=2) break;
      int nx=other_inc(e,tailFrom); if(nx<0||vis[nx]) break;
      vis[nx]=1;
      int aligned = !strcmp(headK[nx],tailKey);
      const double *nc=fr[nx].xy; int nn=fr[nx].n;
      for(int j=1;j<nn;j++){ int src=aligned?j:(nn-1-j);
        if(cn==ccap){ccap*=2;co=realloc(co,sizeof(double)*2*ccap);}
        co[cn*2]=nc[src*2]; co[cn*2+1]=nc[src*2+1]; cn++; }
      char *nt=strdup(aligned?tailK[nx]:headK[nx]); free(tailKey); tailKey=nt;
      tailFrom=nx;
    }
    free(tailKey);
    /* extend backward (head): prepend prependCoords.slice(0,-1) */
    char *headKey=strdup(headK[start]); int headFrom=start;
    for(;;){ eslot *e=eget(&m,headKey,0);
      if(!e||e->deg!=2) break;
      int pv=other_inc(e,headFrom); if(pv<0||vis[pv]) break;
      vis[pv]=1;
      int prevTailIsHead = !strcmp(tailK[pv],headKey);
      const double *pc=fr[pv].xy; int pn=fr[pv].n;
      /* prependCoords = prevTailIsHead? pc : reverse(pc); drop its last */
      int addn=pn-1;
      double *nco=malloc(sizeof(double)*2*(addn+cn));
      for(int j=0;j<addn;j++){ int src=prevTailIsHead?j:(pn-1-j);
        nco[j*2]=pc[src*2]; nco[j*2+1]=pc[src*2+1]; }
      memcpy(nco+addn*2, co, sizeof(double)*2*cn);
      free(co); co=nco; cn+=addn; ccap=cn;
      char *nh=strdup(prevTailIsHead?headK[pv]:tailK[pv]); free(headKey); headKey=nh;
      headFrom=pv;
    }
    free(headKey);
    int sn; double *sm=chaikin_smooth(co,cn,&sn); free(co);
    cJSON_AddItemToArray(out, mkfeat(sm,sn,base));
    free(sm);
  }

  for (int i=0;i<m.cap;i++){ if(m.s[i].k){ free(m.s[i].k); free(m.s[i].inc); } }
  free(m.s);
  for (int i=0;i<nfrag;i++){ free(fr[i].xy); free(headK[i]); free(tailK[i]); }
  free(fr); free(headK); free(tailK); free(vis);
  return out;
}
