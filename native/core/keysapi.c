#include "keysapi.h"
#include "credtab.h"
#include "audit.h"
#include "../third_party/cJSON.h"
#include "../third_party/sqlite3.h"
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>

#ifndef JO_REPO_ROOT
#define JO_REPO_ROOT "/Users/rayan/JapanOSINT"
#endif

/* ── helpers ──────────────────────────────────────────────────────────── */
static char *jerr(int *st, int code, const char *msg) {
  *st = code;
  cJSON *o = cJSON_CreateObject(); cJSON_AddStringToObject(o,"error",msg);
  char *j = cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}
static char *jstr(cJSON *o, int *st, int code) {
  *st = code; char *j = cJSON_PrintUnformatted(o); cJSON_Delete(o); return j;
}
static int is_known(const char *name, const char **role) {
  const char *N[64], *R[64];
  int n = cred_known_vars(N, R, 64);
  for (int i = 0; i < n; i++)
    if (strcmp(N[i], name) == 0) { if (role) *role = R[i]; return 1; }
  return 0;
}
static int b64_decode(const char *in, unsigned char *out, int outcap) {
  static int rev[256]; static int init = 0;
  if (!init) { for (int i=0;i<256;i++) rev[i]=-1;
    const char *T="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i=0;i<64;i++) rev[(unsigned char)T[i]]=i; init=1; }
  int val=0,bits=-8,o=0;
  for (const char *p=in; *p; p++) {
    if (*p=='=' ) break;
    int c=rev[(unsigned char)*p]; if (c<0) continue;
    val=(val<<6)|c; bits+=6;
    if (bits>=0){ if(o<outcap) out[o++]=(unsigned char)((val>>bits)&0xFF); bits-=8; }
  }
  return o;
}

/* ── api-keys.json overlay (apiKeysStore.js) ──────────────────────────── */
static void overlay_path(char *p, size_t n) {
  snprintf(p, n, "%s/data/api-keys.json", JO_REPO_ROOT);
}
static cJSON *overlay_read(void) {
  char p[1024]; overlay_path(p, sizeof p);
  FILE *f = fopen(p, "rb"); if (!f) return cJSON_CreateObject();
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  cJSON *j = NULL;
  if (n > 0 && n < (1<<20)) {
    char *b = malloc(n+1);
    if (fread(b,1,n,f)==(size_t)n) { b[n]=0; j=cJSON_Parse(b); }
    free(b);
  }
  fclose(f);
  if (!j || !cJSON_IsObject(j)) { cJSON_Delete(j); j=cJSON_CreateObject(); }
  return j;
}
static void overlay_write(cJSON *o) {
  char p[1024], tmp[1056]; overlay_path(p, sizeof p);
  snprintf(tmp, sizeof tmp, "%s.tmp", p);
  char *txt = cJSON_Print(o);            /* pretty, like JSON.stringify(,,2) */
  FILE *f = fopen(tmp, "wb");
  if (f) { fwrite(txt, 1, strlen(txt), f); fclose(f); rename(tmp, p); }
  free(txt);
}
/* resolved value: overlay non-empty string wins, else process env. */
static const char *resolved_env(cJSON *ov, const char *name) {
  cJSON *v = cJSON_GetObjectItem(ov, name);
  if (v && cJSON_IsString(v) && v->valuestring[0]) return v->valuestring;
  const char *e = getenv(name);
  return (e && *e) ? e : NULL;
}
static int env_set_trim(cJSON *ov, const char *name) {
  const char *v = resolved_env(ov, name);
  if (!v) return 0;
  while (*v && isspace((unsigned char)*v)) v++;
  return *v != 0;
}

/* ── per-tenant secret crypto (credentials.js) ────────────────────────── */
static int master_key(unsigned char out[32]) {
  const char *raw = getenv("SECRETS_MASTER_KEY");
  if (!raw || !*raw) return 0;
  size_t L = strlen(raw);
  int hex = (L == 64);
  for (size_t i = 0; hex && i < L; i++) if (!isxdigit((unsigned char)raw[i])) hex = 0;
  if (hex) {
    for (int i = 0; i < 32; i++) { unsigned x; sscanf(raw+2*i,"%2x",&x); out[i]=(unsigned char)x; }
    return 1;
  }
  unsigned char buf[256];
  int n = b64_decode(raw, buf, sizeof buf);
  if (n < 32) return 0;
  memcpy(out, buf, 32);
  return 1;
}
static int derive_key(const char *tenant_id, unsigned char out[32]) {
  unsigned char mk[32];
  if (!master_key(mk)) return 0;
  EVP_PKEY_CTX *c = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
  if (!c) return 0;
  size_t outlen = 32; int ok = 0;
  if (EVP_PKEY_derive_init(c) == 1 &&
      EVP_PKEY_CTX_set_hkdf_md(c, EVP_sha256()) == 1 &&
      EVP_PKEY_CTX_set1_hkdf_key(c, mk, 32) == 1 &&
      EVP_PKEY_CTX_set1_hkdf_salt(c, (const unsigned char *)tenant_id,
                                  (int)strlen(tenant_id)) == 1 &&
      EVP_PKEY_CTX_add1_hkdf_info(c,
        (const unsigned char *)"JapanOSINT.tenant_secrets.v1", 28) == 1 &&
      EVP_PKEY_derive(c, out, &outlen) == 1 && outlen == 32)
    ok = 1;
  EVP_PKEY_CTX_free(c);
  return ok;
}
/* blob = [12 nonce][16 tag][ct]. Returns malloc'd blob, sets *blen. */
static unsigned char *secret_encrypt(const char *tid, const char *pt, int *blen) {
  unsigned char key[32]; if (!derive_key(tid, key)) return NULL;
  unsigned char nonce[12]; RAND_bytes(nonce, 12);
  int ptl = (int)strlen(pt);
  unsigned char *blob = malloc(12 + 16 + ptl + 16);
  memcpy(blob, nonce, 12);
  EVP_CIPHER_CTX *x = EVP_CIPHER_CTX_new();
  int len, ok = 0;
  if (EVP_EncryptInit_ex(x, EVP_aes_256_gcm(), NULL, key, nonce) == 1) {
    int ctl = 0;
    if (EVP_EncryptUpdate(x, blob+28, &len, (const unsigned char *)pt, ptl)==1) {
      ctl = len;
      if (EVP_EncryptFinal_ex(x, blob+28+ctl, &len) == 1) {
        ctl += len;
        if (EVP_CIPHER_CTX_ctrl(x, EVP_CTRL_GCM_GET_TAG, 16, blob+12) == 1) {
          *blen = 28 + ctl; ok = 1;
        }
      }
    }
  }
  EVP_CIPHER_CTX_free(x);
  if (!ok) { free(blob); return NULL; }
  return blob;
}
static char *secret_decrypt(const char *tid, const unsigned char *blob, int bl) {
  if (bl < 28) return NULL;
  unsigned char key[32]; if (!derive_key(tid, key)) return NULL;
  int ctl = bl - 28;
  char *pt = malloc(ctl + 1);
  EVP_CIPHER_CTX *x = EVP_CIPHER_CTX_new();
  int len, ok = 0;
  if (EVP_DecryptInit_ex(x, EVP_aes_256_gcm(), NULL, key, blob) == 1) {
    int pl = 0;
    if (EVP_DecryptUpdate(x, (unsigned char *)pt, &len, blob+28, ctl) == 1) {
      pl = len;
      EVP_CIPHER_CTX_ctrl(x, EVP_CTRL_GCM_SET_TAG, 16, (void *)(blob+12));
      if (EVP_DecryptFinal_ex(x, (unsigned char *)pt+pl, &len) == 1) {
        pl += len; pt[pl] = 0; ok = 1;
      }
    }
  }
  EVP_CIPHER_CTX_free(x);
  if (!ok) { free(pt); return NULL; }
  return pt;
}

/* tenants.key_write_policy / key_write_member_id */
static void key_policy(db_handle *db, const char *tid, char pol[32], char mid[64]) {
  snprintf(pol, 32, "owner_only"); mid[0] = 0;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "SELECT key_write_policy,key_write_member_id FROM tenants WHERE id=?1",
        -1,&s,NULL)==SQLITE_OK) {
    sqlite3_bind_text(s,1,tid,-1,SQLITE_TRANSIENT);
    if (sqlite3_step(s)==SQLITE_ROW) {
      if (sqlite3_column_type(s,0)!=SQLITE_NULL)
        snprintf(pol,32,"%s",(const char*)sqlite3_column_text(s,0));
      if (sqlite3_column_type(s,1)!=SQLITE_NULL)
        snprintf(mid,64,"%s",(const char*)sqlite3_column_text(s,1));
    }
  }
  sqlite3_finalize(s);
}
static int is_admin_role(const char *r) {
  return r && (strcmp(r,"owner")==0 || strcmp(r,"admin")==0);
}
static int can_manage(db_handle *db, const tenant_ctx *t) {
  if (is_admin_role(t->role)) return 1;
  char pol[32], mid[64]; key_policy(db, t->tenant_id, pol, mid);
  if (strcmp(pol,"all_members")==0) return 1;
  if (strcmp(pol,"selected_member")==0) return strcmp(t->user_id, mid)==0;
  return 0;
}

