# Batch 39 — Japan and China OSINT sources: authoring brief for research agents

You are authoring ONE beat of batch 39. Read this whole file, then
`/Users/rayan/OSINTsaas/CLAUDE.md` (house rules; rule 1 is absolute: NEVER
fabricate — every row must name an endpoint you fetched and saw real records in).

The goal of batch 39 is **data yield**: sources that hand back many real records
per run (hundreds to tens of thousands), or a steady stream of new ones. A row
that returns 12 records is worth writing only if nothing better exists on that
beat. Prefer the deep endpoint (the full register, the whole layer, the complete
list) over a catalogue listing or a "latest 10" widget.

Machine: macOS, zsh. Repo at `/Users/rayan/OSINTsaas`. `curl` and `python3` are
local. There is no `timeout` command; use `curl --max-time`.

## Output

One file: `/Users/rayan/OSINTsaas/docs/candidate-sources-batch39.<beat>.txt`
Pipe-delimited, 13 fields, one row per line, `#` comment lines allowed, LF
endings, UTF-8. Start the file with 2–4 `#` lines saying what the beat covers
and when it was fetched.

```
id|mode|want|category|record_type|tags|portal|name|name_ja|url|probe|description|opts
```

* `id` — `JO39_<BEAT>_<SHORTNAME>` uppercase ASCII `[A-Z0-9_]`, unique. Must NOT
  appear in `docs/batch39/existing_ids.txt` (18,267 registered ids).
* `mode` — `json` | `csv` | `xml` | `html` | `xlsx` — what the URL body actually is.
  (GeoJSON is `json` with `array_path=features`. RSS/Atom is `xml`.)
* `want` — `any` (these are bulk/scheduled rows, not entity pivots).
* `category` — short slug (e.g. `government`, `infrastructure`, `finance`, `registry`, `geo`).
* `record_type` — what ONE record is, e.g. `npo-corporation`, `traffic-count`, `shelter`.
* `tags` — comma list, include `japan` or `china` (plus `hong-kong`/`macau` on cnsar).
* `portal` — the human landing page (documentation only).
* `name` / `name_ja` — English / Japanese names.
* `url` — the MACHINE-READABLE endpoint the engine fetches. Must NOT appear in
  `docs/batch39/existing_urls.txt` (that file is lower-cased and scheme-stripped:
  compare the same way — `grep -iF 'host/path' docs/batch39/existing_urls.txt`).
  Also check the HOST: `grep -iF 'hostname' docs/batch39/existing_urls.txt` — if
  the host is already there, make sure your endpoint is genuinely different data,
  not the same list through a second URL. Non-ASCII in URLs is allowed (the
  engine percent-encodes) but prefer percent-encoding it yourself.
* `probe` — same as url unless a cheaper proof-of-life URL exists.
* `description` — 2–4 sentences of what YOU OBSERVED: record count seen, fields
  present, date of newest record, what an analyst gets. No boilerplate, no
  guesses. Do not use the `|` character inside any field.
* `opts` — `k=v;k=v`. Escape a literal `;` inside a value as `\;`.

### opts vocabulary (anything else is rejected by the generator)

String: `array_path title_keys id_keys link_keys link_tmpl charset next_tmpl
csv_delim csv_comment xlsx_sheet date_keys body_keys lat_key lon_key latlon_key
lonlat_key href_must base detail_url detail_key detail_path next_path page_param
post_body content_type key_env type collector`
Integer: `detail_max page_start page_size page_max max_items timeout_ms
page_zero_based csv_skip_lines xlsx_sheet_index filter_query csv_no_header
free_tier interval`
Headers: `header1=Name: value` (`header2`, `header3`).
Doc-only: `pagination_ok=<measured reason>`.

* **`interval=<seconds>` is REQUIRED on every row** (house rule 3). 600–3600 for
  live telemetry, 86400 for daily lists, 604800 for weekly/static registers.
* `array_path` — dotted path to the record array in JSON/XML (`result.records`,
  `features`, `channel.item`, `entry`). Declare it; the probe judges the
  declared path. For a bare top-level JSON array omit it.
