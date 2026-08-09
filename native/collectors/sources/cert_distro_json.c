/* cert_distro_json.c — the five keyless JSON errata/security-tracker APIs of the
 * major Linux distributions. One table-driven run() dispatching on
 * ctx->source_id; five source_defs.
 *
 *   archlinux-security-json   https://security.archlinux.org/json
 *       Bare array of AVG vulnerability groups: package, affected version,
 *       fixed version, status, severity and the exact CVE set (issues[]).
 *   alpine-secdb-main         https://secdb.alpinelinux.org/v3.21/main.json
 *       doc.packages[].pkg.secfixes is a map fixed-version -> [CVE, …]. Alpine
 *       underpins most container base images, so this is the container-layer
 *       CVE ground truth.
 *   almalinux-errata-full     https://errata.almalinux.org/9/errata.full.json
 *       doc.data[] ALSA/ALBA/ALEA errata; dates are unix epoch SECONDS. The
 *       document is ~25 MB, hence the daily cadence AND the recent-window
 *       filter below — the whole history is not re-emitted every run.
 *   rockylinux-errata-api     https://apollo.build.resf.org/api/v3/advisories/
 *       doc.advisories[]; page is 1-BASED (page=0 returns HTTP 422 — the trap
 *       recorded in the probe notes, so the request pins page=1).
 *   redhat-csaf-advisories    https://access.redhat.com/hydra/rest/securitydata/csaf.json
 *       Bare array of RHSA index rows: severity, released_on, the CVE set and
 *       the released package NEVRAs, plus the resource_url Red Hat itself
 *       publishes for the full CSAF document.
 *
 * Every emitted value is read out of the response body. Where a row carries a
 * link it is a URL the upstream document contained (Red Hat's resource_url),
 * never one assembled from an identifier (R1).
 *
 * R2: no geometry — a distribution erratum has no location.
 * R3: each source returns -1 only when its own fetch/parse fails; a fetch that
 * succeeded with zero matching rows returns 0.
 *
 * All keyless. Licences: none of these five publishers states one on the
 * endpoint itself beyond public availability. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARCH_URL   "https://security.archlinux.org/json"
#define ALPINE_URL "https://secdb.alpinelinux.org/v3.21/main.json"
#define ALMA_URL   "https://errata.almalinux.org/9/errata.full.json"
#define ROCKY_URL  "https://apollo.build.resf.org/api/v3/advisories/?page=1&size=100"
#define RH_URL     "https://access.redhat.com/hydra/rest/securitydata/csaf.json?per_page=100"

#define ALMA_WINDOW_SEC (365L * 24 * 3600)   /* recent window, see header */
#define ALMA_MAX_ROWS   800

/* Join an array of strings into "a, b, c" (bounded). */
static void join_strings(cJSON *arr, char *out, size_t n) {
  out[0] = 0;
  if (!cJSON_IsArray(arr)) return;
  size_t j = 0;
  cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    if (!cJSON_IsString(e) || !e->valuestring || !e->valuestring[0]) continue;
    const char *v = e->valuestring;
    if (j && j + 2 < n) { out[j++] = ','; out[j++] = ' '; }
    while (*v && j + 1 < n) out[j++] = *v++;
  }
  out[j] = 0;
}

/* Unix epoch seconds out of a value that may be a plain number, a numeric
 * string, or a Mongo-style {"$date": …}. 0 when there is no usable date. */
static long epoch_of(cJSON *v) {
  if (!v) return 0;
  if (cJSON_IsNumber(v)) {
    double d = v->valuedouble;
    if (d > 1e11) d /= 1000.0;            /* milliseconds */
    return (long)d;
  }
  if (cJSON_IsString(v) && v->valuestring) return (long)strtod(v->valuestring, NULL);
  if (cJSON_IsObject(v)) {
    cJSON *d = cJSON_GetObjectItem(v, "$date");
    if (d) return epoch_of(d);
    cJSON *nl = cJSON_GetObjectItem(v, "$numberLong");
    if (nl) return epoch_of(nl);
  }
  return 0;
}

