/* lib/pagewalk.h — paging and truncation disclosure for list endpoints.
 *
 * WHY THIS EXISTS
 * The generated collectors (collectors/sources/_verified_macros.inc: VJSON,
 * VGEO, VCSV) did ONE fetch of ONE literal URL. 2,727 of those URLs carry a
 * hardcoded page-size parameter — `limit=`, `rows=`, `per_page=`, `$top=`,
 * `resultRecordCount=` — so every one of those sources returned page 1 and
 * nothing else, forever, with no record of what it left behind. ROR answers 20
 * of ~110,000 organisations and puts `number_of_results` in the same envelope.
 * `make audit-sources` cannot see any of it: it greps C control flow, and these
 * discards live entirely inside string literals.
 *
 * WHAT THIS DOES, AND DELIBERATELY DOES NOT DO
 * It continues a walk only when the upstream itself said how:
 *   1. a next-page LINK in the response envelope (JSON:API `links.next`, DRF
 *      `next`, OData `@odata.nextLink`, and friends) — the server handed us the
 *      exact URL, so following it invents nothing;
 *   2. an offset/page parameter ALREADY PRESENT in the collector's own URL —
 *      advancing a number the author wrote is not guessing either. An OFFSET
 *      additionally needs a declared size, because that size is the stride;
 *      a PAGE NUMBER does not, because it always advances by one, and the
 *      server's own first page supplies the "was that page full" yardstick.
 *      (That second case is why ROR below is walked at all: `?page=1` states
 *      no size, so requiring one meant this module did a single fetch of the
 *      very source its own docstring cites.)
 * It never adds a parameter that was not there. Only ~135 of those 2,727 URLs
 * carry both a size and an offset, and the rest use conventions that differ per
 * API; inventing `&offset=` for them would fabricate requests, and a wrong guess
 * against 2,592 endpoints is how you get banned rather than complete.
 *
 * So for everything it cannot legitimately continue, it does the other half of
 * the house rule: it DISCLOSES. A page that came back exactly full is evidence
 * that more exists, and that fact is emitted as a `collector-truncation-notice`
 * record — the same shape lib/hpengine.c uses — rather than a log line nobody
 * reads. Silence is the violation; a stated bound is not.
 *
 * ENV
 *   JO_PAGE_MAX   pages per run, default 20, for EVERY walk in the tree — this
 *                 module, lib/jsonlist.c and lib/hpengine.c alike. It used to
 *                 reach only this one; see jo_page_max() in pagewalk.c for what
 *                 the other two did instead. 1 restores the old single-fetch
 *                 behaviour while KEEPING the disclosure.
 *   JO_PAGE_WALK  0 disables continuation entirely (disclosure still emitted).
 */
#ifndef JO_PAGEWALK_H
#define JO_PAGEWALK_H

#include "../source.h"
#include "../third_party/cJSON.h"

/* Fetch one page. Returns a parsed document the caller owns, or NULL. */
typedef cJSON *(*pw_fetch_fn)(const source_ctx *c, const char *url, void *ud);

/* Emit every record in one already-fetched page.
 *
 * Returns the number EMITTED, and must report through `seen` (never NULL) the
 * number of records the page CONTAINED. The two differ whenever the emitter
 * refuses a record — jsonlist_emit drops any record it cannot derive a title
 * for, geojson_emit_features skips non-objects.
 *
 * The distinction is load-bearing: `seen` is what decides "did this page come
 * back full", and therefore both whether to continue and whether to disclose.
 * Driving that off the emitted count meant a full page holding two unlabelled
 * records looked short, so the walk stopped AND suppressed its own truncation
 * notice — a silent stop plus a silent claim of completeness, which is the
 * exact failure this module exists to prevent. */
typedef int (*pw_emit_fn)(const source_ctx *c, intel_sink *s, const char *id,
                          cJSON *doc, void *ud, int *seen);

/* Walk `url` to exhaustion where the upstream permits, emitting through
 * `emit_page`, and disclose anything left behind as a truncation-notice record.
 *
 * Returns the number of records emitted (the notice is not counted), or -1 if
 * the FIRST fetch failed — a dead endpoint is an error, an empty one is not. */
/* The page ceiling every walk in the tree uses: $JO_PAGE_MAX when set, else
 * `legacy_env` when that name is set (NULL for none), else 20. One function
 * because there were three bounds with two numbers and two knobs, and one of
 * them had no knob at all — see jo_page_max() in pagewalk.c. */
int jo_page_max(const char *legacy_env);

/* The same ceiling for a caller that carries a source's own DECLARED page_max
 * (0 for none): $JO_PAGE_MAX when set — in both directions, because it is the
 * operator's brake — else the declared value, else the default above. Call
 * this rather than `declared > 0 ? declared : jo_page_max(NULL)`, which is
 * what hpengine did, and which left the knob unable to bound the 669 rows that
 * declare a ceiling of their own. */
