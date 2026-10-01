# Batch 32 — Japan OSINT sources: authoring brief for research agents

You are authoring ONE beat of batch 32. Read this whole file, then
`/Users/rayan/OSINTsaas/CLAUDE.md` (house rules; rule 1 is absolute: NEVER
fabricate — every row must name an endpoint you fetched and saw real records in).

The goal of batch 32 is **data yield**: sources that hand back many real records
per run (hundreds to tens of thousands), or a steady stream of new ones. A row
that returns 12 records is worth writing only if nothing better exists on that
beat. Prefer the deep endpoint (the full register, the whole layer, the complete
list) over a catalogue listing or a "latest 10" widget.

Machine: macOS, zsh. Repo at `/Users/rayan/OSINTsaas`. `curl` and `python3` are
local. There is no `timeout` command; use `curl --max-time`.

## Output

One file: `/Users/rayan/OSINTsaas/docs/candidate-sources-batch32.<beat>.txt`
Pipe-delimited, 13 fields, one row per line, `#` comment lines allowed, LF
endings, UTF-8. Start the file with 2–4 `#` lines saying what the beat covers
and when it was fetched.

```
id|mode|want|category|record_type|tags|portal|name|name_ja|url|probe|description|opts
```

* `id` — `JO32_<BEAT>_<SHORTNAME>` uppercase ASCII `[A-Z0-9_]`, unique. Must NOT
  appear in `docs/batch32/existing_ids.txt` (17,829 registered ids).
* `mode` — `json` | `csv` | `xml` | `html` | `xlsx` — what the URL body actually is.
  (GeoJSON is `json` with `array_path=features`. RSS/Atom is `xml`.)
* `want` — `any` (these are bulk/scheduled rows, not entity pivots).
* `category` — short slug (e.g. `government`, `infrastructure`, `finance`, `registry`, `geo`).
* `record_type` — what ONE record is, e.g. `npo-corporation`, `traffic-count`, `shelter`.
* `tags` — comma list, include `japan`.
* `portal` — the human landing page (documentation only).
* `name` / `name_ja` — English / Japanese names.
* `url` — the MACHINE-READABLE endpoint the engine fetches. Must NOT appear in
  `docs/batch32/existing_urls.txt` (that file is lower-cased and scheme-stripped:
  compare the same way — `grep -iF 'host/path' docs/batch32/existing_urls.txt`).
  Also check the HOST: `grep -iF 'hostname' docs/batch32/existing_urls.txt` — if
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
python3 tools/probe_hp_batch.py ../docs/candidate-sources-batch32.<beat>.txt --jobs 6 --out ../docs/batch32/probe32_<beat>.tsv
grep -vc PASS ../docs/batch32/probe32_<beat>.tsv; grep -v PASS ../docs/batch32/probe32_<beat>.tsv | head -40
```

   Then the dedup and reachability gates (both must be clean):

```sh
cd /Users/rayan/OSINTsaas/native
python3 tools/batch_exclusions.py --bin ./bin/japanosint --check ../docs/candidate-sources-batch32.<beat>.txt
python3 tools/audit_batch_reachable.py ../docs/candidate-sources-batch32.<beat>.txt
python3 tools/audit_batch_pagination.py ../docs/candidate-sources-batch32.<beat>.txt
```

6. Keep a rejects file `docs/rejected-sources-batch32.<beat>.tsv`
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
* Japan only: `.go.jp`, `.lg.jp`, `.ac.jp`, `.or.jp`, `.co.jp`, `.jp`,
  prefectural/municipal hosts, Japanese utilities and institutions, and
  Japanese public bodies' layers hosted on ArcGIS Online. Not global aggregators.
* When done, reply with: rows written, probe PASS count, rejects count, the
  total records the manifest's rows expose per run (sum of your observed
  counts), the 5 highest-yield rows, and anything the engine could not express.