static void iso_of(long epoch, char *out, size_t n) {
  out[0] = 0;
  if (epoch <= 0) return;
  time_t t = (time_t)epoch;
  struct tm tm;
  gmtime_r(&t, &tm);
  strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

/* ---- archlinux-security-json ---------------------------------------------- */

static int arch_run(const source_ctx *c, intel_sink *s) {
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(c->http, ARCH_URL, hdrs, 40000);
  if (!doc) { fprintf(stderr, "[archlinux-security-json] fetch/parse failed\n"); return -1; }
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "data");
  int n = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *g;
    cJSON_ArrayForEach(g, arr) {
      const char *name = jo_sv(g, "name");
      if (!name) continue;                 /* no AVG id -> no row (R1) */
      char pkgs[512], cves[1024];
      join_strings(cJSON_GetObjectItem(g, "packages"), pkgs, sizeof pkgs);
      join_strings(cJSON_GetObjectItem(g, "issues"),   cves, sizeof cves);
      const char *sev  = jo_sv(g, "severity");
      const char *type = jo_sv(g, "type");

      char title[600];
      snprintf(title, sizeof title, "%s: %s%s%s%s%s", name,
               pkgs[0] ? pkgs : "(unnamed package)",
               type ? " - " : "", type ? type : "",
               sev ? " / " : "", sev ? sev : "");
      jo_utf8_trunc(title, 320);

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "avg", name);
      const char *v;
      if ((v = jo_sv(g, "status")))   cJSON_AddStringToObject(p, "status", v);
      if (sev)                     cJSON_AddStringToObject(p, "severity", sev);
      if (type)                    cJSON_AddStringToObject(p, "type", type);
      if ((v = jo_sv(g, "affected"))) cJSON_AddStringToObject(p, "affected_version", v);
      if ((v = jo_sv(g, "fixed")))    cJSON_AddStringToObject(p, "fixed_version", v);
      cJSON *pk = cJSON_GetObjectItem(g, "packages");
      if (cJSON_IsArray(pk)) cJSON_AddItemToObject(p, "packages", cJSON_Duplicate(pk, 1));
      cJSON *is = cJSON_GetObjectItem(g, "issues");
      if (cJSON_IsArray(is)) cJSON_AddItemToObject(p, "cves", cJSON_Duplicate(is, 1));
      cJSON *ad = cJSON_GetObjectItem(g, "advisories");
      if (cJSON_IsArray(ad)) cJSON_AddItemToObject(p, "advisories", cJSON_Duplicate(ad, 1));
      cJSON_AddStringToObject(p, "source", "archlinux_security_tracker");
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      intel_item it = {0};
      it.remote_key      = name;
      it.title           = title;
      it.body            = cves[0] ? cves : NULL;
      it.summary         = cves[0] ? cves : NULL;
      it.lang            = "en";
      it.record_type     = "distro-advisory";
      it.properties_json = pj ? pj : "{}";
      it.tags_json       = "[\"advisory\",\"cyber\",\"arch-linux\",\"linux-distro\"]";
      if (s->emit(s, &it) >= 0) n++;
      free(pj);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[archlinux-security-json] emitted %d\n", n);
  return 0;
}

/* ---- alpine-secdb-main ---------------------------------------------------- */