* `title_keys` / `id_keys` — comma = CHOOSE the first present; `+` = COMPOSE
  (`station_id+observed_at`). Identity must be unique PER RECORD, not per group.
  A facility list keys on the facility id; a time series keys on
  `station+timestamp`. **Check it**: count distinct values of your id_keys over
  the page you fetched (a 3-line python snippet) and say so in the description.
  Duplicate ids = records silently collapse at the sink (CLAUDE.md rule 4b).
  For CSV, keys are the header names exactly as written (Japanese is fine).
* `date_keys`, `lat_key`/`lon_key` when present — declare them.
* Paging: `page_param=<name>` + `page_size` + `page_max`, or `next_path`
  (`links.rel=next.href` form) + `next_tmpl` with `{v}`. `page_zero_based=1` for
  0-based offsets. If the endpoint truly returns everything in one response, say
  so: `pagination_ok=<what you measured>`. `page_max` must be large enough to
  reach the END of the data you measured (it is a ceiling, not a sample size).
* ArcGIS FeatureServer/MapServer layers: `…/query?where=1%3D1&outFields=*&outSR=4326&f=json&resultRecordCount=1000&resultOffset=0`
  with `array_path=features;id_keys=attributes.OBJECTID;title_keys=attributes.<name field>;lat_key=geometry.y;lon_key=geometry.x;page_param=resultOffset;page_size=1000;page_zero_based=1;page_max=<ceil(count/1000)+2>`.
  Check the layer's `maxRecordCount` (`…/FeatureServer/0?f=json`) and use it as
  the page size if smaller than 1000; get the count with `returnCountOnly=true`.
  Use `f=geojson` + `array_path=features;id_keys=id` only if you checked `id` is
  present. Polygon layers have no geometry.x/y — omit lat/lon for them.
* CKAN datastore: `…/api/3/action/datastore_search?resource_id=<id>&limit=1000&offset=0`
  → `array_path=result.records;id_keys=_id;page_param=offset;page_size=1000;page_zero_based=1`.
  A CKAN `package_search` catalogue listing is NOT what this batch wants.
* RSS/Atom: `mode=xml;array_path=channel.item` (RSS 2.0) or `array_path=item`
  (RSS 1.0/RDF) or `array_path=entry` (Atom); `title_keys=title;id_keys=guid,link,id;date_keys=pubDate,dc:date,updated,published`.
* CSV: header row names become keys. `csv_delim=tab|semi|pipe` when not comma.
  `csv_skip_lines=N` to skip a title banner above the header. Shift_JIS/CP932
  files (very common on .go.jp / .lg.jp): `charset=shift_jis`. Check with
  `curl -s URL | head -c 400 | iconv -f cp932 -t utf-8`.
* XLSX: `mode=xlsx`, `xlsx_sheet=<exact tab name>` or `xlsx_sheet_index=N`,
  `csv_skip_lines=N` for banner rows above the header.
* HTML page listing links: `mode=html;href_must=<substring every wanted link contains>;base=https://host`
  — the engine emits one record per matching anchor. Fallback only, when there
  is no feed/API/CSV; verify the anchors exist in the fetched HTML.
* `timeout_ms=60000` for slow government hosts.
* `header1=User-Agent: JapanOSINT/1.0` when a WAF rejects the default UA
  (honest self-identification is fine; never impersonate a browser).

## Verification you MUST do per row (in this order)

1. **Fetch the exact `url` with curl.** Read the body. Confirm it is the declared
   mode, contains ≥1 real record with the fields you name in
   `title_keys`/`id_keys`, and is not an error page, empty set, login wall or bot
   challenge. Note the record count (and the total the upstream says exists).
2. If paged, fetch page 2 (or the next link) and confirm it differs from page 1.
3. Check id uniqueness over the records you fetched (see id_keys above).
4. Write the row from what you saw. The description cites observed numbers.
5. After every ~20 rows, run the project's probe on your manifest and fix or
   drop failures:

```sh
cd /Users/rayan/OSINTsaas/native
python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch39.<beat>.txt --jobs 6 --out ../docs/batch39/probe39_<beat>.tsv
grep -vc PASS ../docs/batch39/probe39_<beat>.tsv; grep -v PASS ../docs/batch39/probe39_<beat>.tsv | head -40
```

   Then the dedup and reachability gates (both must be clean):

