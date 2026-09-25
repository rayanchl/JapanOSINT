# Batch 30 — Japan OSINT sources: authoring brief for research agents

You are authoring ONE beat of batch 30. Read this whole file, then
`C:\Users\rayan\sources\repos\OSINTsaas\CLAUDE.md` (house rules; rule 1 is
absolute: NEVER fabricate — every row must name an endpoint you fetched and
saw real records in).

## Output

One file: `C:\Users\rayan\sources\repos\OSINTsaas\docs\candidate-sources-batch30.<beat>.txt`
Pipe-delimited, 13 fields, one row per line, `#` comment lines allowed, LF endings, UTF-8.

```
id|mode|want|category|record_type|tags|portal|name|name_ja|url|probe|description|opts
```

* `id` — `JO30_<BEAT>_<SHORTNAME>` uppercase ASCII, unique. Must NOT appear in
  `docs/batch30/existing_ids.txt` (16,702 registered ids).
* `mode` — `json` | `csv` | `xml` | `html` | `xlsx` — what the URL body actually is.
* `want` — `any` (use this; these are bulk/scheduled rows, not entity pivots).
* `category` — short slug (e.g. `library`, `webcam`, `infrastructure`, `finance`, `government`).
* `record_type` — what one record is, e.g. `library-catalogue-record`, `camera-feed`, `dam-reading`.
* `tags` — comma list, include `japan`.
* `portal` — the human landing page (documentation only).
* `name` / `name_ja` — English / Japanese names.
* `url` — the MACHINE-READABLE endpoint the engine fetches. Must NOT appear in
  `docs/batch30/existing_urls.txt`. Non-ASCII allowed (the engine percent-encodes).
* `probe` — same as url unless a cheaper proof-of-life URL exists.
* `description` — 2–4 sentences of what YOU OBSERVED: record count seen, fields
  present, date of newest record, what an analyst gets. No boilerplate, no guesses.
* `opts` — `k=v;k=v`. Escape a literal `;` inside a value as `\;`.

### opts vocabulary (anything else is rejected by the generator)

String: `array_path title_keys id_keys link_keys link_tmpl charset next_tmpl csv_delim
csv_comment xlsx_sheet date_keys body_keys lat_key lon_key href_must base detail_url
detail_key detail_path next_path page_param post_body content_type key_env type collector`
Integer: `detail_max page_start page_size page_max max_items timeout_ms page_zero_based
csv_skip_lines xlsx_sheet_index filter_query csv_no_header free_tier interval`
Headers: `header1=Name: value` (`header2`, `header3`).
Doc-only: `pagination_ok=<measured reason>`.

* **`interval=<seconds>` is REQUIRED on every row** (house rule 3). 3600 for live
  camera/telemetry lists, 86400 for daily, 604800 for weekly catalogues.
* `array_path` — dotted path to the record array in JSON/XML (`result.records`, `features`, `channel.item`, `record`). Declare it; the probe judges the declared path.
* `title_keys` / `id_keys` — comma = CHOOSE first present; `+` = COMPOSE (`station_id+observed_at`). Identity must be unique PER RECORD, not per group. A camera list keys on the camera id; a time series keys on `station+timestamp`.
* `date_keys`, `lat_key`/`lon_key` when present — cameras and stations have coordinates, declare them.
* Paging: `page_param=<name>` + `page_size` + `page_max`, or `next_path` (`links.rel=next.href` form) + `next_tmpl` with `{v}`. `page_zero_based=1` for 0-based APIs. If the endpoint truly cannot page, say so: `pagination_ok=<what you measured>`.
* ArcGIS FeatureServer/MapServer layers: `…/query?where=1%3D1&outFields=*&f=json&resultRecordCount=1000&resultOffset=0` with `array_path=features;id_keys=attributes.OBJECTID;page_param=resultOffset;page_size=1000;page_zero_based=1;page_max=200` — check `maxRecordCount` on the layer first.
* CKAN: `…/api/3/action/package_search?rows=100&start=0` → `array_path=result.results;id_keys=id;title_keys=title;page_param=start;page_size=100;page_zero_based=1`.
* RSS/Atom: `mode=xml;array_path=channel.item` (RSS) or `array_path=entry` (Atom); `title_keys=title;id_keys=guid,link,id;date_keys=pubDate,updated,published`.
* HTML page listing links: `mode=html;href_must=<substring every wanted link contains>;base=https://host` — the engine emits one record per matching anchor. Only use when there is no feed/API; verify the anchors exist in the fetched HTML.
* Shift_JIS / EUC-JP pages: `charset=shift_jis` or `charset=euc-jp`.
* `timeout_ms=60000` for slow government hosts.

## Verification you MUST do per row (in this order)

1. **WebFetch the exact `url`.** Read the body. Confirm it is the declared mode,
   contains ≥1 real record with the fields you name in `title_keys`/`id_keys`, and
   is not an error page, empty set, login wall or bot challenge. Note the count.
2. If paged, fetch page 2 (or the next link) and confirm it differs from page 1.
3. Write the row from what you saw. Description cites observed numbers.
4. After every ~25 rows, run the project's probe on your manifest and fix or drop failures:

```
wsl.exe -d Ubuntu-24.04 -- bash -s <<'EOF'
cd ~/jolive/native
python3 tools/probe_hp_batch.py /mnt/c/Users/rayan/sources/repos/OSINTsaas/docs/candidate-sources-batch30.<beat>.txt --jobs 6 --out ~/probe30_<beat>.tsv
grep -vc PASS ~/probe30_<beat>.tsv; grep -v PASS ~/probe30_<beat>.tsv | head -40
EOF
```

   Dedup check (ids and endpoints against the whole registry):
```
wsl.exe -d Ubuntu-24.04 -- bash -s <<'EOF'
cd ~/jolive/native
python3 tools/batch_exclusions.py --bin ./bin/japanosint --check /mnt/c/Users/rayan/sources/repos/OSINTsaas/docs/candidate-sources-batch30.<beat>.txt
python3 tools/audit_batch_reachable.py /mnt/c/Users/rayan/sources/repos/OSINTsaas/docs/candidate-sources-batch30.<beat>.txt
EOF
```

5. Keep a rejects file `docs/rejected-sources-batch30.<beat>.tsv` (`id<TAB>url<TAB>reason`)
   for every candidate you fetched and dropped. Rejects are data.

## Rules of engagement

* Real fetch or nothing. A row you could not fetch does not go in the manifest.
* Do NOT impersonate a browser User-Agent to get past a bot wall or key
  requirement. `header1=User-Agent: JapanOSINT/1.0` (honest self-identification)
  is fine when a WAF rejects the default UA.
* Do NOT spawn sub-agents. Do the work yourself, sequentially, with WebSearch/WebFetch.
* Prefer machine-readable endpoints (JSON/XML/CSV/RSS/ArcGIS/CKAN). HTML anchor
  scraping is the fallback, not the default.
* Quality over count. 80 real rows beat 110 with 30 padded. Stop when you run out
  of genuine sources; report the true number.
* Japan only. `.go.jp`, `.lg.jp`, `.ac.jp`, `.or.jp`, `.co.jp`, prefectural/municipal
  hosts, Japanese utilities and institutions. Not global aggregators.
* When done, reply with: rows written, probe PASS count, rejects count, the 5
  highest-value rows, and anything the engine could not express (so it can be fixed).
