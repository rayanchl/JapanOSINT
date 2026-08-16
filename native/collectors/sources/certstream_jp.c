/* collectors/cyber/sources/certstream_jp.c
 * Port of server/src/collectors/certstreamJp.js + utils/certstreamBuffer.js.
 * Node keeps a long-lived ring buffer fed by a persistent wss subscriber;
 * the run-once C model instead opens wss://certstream.calidog.io/ for a
 * bounded window (env CERTSTREAM_WINDOW_MS, default 10000) collecting up to
 * 500 .jp-domain certificate_update events, then emits each as an intel row
 * (uid = certstream-jp|<seen>_<i>_<cn> == intelUid; sink derives it from
 * remote_key). intelEnvelope/_meta dropped per RULE 8. Exact-uid parity with
 * Node is impossible (its buffer is long-lived process state) — shape /
 * fields / filters are faithful; documented post-parity. */
#include "source.h"
#include "lib/ws.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/time.h>

#define WS_URL "wss://certstream.calidog.io/"
#define MAX_EVENTS 500

typedef struct { cJSON *events; } cs_ud;

static void now_iso(char *o, size_t n) {        /* new Date().toISOString() */
  struct timeval tv; gettimeofday(&tv, NULL);
  time_t t = tv.tv_sec; struct tm tm; gmtime_r(&t, &tm);
  char base[32]; strftime(base, sizeof base, "%Y-%m-%dT%H:%M:%S", &tm);
  snprintf(o, n, "%s.%03dZ", base, (int)(tv.tv_usec / 1000));
}