```sh
cd /Users/rayan/OSINTsaas/native
python3 tools/batch_exclusions.py --bin ./bin/japanosint --check ../docs/candidate-sources-batch39.<beat>.txt
python3 tools/audit_batch_reachable.py ../docs/candidate-sources-batch39.<beat>.txt
python3 tools/audit_batch_pagination.py ../docs/candidate-sources-batch39.<beat>.txt
```

6. Keep a rejects file `docs/rejected-sources-batch39.<beat>.tsv`
   (`id<TAB>url<TAB>reason`) for every candidate you fetched and dropped.
   Rejects are data.

Do NOT run `make`, do NOT edit anything under `native/`, and do NOT run
`gen_hp_batch.py` — several beats run at once and the build is done centrally
afterwards (generation, compile, and the emit/store audit through the real
binary). A row that probes PASS can still emit nothing if its title/id keys are
wrong, so get them right from the record you actually looked at.

## Rules of engagement

* Real fetch or nothing. A row you could not fetch does not go in the manifest.
* No API keys. A row that needs a key/appId/token you do not have is a reject.
* Do NOT impersonate a browser User-Agent to get past a bot wall.
* Do NOT spawn sub-agents. Do the work yourself with WebSearch/WebFetch/curl.
* Prefer machine-readable endpoints (JSON/XML/CSV/XLSX/RSS/ArcGIS/CKAN
  datastore). HTML anchor scraping is the fallback, not the default.
* Quality over count. Stop when you run out of genuine sources; report the true
  number. Do not pad with near-duplicates (the same list via a second URL, the
  same feed in two formats).
* Scope is set by your beat (below): Japanese beats stay Japanese (`.go.jp`,
  `.lg.jp`, `.ac.jp`, `.or.jp`, `.co.jp`, `.jp`, prefectural/municipal hosts,
  Japanese utilities and institutions, Japanese public bodies' ArcGIS Online
  layers); Chinese beats stay Chinese (PRC bodies, exchanges, institutions and
  Chinese data companies; the `cnsar` beat is Hong Kong and Macau). Not
  global aggregators that merely mention the country.
* **Host diversity.** No more than 8 rows from one host. A data centre with
  hundreds of report names (Eastmoney `datacenter-web`, a CKAN portal, one
  ArcGIS org) is ONE host: pick its deepest and most analyst-relevant tables,
  not 40 variations of the same one.
* When done, reply with: rows written, probe PASS count, rejects count, the
  total records the manifest's rows expose per run (sum of your observed
  counts), the 5 highest-yield rows, and anything the engine could not express.

## Batch 39 specifics — sources unlocked by four new engine mechanisms

Batch 39 is not a discovery batch. Every candidate was REJECTED by an earlier
batch because the engine or the probe could not express it. Those mechanisms
now exist (2026-10-06), the probe (`native/tools/probe_hp_batch.py`) verifies
all of them the way the engine runs them, and the generator accepts the opts.
Your job: take each candidate on your list, build the row with the new
mechanism, fetch it, prove it, and write it — or reject it with the measured
reason (a candidate that still cannot be proven is a reject, not a guess).
There is no row target: ship every candidate that genuinely works.

### 1. Date tokens (url, post_body, headers)

* `{date}` = today, `YYYY-MM-DD`. `{date-1d}`, `{date+2w}`, `{date-3m}`,
  `{date-1y}` shift it (units d w m y; months clamp to month end).
* `{date-1d:%Y%m%d}` — any strftime format after the colon.
  `{date:@s}` / `{date:@ms}` = epoch seconds / milliseconds.
* `tz_minutes=<int>` renders the date in UTC+N minutes: **540 for Japan,
  480 for China and Hong Kong**. Default 0 (UTC). Always set it: an exchange
  file dated by its local trading day must use the local date.
* `date_back=<N>`: if the FIRST request answers 4xx or carries no records
  (weekend, holiday, file not yet published), every `{date…}` token steps back
  one unit and the request is retried, up to N times. `date_step=d|w|m|y`
  (default d) is the unit — `m` for a monthly file, `y` for annual data.