static int alpine_run(const source_ctx *c, intel_sink *s) {
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(c->http, ALPINE_URL, hdrs, 30000);
  if (!doc) { fprintf(stderr, "[alpine-secdb-main] fetch/parse failed\n"); return -1; }
  const char *dv   = jo_sv(doc, "distroversion");
  const char *repo = jo_sv(doc, "reponame");
  cJSON *pkgs = cJSON_GetObjectItem(doc, "packages");
  int n = 0;
  if (cJSON_IsArray(pkgs)) {
    cJSON *e;
    cJSON_ArrayForEach(e, pkgs) {
      cJSON *pkg = cJSON_GetObjectItem(e, "pkg");
      if (!pkg) continue;
      const char *name = jo_sv(pkg, "name");
      cJSON *secfixes = cJSON_GetObjectItem(pkg, "secfixes");
      if (!name || !cJSON_IsObject(secfixes)) continue;   /* nothing fetched */

      /* secfixes = { "<fixed-version>": ["CVE-…", …], … } */
      cJSON *cves = cJSON_CreateArray();
      int nfix = 0;
      cJSON *fv;
      cJSON_ArrayForEach(fv, secfixes) {
        nfix++;
        if (!cJSON_IsArray(fv)) continue;
        cJSON *id;
        cJSON_ArrayForEach(id, fv)
          if (cJSON_IsString(id) && id->valuestring && id->valuestring[0])
            cJSON_AddItemToArray(cves, cJSON_CreateString(id->valuestring));
      }
      char cvelist[1024];
      join_strings(cves, cvelist, sizeof cvelist);

      char rk[256], title[512];
      snprintf(rk, sizeof rk, "%s/%s/%s", dv ? dv : "alpine",
               repo ? repo : "main", name);
      snprintf(title, sizeof title, "Alpine %s %s: %s - %d fixed version%s",
               dv ? dv : "", repo ? repo : "", name, nfix, nfix == 1 ? "" : "s");
      jo_utf8_trunc(title, 300);

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "package", name);
      if (dv)   cJSON_AddStringToObject(p, "distroversion", dv);
      if (repo) cJSON_AddStringToObject(p, "reponame", repo);
      cJSON_AddItemToObject(p, "secfixes", cJSON_Duplicate(secfixes, 1));
      cJSON_AddItemToObject(p, "cves", cves);      /* ownership moves to p */
      cJSON_AddStringToObject(p, "source", "alpine_secdb");
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      intel_item it = {0};
      it.remote_key      = rk;
      it.title           = title;
      it.body            = cvelist[0] ? cvelist : NULL;
      it.summary         = cvelist[0] ? cvelist : NULL;
      it.lang            = "en";
      it.record_type     = "distro-advisory";
      it.properties_json = pj ? pj : "{}";
      it.tags_json       = "[\"advisory\",\"cyber\",\"alpine\",\"linux-distro\",\"container\"]";
      if (s->emit(s, &it) >= 0) n++;
      free(pj);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[alpine-secdb-main] emitted %d\n", n);
  return 0;
}

/* ---- almalinux-errata-full ------------------------------------------------ */

static int alma_run(const source_ctx *c, intel_sink *s) {
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(c->http, ALMA_URL, hdrs, 90000);
  if (!doc) { fprintf(stderr, "[almalinux-errata-full] fetch/parse failed\n"); return -1; }
  cJSON *arr = cJSON_GetObjectItem(doc, "data");
  if (!cJSON_IsArray(arr) && cJSON_IsArray(doc)) arr = doc;

  /* Pass 1: newest issued_date actually present, so the window is relative to
   * the document rather than to a hardcoded date. */
  long newest = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *e;
    cJSON_ArrayForEach(e, arr) {
      long t = epoch_of(cJSON_GetObjectItem(e, "issued_date"));
      if (t > newest) newest = t;
    }
  }
  long cutoff = newest > 0 ? newest - ALMA_WINDOW_SEC : 0;

  int n = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *e;
    cJSON_ArrayForEach(e, arr) {
      if (n >= ALMA_MAX_ROWS) break;
      const char *id = jo_sv(e, "id");
      if (!id) continue;                     /* no erratum id -> no row (R1) */
      long issued = epoch_of(cJSON_GetObjectItem(e, "issued_date"));
      if (cutoff && issued && issued < cutoff) continue;

      const char *ttl  = jo_sv(e, "title");
      const char *desc = jo_sv(e, "description");
      const char *sev  = jo_sv(e, "severity");
      const char *typ  = jo_sv(e, "type");

      char title[600];
      if (ttl) snprintf(title, sizeof title, "%s: %s", id, ttl);
      else     snprintf(title, sizeof title, "%s", id);
      jo_utf8_trunc(title, 320);

      char pub[40];
      iso_of(issued, pub, sizeof pub);

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "erratum_id", id);
      if (sev) cJSON_AddStringToObject(p, "severity", sev);
      if (typ) cJSON_AddStringToObject(p, "type", typ);
      if (pub[0]) cJSON_AddStringToObject(p, "issued_at", pub);
      long upd = epoch_of(cJSON_GetObjectItem(e, "updated_date"));
      if (upd) { char u[40]; iso_of(upd, u, sizeof u);
                 if (u[0]) cJSON_AddStringToObject(p, "updated_at", u); }
      cJSON *refs = cJSON_GetObjectItem(e, "references");
      if (cJSON_IsArray(refs)) cJSON_AddItemToObject(p, "references", cJSON_Duplicate(refs, 1));
      cJSON *pk = cJSON_GetObjectItem(e, "packages");
      if (cJSON_IsArray(pk))
        cJSON_AddNumberToObject(p, "package_count", cJSON_GetArraySize(pk));
      cJSON_AddStringToObject(p, "source", "almalinux_errata");
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      intel_item it = {0};
      it.remote_key      = id;
      it.title           = title;
      it.body            = desc;
      it.summary         = desc ? desc : ttl;
      it.lang            = "en";
      it.published_at    = pub[0] ? pub : NULL;
      it.record_type     = "distro-advisory";
      it.properties_json = pj ? pj : "{}";
      it.tags_json       = "[\"advisory\",\"cyber\",\"almalinux\",\"linux-distro\"]";
      if (s->emit(s, &it) >= 0) n++;
      free(pj);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[almalinux-errata-full] emitted %d (window %ld d)\n",
          n, (long)(ALMA_WINDOW_SEC / 86400));
  return 0;
}

