/* lib/hpengine.h — the "high-penetrancy" record engine.
 *
 * WHY THIS EXISTS. The registry already had ~2200 sources, but most of them
 * are one HTTP call deep: a search endpoint that answers "does this name
 * appear?" and stops. Penetrancy is the other axis — how far past the search
 * hit a source reaches into the underlying record: the officers behind a
 * company, the persons with significant control behind the officers, the
 * charges filed against the company, the routing history behind an ASN, the
 * inspection detentions behind a ship's IMO number. Those endpoints exist and
 * are public; nobody had wired them because each one needs its own fetch +
 * parse, and 300 bespoke parsers is 30k lines nobody can audit.
 *
 * So: one engine, N declarative rows. A row names a real endpoint, how to
 * build its URL from the pivot entity, and (optionally) the follow-up endpoint
 * that turns a list hit into a full record. Everything emitted is REAL fetched
 * content — the engine has no fixture path, no fallback table, no synthesized
 * row. It cannot fabricate: it only ever forwards fields that came back over
 * the wire. When the fetch fails, the credential is missing, or the shape is
 * not what a row expected, it emits nothing and says so (honest empty), in
 * line with SOURCE_REALITY_REPORT.md's no-fabrication rule.
 *
 * The emitted intel_item keeps EVERY scalar field of the upstream record in
 * properties (flattened, dotted keys), not a hand-picked three. That is the
 * point: the record, not a headline.
 */
#ifndef JO_HPENGINE_H
#define JO_HPENGINE_H

#include "../source.h"

typedef enum {
  HP_JSON = 0,   /* JSON document → record array (or the root object)      */
  HP_HTML = 1,   /* server-rendered listing → real <a> hits                */
  HP_CSV  = 2,   /* CSV/TSV with a header row → one record per row         */
} hp_mode;

/* Shape gate: a row that only makes sense for a domain must not burn a request
 * on a person's name. Mismatch = skip (0 emitted), never a fabricated answer. */
typedef enum {
  HP_ANY = 0,
  HP_DOMAIN,     /* has a dot, no spaces (host, or a URL/email we reduce)   */
  HP_IP,         /* dotted-quad                                            */
  HP_EMAIL,
  HP_NUMERIC,    /* contains ≥4 digits (CIK, SIREN, IMO, MMSI, KRS, LEI…)  */
  HP_HASH,       /* 32/40/64 hex (md5/sha1/sha256)                          */
  HP_ICAO24,     /* 6 hex chars — a Mode-S / ICAO24 aircraft address        */
  HP_ETH,        /* 0x + 40 hex                                            */
  HP_BTC,        /* base58/bech32-ish bitcoin address                      */
  HP_ASN,        /* AS1234 / 1234                                          */
} hp_want;

/* One collector. Only `id`, `name`, `url` and the identity fields are
 * mandatory; everything else has a working default.
 *
 * URL / body / header templates expand these tokens (all URL-encoded except
 * {Q}, which is the raw entity for POST-JSON bodies):
 *   {q}      entity, URL-encoded            {Q}       entity, raw
 *   {qd}     digits only (CIK/SIREN/IMO)    {qc}      digits zero-padded to 10
 *   {qh}     host part of a URL/email       {qu}      local part of an email
 *   {ql}/{qU} lower/upper-cased entity      {qn}      spaces stripped
 *   {key}    credential from key_env        {keyb64}  base64("<key>:")
 * The detail (second-hop) URL additionally expands {v} — the value picked out
 * of the list record by `detail_key`. */
