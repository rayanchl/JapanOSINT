# Batch 35 — strings: four sources a comment had written off

4 rows, `collectors/pivot/table/hp3b35_strings.c`, manifest
`docs/candidate-sources-batch35.strings.txt`.

This batch is a **correction**, and the correction is worth more than the rows.

## What went wrong

`docs/rejected-sources-batch34.tsv` ruled four reachable, live endpoints
unregisterable in one entry:

> Not registerable: `"repositories"` is an array of bare STRINGS, and
> `lib/hpengine.c` counts a bare scalar in a JSON array as an `empty` slot
> rather than a record (deliberately — it is how a trailing newline or a null
> stops being reported as a shortfall). The same bar blocks
> `api.nuget.org/v3-flatcontainer`, `packagist.org/packages/list.json`
> (470,134 names) and `pub.dev/api/package-names` (91,325).

That is a capability claim read out of a comment. The comment is about a NULL,
a number, or an **empty** string — the trailing newline of a CSV, which is
exactly the case it names. `hp_json_flat()` has always mapped a string *with
content* to `{"value": "<the string>"}`:

```c
} else if (cJSON_IsString(rec) && rec->valuestring[0])
    cJSON_AddStringToObject(flat, "value", rec->valuestring);
```

So `title_keys=value;id_keys=value` is the whole of what a string-array row
needs, and no engine change was required for any of the four.

## Measured

```
MCR_IMAGE_CATALOG             emitted 3851 of 3851      stored 3851
NUGET_PACKAGE_VERSIONS_FLAT   emitted 86 of 86          stored 86
PUBDEV_ALL_PACKAGE_NAMES      emitted 91388 of 91388    stored 91388
PACKAGIST_ALL_PACKAGE_NAMES   emitted 464521 of 464521  stored 464521
```

All four exact, no collisions. Probe 4/4 PASS with `--check-filter`.

## The rows

* **`MCR_IMAGE_CATALOG`** — every container repository Microsoft publishes,
  from its own OCI catalogue endpoint. 3,851 paths, anonymous and keyless,
  unlike Docker Hub's and AWS ECR Public's equivalents. There is no search UI
  for this and no other machine-readable list of it.
* **`NUGET_PACKAGE_VERSIONS_FLAT`** — the version list `dotnet restore` itself
  consults, flattened with no catalog-page envelope, so a version present here
  is installable. Uses `{ql}`, not `{q}`: the flat container is case-sensitive
  and serves lowercase ids only, so an un-lowercased pivot 404s on
  `Newtonsoft.Json` while working for `newtonsoft.json`.
* **`PACKAGIST_ALL_PACKAGE_NAMES`** — the complete Composer namespace,
  464,521 names. Weekly: 2 m 56 s and 1.13 GB per pass at 2.43 KB per record.
  A denominator has to be fresh to the week, not the hour — the same call
  `PYPI_SIMPLE_INDEX` makes in batch 33.
* **`PUBDEV_ALL_PACKAGE_NAMES`** — the same namespace `PUBDEV_ALL_PACKAGES`
  walks in 914 paged requests, in one. Both are kept deliberately: this is the
  cheap complete denominator for a name question, the paged one carries each
  package's full latest release and pubspec. Declares
  `Accept-Encoding: gzip`, because pub.dev answers a client that does not with
  HTTP 406 and no data — a refusal a probe would otherwise log as a dead
  endpoint.

## What this says about the rejects files

`docs/rejected-sources-batch{33,34}.tsv` exist so a later session does not
re-derive work already done. That only helps if the entries are measurements.
This one was an inference, it was wrong, and it cost four sources. The batch-34
entry is now marked `CORRECTED` in place rather than deleted, because the
mistake is the useful part of the record.

## MEASURED, 2026-10-20 — the 100 WEKO pivots answer

`hp3b35_jpweko.c`'s rows shipped unprobed on an endpoint and key set copied from
batch 25. All 100 were asked about one entity (`研究`) through the real binary on
2026-10-20; the readings are in **`docs/emit-sources-batch34-35.tsv`** beside
batch 34's.

**61 OK, 38 `EMPTY_UPSTREAM`, 1 HTTP 500** (`asahikawa-med`, its own error on
both passes). The copied endpoint and keys hold: every one of the 61 turned a
WEKO3 `/api/records/?q=` body into records, which is what this batch could not
check when it was written.

Read the counts for what they are. 1,204 records across the 61, 184 at the
largest (`JO35_WEKO_WASEDA`), is **one entity's worth from each repository**, not
a corpus size — these are on-demand pivots and the number moves with the
question. A repository holding nothing for `研究` is an honest empty, not a dead
row; a different entity moves rows between the two columns. What the pass proves
is reach and shape, not how much any repository holds.

The identity question is still open: this audit reads EMITTED, so records keying
onto each other inside a run would not show. Batch 35's keys are inherited
rather than measured, which makes them the first to put through
`audit_registry_emit.py`'s stored reading.

## Rules 4b and 4d measured on the 100 WEKO pivots (2026-10-21)

Both questions this doc left open are now answered, and the first one needed a
different tool than the obvious one.

**Rule 4b — do they STORE what they emit?** `audit_registry_emit.py` cannot
answer it for a pivot: it runs every row with no entity, so all 100 came back
`NEEDS_ENTITY` in the registry sweep, a verdict that reads like a pass and is an
abstention. Asking the binary directly instead — `--run <id> 研究`, which prints
both numbers — gives, over **76 of the 100** rows (the run was stopped twice at
a background time limit, not by a failure):

    76 rows   68,278 emitted   68,312 stored   0 rows losing records

`docs/sweep-stored-batch35-pivots-2026-10-21.tsv`. Stored runs slightly ABOVE
emitted because each truncation notice is itself a stored uid. **The keys copied
from batch 25 hold**, which is what this batch could not check when it shipped.

Note how much larger these numbers are than the 1,204 records the first emit
audit reported for all 100 rows: that pass used a narrower probe entity. **A
pivot's record count is a property of the question asked, not of the source.**

**Several rows stop at exactly 2,000** — the 20-page ceiling x 100 — and each
files a truncation notice, which is why stored is one higher than emitted on
those rows. That is rule 2's bounded view, disclosed in band: WEKO publishes
`hits.total` (nara-edu holds 3,132 for 研究), so a per-row `page_max` would
complete *this* entity's answer while another entity could hold half a million.
Entity runs are excluded from cycling for the same reason. Left bounded and
disclosed deliberately.

**Rule 4d — does the filter filter?** `probe_hp_batch.py --check-filter`:
**100/100 PASS, no `FILTER_IGNORED` and no `FILTER_UNCHECKED`.** Zero findings
from a check written to find confident wrong answers deserves one suspicious
look, so two hosts were then compared by hand:

    nara-edu   研究  707,325 bytes  100 records  hits.total 3,132
               impossible entity   149 bytes    0 records  total 0
    ehime-u    研究  532,588 bytes  100 records  hits.total 1,372
               impossible entity   148 bytes    0 records  total 0

A real search API, not a collection handed back whatever is asked. For context,
65 of batch 31's 360 rows failed this same check.