* Rows with date tokens are scheduled rows: `interval` as usual.
* `date_field=<name>` (added mid-batch): stamps every record with the date the
  url's FIRST `{date…}` token rendered to (after any step back), as field
  <name>, before keying. Use it when a dated file's rows carry no date of
  their own, and put it in `id_keys` — otherwise each day's rows land on the
  previous day's uids. The probe treats the field as present.
* Write the token in BOTH `url` and `probe` (the probe expands them).

### 2. JSONP — automatic

`cb({...})`, `null([...])`, `({...})`, a callback behind an empty block
comment, and the `)]}'` XSSI guard are unwrapped before parsing, in the engine
and in the probe. Nothing to declare: just use `mode=json` and the array_path
INSIDE the wrapper (e.g. `null([{"content":[...]}])` -> `array_path=0.content`).

### 3. POST bodies, now verifiable

`post_body=<body>` + `content_type=Content-Type: application/x-www-form-urlencoded`
(form) or the default `Content-Type: application/json`. The probe now POSTs.
Escape a literal `;` in an opts value as `\;`; never use `|` in any field.
Entity tokens (`{q}`, `{Q}`) work in bodies; batch 39 rows should normally be
scheduled (no entity) unless the source is genuinely a pivot.

### 4. Paging inside the body or the path

`{page}` `{offset}` `{offset1}` `{offset_end}` `{size}` are re-expanded for
every page, in the url AND the post_body:
* `{page}` = page number: starts at `page_start`, else 1, or 0 with
  `page_zero_based=1`.
* `{offset}` = 0-based record offset = page_index * page_size;
  `{offset1}` = offset+1; `{offset_end}` = offset+page_size-1 (inclusive);
  `{size}` = page_size.
* Declare `page_size` (required for {offset…}) and `page_max` (a ceiling that
  must reach the end of the data you measured). Do NOT also declare
  `page_param`. The walk stops on an empty page, a 4xx past the end, or a
  page byte-identical to the previous one.
* Example (body): `post_body={"pageNum":{page},"pageSize":100}`
* Example (path): `url=…/rows/{offset}:{offset_end}/JSON;page_size=2000`

### 4b. HTML tables as records (added 2026-10-06)

`mode=html` + `table_must=<text>` reads a `<table>` instead of anchors: the
`table_index`-th (default 0) table whose raw HTML contains `table_must`
(a caption or header word that identifies it). The first row after
`table_skip_rows` (default 0) names the columns; every later row is a record
keyed by those names. An empty header becomes `colN`, and a repeated one
becomes `name_2` and `name_3`. `colspan` leaves the extra slots empty, and `rowspan`
carries the value down. Entities are decoded and `<br>` becomes a space. A table
nested in a cell is that cell's text. The row's first link becomes `link`,
resolved against `base` or the page. All-empty rows and repeats of the header
row are dropped. `title_keys` / `id_keys` / `date_keys` name header texts
EXACTLY as rendered (check the probe's `header=` note). Works with `charset`,
`post_body`, `{page}` paging, date tokens and `date_field`. The probe verifies
it the same way and reports `KEY_MISSING` if a key is not a header.

### 5. Also available: `index_url` + `index_href_must`

For a file re-published under a new name each edition when the name is NOT
predictable from the date: the engine fetches `index_url`, takes the first
`<a href>` containing `index_href_must`, and reads that. Prefer date tokens
when the name IS predictable.

### Ids, files, gates

* Beats and prefixes: `cn39` → `JO39_CN_`, `jp39` → `JO39_JP_`.
* Output: `docs/candidate-sources-batch39.<beat>.txt`, rejects in
  `docs/rejected-sources-batch39.<beat>.tsv`, probe output in
  `docs/batch39/probe39_<beat>.tsv`. Scratch only under
  `docs/batch39/<beat>_scratch/` and DELETE downloaded upstream data from it
  before you finish (keep scripts).
* Every row: probe PASS, batch_exclusions clean against
  `docs/batch39/existing_ids.txt` / `existing_urls.txt`, reachable,
  pagination audit clean, `id_keys` measured unique per record.
* Host diversity cap: 8 rows per host.
* Do NOT edit anything under `native/`, run make or gen_hp_batch.py, touch
  git, or spawn sub-agents. If an EXISTING registered row should be replaced
  by your new row (same data, now working), say so in your report with its id;
  do not edit it.