typedef struct hp_source {
  const char *id;             /* registry id — unique across the whole set  */
  const char *name;
  const char *name_ja;
  const char *collector;      /* NULL → "osint"                            */
  const char *category;       /* NULL → "investigation"                    */
  const char *type;           /* NULL → "api"                              */
  const char *portal;         /* human-facing base URL (source_def.url)    */
  const char *description;
  const char *record_type;    /* stamped on every emitted item             */
  const char *tags;           /* extra JSON array members, e.g. "\"uk\""   */
  /* BCP-47-ish language of the upstream's own text. NULL → "en", which is the
   * only value this engine used to emit: every row was stamped English no
   * matter what it fetched. That is a claim about the content, and it was wrong
   * for a Brazilian transparency portal or a Korean disclosure filing — the
   * translate and search paths downstream read it. */
  const char *lang;

  hp_mode mode;
  hp_want want;

  const char *url;            /* endpoint template (required)              */
  const char *post_body;      /* non-NULL → POST with this body template   */
  const char *content_type;   /* NULL → "application/json" when posting    */
  const char *headers[5];     /* extra header templates, NULL-terminated   */
  const char *key_env;        /* required credential env var; NULL = free  */

  /* JSON shaping. All optional: with no array_path the engine finds the
   * densest array of objects itself, and with no *_keys it falls back to a
   * conventional key list — a row whose upstream changed shape degrades to
   * fewer resolved fields, never to invented ones. */
  const char *array_path;     /* dotted path ("a.b.c"), "" / NULL = auto    */
  const char *title_keys;     /* comma-separated candidates, first wins     */
  const char *id_keys;
  const char *link_keys;
  const char *link_tmpl;      /* {v} = link_keys value; else the raw value  */
  const char *date_keys;
  const char *body_keys;
  const char *lat_key, *lon_key;

  /* HTML mode */
  const char *href_must;      /* anchor href must contain this (NULL = any) */
  const char *base;           /* prepended to root-relative hrefs           */

  /* Second hop — the actual penetrancy. detail_max 0 (the default) deepens
   * EVERY list record, bounded operationally by $JO_HP_DETAIL_MAX (default 25)
   * so one pivot cannot fire thousands of requests. Records that were not
   * deepened carry `_detail_pending: true` — an un-fetched detail is reported,
   * not quietly treated as absent. */
  const char *detail_url;     /* template, {v} from detail_key              */
  const char *detail_key;     /* field in the list record holding the id    */
  const char *detail_path;    /* array_path for the detail doc (or NULL)    */
  int         detail_max;     /* 0 = every record (see JO_HP_DETAIL_MAX)    */

  /* Pagination — part of the exhaustive-use rule
   * (docs/SOURCE_EXHAUSTIVENESS.md): a paged endpoint that is read once has
   * silently discarded every page after the first. Set ONE of these and the
   * engine keeps fetching until the upstream stops producing records.
   *   next_path   — dotted path to an absolute "next page" URL in the response
   *   page_param  — query parameter to append/increment ("page", "offset", …)
   * page_size is what one page returns (needed for offset-style paging), and
   * page_max bounds the walk (default 10 pages) so a runaway feed cannot spin
   * forever — when that bound bites it is stamped on every record, never
   * silent. */
  const char *next_path;
  const char *page_param;
  int         page_start;     /* first value of page_param (default 1, or 0
                               * when the param name contains "offset")      */
  int         page_size;      /* records per page, for offset-style paging    */
  int         page_max;       /* max pages to walk (default 10)              */

  int csv_no_header;          /* CSV mode: file has no header row → col0..colN */
  int filter_query;           /* 1 = keep only records mentioning the query */
  /* Cap on emitted records. 0 (the default) means EVERY record the upstream
   * returned — the engine does not invent a limit the caller did not ask for.
   * A non-zero value is the row author's explicit choice and is reported in
   * each record's `_records_truncated` marker when it bites. */
  int max_items;
  int free_tier;              /* 1 = usable without payment                 */
  int interval;               /* 0 = on-demand pivot (the norm here)        */
} hp_source;

/* Register `n` rows. `defs` must be static storage of at least n entries owned
 * by the caller (source_def pointers live for the process lifetime). */
void hp_register(const hp_source *specs, int n, source_def *defs);

/* Declare + register a table in one line at the bottom of a collector file. */
#define HP_REGISTER_TABLE(TBL)                                                \
  static source_def TBL##_defs[sizeof(TBL) / sizeof((TBL)[0])];               \
  __attribute__((constructor)) static void hp_reg_##TBL(void) {               \
    hp_register((TBL), (int)(sizeof(TBL) / sizeof((TBL)[0])), TBL##_defs);    \
  }

#endif
