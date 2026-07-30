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
| `native/lib/hpengine.c` | Emits every record of every page; `max_items` defaults to *no cap*; flattens every scalar (bounds are memory guards at 2048 keys / 256 array members / depth 8 and stamp `_fields_dropped` / `_array_truncated` when they bite); walks `next_path` / `page_param` pagination; second hop deepens every record up to `$JO_HP_DETAIL_MAX` (default 25) and stamps `_detail_pending` / `_detail_error` otherwise; emits a `collector-truncation-notice` record if anything was left unused |
| `native/core/osint_dispatch.c` | Captures **every** emitted record — `data = {"record_count":N,"records":[…]}`. (It previously kept only the last payload, so a 40-record service handed 1 record to Phase-2, the synthesis prompt and the API.) |
| `native/core/pipeline.c` | Stores and serves all records; the LLM prompt gets a labelled view via `results_view_for_prompt()` — `records_shown`, `record_count`, `prompt_truncated` and a note that the rest are persisted. Bound size: `$JO_PROMPT_RECORDS_PER_SERVICE` (default 8) |
| `native/core/intel.c` | Upserts every emitted item; `properties` is stored verbatim |

## Checking your work

```sh
cd native
make audit-sources     # scans every collector for discard patterns
make hptest            # engine-level guarantees, offline
```

`make audit-sources` reports, per file, the patterns that usually mean discarded
data: hardcoded record caps, `break` in a record loop, first-element-only access,
single-page fetches of paged APIs, and hand-picked field lists. It is a report,
not a gate — the pre-existing collectors carry a known backlog (see the count it
prints) and the rule applies as they are touched. New collectors are expected to
come out clean.

## When you genuinely cannot take everything

Say so, in the data:

- the response is a 2 GB bulk file → stream it and record how far you got
- the API caps at 1000 results for a common query → record `records_available`
  and the cap
- an LLM prompt cannot hold 400 records → bound the *view*, keep the store, and
  label the view

The test is simple: **could a reader of the output tell that something was left
out?** If not, it is a violation.