/* ── /api/keys (requirePlatformAdmin) ─────────────────────────────────── */
char *keysapi_platform(db_handle *db, const tenant_ctx *t, const char *method,
                       const char *name, const char *body, int *st) {
  (void)db;
  if (!is_admin_role(t->role)) return jerr(st,403,"Admin role required");
  cJSON *ov = overlay_read();

  if (!name[0]) {
    if (strcmp(method,"GET")!=0) { cJSON_Delete(ov); return jerr(st,404,"not_found"); }
    const char *N[64],*R[64]; int n=cred_known_vars(N,R,64);
    cJSON *arr=cJSON_CreateArray();
    for (int i=0;i<n;i++){
      cJSON *m=cJSON_CreateObject();
      cJSON_AddStringToObject(m,"name",N[i]);
      cJSON_AddStringToObject(m,"role",R[i]);
      cJSON_AddBoolToObject(m,"set",env_set_trim(ov,N[i])?1:0);
      cJSON_AddBoolToObject(m,"hasOverlay",cJSON_HasObjectItem(ov,N[i])?1:0);
      cJSON_AddItemToArray(arr,m);
    }
    cJSON_Delete(ov); return jstr(arr,st,200);
  }

  const char *role=NULL; int known=is_known(name,&role);
  if (strcmp(method,"GET")==0) {
    if (!known) { cJSON_Delete(ov); return jerr(st,404,"Unknown key"); }
    const char *v=resolved_env(ov,name);
    cJSON *o=cJSON_CreateObject();
    cJSON_AddStringToObject(o,"name",name);
    cJSON_AddItemToObject(o,"value", (v&&*v)?cJSON_CreateString(v):cJSON_CreateNull());
    cJSON_Delete(ov); return jstr(o,st,200);
  }
  if (strcmp(method,"PUT")==0) {
    if (!known) { cJSON_Delete(ov); return jerr(st,400,"Unknown key"); }
    cJSON *jb = (body&&*body)?cJSON_Parse(body):NULL;
    cJSON *jv = jb?cJSON_GetObjectItem(jb,"value"):NULL;
    const char *val = (jv&&cJSON_IsString(jv))?jv->valuestring:"";
    cJSON_DeleteItemFromObject(ov,name);          /* clear */
    if (val[0]) cJSON_AddStringToObject(ov,name,val);  /* set when non-empty */
    overlay_write(ov);
    if (jb) cJSON_Delete(jb);
    cJSON *o=cJSON_CreateObject();
    cJSON_AddStringToObject(o,"name",name);
    cJSON_AddStringToObject(o,"role",role);
    cJSON_AddBoolToObject(o,"set",env_set_trim(ov,name)?1:0);
    cJSON_AddBoolToObject(o,"hasOverlay",cJSON_HasObjectItem(ov,name)?1:0);
    cJSON_Delete(ov); return jstr(o,st,200);
  }
  cJSON_Delete(ov); return jerr(st,404,"not_found");
}

