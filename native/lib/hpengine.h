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
  /* XML document → one record per repeated element. Added because the most
   * authoritative sanctions lists in existence — the UK OFSI consolidated
   * list, the EU financial sanctions file and the Swiss SECO whole list — are
   * published as XML and only as XML, and every one of them was unreachable:
   * hp_run's switch fell through to hp_run_json, cJSON refused the body, and
   * the row emitted nothing forever while still registering as a source.
   * `array_path` names the record element; leave it empty to auto-detect the
   * most repeated one. */
  HP_XML  = 3,
  HP_XLSX = 4,   /* one worksheet -> CSV via lib/xlsx.c, then the csv path */
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
  /* Record identity — the uid the sink upserts on. FIRST-MATCH per record:
   * the engine takes the first listed key that resolves to a non-empty value
   * and never looks at the rest, so the ORDER is the declaration. A feed
   * that points several items at one section page (IPA's newsonly-rss.rdf,
   * three items → one /about/ link) collides on `link`, and the sink keeps
   * one of them; declare `guid` or `title` FIRST in that case, and `link`
   * after as the fallback for items that carry neither. The collision guard
   * disambiguates records that merely share a key within one page, but a
   * key that is not the identity is still the wrong key. */
  const char *id_keys;
  const char *link_keys;
  const char *link_tmpl;      /* {v} = link_keys value; else the raw value  */
  const char *date_keys;
  const char *body_keys;
  const char *lat_key, *lon_key;

  /* HTML mode */
  const char *href_must;      /* anchor href must contain this (NULL = any) */
  /* Base URL for resolving a relative href. Every href that is not absolute
   * is resolved per RFC 3986 against, in order: this field when set, the
   * page's own `<base href>` when it has one, else the URL the page was
   * fetched from. Before this the engine only ever PREFIXED a root-relative
   * href with `base`, so a listing that links `../profile/x.htm` or
   * `meisai/y.htm` — Sangiin, courts.go.jp, Yamaha's advisories, EC-CUBE —
   * stored an unreachable link, or the row was rejected. Set it only to
   * override the page's own context (a listing served from one host that
   * links into another). */
  const char *base;

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
   *   {page}      — a token IN THE URL, substituted with the page number, for
   *                 an upstream that pages by path segment (kanpou.ai's
   *                 /tosan/p/N). Same page_start / page_zero_based / page_max
   *                 semantics as page_param, and the same stop rules (an
   *                 empty or repeated page ends the walk). Mutually exclusive
   *                 with page_param: gen_hp_batch.py rejects a row with both.
   * page_size is what one page returns (needed for offset-style paging), and
   * page_max bounds the walk (default 10 pages) so a runaway feed cannot spin
   * forever — when that bound bites it is stamped on every record, never
   * silent. */
  const char *next_path;
  /* Template for building the next-page URL from the value `next_path`
   * resolved to, with `{v}` standing for that value (URL-encoded).
   *
   * `next_path` alone assumes the upstream hands back an absolute URL. OAI-PMH
   * hands back an opaque `resumptionToken` instead, and the continuation
   * request is `?verb=ListRecords&resumptionToken=<token>` — with
   * metadataPrefix deliberately NOT repeated, which the protocol forbids. A
   * bare token used as a URL simply fails, so before this existed every
   * OAI-PMH row read its first page and stopped: 100 records of a repository
   * holding 69,738, silently, which is precisely what house rule 2 forbids.
   *
   *   next_path=resumptionToken
   *   next_tmpl=https://x.repo.nii.ac.jp/oai?verb=ListRecords&resumptionToken={v}
   */
  const char *next_tmpl;
  const char *page_param;
  int         page_start;     /* first value of page_param (default 1, or 0
                               * when the param name contains "offset")      */
  /* `page_start` cannot express "this API's first page is 0", because 0 is
   * also its unset value and the engine coerces that to 1 for a non-offset
   * param. A 0-based API (CKAN's `start`, opendata.ch's `page`) therefore had
   * its first extra page computed as 2 and page 1 was never fetched — silent
   * data loss on every paged read. Set this instead of hunting for a negative
   * `page_start` that happens to cancel out. */
  int         page_zero_based;/* 1 = the first page is numbered 0, not 1      */
  int         page_size;      /* records per page, for offset-style paging    */
  int         page_max;       /* max pages to walk (default 10)              */

  int csv_no_header;          /* CSV mode: file has no header row → col0..colN */
  /* CSV mode: field delimiter, if not a comma. DataPlane.org's feeds are
   * `ASN | AS name | ip | lastseen | category`; parsed on commas the whole line
   * became one cell, so five real fields were stored as one blob nothing could
   * query. Named forms, because the manifest is itself pipe-delimited:
   *   "tab" / "\t"  U+0009        "pipe"  '|'        "semi"  ';'
   *   "ws"          a run of blanks — fixed-width text tables such as JPNIC's
   *                 as-numbers.txt (ruler lines of `-----` are skipped)
   *   "lit:<>"      any literal token of any length — 2ch's subject.txt
   * Anything else: its first character. */
  const char *csv_delim;
  /* CSV mode: lines starting with this prefix are comments, not records. Both
   * DataPlane and URLhaus ship a `#` banner (URLhaus puts its column names
   * there), and emitting those as findings would file documentation as
   * intelligence. */
  const char *csv_comment;
  /* Declared body charset, for an endpoint that serves a legacy Japanese
   * encoding. "sjis" / "shift_jis" / "cp932" / "euc-jp" all transcode to UTF-8
   * before the body is parsed.
   *
   * Why this is not inferred from the host. hpengine transcodes a non-UTF-8
   * body automatically when the host ends in `.jp`, which is the gate
   * lib/feedlib.c uses and the one that keeps a Latin-1 European feed from
   * being read as Shift_JIS ("Z\xfcrich" is valid Shift_JIS). But the
   * 2ch-family boards are Japanese sites on foreign TLDs — machi.to is Tonga,
   * open2ch.net is .net — so the host gate correctly refuses them and their
   * titles stored as mojibake. The encoding is a property of the endpoint, not
   * of its TLD, so the row states it. */
  const char *charset;
  int filter_query;           /* 1 = keep only records mentioning the query */
  /* Cap on emitted records. 0 (the default) means EVERY record the upstream
   * returned — the engine does not invent a limit the caller did not ask for.
   * A non-zero value is the row author's explicit choice and is reported in
   * each record's `_records_truncated` marker when it bites. */
  int max_items;
  /* Per-row HTTP timeout in milliseconds; 0 uses the engine default (20 s).
   *
   * Raising the default globally is not the fix: 20 s is right for the great
   * majority of the registry, and multiplying it would make every genuinely
   * dead host hang proportionally longer on every scheduled sweep. But some
   * endpoints are legitimately slow — a WEKO/JAIRO Cloud OAI-PMH repository
   * renders a 100-record ListRecords page in 20-120 s, so all 95 Japanese
   * institutional repositories failed with `transport failure` at the default
   * despite being live and returning records when asked patiently. Slowness is
   * a property of the endpoint, so the row states it. */
  int timeout_ms;
  int free_tier;              /* 1 = usable without payment                 */
  int interval;               /* 0 = on-demand pivot (the norm here)        */

  /* Map layer id (core/layers.def taxonomy), passed straight through to
   * source_def.layer. OPTIONAL AND APPENDED LAST ON PURPOSE: thousands of
   * existing rows initialize this struct with designated initializers and
   * must keep compiling unchanged, so the field defaults to NULL — which
   * means what it always meant: not a map layer (right for an entity-pivot
   * service). Declare it only on a row whose records belong on the map AND
   * whose layer's modality is known; core/layertab.c can also assign a
   * source by id/category match without any change here. */
  const char *layer;
  /* CSV mode: physical lines to drop before the header is read. The MEXT
   * school-code files, Kawasaki's pharmacy licences and Saitama's operator
   * lists all put a title line ABOVE the header; the only way to read them
   * used to be csv_no_header=1, which named every column col0..colN and
   * emitted the title and the header as two junk records per file.
   * Appended after  so no existing initializer moves. */
  int csv_skip_lines;
  /* HP_XLSX: which worksheet. `xlsx_sheet` is an exact tab name; when NULL the
   * 0-based tab index is used (default 0 = first sheet). Appended so no
   * existing initializer moves. */
  const char *xlsx_sheet;
  int         xlsx_sheet_index;
} hp_source;

/* Register `n` rows. `defs` must be static storage of at least n entries owned
 * by the caller (source_def pointers live for the process lifetime). */
void hp_register(const hp_source *specs, int n, source_def *defs);

/* XML text → the characters it denotes, in place: unwraps every CDATA section
 * (payload verbatim), decodes the five predefined entities and numeric
 * character references to UTF-8, leaves anything undecodable literal, never
 * writes a NUL and never grows the string. Shared here so lib/rss_atom.c's
 * hand-written feed parser can call the same decoder the XML record path
 * uses. Text with no '&' and no '<' is untouched. */
void hp_xml_decode(char *s);

/* Declare + register a table in one line at the bottom of a collector file. */
#define HP_REGISTER_TABLE(TBL)                                                \
  static source_def TBL##_defs[sizeof(TBL) / sizeof((TBL)[0])];               \
  __attribute__((constructor)) static void hp_reg_##TBL(void) {               \
    hp_register((TBL), (int)(sizeof(TBL) / sizeof((TBL)[0])), TBL##_defs);    \
  }

#endif
