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
  /* "Record-jar": `Key: Value` lines, records separated by a line of `%%`,
   * continuation lines indented. This is the format of the IANA registries —
   * the language-subtag registry (RFC 5646 §3.1.2) above all — and it is not
   * CSV, JSON or XML, so before this mode existed the only way to declare such
   * a row was to lie about its shape. IANA_LANGUAGE_SUBTAGS was declared
   * HP_CSV, which made every LINE a "record": 49,312 emitted, 19,461 stored,
   * 29,851 lost per pass, and the rows it did store were fragments like
   * `Description: Afar` rather than subtag records. The real file holds ~9,600
   * subtags. A repeated key inside one record (IANA gives a subtag several
   * `Description:` lines) is JOINED with "; " rather than overwritten, because
   * dropping the later ones would discard real content. */
  HP_RECJAR = 5,
  /* NDJSON / JSON Lines: one complete JSON value per line, no enclosing array
   * and no commas between records. Added because two of the most useful
   * machine-readable feeds in the software-supply-chain space publish only in
   * this shape and were therefore undeclarable:
   *
   *   index.golang.org/index   the GLOBAL Go module publish stream, every
   *                            {Path, Version, Timestamp} since 2019, paged by
   *                            a `since` cursor taken from the last record
   *   index.crates.io/<a>/<b>/<name>
   *                            the crates.io sparse index — one line per
   *                            version with its full dependency list,
   *                            features, yanked flag, checksum and pubtime
   *
   * Declaring either as HP_JSON gets `cJSON_Parse` refusing the body at the
   * second line, so the row emits nothing forever while still registering —
   * the same invisible nothing an EMPTY_RESULTSET source is. Declaring them
   * HP_CSV would make every line one unqueryable cell, which is the mistake
   * IANA_LANGUAGE_SUBTAGS made above.
   *
   * Implemented by rewriting the lines into ONE JSON array and handing that to
   * the HP_JSON path, deliberately rather than as a parallel record loop:
   * array_path, title_keys, id_keys, date_keys, the content-hash collision
   * guard, the detail second hop, record caps, paging and every disclosure
   * notice then apply to an NDJSON row exactly as they do to a JSON one. The
   * cost is holding the body twice while it is converted, which is why this
   * mode is for line-oriented FEEDS and not for bulk dumps.
   *
   * A line that is not valid JSON is a LOST RECORD, not an empty slot: it is
   * counted into `malformed`, reported in the run line, and disclosed as a
   * collector-truncation-notice. Silently skipping it is what would make a
   * feed that half-broke upstream look complete. */
  HP_NDJSON = 6,
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
 *   {ago:<seconds>}  RFC 3339 UTC of (now - seconds). NOT entity-derived: it
 *            is for an upstream whose only ordering is "everything at or
 *            after this instant" and which offers no reverse order and no
 *            "latest" — index.golang.org is exactly that, append-only from
 *            2019-04-10, so `since={ago:7200}` is the difference between the
 *            live publish tail and re-reading 2019 on every run. Expanded
 *            once per run, not per page.
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

  /* endpoint template (required). Besides the entity tokens ({q} …) it takes
   * {date:FORMAT} / {date:FORMAT:±N}: today's UTC date shifted N days, through
   * strftime — for an upstream that answers one day at a time. */
  const char *url;
  const char *post_body;      /* non-NULL → POST with this body template   */
  const char *content_type;   /* NULL → "application/json" when posting    */
  const char *headers[5];     /* extra header templates, NULL-terminated   */
  const char *key_env;        /* required credential env var; NULL = free  */

  /* JSON shaping. All optional: with no array_path the engine finds the
   * densest array of objects itself, and with no *_keys it falls back to a
   * conventional key list — a row whose upstream changed shape degrades to
   * fewer resolved fields, never to invented ones.
   *
   * `array_path = "."` is THE ROOT of the document, and a row whose upstream
   * answers with a bare JSON array should say so rather than leave the choice
   * to discovery. Discovery picks the DENSEST array of objects, which is not
   * the root whenever a root record carries a longer nested array of its own:
   * hex.pm's `?sort=updated_at` returns 100 package objects, each with its own
   * `releases` array, and on a 60-page walk the engine mined `[33].releases`
   * (133 records) instead of the root (100) on 34 of those pages — 9,643
   * records emitted where the 60 pages hold 6,000, keyed on a field the
   * release entries do not have, 2,881 of them collapsing at the sink
   * (measured 2026-10-04, batch 33). Declaring `array_path = "."` took the
   * same row to 6,000 emitted and 6,000 stored.
   *
   * It is reported, not silent — the engine stamps a shape notice naming the
   * array it mined, the runner-up, and the pages it happened on — but the
   * notice is a record in the database, not a build error, so a row that is
   * never read after its first run keeps doing it. */
  const char *array_path;     /* dotted path ("a.b.c"), "." = the root,
                               * "" / NULL = auto-detect; HP_JSON only:
                               * "a+b+c" emits several sibling arrays of one
                               * response (each part resolved as above)     */
  const char *title_keys;     /* comma-separated candidates, first wins     */
  /* Record identity — the uid the sink upserts on. FIRST-MATCH per record:
   * the engine takes the first listed key that resolves to a non-empty value
   * and never looks at the rest, so the ORDER is the declaration. A feed
   * that points several items at one section page (IPA's newsonly-rss.rdf,
   * three items → one /about/ link) collides on `link`, and the sink keeps
   * one of them; declare `guid` or `title` FIRST in that case, and `link`
   * after as the fallback for items that carry neither. The collision guard
   * disambiguates records that merely share a key within one page, but a
   * key that is not the identity is still the wrong key.
   *
   * `+` COMPOSES, `,` CHOOSES. `"a+b+c"` joins all three parts with '|' into
   * one key; `"a,b,c"` picks whichever of the three appears first. They nest:
   * `"a+b,c"` means "a+b, or else c". Use `+` whenever a record's identity is
   * a TUPLE OF DIMENSIONS — one row per country x week x age group, per firm
   * x period x tier — which is most statistical and regulatory tables.
   *
   * This distinction is the single easiest thing to get wrong here, and it
   * fails silently: 26 rows written in the 2026-09 fix pass declared
   * `"year,quarter,item"` MEANING a composite, and got "key on `year`". They
   * looked correct because the in-page collision guard content-hashes rows
   * that collide within ONE page — the same key recurring on a LATER page
   * still collapsed at the sink. FINRA_OTC_BLOCKS emitted 2,000 and stored
   * 476 (= its distinct first-token values) until it became `+`, then 2,000.
   * A missing part joins as empty rather than failing the whole key (a
   * dimension may legitimately be null); at least one part must be present;
   * an over-long composite becomes its FNV hex rather than being truncated,
   * because truncation would manufacture collisions in the very field meant
   * to prevent them. */
  const char *id_keys;
  const char *link_keys;
  const char *link_tmpl;      /* {v} = link_keys value; else the raw value  */
  const char *date_keys;
  const char *body_keys;
  const char *lat_key, *lon_key;
  /* One field holding both coordinates as "a,b" (or "a b"): `latlon_key`
   * reads it as lat,lon; `lonlat_key` as lon,lat. CALIL's library directory
   * ships `geocode: "139.69,35.68"` — 7,606 geocoded branches that
   * lat_key/lon_key, which each name a whole field, could not place. */
  const char *latlon_key, *lonlat_key;

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
   * EVERY list record, bounded operationally by $JO_HP_DETAIL_MAX (default
   * 1,000) and by $JO_HP_DETAIL_SEC of detail fetching per run (default 120;
   * the host gate paces each hop, so time is the honest politeness unit).
   * Records that were not deepened carry `_detail_pending: true`, and the run
   * files ONE collector-truncation-notice counting fetched vs pending — an
   * un-fetched detail is reported, not quietly treated as absent. Every mode
   * deepens (JSON, XML, CSV/XLSX columns, HTML anchors' title/url/id), the hop
   * carries the row's own headers ({key} expanded) and is parsed as JSON. */
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
   * page_max bounds the walk (default jo_page_max(), 20 pages, raised or
   * lowered for the whole tree by $JO_PAGE_MAX — lib/pagewalk.c) so a runaway
   * feed cannot spin forever — when that bound bites it is stamped on every record, never
   * silent. */
  /* Dotted path to the next-page cursor or URL in the response. A segment of
   * `$last` is the final element of an array, for an upstream whose cursor is
   * the last record's own field rather than anything in an envelope: the Go
   * module index publishes no next link and pages by `since=<the Timestamp of
   * the last record it gave you>`, so the row declares
   * `next_path=$last.Timestamp` with a `next_tmpl` that spends it. */
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
  int         page_max;       /* max pages to walk (default jo_page_max())   */
  /* Walk pages the way a VJSON collector is walked, for a row that declares
   * none of next_path / page_param / {page}: the server's own next link, else
   * the cursor its declared page size pairs with, advanced only while pages
   * come back full (jsonlist_next_page — one decision for both engines).
   * Opt-in, and HP_JSON only: it exists so a verified list collector moved onto
   * this engine to walk its detail hop does not lose every page after the
   * first. The ceiling is page_max when set, else jo_page_max() — 20, under
   * $JO_PAGE_MAX or the legacy $JO_JSONLIST_PAGE_MAX. */
  int         page_walk;

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
  /* ── the data URL is published on an index PAGE, not fixed ────────────────
   *
   * Some publishers never serve a stable link: Kawasaki city puts its licence
   * registers at `…/01riyoujo202608.csv` and rolls the stamp monthly (the
   * pharmacy pages use a Japanese era stamp, `yakkyokuR8.8.csv`), and the page
   * says so — "updated monthly", no undated path. A row with the stamp baked
   * into `.url` therefore works until the next refresh and then 404s. Nine
   * rows were repointed by hand on 2026-09-19; all nine had been dead, and one
   * had not merely re-dated but been RENAMED (`03kuri-ninngu` → `03cleaning`),
   * which no date arithmetic would have caught.
   *
   * With `index_url` set, the engine fetches that page FIRST, scans its
   * anchors (lib/htmlparse.c — the one scanner), takes the first href
   * containing `index_href_must`, resolves it against the index URL, and uses
   * the result as the data URL. Everything after that — mode, delimiter,
   * charset, paging, keys — is unchanged, so this is one extra request, not a
   * new code path.
   *
   * `index_href_must` is required alongside it: without a discriminator the
   * first link on a page of nine CSVs is not the one the row wants. If the
   * index cannot be fetched, or no href matches, the row fails honestly (and
   * says which pattern found nothing) rather than falling back to a URL that
   * may be a year stale. Appended last, like `layer` above, so every existing
   * initializer keeps compiling. */
  const char *index_url;
  const char *index_href_must;
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
  /* ── READ THE WHOLE COLLECTION OVER A CYCLE, one run's pages at a time.
   *
   * A page ceiling answers "how many requests may one run make". It does not
   * answer "how is a collection bigger than that ever read", and for 89
   * scheduled rows the honest answer was "it is not": measured 2026-10-12
   * (docs/measured-page-ceilings-2026-10-12.tsv), they declare totals needing
   * 201 to 55,971 pages — 1.93 billion records between them — so every run
   * re-read the same first 20 pages and the rest was disclosed and never
   * fetched. 534 more page past the ceiling without publishing a total at all.
   * Raising page_max does not serve those rows; 55,971 requests a run against
   * one host is not exhaustiveness, it is an outage.
   *
   * With `page_resume = 1` the walk starts where the previous run stopped and
   * stores where this one stopped (source_ctx's resume_in / resume_out, which
   * core/scheduler.c persists). When the walk reaches the end the position is
   * CLEARED, so the next run starts over and the cycle repeats: the whole
   * collection is read over ceiling-sized slices, and each notice says which
   * slice this run read. collectors/sources/wayback_jp.c has done exactly this
   * by hand since before the field existed — mhlw's 2,064-page index over ~207
   * daily runs — and that row is the worked example.
   *
   * It is ignored, with a line on stderr, when:
   *   - the run carries an ENTITY. "What do we have on X" must answer from the
   *     start of X's results, not from page 37 of the last entity asked about.
   *     The scheduler passes no resume on a pivot run for the same reason.
   *   - the URL or body carries a {date…} or {ago:…} token. The stored
   *     position is a URL, so resuming would re-fetch a stale day forever;
   *     those rows are a live tail and already walk forward by date.
   *   - the stored URL's host is not the row's own host. The token comes from
   *     the database, and a reading position is not a licence to fetch
   *     somewhere else.
   * Appended last, like every field above it, so no existing initializer
   * moves. */
  int         page_resume;
  /* Date tokens. `{date}` in the url, post_body or a header is today's date
   * (YYYY-MM-DD); `{date-1d}`, `{date+2w}`, `{date-3m}`, `{date-1y}` shift it;
   * `{date-1d:%Y%m%d}` formats it (strftime); `{date:@s}` / `{date:@ms}` is
   * the epoch instant. Rendered in UTC+tz_minutes (540 = JST, 480 = China).
   * Before these existed, a daily file or a date-window query could only be
   * registered with the day it was authored frozen into it: 31 scheduled rows
   * re-fetched one past week, month or day forever.
   *
   * date_back: when the FIRST request of a dated row answers 4xx or carries no
   * records (a weekend, a holiday, a file not yet published), step every
   * {date…} token back one day and try again, up to N times. Appended so no
   * existing initializer moves. */
  int         tz_minutes;
  int         date_back;
  /* The unit date_back steps in: "d" (default), "w", "m" or "y". A monthly
   * file not yet out steps back a month; EPA's TRI reporting year is loaded
   * months after it ends, so its rows step back a year. */
  const char *date_step;
  /* date_field=<name>: add the date this run's FIRST {date…} token rendered
   * to (after any date_back step) to every record as field <name>, before it
   * is keyed. A dated file whose rows carry no date of their own — SHFE's
   * daily kx file, CZCE's daily table, SZSE's margin balances — otherwise
   * re-keys every day's rows onto the previous day's uids and overwrites
   * them. id_keys can then name it: `id_keys=trade_date+contract`. */
  const char *date_field;
  /* page_delay_ms: pause before every page after the first. A firewall that
   * answers a burst with 403 (the Palace Museum's after 31 pages of a 9,300-
   * page walk; NFRA after 40) ends the walk; a paced walk can finish. */
  int         page_delay_ms;
  /* HP_HTML table mode: records are the rows of the `table_index`-th <table>
   * whose HTML contains `table_must` (lib/htmlparse.c html_table_parse). The
   * first row after `table_skip_rows` names the columns; each later row is a
   * record, with its first link as `link`. Without table_must, html mode is
   * the anchor scanner it always was. A register published as an HTML table
   * (FSA's money lenders, isms.jp's certified organisations, prefectural
   * water levels) was unreadable before: its rows are cells, not anchors. */
  const char *table_must;
  int         table_index;
  int         table_skip_rows;
  /* ── A COLLECTION WHOSE ONLY SLICING AXIS IS A DATE, read over a cycle.
   *
   * `page_resume` above reads a big collection a ceiling of PAGES at a time.
   * That needs the upstream to page at all, and some do not: EBI's ENA portal
   * API answers `{"message":"Unsupported param offset"}` with HTTP 400 to any
   * offset, so `ENA_PATHOGEN_SARSCOV2_RUNS` read 1,000 records of 7,556,500
   * and — because a failed page deliberately stores no position — read the
   * same 1,000 on every run for ever, at rc=0, records=1000, stored=1000.
   * Measured 2026-10-19. No paging flag can reach the other 7.5 million,
   * because the endpoint has no second page to ask for. What it does have is a
   * date predicate: `first_public>=A AND first_public<=B` with `limit=0`
   * returns that window COMPLETE (5,910 records for a 7-day window, equal to
   * the endpoint's own /count for the same query).
   *
   * So the slice is a date window, and `window_days = N` reads one per run:
   *
   *   url = "…&query=tax_tree(2697049) AND first_public>={wstart} AND "
   *         "first_public<={wend}&limit=0"
   *   window_days = 7, window_from = "2020-01-01"
   *
   * `{wstart}` and `{wend}` render like `{date}` (`{wstart:%Y%m%d}` formats
   * with strftime) in the url, post_body, headers and detail_url.
   *
   * THE WALK GOES FORWARD, from `window_from` toward today, and that direction
   * is the whole design rather than a preference. A backward walk from today
   * would not see a record published this morning until it had finished the
   * history — a year of daily runs for ENA — which is the same mistake as
   * cycling a newest-first collection (see the ascending-sort note under
   * page_resume). Walking forward, the cycle CATCHES UP: once a window reaches
   * today the row pins itself to the trailing window and every later run reads
   * the live tail. One row is therefore both the backfill and the live feed,
   * and nothing is ever read a second time except that trailing window, which
   * must be re-read because records land in it continuously.
   *
   * The position is the next window's start, stored through the same
   * source_ctx resume_in / resume_out that page_resume uses, as `w=<ISO date>`
   * — a form no stored URL can collide with, and one a collector that does not
   * recognise it treats as absent, which is the contract source.h documents.
   *
   * window_from is the earliest date worth asking for; a row that leaves it
   * unset starts at the trailing window, i.e. it is a live tail with no
   * backfill. A window that is wider than its own upstream can answer in one
   * response still pages normally inside the window and still discloses what
   * it did not reach — the remedy there is a narrower window, and the notice
   * says so by naming the window it read.
   *
   * ONE WINDOW A RUN, including an empty one, and that is deliberate. Walking
   * several empty windows inside a run was written and then dropped: an empty
   * window costs ~5 bytes, so skipping through sparse history looks free, and
   * it would have shortened ENA's backfill by about a dozen runs out of 354.
   * But an HTTP 200 carrying an empty result set is indistinguishable from a
   * real empty one — the trap `EMPTY_RESULTSET` exists for — so a transient
   * empty answer would have advanced the cycle past up to 24 windows of real
   * records instead of one, in silence. A 3% faster backfill is not worth
   * multiplying a silent loss by 24. A long backfill is a stated trade with
   * precedent here (a monthly `interval` takes a 500-page collection about two
   * years to come round) and the notice says which window it is on.
   *
   * A DATE PREDICATE IS ONLY EXHAUSTIVE WHERE EVERY RECORD IS DATED, and that
   * is the row author's measurement to make, not something the engine can
   * check. ENA's influenza SAMPLE collection holds 61,537 records of which
   * 1,324 carry no first_public at all, so a first_public window reaches
   * 60,213 of them and no setting of window_days would ever find the rest —
   * that row reads its collection whole instead. Ask the upstream for its own
   * count both with and without the predicate before windowing a row.
   *
   * Ignored, with a line on stderr, when the run carries an ENTITY (a pivot
   * answers from the start of that entity's results, exactly as for
   * page_resume) or when no `{wstart}`/`{wend}` token appears in the url or
   * body — there is nothing to slice, and silently reading one window would be
   * indistinguishable from reading the whole collection. Appended last so no
   * existing initializer moves. */
  int         window_days;
  const char *window_from;
} hp_source;