int jo_page_max_for(int declared);

/* OData v2 "verbose" JSON states its total as `d.__count`, and as a STRING:
 * `{"d":{"results":[…],"__count":"46144"}}` when the request carries
 * `$inlinecount=allpages`. Returns that number, or -1 — for an absent key, an
 * empty string, or any string that is not entirely digits. One reader, called
 * by both declared-total lists (pagewalk's and lib/jsonlist.c's), because a
 * total read one way in one engine and not at all in the other is the shape
 * this tree has had to repair three times. */
long pw_odata_v2_count(const cJSON *doc);

int pw_walk(const source_ctx *c, intel_sink *s, const char *id,
            const char *url, pw_fetch_fn fetch, pw_emit_fn emit_page, void *ud);

/* As pw_walk, with an explicit page ceiling for a source whose collection size
 * has been MEASURED and is larger than the 20-page default.
 *
 * The default exists as a brake on a runaway walk against an unknown upstream,
 * and raising it globally would lift that brake for all 2,727 of them. But a
 * source whose total the upstream itself publishes — EMSC's 48,729 moment
 * tensors, CBS's 159,120 CPI rows, OpenAerialMap's 21,437 images — is not
 * unknown, and capping it at 20 pages turns a complete read into a permanently
 * disclosed shortfall. Pass the number of pages that covers the measured total
 * with headroom, and say in the caller where the measurement came from.
 *
 * 0 means "use the default". JO_PAGE_MAX still overrides, in both directions:
 * it is the operator's brake, so a source cannot talk past it, and an operator
 * who lowers it to make a run cheap gets that from every source. */
int pw_walk_max(const source_ctx *c, intel_sink *s, const char *id,
                const char *url, pw_fetch_fn fetch, pw_emit_fn emit_page,
                void *ud, int page_max_declared);

/* The default fetcher: feed_get_json with the 25 s timeout the macros used. */
cJSON *pw_fetch_json(const source_ctx *c, const char *url, void *ud);


/* ═══ THE PAGING CORE (2026-11-16) ══════════════════════════════════════════
 * Everything below is the ONE copy of a decision that four engines used to make
 * for themselves — lib/jsonlist.c (VJSON), lib/geojson.c (VGEO), pw_walk above
 * and lib/hpengine.c — and that the repo's history records being fixed in one
 * copy and missed in the others at least four times. lib/jsonlist.h,
 * lib/geojson.h and lib/hpengine.c keep their historic names as thin aliases;
 * tests/unit/test_paging_core_single.c fails if a second declared-total list or
 * a second pager table reappears. */

/* (a) the upstream's declared total. -1 when it did not say. */
#define PW_TOT_COUNT 0x1u   /* also accept bare `count` / `meta.count` (jsonlist,
                             * geojson, hpengine do; pw_walk deliberately not) */
long pw_declared_total(const cJSON *doc, unsigned flags);
/* The same over an XML envelope: only text BEFORE the first record element. */
long pw_declared_total_xml(const char *body, const char *rec_open);

/* URL query-string arithmetic. */
long  pw_url_int(const char *url, const char *name);               /* -1 if absent / not wholly an integer */
char *pw_url_set(const char *url, const char *name, long value);   /* replace or append; caller frees */

/* (b) the next page. */
#define PWP_JSON 1u   /* paired walk (jsonlist, hpengine page_walk): a size moves through its own cursor */
#define PWP_DECL 2u   /* pw_walk: the size is a stride, any offset/page parameter in the URL advances   */
#define PWP_GEO  4u
#define PWP_HP   8u   /* a size spelling hpengine reads off a row's own URL as its page yardstick */   /* a size spelling the GeoJSON walk recognises as "a full page"                    */
typedef struct {
  const char *size, *cursor, *alt_cursor;
  int         page_numbered;
  unsigned    uses;
} pw_pager;
extern const pw_pager PW_PAGERS[];                     /* THE pager table */
const pw_pager *pw_pager_find(const char *size_param);
long  pw_declared_size(const char *url, unsigned uses);
/* The first PW_PAGERS size spelling (PWP_DECL) the URL declares, or NULL; *size gets its value.
 * core/pagination_probe.c reads this instead of keeping a size list of its own. */
const char *pw_declared_size_param(const char *url, long *size);
/* 1 when the URL already carries a cursor spelling from the cursor table (offset, page, ...). */
int   pw_url_has_cursor(const char *url);
long  pw_declared_size_below(const char *url, unsigned uses, long below);   /* first size in (0, below) */  /* the size the URL declares, or -1 */