/* ---- rockylinux-errata-api ------------------------------------------------ */

static int rocky_run(const source_ctx *c, intel_sink *s) {
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(c->http, ROCKY_URL, hdrs, 30000);
  if (!doc) { fprintf(stderr, "[rockylinux-errata-api] fetch/parse failed\n"); return -1; }
  cJSON *arr = cJSON_GetObjectItem(doc, "advisories");
  if (!cJSON_IsArray(arr) && cJSON_IsArray(doc)) arr = doc;
  int n = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *e;
    cJSON_ArrayForEach(e, arr) {
      const char *name = jo_sv(e, "name");
      if (!name) continue;                   /* no RLSA id -> no row (R1) */
      const char *syn  = jo_sv(e, "synopsis");
      const char *desc = jo_sv(e, "description");
      const char *sev  = jo_sv(e, "severity");
      const char *pub  = jo_sv(e, "published_at");

      char title[600];
      if (syn) snprintf(title, sizeof title, "%s: %s", name, syn);
      else     snprintf(title, sizeof title, "%s", name);
      jo_utf8_trunc(title, 320);

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "advisory", name);
      if (syn) cJSON_AddStringToObject(p, "synopsis", syn);
      if (sev) cJSON_AddStringToObject(p, "severity", sev);
      const char *v;
      if ((v = jo_sv(e, "kind")))       cJSON_AddStringToObject(p, "kind", v);
      if (pub)                       cJSON_AddStringToObject(p, "published_at", pub);
      if ((v = jo_sv(e, "updated_at"))) cJSON_AddStringToObject(p, "updated_at", v);
      cJSON *cves = cJSON_GetObjectItem(e, "cves");
      if (cJSON_IsArray(cves)) cJSON_AddItemToObject(p, "cves", cJSON_Duplicate(cves, 1));
      cJSON *ap = cJSON_GetObjectItem(e, "affected_products");
      if (cJSON_IsArray(ap))
        cJSON_AddNumberToObject(p, "affected_product_count", cJSON_GetArraySize(ap));
      cJSON_AddStringToObject(p, "source", "rocky_apollo_errata");
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      intel_item it = {0};
      it.remote_key      = name;
      it.title           = title;
      it.body            = desc;
      it.summary         = syn ? syn : desc;
      it.lang            = "en";
      it.published_at    = pub;
      it.record_type     = "distro-advisory";
      it.properties_json = pj ? pj : "{}";
      it.tags_json       = "[\"advisory\",\"cyber\",\"rocky-linux\",\"linux-distro\"]";
      if (s->emit(s, &it) >= 0) n++;
      free(pj);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[rockylinux-errata-api] emitted %d\n", n);
  return 0;
}

/* ---- redhat-csaf-advisories ----------------------------------------------- */