/* ── /api/tenant-keys ─────────────────────────────────────────────────── */
static int tenant_has_secret(db_handle *db,const char*tid,const char*name){
  sqlite3_stmt *s; int has=0;
  if (sqlite3_prepare_v2(db->h,
    "SELECT 1 FROM tenant_secrets WHERE tenant_id=?1 AND key_name=?2",
    -1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_text(s,1,tid,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,name,-1,SQLITE_TRANSIENT);
    has=sqlite3_step(s)==SQLITE_ROW;
  }
  sqlite3_finalize(s); return has;
}

char *keysapi_tenant(db_handle *db, const tenant_ctx *t, const char *method,
                     const char *seg, const char *body, int *st) {
  cJSON *ov = overlay_read();

  if (!seg[0]) {                                   /* GET / */
    if (strcmp(method,"GET")!=0){cJSON_Delete(ov);return jerr(st,404,"not_found");}
    const char *N[64],*R[64]; int n=cred_known_vars(N,R,64);
    cJSON *items=cJSON_CreateArray();
    for (int i=0;i<n;i++){
      int byok=tenant_has_secret(db,t->tenant_id,N[i]);
      const char *src=NULL;
      if (byok) src="tenant";
      else if (env_set_trim(ov,N[i])) src="platform";
      cJSON *m=cJSON_CreateObject();
      cJSON_AddStringToObject(m,"name",N[i]);
      cJSON_AddStringToObject(m,"role",R[i]);
      cJSON_AddBoolToObject(m,"byok",byok);
      cJSON_AddBoolToObject(m,"set",src!=NULL);
      cJSON_AddItemToObject(m,"source",src?cJSON_CreateString(src):cJSON_CreateNull());
      cJSON_AddItemToArray(items,m);
    }
    char pol[32],mid[64]; key_policy(db,t->tenant_id,pol,mid);
    cJSON *o=cJSON_CreateObject();
    cJSON_AddItemToObject(o,"items",items);
    cJSON_AddBoolToObject(o,"canManage",can_manage(db,t)?1:0);
    cJSON_AddStringToObject(o,"policy",pol);
    cJSON_AddItemToObject(o,"policyMemberId",mid[0]?cJSON_CreateString(mid):cJSON_CreateNull());
    cJSON_Delete(ov); return jstr(o,st,200);
  }

  if (strcmp(seg,"policy")==0) {
    cJSON_Delete(ov);
    if (strcmp(method,"GET")==0) {
      char pol[32],mid[64]; key_policy(db,t->tenant_id,pol,mid);
      cJSON *mem=cJSON_CreateArray();
      sqlite3_stmt *s;
      if (sqlite3_prepare_v2(db->h,
        "SELECT u.id,u.email,m.role FROM memberships m JOIN users u "
        "ON u.id=m.user_id WHERE m.tenant_id=?1 ORDER BY m.created_at ASC",
        -1,&s,NULL)==SQLITE_OK){
        sqlite3_bind_text(s,1,t->tenant_id,-1,SQLITE_TRANSIENT);
        while(sqlite3_step(s)==SQLITE_ROW){
          cJSON *r=cJSON_CreateObject();
          cJSON_AddStringToObject(r,"id",(const char*)sqlite3_column_text(s,0));
          cJSON_AddStringToObject(r,"email",(const char*)sqlite3_column_text(s,1));
          cJSON_AddStringToObject(r,"role",(const char*)sqlite3_column_text(s,2));
          cJSON_AddItemToArray(mem,r);
        }
      }
      sqlite3_finalize(s);
      cJSON *o=cJSON_CreateObject();
      cJSON_AddStringToObject(o,"policy",pol);
      cJSON_AddItemToObject(o,"memberId",mid[0]?cJSON_CreateString(mid):cJSON_CreateNull());
      cJSON_AddItemToObject(o,"members",mem);
      return jstr(o,st,200);
    }
    if (strcmp(method,"PUT")==0) {
      if (strcmp(t->role,"owner")!=0)
        return jerr(st,403,"Only the workspace owner can change this policy");
      cJSON *jb=(body&&*body)?cJSON_Parse(body):NULL;
      cJSON *jp=jb?cJSON_GetObjectItem(jb,"policy"):NULL;
      const char *pol=(jp&&cJSON_IsString(jp))?jp->valuestring:"";
      if (strcmp(pol,"owner_only")&&strcmp(pol,"selected_member")&&strcmp(pol,"all_members")){
        if(jb)cJSON_Delete(jb); return jerr(st,400,"Invalid policy");
      }
      char mid[64]={0};
      if (strcmp(pol,"selected_member")==0){
        cJSON *jm=jb?cJSON_GetObjectItem(jb,"memberId"):NULL;
        const char *m=(jm&&cJSON_IsString(jm))?jm->valuestring:NULL;
        int ok=0;
        if (m&&*m){
          sqlite3_stmt *s;
          if (sqlite3_prepare_v2(db->h,
            "SELECT 1 FROM memberships WHERE tenant_id=?1 AND user_id=?2",
            -1,&s,NULL)==SQLITE_OK){
            sqlite3_bind_text(s,1,t->tenant_id,-1,SQLITE_TRANSIENT);
            sqlite3_bind_text(s,2,m,-1,SQLITE_TRANSIENT);
            ok=sqlite3_step(s)==SQLITE_ROW;
          }
          sqlite3_finalize(s);
        }
        if (!ok){ if(jb)cJSON_Delete(jb);
          return jerr(st,400,"memberId must be a member of this workspace"); }
        snprintf(mid,sizeof mid,"%s",m);
      }
      sqlite3_stmt *s;
      sqlite3_prepare_v2(db->h,
        "UPDATE tenants SET key_write_policy=?1,key_write_member_id=?2 WHERE id=?3",
        -1,&s,NULL);
      sqlite3_bind_text(s,1,pol,-1,SQLITE_TRANSIENT);
      if (mid[0]) sqlite3_bind_text(s,2,mid,-1,SQLITE_TRANSIENT);
      else sqlite3_bind_null(s,2);
      sqlite3_bind_text(s,3,t->tenant_id,-1,SQLITE_TRANSIENT);
      sqlite3_step(s); sqlite3_finalize(s);
      /* Who may touch this workspace's credentials is itself a security
       * decision, so the change is on the record. */
      { cJSON *ap=cJSON_CreateObject();
        cJSON_AddStringToObject(ap,"policy",pol);
        if (mid[0]) cJSON_AddStringToObject(ap,"memberId",mid);
        char *apj=cJSON_PrintUnformatted(ap); cJSON_Delete(ap);
        audit_write(db,t->tenant_id,t->user_id,"tenant_key.policy",NULL,apj);
        free(apj); }
      cJSON *o=cJSON_CreateObject();
      cJSON_AddStringToObject(o,"policy",pol);
      cJSON_AddItemToObject(o,"memberId",mid[0]?cJSON_CreateString(mid):cJSON_CreateNull());
      if(jb)cJSON_Delete(jb);
      return jstr(o,st,200);
    }
    return jerr(st,404,"not_found");
  }

  /* /:name */
  const char *role=NULL; int known=is_known(seg,&role);
  if (strcmp(method,"GET")==0) {
    cJSON_Delete(ov);
    if (!known) return jerr(st,404,"Unknown key");
    /* Plaintext reveal is NOT the same permission as "may manage keys".
     * can_manage() is true for EVERY member under the all_members policy, so
     * this route handed a viewer every third-party credential the workspace
     * stores — the write policy was being reused as a read policy. Reveal now
     * needs owner/admin (the collection listing above still shows every member
     * which keys are set, which is what the settings screen actually needs),
     * and httpd.c additionally puts the platform-operator gate in front of
     * this exact route, mirroring the breach-reveal path. */
    if (!is_admin_role(t->role))
      return jerr(st,403,"Only a workspace owner or admin can reveal a key");
    char *val=NULL;
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->h,
      "SELECT encrypted_value FROM tenant_secrets WHERE tenant_id=?1 AND key_name=?2",
      -1,&s,NULL)==SQLITE_OK){
      sqlite3_bind_text(s,1,t->tenant_id,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,2,seg,-1,SQLITE_TRANSIENT);
      if (sqlite3_step(s)==SQLITE_ROW && sqlite3_column_type(s,0)==SQLITE_BLOB){
        const unsigned char *bl=sqlite3_column_blob(s,0);
        int n=sqlite3_column_bytes(s,0);
        val=secret_decrypt(t->tenant_id,bl,n);
      }
    }
    sqlite3_finalize(s);
    /* Audited whether or not a value came back: an attempt to read a secret is
     * the interesting event, and this file previously wrote no audit rows at
     * all. The key NAME is recorded; the value obviously is not. */
    { cJSON *ap=cJSON_CreateObject();
      cJSON_AddBoolToObject(ap,"revealed",val?1:0);
      char *apj=cJSON_PrintUnformatted(ap); cJSON_Delete(ap);
      audit_write(db,t->tenant_id,t->user_id,"tenant_key.reveal",seg,apj);
      free(apj); }
    cJSON *o=cJSON_CreateObject();
    cJSON_AddStringToObject(o,"name",seg);
    cJSON_AddItemToObject(o,"value",val?cJSON_CreateString(val):cJSON_CreateNull());
    free(val);
    return jstr(o,st,200);
  }
  if (strcmp(method,"PUT")==0) {
    if (!known){cJSON_Delete(ov);return jerr(st,400,"Unknown key");}
    if (!can_manage(db,t)){cJSON_Delete(ov);
      return jerr(st,403,"Not permitted to edit this workspace's keys");}
    cJSON *jb=(body&&*body)?cJSON_Parse(body):NULL;
    cJSON *jv=jb?cJSON_GetObjectItem(jb,"value"):NULL;
    const char *val=(jv&&cJSON_IsString(jv))?jv->valuestring:"";
    sqlite3_stmt *s;
    if (val[0]==0) {
      sqlite3_prepare_v2(db->h,
        "DELETE FROM tenant_secrets WHERE tenant_id=?1 AND key_name=?2",-1,&s,NULL);
      sqlite3_bind_text(s,1,t->tenant_id,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,2,seg,-1,SQLITE_TRANSIENT);
      sqlite3_step(s); sqlite3_finalize(s);
    } else {
      int bl=0; unsigned char *blob=secret_encrypt(t->tenant_id,val,&bl);
      if (!blob){ if(jb)cJSON_Delete(jb); cJSON_Delete(ov);
        return jerr(st,500,"Failed to write key"); }
      sqlite3_prepare_v2(db->h,
        "INSERT INTO tenant_secrets (tenant_id,key_name,encrypted_value,"
        "fallback_to_platform,created_by) VALUES (?1,?2,?3,1,?4) "
        "ON CONFLICT(tenant_id,key_name) DO UPDATE SET "
        "encrypted_value=excluded.encrypted_value,"
        "fallback_to_platform=excluded.fallback_to_platform,last_used_at=NULL",
        -1,&s,NULL);
      sqlite3_bind_text(s,1,t->tenant_id,-1,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,2,seg,-1,SQLITE_TRANSIENT);
      sqlite3_bind_blob(s,3,blob,bl,SQLITE_TRANSIENT);
      sqlite3_bind_text(s,4,t->user_id,-1,SQLITE_TRANSIENT);
      sqlite3_step(s); sqlite3_finalize(s);
      free(blob);
    }
    int byok = val[0]!=0;
    int platform = env_set_trim(ov,seg);
    audit_write(db,t->tenant_id,t->user_id,
                byok ? "tenant_key.set" : "tenant_key.clear", seg, NULL);
    cJSON *o=cJSON_CreateObject();
    cJSON_AddStringToObject(o,"name",seg);
    cJSON_AddBoolToObject(o,"byok",byok);
    cJSON_AddBoolToObject(o,"set",byok||platform);
    cJSON_AddItemToObject(o,"source",
      byok?cJSON_CreateString("tenant"):platform?cJSON_CreateString("platform"):cJSON_CreateNull());
    if(jb)cJSON_Delete(jb); cJSON_Delete(ov);
    return jstr(o,st,200);
  }
  cJSON_Delete(ov); return jerr(st,404,"not_found");
}