#define PW_RESOLVE_PATHS 1u   /* also resolve a bare relative path ("../x"); see pw_resolve_url */
char *pw_resolve_url(const char *base, const char *ref, unsigned flags);
char *pw_host_of(const char *s);
int   pw_same_host(const char *a, const char *b);      /* the cross-host rule: credentials stay with the row's host */
void  pw_strip_secret(char *url, const char *secret);

char *pw_next_link(const cJSON *doc, const char *base);   /* base NULL: absolute http(s) only */
char *pw_next_page(const cJSON *doc, const char *page_url, int got,
                   long available, int total, int *full_unadvanced);
char *pw_next_page_geo(const cJSON *doc, const char *page_url, int got,
                       long available, int total, int *full_unadvanced);
char *pw_page_one_retry(const char *prev_url, const char *page_url);
char *pw_seed_cursor(const char *url);

/* (c) did the upstream move? */
#define PW_FNV_BASIS 1469598103934665603ULL
unsigned long long pw_fnv_str(unsigned long long h, const char *s);
unsigned long long pw_text_hash(const char *s);            /* a raw body */
unsigned long long pw_body_fp(const cJSON *doc);           /* a parsed page, whole */
unsigned long long pw_page_fp(const cJSON *arr);           /* a page's first eight records, ordered */
unsigned long long *pw_page_fps(const cJSON *arr, int *n_out);   /* identity SET, caller frees */
int pw_fps_has(const unsigned long long *fps, int n, const cJSON *rec);

/* (d) the ceiling: $JO_PAGE_MAX, else `declared`, else $<legacy_env>, else 20. */
int pw_page_ceiling(const char *legacy_env, int declared);
const char *pw_ceiling_name(const char *legacy_env, int declared);

/* (e) a later page failed: end of data, or a walk cut short? */
int  pw_later_page_cut(long status, int prev_short, int prev_full,
                       int link_given, long declared_total, long fetched);
void pw_cut_reason(long status, int unreadable, char *out, size_t cap);
void pw_failed_page_reason(int page, long status, char *out, size_t cap);
const char *pw_cut_remedy(long status);
#define PW_REASON_NEXT_LINK_SELF \
  "the upstream's next-page link pointed back at the page just fetched, so the " \
  "walk stopped rather than re-collecting the same records"

/* (f) the disclosure. Facts the caller does not know stay at their INIT value
 * (-1 / NULL / 0) and are not emitted. `extra` is CONSUMED. */
#define PW_AVAIL_NULL   0   /* unknown total -> JSON null  */
#define PW_AVAIL_STRING 1   /* unknown total -> "unknown — upstream declared no total" */
#define PW_KEY_PLAIN    0   /* remote_key "truncation" (one row per source) */
#define PW_KEY_SCOPED   1   /* "<id>|truncation:<query>" */
typedef struct {
  intel_sink *sink;
  const char *source_id, *query;
  const char *url_prop, *url;           /* property name ("endpoint" / "url"), NULL = omit */
  long  used, available;                /* available < 0: unknown */
  int   avail_style;
  const char *basis;                    /* records_available_basis */
  long  fetched;                        /* < 0: omit */
  int   pages_read, page_ceiling, more_pages_pending;   /* <0 / <=0 / <0: omit */
  long  dropped;                        /* < 0: omit */
  int   failed_page; long failed_status; const char *failed_url; int failed_unreadable;
  cJSON *extra;
  const char *reason, *remedy, *title, *tags_json;
  int   key_style;
} pw_notice;
#define PW_NOTICE_INIT { .fetched = -1, .pages_read = -1, .more_pages_pending = -1, \
                         .dropped = -1, .available = -1, .failed_status = -1 }
void pw_notice_emit(intel_sink *sink, pw_notice *n);

/* The list walk, for drivers whose page is a LIST of records. */
#define PW_FLAVOR_JSON 0   /* jsonlist_emit_paged: paired pager, page-1 probe, declared-short */
#define PW_FLAVOR_GEO  1   /* geojson_emit_paged: exceededTransferLimit, cursor by records held */
typedef struct {
  void *ud;
  cJSON *(*fetch)(void *ud, const char *url);                    /* NULL = failed */
  cJSON *(*records)(void *ud, cJSON *doc);                       /* the page's record array, or NULL */
  int    (*emit)(void *ud, cJSON *doc,                           /* returns emitted; skip = probe's identity set */
                 const unsigned long long *skip, int nskip);
} pw_list_ops;
int pw_walk_list(int flavor, intel_sink *sink, const char *source_id,
                 const char *start_url, const char *endpoint,
                 const char *legacy_env, const pw_list_ops *ops);

#endif /* JO_PAGEWALK_H */
