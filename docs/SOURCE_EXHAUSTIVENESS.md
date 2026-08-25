# House rule: a source that is called must be used exhaustively

**If we spend a request on a source, we use everything it gave back.**

This is a hard rule across the whole codebase — collectors, the OSINT
dispatcher, the pipeline, the API layer, the iOS and web clients. It sits
alongside the existing no-fabrication rule (`native/collectors/SOURCE_REALITY_REPORT.md`)
and they point the same way: **the data we show is exactly the data we got —
all of it, and nothing else.**

Fabricating data invents facts that were never fetched. Discarding data throws
away facts that *were* fetched. Both produce a false picture of what is known,
and the second one is worse because it looks clean: no error, no warning, a
plausible-looking result that is missing the record that mattered.

## The rule

1. **Emit every record the response contains.** If an endpoint returns 40 hits,
   40 records reach the sink. No implicit "first N".
2. **Emit every field of every record.** A record is forwarded whole. Do not
   hand-pick three fields and drop the rest; put the full record in
   `properties` and let consumers choose.
3. **Follow pagination to the end.** A paged endpoint read once has discarded
   every page after the first. Walk `next` links / offsets until the upstream
   stops producing records.
4. **Follow the second hop.** If a list record names a detail endpoint that
   carries the substance (officers, roles, filings, ownership), fetch it — for
   every record, not the first three.
5. **Never discard at a seam.** Between collector and sink, sink and API, API
   and client, the payload passes through complete. Capture layers do not get
   to decide what is interesting.
6. **A bound must belong to the consumer, and must be visible.** Some consumers
   genuinely cannot take everything — an LLM prompt has finite context, a mobile
   list renders 50 rows. That is allowed, on two conditions:
   - the full data is still fetched, stored and served, and
   - the bounded view *says so in-band*: how many it is showing, how many exist,
     and that the rest are available. Never a silent slice.
7. **If something was left unused, report it as data, not as a log line.** A log
   nobody reads is not a disclosure. The engine emits a
   `collector-truncation-notice` record naming the source, the query, records
   used, records available and why.
8. **"Available" counts records, not array slots — and a skipped slot says why.**
   A disclosure that reports a shortfall which did not happen is worse than no
   disclosure, because it teaches everyone to ignore the real ones. Measured on
   batch 19 before this was fixed: 95 rows reported a shortfall and essentially
   none had lost anything. 87 were short by exactly one — the trailing newline
   at the end of a CSV. `ECMA_PUBLISHED_STANDARDS` reported 295 of 590, a
   perfect 50% loss that was a perfect 2x duplication (each item linked from
   both its icon and its title). `MALTRAIL_COBALTSTRIKE` reported 22,954 of
   36,002 against a feed with 13,048 comment lines.

   So `hp_run` now subtracts, and names, the four reasons a slot never becomes a
   record — and one of them was previously invisible in every sense:

   | counter | meaning |
   | --- | --- |
   | `empty` | nothing survived flattening (a blank CSV line, a `null` element) |
   | `duplicate` | the same href twice on one page — one record, not a discard |
   | `filtered out` | the row's own `filter_query` excluded it, as asked |
   | `refused by sink` | the store declined it. **A real discard**, and it used to show only as a smaller number with no cause |

   ```
   [hp:ECMA_PUBLISHED_STANDARDS] emitted 295 of 295 available across 1 page(s)
                                 [0 empty, 295 duplicate, 0 filtered out, 0 refused by sink]
   ```

   A non-zero `refused by sink` is the one to chase. The others are accounting.

## What a violation looks like

```c
/* WRONG — 39 of 40 fetched records thrown away */
if (payload) { free(d->cap); d->cap = strdup(payload); }   /* keeps the last */

/* WRONG — arbitrary cap the upstream never asked for */
int max = 25;                     /* why 25? */
cJSON_ArrayForEach(rec, arr) { if (n >= max) break; ... }

/* WRONG — first item only, rest of the array ignored */
cJSON *first = cJSON_GetArrayItem(items, 0);
emit(first);

/* WRONG — three fields kept, the record's other 40 dropped */
cJSON_AddStringToObject(props, "name", jo_sv(rec, "name"));
cJSON_AddStringToObject(props, "city", jo_sv(rec, "city"));

/* WRONG — page 1 only, of 12 pages */
GET /api/companies?q=acme&page=1
```

```c
/* RIGHT — everything, with the whole record preserved */
cJSON_ArrayForEach(rec, arr) {
  cJSON *flat = cJSON_CreateObject();
  hp_flatten(rec, "", flat, 0);        /* every scalar, dotted keys */
  emit_record(flat);
}
/* … and keep walking pages until the upstream runs dry */
```

## How the shared machinery enforces it

