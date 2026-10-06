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