/* ── /admin/break-glass/login (breakGlass.js) ─────────────────────────── */
static int base32_decode(const char *s, unsigned char *out, int cap) {
  int val=0,bits=0,o=0;
  for (const char *p=s; *p; p++) {
    if (*p=='='||isspace((unsigned char)*p)) continue;
    char ch=*p; if (ch>='a'&&ch<='z') ch-=32;
    int idx;
    if (ch>='A'&&ch<='Z') idx=ch-'A';
    else if (ch>='2'&&ch<='7') idx=26+(ch-'2');
    else return -1;
    val=(val<<5)|idx; bits+=5;
    if (bits>=8){ bits-=8; if(o<cap) out[o++]=(unsigned char)((val>>bits)&0xFF); }
  }
  return o;
}
static void totp_at(const unsigned char *key,int klen,long step,char out[7]){
  unsigned char buf[8];
  for (int i=7;i>=0;i--){ buf[i]=(unsigned char)(step&0xFF); step>>=8; }
  unsigned char mac[20]; unsigned int ml=20;
  HMAC(EVP_sha1(),key,klen,buf,8,mac,&ml);
  int off=mac[ml-1]&0x0F;
  unsigned bin=((mac[off]&0x7F)<<24)|((mac[off+1]&0xFF)<<16)|
               ((mac[off+2]&0xFF)<<8)|(mac[off+3]&0xFF);
  snprintf(out,7,"%06u",bin%1000000u);
}
static char *b64url(const unsigned char *in,int len){
  static const char T[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  char *o=malloc(((len+2)/3)*4+1); int p=0;
  for (int i=0;i<len;i+=3){
    unsigned a=in[i],b=i+1<len?in[i+1]:0,c=i+2<len?in[i+2]:0,v=(a<<16)|(b<<8)|c;
    o[p++]=T[(v>>18)&63]; o[p++]=T[(v>>12)&63];
    if(i+1<len)o[p++]=T[(v>>6)&63]; if(i+2<len)o[p++]=T[v&63];
  }
  o[p]=0; return o;
}

char *keysapi_breakglass(db_handle *db, const char *body,
                         const char *ip, const char *ua, int *st) {
  const char *enabled=getenv("BREAK_GLASS_ENABLED");
  if (!enabled || strcmp(enabled,"1")!=0) return jerr(st,404,"Not found");
  const char *totp=getenv("ADMIN_TOTP_SECRET");
  const char *jwt=getenv("BREAK_GLASS_JWT_SECRET");
  const char *sup=getenv("SUPABASE_JWT_SECRET");
  if (!totp||!*totp||!jwt||!*jwt) return jerr(st,503,"Break-glass not configured");
  if (sup && *sup && strcmp(jwt,sup)==0)            /* secret-reuse guard */
    return jerr(st,503,"Break-glass not configured");

  cJSON *jb=(body&&*body)?cJSON_Parse(body):NULL;
  cJSON *jc=jb?cJSON_GetObjectItem(jb,"code"):NULL;
  char code[32]={0}; int ci=0;
  if (jc&&cJSON_IsString(jc))
    for (const char *p=jc->valuestring; *p && ci<31; p++)
      if (!isspace((unsigned char)*p)) code[ci++]=*p;
  int six = strlen(code)==6;
  for (int i=0;six&&i<6;i++) if (!isdigit((unsigned char)code[i])) six=0;
  if (!six){ if(jb)cJSON_Delete(jb); return jerr(st,400,"Six-digit TOTP code required"); }

  unsigned char key[128];
  int klen=base32_decode(totp,key,sizeof key);
  long now=(long)time(NULL), step=now/30;
  int valid=0;
  for (int d=-1; d<=1 && !valid; d++){ char e[7]; totp_at(key,klen,step+d,e);
    if (strcmp(e,code)==0) valid=1; }

  /* audit (raw platform-scope insert; matches breakGlass.insertAudit) */
  char uid[37]; { unsigned char b[16]; RAND_bytes(b,16);
    b[6]=(b[6]&0x0F)|0x40; b[8]=(b[8]&0x3F)|0x80;
    snprintf(uid,37,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]); }
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
    "INSERT INTO audit_events (id,tenant_id,user_id,action,target,payload_json,"
    "ts,ip,ua) VALUES (?1,'platform',NULL,?2,NULL,'{}',datetime('now'),?3,?4)",
    -1,&s,NULL)==SQLITE_OK){
    sqlite3_bind_text(s,1,uid,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2, valid?"break_glass.login.ok":"break_glass.login.denied",
                      -1,SQLITE_TRANSIENT);
    if (ip) sqlite3_bind_text(s,3,ip,-1,SQLITE_TRANSIENT); else sqlite3_bind_null(s,3);
    if (ua) sqlite3_bind_text(s,4,ua,-1,SQLITE_TRANSIENT); else sqlite3_bind_null(s,4);
    sqlite3_step(s);
  }
  sqlite3_finalize(s);

  if (!valid){ if(jb)cJSON_Delete(jb); return jerr(st,401,"Invalid TOTP"); }

  /* HS256 JWT, 1h, platform admin scope */
  cJSON *hd=cJSON_CreateObject();
  cJSON_AddStringToObject(hd,"alg","HS256"); cJSON_AddStringToObject(hd,"typ","JWT");
  char *hj=cJSON_PrintUnformatted(hd); cJSON_Delete(hd);
  cJSON *pl=cJSON_CreateObject();
  cJSON_AddStringToObject(pl,"sub","break-glass-admin");
  cJSON_AddStringToObject(pl,"email","break-glass@local");
  cJSON_AddStringToObject(pl,"role","service_role");
  cJSON_AddStringToObject(pl,"tenant_id","platform");
  cJSON_AddBoolToObject(pl,"break_glass",1);
  cJSON_AddNumberToObject(pl,"iat",(double)now);
  cJSON_AddNumberToObject(pl,"exp",(double)(now+3600));
  char *pj=cJSON_PrintUnformatted(pl); cJSON_Delete(pl);
  char *h64=b64url((unsigned char*)hj,strlen(hj));
  char *p64=b64url((unsigned char*)pj,strlen(pj));
  char si[4096]; snprintf(si,sizeof si,"%s.%s",h64,p64);
  unsigned char mac[32]; unsigned int ml=32;
  HMAC(EVP_sha256(),jwt,(int)strlen(jwt),(unsigned char*)si,strlen(si),mac,&ml);
  char *s64=b64url(mac,32);
  char tok[4608]; snprintf(tok,sizeof tok,"%s.%s.%s",h64,p64,s64);
  free(hj);free(pj);free(h64);free(p64);free(s64);
  if(jb)cJSON_Delete(jb);
  cJSON *o=cJSON_CreateObject();
  cJSON_AddStringToObject(o,"token",tok);
  cJSON_AddNumberToObject(o,"expires_in",3600);
  return jstr(o,st,200);
}
