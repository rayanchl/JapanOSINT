# Source authoring contract

Every new collector in `collectors/sources/` MUST satisfy this. It exists
because the 2026-07-31 fleet audit (`tests/audit/SUMMARY.md`) found that a
large share of the then-existing sources passed every row-count sweep while
carrying no fetched data at all. The rules below are that audit's findings
turned into acceptance criteria. A source that violates any of them is worse
than no source: it inflates the dashboard, and it teaches the analyst to
distrust the ones that are real.

## R1 — Real fetch, real fields

The row you emit must contain values that came back from the network (or from
genuine local computation: DNS, sockets, OpenSSL, a local index).

**Forbidden — the "names, not data" pattern.** Do not emit a row whose content
is a registry/provider name plus a constructed lookup URL. `COMPANY_LOOKUP`
had 7 of these, `VEHICLE_LOOKUP` ~16, each tagged `real_data:true` while
fetching nothing. If you cannot fetch the value, do not emit a row about it.

**Forbidden — the portal probe.** Do not emit one row of
`{operator, reachable:true}` with a hardcoded title. ~73 sources do this and
they are the reason "sources with data" is not a number anyone can trust.

## R2 — No invented geometry

Set `has_geo`/`lat`/`lon`/`geometry_geojson` ONLY from coordinates the upstream
actually returned.

Never fall back to a prefecture centroid, a head-office address, a capital
city, or a fixed point. The audit removed: 1,112 rows stacked on Tokyo Station,
46 CVE markers pinned to vendor head offices, and `classifieds`' random ±0.015°
jitter that moved every run. A row with no location is correct; a row with an
invented location is a lie the map presents as a fact.

If the location is real but coarse, say so in properties
(`"geo_precision": "area-centroid"`), and only when the upstream itself is
giving you an area.

## R3 — Honest empty returns 0, not -1

```c
return n > 0 ? 0 : -1;   /* WRONG when n==0 just means "nothing today" */
```

`core/scheduler.c` logs a non-zero rc as `status=error` and feeds
`anomaly_detect`, which quarantines the source. So `-1` on an honest empty
takes a working collector offline on a quiet day. Distinguish:

- fetch failed / unparseable / upstream error → `-1`
- fetched fine, upstream had nothing → `0`
- not configured (no key/url) → `0` (gating is modelled separately)

And never `return n;` — a run returning its own row count is read as an error
code. The audit found 17 sources that were quarantined *for working*, including
an IOC lookup that had found the indicator.

## R4 — Prove it returns rows

A source is not done until this prints a non-zero record count:

```
JO_DB=/tmp/probe.db ./bin/japanosint --run <your-source-id>
```

Paste that line's output in your report. `records=0` is not acceptance unless
you also show the upstream genuinely returned nothing right now (and then the
source does not count toward the delivered total).

## R5 — Unique id and unique filename

Ids are lowercase-kebab for feeds (`jma-earthquake`) and UPPER_SNAKE for
entity-pivot OSINT services (`DOMAIN_WHOIS`).

The AUTHORITATIVE list is the built binary, because some sources compute their
ids rather than writing them as literals:

```
./bin/japanosint --list-sources | cut -d' ' -f1 | sort -u
```

