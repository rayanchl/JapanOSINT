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
| `native/lib/hpengine.c` | Emits every record of every page; `max_items` defaults to *no cap*; flattens every scalar (bounds are memory guards at 2048 keys / 256 array members / depth 8 and stamp `_fields_dropped` / `_array_truncated` when they bite); walks `next_path` / `page_param` pagination, **and rows that declare neither are walked by `lib/pager.c` on the upstream's own evidence** (this was one request, full stop, which meant a row moved here to gain a detail hop paid for it with every later page); second hop deepens every record up to `$JO_HP_DETAIL_MAX` (default 25) and stamps `_detail_pending` / `_detail_error` otherwise; emits a `collector-truncation-notice` record if anything was left unused |
| `native/core/osint_dispatch.c` | Captures **every** emitted record — `data = {"record_count":N,"records":[…]}`. (It previously kept only the last payload, so a 40-record service handed 1 record to Phase-2, the synthesis prompt and the API.) |
| `native/core/pipeline.c` | Stores and serves all records; the LLM prompt gets a labelled view via `results_view_for_prompt()` — `records_shown`, `record_count`, `prompt_truncated` and a note that the rest are persisted. Bound size: `$JO_PROMPT_RECORDS_PER_SERVICE` (default 8) |
| `native/core/intel.c` | Upserts every emitted item; `properties` is stored verbatim |
| `native/lib/htmlparse.c` | `html_anchor_next()` + the growable `seen_set` are THE anchor scanner and dedupe for the whole tree (both `jo_emit_anchors` and the engine's HP_HTML rows). `jo_emit_anchors(max<=0)` means every matching anchor; a caller-imposed cap logs both numbers and emits a truncation notice |
| `native/lib/seenset.c` | One growable "already seen" set. Fixed-size dedupe rings were a recurring violation: `char *seen[500]` stops collecting once full, so a domain with 600 certificates silently lost 100 |
| `native/lib/pager.c` | **The** page walk, shared by `jsonlist_emit_paged()` and `hp_run()` so a row moved between the two engines keeps it. Advances only on upstream evidence: a next link it published; a cursor whose page-size sibling is in the URL and whose page came back exactly full; a house-named page size (`rp`, `itemsPerPage`) **proven** by equalling the record count; or, when the URL declares no page size at all, its own declared total saying records remain. Never a page that was never offered — and never an offset cursor without a real stride |
| `native/lib/truncnotice.c` | One emitter for the `collector-truncation-notice` shape, so a consumer does not have to recognise five hand-rolled variants. `records_available` is reported as *unknown* when the upstream declared no total, because an unread remainder and no remainder are different facts |

## Checking your work

```sh
cd native
make audit-sources     # scans every collector for discard patterns
make hptest            # engine-level guarantees, offline
```

`make audit-sources` reports, per file, the patterns that usually mean discarded
data: hardcoded record caps, `break` in a record loop, first-element-only access,
single-page fetches of paged APIs, and fixed dedupe rings.

**Where it actually stands: 0 findings in the strict gated set (159 files), and
109 findings across 68 of 1,523 files in the rest of the tree** — 40
`first-only`, 32 `record-cap`, 26 `loop-break`, 11 `single-page`. `limit-one`,
`dedupe-ring` and `loop-cap` are at zero.

This paragraph used to read "the tree is currently at zero findings across all
685 scanned files", and by the time anyone noticed, the tree had grown to 1,523
scanned files with 168 findings in them. A number in a document is a claim with a
date on it; treat an undated one as expired. `make audit-sources` prints the
current figures in two seconds, and the strict set is the only part the Makefile
holds at zero.

Two things that made the number itself misleading, both now fixed in the tool:

* **It did not know what the engines do.** `?page=1&per_page=100` is walked by
  `jsonlist_emit_paged`, but the check saw `page=1` in a string and called it a
  discard — 21 rows of false alarm, which is how the real findings underneath got
  ignored. It now asks whether the row is on a paging engine AND whether
  `lib/pager.c` can actually move that URL.
* **`--strict` silently kept only the last glob** when given two, printing
  "0 findings" for a set it never opened.
* **`loop-cap` did not exist**, so a bound written in a loop's own condition was
  invisible to the audit entirely: `while (cJSON_GetArraySize(akas) < 24 && …)`
  is neither a `#define` nor a `break`. That one was dropping a sanctioned
  person's aliases past the 24th. Twelve more surfaced the moment the check
  existed, and all thirteen are fixed. If you are about to bound a record loop, the check will find it — put
  the bound where a reader can see it and disclose it when it bites.

An `exhaustive-ok` marker is read **per line** and must sit on the flagged line
itself; one on the line above does nothing.

Where the tree got clean, it got there by fixing, not by silencing: arbitrary
per-loop emit caps were deleted,
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