/* Register `n` rows. `defs` must be static storage of at least n entries owned
 * by the caller (source_def pointers live for the process lifetime). */
/* `table` is the registering file's basename without `.c` (HP_REGISTER_TABLE
 * passes __FILE__): it becomes the default `collector` of every row in the
 * table that declares none. It used to be the literal "osint" for all of
 * them, which put 8,445 rows from 183 unrelated tables into one collector —
 * a Japanese media beat, a SCADA beat and a Brazilian registry beat
 * indistinguishable in every client. The beat a row was authored and probed
 * in IS its natural group, and the file name is the one spelling of that
 * beat every gate already uses. A row that names its own collector keeps it.
 * NULL/"" falls back to "osint" (the offline test harness, nothing live). */
void hp_register(const hp_source *specs, int n, source_def *defs,
                 const char *table);

/* Serialise one registered row AS DECLARED, for the source detail API:
 * every non-default field by name, enums as words, plus the derived facts a
 * client would otherwise have to re-derive from the C — `kind`
 * (pivot | scheduled | scheduled-pivot | unreachable), the entity tokens the
 * URL and body use, the paging strategy as one word, whether a detail hop is
 * declared and whether {date…}/{ago:} tokens are rendered. Header templates
 * are shown verbatim: they carry `{env}` tokens, never values. malloc'd JSON,
 * caller frees; NULL for an id this engine does not run. */
char *hp_describe_json(const char *id);
/* The declared row itself (read-only, lives for the process), or NULL when
 * `id` is not an hpengine source. For callers that want one field, not JSON. */
const hp_source *hp_source_get(const char *id);
/* 1 when `id` is an hpengine row whose URL or POST body carries an entity
 * token, or that declares filter_query (a bulk fetch filtered by the entity
 * locally), i.e. it is dispatchable as an entity pivot. 0 for a fixed-endpoint
 * row AND for an id this engine does not run — callers that need to tell
 * those apart ask hp_describe_json(). */
int hp_source_uses_entity(const char *id);

/* The registered row behind source id `id`, or NULL when `id` is not an
 * hpengine row (a hand-written collector). Read-only: for code that
 * describes a row from its declaration (core/source_card.c reads `want` and
 * the entity tokens of `url`/`post_body`). A linear scan; callers that need
 * every row cache the answer. */
const hp_source *hp_spec_get(const char *id);

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
    hp_register((TBL), (int)(sizeof(TBL) / sizeof((TBL)[0])), TBL##_defs,    \
                __FILE__);                                                    \
  }

#endif