| Layer | Guarantee |
|---|---|
| `native/lib/hpengine.c` | Emits every record of every page; `max_items` defaults to *no cap*; flattens every scalar (bounds are memory guards at 2048 keys / 256 array members / depth 8 and stamp `_fields_dropped` / `_array_truncated` when they bite); walks `next_path` / `page_param` pagination; second hop deepens every record up to `$JO_HP_DETAIL_MAX` (default 25) and stamps `_detail_pending` / `_detail_error` otherwise; emits a `collector-truncation-notice` record if anything was left unused |
| `native/core/osint_dispatch.c` | Captures **every** emitted record — `data = {"record_count":N,"records":[…]}`. (It previously kept only the last payload, so a 40-record service handed 1 record to Phase-2, the synthesis prompt and the API.) |
| `native/core/pipeline.c` | Stores and serves all records; the LLM prompt gets a labelled view via `results_view_for_prompt()` — `records_shown`, `record_count`, `prompt_truncated` and a note that the rest are persisted. Bound size: `$JO_PROMPT_RECORDS_PER_SERVICE` (default 8) |
| `native/core/intel.c` | Upserts every emitted item; `properties` is stored verbatim |
| `native/lib/htmlparse.c` | `html_anchor_next()` + the growable `seen_set` are THE anchor scanner and dedupe for the whole tree (both `jo_emit_anchors` and the engine's HP_HTML rows). `jo_emit_anchors(max<=0)` means every matching anchor; a caller-imposed cap logs both numbers and emits a truncation notice |
| `native/lib/pagewalk.c` | The paging + disclosure engine behind the generated `VJSON`/`VGEO`/`VCSV` collectors. Continues a walk ONLY where the upstream said how — a next link in the response envelope, or an offset/page parameter the collector's own URL already carries — and never invents a query parameter. Whatever it cannot legitimately reach is emitted as a `collector-truncation-notice`. Bounds: `$JO_PAGE_MAX` (default 20 pages); `JO_PAGE_WALK=0` restores single-fetch behaviour and **keeps** the disclosure. **It is the only page walk in the tree** — `jsonlist_emit_paged()` is an adapter onto it, not a second implementation, because two engines answering "is there more?" differently is how a truncation notice becomes a false claim |
| `native/lib/jsonlist.c` | The JSON-array-of-records emitter, and the jsonlist-shaped door onto the walk above. `jsonlist_emit_ex()` reports records **seen** as well as emitted, which is what the walk's "did this page come back full" test reads: driving that off the emitted count meant a full page holding two unlabelled records looked short, so the walk stopped AND suppressed its own notice. A shortfall no caller claimed is disclosed here instead |
| `native/lib/jocore.h` | `jo_truncation_notice()` / `jo_truncation_notice_ex()` — **the** builder for `collector-truncation-notice`, used by every emitter in the tree (hand-written collectors, `lib/hpengine.c`, `lib/pagewalk.c`, `_jp_osint.inc`, `diet_records.c`), so the record has one record_type, one uid convention (`<source_id>\|truncation:<query>`), one tag set (`["truncation-notice"]`) and one shape. Base properties: `source_id`, `query`, `records_used`, `records_available`, `reason`, `remedy`. Pass `available = -1` when the upstream did not state a total — it publishes as `"records_available": null`, never 0 and never a missing key; a guessed total is a rule-1 violation. The `_ex` form takes an `extra` object whose members are merged alongside the base six, for facts only one caller can know (`url`, `pages_read`, `records_dropped`, `more_pages_pending`, `declared_max_items`, `declared_max_pages`, `next_record_position`, `window_from`/`window_until`); a member colliding with a base key is ignored, so the stable half cannot be redefined |
| `native/lib/seenset.c` | One growable "already seen" set. Fixed-size dedupe rings were a recurring violation: `char *seen[500]` stops collecting once full, so a domain with 600 certificates silently lost 100 |

## Checking your work

```sh
cd native
make audit-sources     # scans every collector for discard patterns
make hptest            # engine-level guarantees, offline
```

`make audit-sources` reports, per file, the patterns that usually mean discarded
data: hardcoded record caps, `break` in a record loop, first-element-only access,
single-page fetches of paged APIs, and fixed dedupe rings.

**`make audit-sources` gates the `hp*_*.c` engine rows strictly, and those are at
zero findings.** The wider tree is not: the same run scans 1,211 files and
reports 66 heuristic findings across 50 of them. They are heuristics that each
need a human read, not proven violations — but do not read a passing
`audit-sources` as "nothing is being discarded". Note also what the scan cannot
see: it greps C control flow, so a discard expressed as a *string literal* — a
URL with `limit=20` and no pagination — is invisible to it. That class was 2,727
generated sources until `lib/pagewalk.c` (above) took it on.

The progress that has been made was by fixing, not by silencing: arbitrary per-loop emit caps were deleted,
paged endpoints (OpenPLZ, Etherscan, grep.app, arXiv, NZ Companies Office, UK
Electoral Commission) now walk their pages, fixed dedupe rings became growable
sets, and multi-valued fields that were cut to their first element now carry the
whole array alongside the display pick (`titles_all`, `institutions_all`,
`addresses_all`, `ciks_all`, `software_all`, `references_all`, …).

Every deliberate exception carries an inline `/* exhaustive-ok: <reason> */`
marker, so `grep -rn exhaustive-ok` lists all of them with their justification.
Legitimate exceptions are: fixed-shape tuples (GeoJSON `[lon,lat]`, RDAP jCard
`[name,params,type,value]`, an OpenSky state vector), CSV header rows,
resolution steps that feed another call (name→coordinates, name→QID, ticker→CIK),
1-element response envelopes, memory guards whose overrun is stamped on the
record, and page-walk runaway guards.

## When you genuinely cannot take everything

Say so, in the data:

- the response is a 2 GB bulk file → stream it and record how far you got
- the API caps at 1000 results for a common query → record `records_available`
  and the cap
- an LLM prompt cannot hold 400 records → bound the *view*, keep the store, and
  label the view

The test is simple: **could a reader of the output tell that something was left
out?** If not, it is a violation.