`collectors/existing_ids.txt` is a convenience snapshot of the ids that appear
as `.id = "…"` literals (regenerate with the grep in this file's history). It
is a subset — treat a miss there as "probably free", not "definitely free".

Whichever you use, the real gate is startup: `registry.c` prints
`[registry] DUPLICATE id` for any collision, and a clean run must print none.
A duplicate is not cosmetic — `registry_get` returns the first match, so the
second definition is scheduled but unreachable by `--run`, by the OSINT
dispatcher, and by anything else that resolves a source by id.

## R6 — Keyless, or gated honestly

Prefer endpoints that work with no credential. If a key is required, read it
with `getenv`, and when it is absent **`return 0`** — never `-1`.
`scheduler_run_source` turns a `-1` into `fetch_log status='error'` plus a
`collector_anomaly` on *every* tick, so an unconfigured source reads as
permanently broken and buries the real breakages.

R6 used to end "log one line and `return 0` — do not emit a row saying a key
is needed", and the thing it was guarding is still absolute: **a gated source
must never emit a FINDING.** A row that a map pin, a feed or a synthesis prompt
would read as an observation about the world is fabricated data (R1), whether
it is a seed point or a "no key" placeholder.

But a bare `return 0` is not honest either. In `fetch_log`, `/api/status` and
anomaly triage it is indistinguishable from a source that ran, spent a request
and genuinely found nothing — the same invisible nothing as an
`EMPTY_RESULTSET` source, and exactly what CLAUDE.md rule 1 names: *"A failure
degrades to an explicit `error` / `not_found` / 'needs credential' note."* A
log line nobody reads is not a disclosure.

The resolution is the shape `dam_water_level.c` already used for its own
not-implemented state, now shared:

```c
#include "_credential_notice.inc"

  const char *key = getenv("WIGLE_API_KEY");
  if (!key || !*key) {
    static const char *const envs[] = { "WIGLE_API_KEY", NULL };
    return jo_needs_credential(sink, "wifi-networks-wigle",
        "WiGLE wireless network search (JP bbox)",
        envs, "https://api.wigle.net/api/v2/network/search",
        "free account at wigle.net");
  }
```

That emits one `record_type = "collector-status-notice"` row — filtered out of
every findings surface by record_type, carrying no observation of any kind, and
keyed on the constant `remote_key = "collector-needs-credential"` so the sink
upserts it. A source gated for a year holds exactly one row, not one per
scheduler tick. It still returns 0.

Twelve collectors were converted in the unification sweep (`abuseipdb-jp`,
`bear-encounters`, `estat-employment`, `facebook-geo`, `fofa-jp`, `jstat-map`,
`msil-umishiru`, `odpt-train`, `resas-population`, `sentinel-japan`,
`softbank-crowd`, `wifi-networks-wigle`). The rest of the gated population —
notably every collector gated inside `lib/threatintel.c`'s
`threatintel_collect()` — still degrades to the log-only form. Use
`jo_needs_credential()` for anything new; the gate belongs in the toolkit for
the threatintel family and that is tracked separately.

## R7 — Cite the upstream in the file header

One comment block: what the endpoint is, what fields are emitted, whether it
is keyless, and the licence/terms if the publisher states one. Do not add a
source whose terms forbid this use — `bom-au-warnings`' replacement API says
"You must not use, copy or share it", and the correct action was to not wire
it up.

## R8 — Compile clean

`-Wall -Wextra` with no new warnings. Free what you allocate; `cJSON_Delete`
every parsed doc on every path including early returns.

## Skeleton

```c
/* <what this is>. Endpoint: <url>. Emits: <real fields>. Keyless.
 * Licence: <if stated>. */
#include "source.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <string.h>

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, "https://…", 20000);
  if (!doc) { fprintf(stderr, "[my-source] fetch failed\n"); return -1; }

  cJSON *arr = cJSON_GetObjectItem(doc, "results");
  int n = 0;
  cJSON *it;
  cJSON_ArrayForEach(it, arr) {
    const char *name = cJSON_GetStringValue(cJSON_GetObjectItem(it, "name"));
    if (!name) continue;                       /* no title -> no row (R1) */
    intel_item row = {0};
    row.title  = name;
    row.record_type = "my-source";
    /* geo ONLY if upstream gave coordinates (R2) */
    cJSON *lat = cJSON_GetObjectItem(it, "lat"), *lon = cJSON_GetObjectItem(it, "lon");
    if (cJSON_IsNumber(lat) && cJSON_IsNumber(lon)) {
      row.has_geo = 1; row.lat = lat->valuedouble; row.lon = lon->valuedouble;
    }
    if (sink->emit(sink, &row) >= 0) n++;
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[my-source] emitted %d\n", n);
  return 0;                                    /* fetched fine (R3) */
}

static const source_def my_source_def = {
  .id = "my-source", .collector = "<category>", .name = "Human name",
  .update_interval_sec = 3600, .run = run,
  .type = "api", .url = "https://…", .free_tier = 1,
  .description = "<what an analyst gets from this>",
};
REGISTER_SOURCE(my_source_def)
```

For RSS use `lib/rss_atom.h` (`rss_collect`), for FeatureCollections
`lib/geojson.h` (`geojson_emit_features`), for CSV `lib/csv.h`, for OSM
`lib/overpass.h`. Leaning on those is what keeps a collector ~30 lines.