static int redhat_run(const source_ctx *c, intel_sink *s) {
  const char *hdrs[] = { "accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(c->http, RH_URL, hdrs, 30000);
  if (!doc) { fprintf(stderr, "[redhat-csaf-advisories] fetch/parse failed\n"); return -1; }
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "data");
  int n = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *e;
    cJSON_ArrayForEach(e, arr) {
      const char *rhsa = jo_sv(e, "RHSA");
      if (!rhsa) rhsa = jo_sv(e, "rhsa");
      if (!rhsa) continue;                   /* no RHSA id -> no row (R1) */
      const char *sev  = jo_sv(e, "severity");
      const char *rel  = jo_sv(e, "released_on");
      /* resource_url is published BY Red Hat in this index, not built here. */
      const char *link = jo_sv(e, "resource_url");

      char cves[1200];
      join_strings(cJSON_GetObjectItem(e, "CVEs"), cves, sizeof cves);

      char title[600];
      snprintf(title, sizeof title, "%s%s%s%s%s%s", rhsa,
               sev ? " (" : "", sev ? sev : "", sev ? ")" : "",
               cves[0] ? ": " : "", cves[0] ? cves : "");
      jo_utf8_trunc(title, 320);

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "rhsa", rhsa);
      if (sev)  cJSON_AddStringToObject(p, "severity", sev);
      if (rel)  cJSON_AddStringToObject(p, "released_on", rel);
      if (link) cJSON_AddStringToObject(p, "resource_url", link);
      cJSON *cv = cJSON_GetObjectItem(e, "CVEs");
      if (cJSON_IsArray(cv)) cJSON_AddItemToObject(p, "cves", cJSON_Duplicate(cv, 1));
      cJSON *bz = cJSON_GetObjectItem(e, "bugzillas");
      if (cJSON_IsArray(bz)) cJSON_AddItemToObject(p, "bugzillas", cJSON_Duplicate(bz, 1));
      cJSON *rp = cJSON_GetObjectItem(e, "released_packages");
      if (cJSON_IsArray(rp)) {
        cJSON_AddNumberToObject(p, "released_package_count", cJSON_GetArraySize(rp));
        cJSON_AddItemToObject(p, "released_packages", cJSON_Duplicate(rp, 1));
      }
      cJSON_AddStringToObject(p, "source", "redhat_securitydata_csaf");
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      intel_item it = {0};
      it.remote_key      = rhsa;
      it.title           = title;
      it.body            = cves[0] ? cves : NULL;
      it.summary         = cves[0] ? cves : NULL;
      it.link            = link;
      it.lang            = "en";
      it.published_at    = rel;
      it.record_type     = "distro-advisory";
      it.properties_json = pj ? pj : "{}";
      it.tags_json       = "[\"advisory\",\"cyber\",\"red-hat\",\"linux-distro\"]";
      if (s->emit(s, &it) >= 0) n++;
      free(pj);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[redhat-csaf-advisories] emitted %d\n", n);
  return 0;
}

static int run(const source_ctx *c, intel_sink *s) {
  if (!c || !c->source_id) return -1;
  if (!strcmp(c->source_id, "archlinux-security-json")) return arch_run(c, s);
  if (!strcmp(c->source_id, "alpine-secdb-main"))       return alpine_run(c, s);
  if (!strcmp(c->source_id, "almalinux-errata-full"))   return alma_run(c, s);
  if (!strcmp(c->source_id, "rockylinux-errata-api"))   return rocky_run(c, s);
  if (!strcmp(c->source_id, "redhat-csaf-advisories"))  return redhat_run(c, s);
  fprintf(stderr, "[cert_distro_json] unknown source id %s\n", c->source_id);
  return -1;
}

static const source_def cert_arch_def = {
  .id = "archlinux-security-json", .collector = "cyber",
  .name = "Arch Linux Security Tracker (AVG)",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "dataset", .url = ARCH_URL,
  .description = "Arch Linux's full vulnerability-group tracker as one JSON "
                 "document: AVG groups mapping package name to affected "
                 "version, fixed version and the exact CVE set.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_arch_def)

static const source_def cert_alpine_def = {
  .id = "alpine-secdb-main", .collector = "cyber",
  .name = "Alpine Linux Security Database (secdb)",
  .update_interval_sec = 43200, .run = run,
  .category = "cyber", .type = "dataset", .url = ALPINE_URL,
  .description = "Alpine's security database mapping main-repo packages to the "
                 "exact apk version that fixed each CVE. Alpine underpins most "
                 "container base images, so this is container-layer CVE ground "
                 "truth.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_alpine_def)

static const source_def cert_alma_def = {
  .id = "almalinux-errata-full", .collector = "cyber",
  .name = "AlmaLinux 9 Errata (full)",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "dataset", .url = ALMA_URL,
  .description = "AlmaLinux 9's errata set (ALSA/ALBA/ALEA) with severity, CVE "
                 "references and the fixed-package list - RHEL-compatible "
                 "advisory coverage without a subscription. Recent window only.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_alma_def)

static const source_def cert_rocky_def = {
  .id = "rockylinux-errata-api", .collector = "cyber",
  .name = "Rocky Linux Errata API (Apollo)",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "api", .url = ROCKY_URL,
  .description = "Rocky Linux's Apollo errata API: RLSA advisories with "
                 "synopsis, severity and the CVE list, paginated so it can be "
                 "polled cheaply for new advisories.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_rocky_def)

static const source_def cert_redhat_def = {
  .id = "redhat-csaf-advisories", .collector = "cyber",
  .name = "Red Hat Security Advisories (CSAF index)",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api", .url = RH_URL,
  .description = "Red Hat's RHSA advisory index with severity, release "
                 "timestamp, the CVE set fixed and the exact NEVRA package "
                 "list.",
  .license = "Red Hat Security Data API, public and keyless.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_redhat_def)