static const char *so(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static int on_msg(const char *data, size_t len, void *udp) {
  (void)len;
  cs_ud *u = (cs_ud *)udp;
  cJSON *msg = cJSON_Parse(data);
  if (!msg) return 0;
  const char *mt = so(msg, "message_type");
  cJSON *d = cJSON_GetObjectItem(msg, "data");
  cJSON *leaf = d ? cJSON_GetObjectItem(d, "leaf_cert") : NULL;
  if (!mt || strcmp(mt, "certificate_update") || !leaf) { cJSON_Delete(msg); return 0; }

  cJSON *all = cJSON_GetObjectItem(leaf, "all_domains");
  cJSON *jp = cJSON_CreateArray();
  if (cJSON_IsArray(all)) {
    cJSON *e;
    cJSON_ArrayForEach(e, all) {
      if (!cJSON_IsString(e)) continue;
      const char *s = e->valuestring; size_t L = strlen(s);
      if (L >= 3 && strcasecmp(s + L - 3, ".jp") == 0)
        cJSON_AddItemToArray(jp, cJSON_CreateString(s));
    }
  }
  if (cJSON_GetArraySize(jp) == 0) { cJSON_Delete(jp); cJSON_Delete(msg); return 0; }

  cJSON *subj = cJSON_GetObjectItem(leaf, "subject");
  cJSON *iss  = cJSON_GetObjectItem(leaf, "issuer");
  cJSON *src  = d ? cJSON_GetObjectItem(d, "source") : NULL;
  char iso[40]; now_iso(iso, sizeof iso);

  cJSON *ev = cJSON_CreateObject();
  cJSON_AddStringToObject(ev, "ts", iso);                 /* capture time */
  cJSON *seen = d ? cJSON_GetObjectItem(d, "seen") : NULL;
  cJSON_AddItemToObject(ev, "seen", seen ? cJSON_Duplicate(seen, 1) : cJSON_CreateNull());
  const char *cn = so(subj, "CN");
  cJSON_AddItemToObject(ev, "cn", cn ? cJSON_CreateString(cn) : cJSON_CreateNull());
  const char *issuer = so(iss, "O");
  cJSON_AddItemToObject(ev, "issuer", issuer ? cJSON_CreateString(issuer) : cJSON_CreateNull());
  cJSON_AddItemToObject(ev, "jp_domains", jp);            /* takes ownership */
  cJSON_AddItemToObject(ev, "all_domains",
    cJSON_IsArray(all) ? cJSON_Duplicate(all, 1) : cJSON_CreateArray());
  cJSON *nb = cJSON_GetObjectItem(leaf, "not_before");
  cJSON *na = cJSON_GetObjectItem(leaf, "not_after");
  cJSON_AddItemToObject(ev, "not_before", nb ? cJSON_Duplicate(nb,1) : cJSON_CreateNull());
  cJSON_AddItemToObject(ev, "not_after",  na ? cJSON_Duplicate(na,1) : cJSON_CreateNull());
  const char *cts = so(src, "name");
  cJSON_AddItemToObject(ev, "ct_source", cts ? cJSON_CreateString(cts) : cJSON_CreateNull());
  const char *cl = so(d, "cert_link");
  cJSON_AddItemToObject(ev, "cert_link", cl ? cJSON_CreateString(cl) : cJSON_CreateNull());

  cJSON_AddItemToArray(u->events, ev);
  cJSON_Delete(msg);
  return cJSON_GetArraySize(u->events) >= MAX_EVENTS;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx;
  const char *we = getenv("CERTSTREAM_WINDOW_MS");
  int window = we ? atoi(we) : 10000;
  if (window <= 0) window = 10000;
  const char *hdrs[] = { "User-Agent: JapanOSINT/1.0", NULL };

  cs_ud u = { cJSON_CreateArray() };
  ws_collect(WS_URL, hdrs, window, MAX_EVENTS, on_msg, &u);

  int n = 0, i = 0;
  cJSON *ev;
  cJSON_ArrayForEach(ev, u.events) {
    const char *cn   = so(ev, "cn");
    const char *iss  = so(ev, "issuer");
    cJSON *jp = cJSON_GetObjectItem(ev, "jp_domains");
    cJSON *all = cJSON_GetObjectItem(ev, "all_domains");
    int njp = cJSON_GetArraySize(jp);
    const char *seen = so(ev, "seen");
    cJSON *seenN = cJSON_GetObjectItem(ev, "seen");
    char seenbuf[32] = "";
    if (!seen && cJSON_IsNumber(seenN)) snprintf(seenbuf, sizeof seenbuf, "%g", seenN->valuedouble);
    const char *seenS = seen ? seen : seenbuf;
    const char *iso = so(ev, "ts");

    char rkey[320];
    snprintf(rkey, sizeof rkey, "%s_%d_%s", seenS, i, cn ? cn : "null");

    char summary[64];
    snprintf(summary, sizeof summary, "Cert for %d .jp domain%s",
             njp, njp != 1 ? "s" : "");

    /* body = `Issuer: ${ev.issuer}\nDomains: ${jp.join(', ')}` (JS ${null}
     * → "null").
     *
     * The budget used to be a flat 64 bytes plus the domain list, which paid
     * for the literal prefix but NOT for the issuer it interpolates. The
     * prefix costs 18 + strlen(iss), and `iss` is an upstream-controlled
     * leaf_cert.issuer.O — an ordinary DigiCert DN is ~78 bytes, so bo ran past
     * blen, `body + bo` pointed outside the allocation and `blen - bo` wrapped
     * to ~SIZE_MAX. That is a heap overflow on routine CT traffic, every 60 s.
     * Budget the issuer too, check the malloc, and clamp the accumulator. */
    const char *issS = iss ? iss : "null";
    size_t blen = 64 + strlen(issS);
    cJSON *jd; cJSON_ArrayForEach(jd, jp) blen += strlen(jd->valuestring) + 2;
    char *body = malloc(blen);
    if (!body) { i++; continue; }
    int bo = snprintf(body, blen, "Issuer: %s\nDomains: ", issS);
    if (bo < 0 || (size_t)bo >= blen) bo = (int)blen - 1;
    int first = 1;
    cJSON_ArrayForEach(jd, jp) {
      int w = snprintf(body + bo, blen - (size_t)bo, "%s%s",
                       first ? "" : ", ", jd->valuestring);
      if (w < 0) break;
      if ((size_t)w >= blen - (size_t)bo) { bo = (int)blen - 1; break; }
      bo += w;
      first = 0;
    }

    cJSON *props = cJSON_CreateObject();          /* EXACT JS key order */
    cJSON_AddItemToObject(props, "cn", cn ? cJSON_CreateString(cn) : cJSON_CreateNull());
    cJSON_AddItemToObject(props, "issuer", iss ? cJSON_CreateString(iss) : cJSON_CreateNull());
    cJSON_AddItemToObject(props, "jp_domains", cJSON_Duplicate(jp, 1));
    const char *prim = njp > 0 ? cJSON_GetArrayItem(jp, 0)->valuestring : cn;  /* exhaustive-ok: display pick; jp_domains carries all */
    cJSON_AddItemToObject(props, "primary_domain",
      prim ? cJSON_CreateString(prim) : cJSON_CreateNull());
    cJSON_AddNumberToObject(props, "all_domains_count",
      cJSON_IsArray(all) ? cJSON_GetArraySize(all) : 0);
    cJSON *cts = cJSON_GetObjectItem(ev, "ct_source");
    cJSON_AddItemToObject(props, "ct_source", cJSON_Duplicate(cts, 1));
    cJSON *nb = cJSON_GetObjectItem(ev, "not_before");
    cJSON *na = cJSON_GetObjectItem(ev, "not_after");
    cJSON_AddItemToObject(props, "not_before", cJSON_Duplicate(nb, 1));
    cJSON_AddItemToObject(props, "not_after", cJSON_Duplicate(na, 1));
    cJSON_AddStringToObject(props, "seen_at", iso ? iso : "");
    char *pj = cJSON_PrintUnformatted(props);

    /* The issuer DN is upstream text and routinely carries a `"` or a `\`
     * (X.500 DNs quote and escape). Pasting it straight into a JSON literal
     * produced tags_json that no parser accepts — and unlike a transient
     * request that corruption is PERSISTED on the row. Let cJSON serialise. */
    cJSON *tagsA = cJSON_CreateArray();
    cJSON_AddItemToArray(tagsA, cJSON_CreateString("ct-log"));
    {
      char issTag[192];
      snprintf(issTag, sizeof issTag, "issuer:%s", iss ? iss : "unknown");
      cJSON_AddItemToArray(tagsA, cJSON_CreateString(issTag));
    }
    char *tags = cJSON_PrintUnformatted(tagsA);

    cJSON *cl = cJSON_GetObjectItem(ev, "cert_link");
    intel_item it = {0};
    it.remote_key = rkey;                          /* sink → certstream-jp|<rkey> */
    /* A precertificate can carry no CN at all (SAN-only). `cn` was passed
     * through as the title regardless, so the row went out with a NULL title;
     * fall back to the first .jp SAN, which is what the row is actually about. */
    it.title = cn ? cn : (njp > 0 ? cJSON_GetArrayItem(jp, 0)->valuestring : NULL);
    if (!it.title) { free(body); cJSON_Delete(props); cJSON_Delete(tagsA);
                     free(tags); free(pj); i++; continue; }
    it.summary = summary;
    it.body = body;
    it.link = (cJSON_IsString(cl) && cl->valuestring[0]) ? cl->valuestring : NULL;
    it.lang = "en";
    it.published_at = iso;
    it.record_type = "certstream-jp";
    it.properties_json = pj;
    it.tags_json = tags;
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj); free(body); free(tags);
    cJSON_Delete(props); cJSON_Delete(tagsA);
    i++;
  }
  cJSON_Delete(u.events);
  fprintf(stderr, "[certstream-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def certstream_jp_def = {
  .id = "certstream-jp", .collector = "cyber",
  .name = "CertStream .jp CT Monitor", .name_ja = "CertStream .jp 証明書監視",
   .update_interval_sec = 60, .run = run };
REGISTER_SOURCE(certstream_jp_def)
