# Batch 14 — 1,001 JP / FR / US high-penetrancy candidate sources

**Status: UNVERIFIED.** Every source in this batch is registered and will run,
but none of the endpoints was fetched at authoring time. That is the one thing
to know before reading anything else here.

Manifest: `docs/candidate-sources-batch14.tsv`
Collectors: `native/collectors/sources/csrc14_*.c` (20 files)
Generator: `native/collectors/gen_candidates_jp_fr_us.py`

## Why these are not verified

Every previous bulk batch went through `collectors/verify_feeds.py`: an
endpoint entered the tree only after it returned 2xx and parsed to at least one
record, and `docs/verified-sources-manifest.tsv` holds that proof. The `vsrc_*`
file headers assert it.

This batch could not do that. It was authored in a sandbox whose egress proxy
denies `CONNECT` to every host outside github/npm/pypi:

```
$ python3 collectors/verify_feeds.py probe.tsv
t1  https://www.federalregister.gov/...  NET_ERR  Tunnel connection failed: 403 Forbidden
t3  https://www.e-stat.go.jp/            NET_ERR  Tunnel connection failed: 403 Forbidden
# 0/4 PASS
```

So rather than inherit a verification claim that is not true of these rows, the
batch is labelled for what it is, in three places: `verified=no` plus a reason
column on every manifest row, an `UNVERIFIED candidate` header on every
generated `.c` file, and this document.

**A dead endpoint here is not a fabrication risk.** The four `V*` macros return
`-1` on a failed fetch and `0` on an honest empty, so an endpoint that has moved
or never existed degrades to an explicit collector error. It cannot degrade to
invented content — house rule 1 holds regardless of how this batch resolves.
What an unverified row costs is a wasted request and a source that reports
`error`, not a false record.

## How the endpoints were chosen

Hostnames were not recalled from memory and hoped for. Each generator is one of
two things:

**(a) A documented platform API applied to a host this repo has already proven
live.** The proven base paths were mined out of
`docs/verified-sources-manifest.tsv`, and the paths appended to them are core
platform API present on every deployment of that platform — CKAN's
`package_search` / `organization_list` / `tag_list`, the Socrata discovery and
`/api/views/metadata/v1` endpoints, OpenDataSoft Explore v2.1
`/catalog/datasets`. Host and contract are then both known-good, and only the
combination is untested.

**(b) A documented national API with a stable public contract** —
`geo.api.gouv.fr`, `data.gouv.fr`, `recherche-entreprises.api.gouv.fr`, the JMA
`bosai` JSON tree, and the Caltrans CWWP district feeds.

Everything is also deduplicated against the tree as it stands: 23 candidates
were dropped because their URL or id already existed, including all five
CERT-FR feeds, which turned out to be present already.

## Composition

Counts are rows that survived deduplication and are actually registered; they
sum to 1,001.

| Group | n | What one request returns |
|---|---:|---|
| `gen_jp_ckan` | 179 | Tokyo / Yokohama / G-Spatial / BODIK CKAN — 40 topical dataset searches plus 5 registry calls per portal |
| `gen_jma` | 112 | Per-office JMA warnings + weekly outlook, all 56 forecast offices |
| `gen_us_datagov` | 109 | data.gov CKAN — 104 topical searches plus the portal registries |
| `gen_fr_communes` | 101 | Every commune of each of the 101 French departments, with INSEE code, SIREN, centroid, population, EPCI |
| `gen_fr_schools` | 101 | The national school directory sliced by department — UAI, type, address, coordinates |
| `gen_us_socrata` | 96 | 48 US portals × (federated discovery + portal-side asset metadata) |
| `gen_fr_datagouv` | 84 | data.gouv.fr topical searches plus its organisation / API / reuse registries |
| `gen_fr_ods` | 49 | French OpenDataSoft portals × (dataset catalogue + facet map) |
| `gen_us_cameras` | 36 | Caltrans CCTV, changeable message signs and lane closures, all 12 districts |
| `gen_us_socrata_search` | 35 | Cross-portal Socrata sweeps (body-worn camera, LPR, use of force, …) |
| `gen_us_arcgis_hub` | 32 | ArcGIS Hub search across US state/county/municipal GIS sites |
| `gen_fr_enterprises` | 29 | SIRENE-backed company search by sector — SIREN, SIRET, NAF, officers |
| `gen_fr_geo_extra` | 20 | French departments, regions, EPCIs, and communes by region |
| `gen_fr_highered` | 18 | Universities, grandes écoles and research bodies by region |
| `gen_fr_cyber` | 0 | 5 CERT-FR/ANSSI feeds generated, all already in the tree |

Against the requested coverage: government and prefectural/state is the bulk of
it (JMA offices, French communes/departments/regions/EPCI, US states and
counties); schools and universities are the 119 French education rows plus the
education topical slices; enterprises are the SIRENE searches, BODACC and the
business-licence sweeps; IT/cyber/SIGINT are the cyber, telecom, spectrum,
broadband, data-centre and network topical slices; cameras are the 36 Caltrans
rows plus the traffic-camera, surveillance-camera, body-worn-camera and LPR
slices.

## Promoting this batch

Run on a machine with egress:

```sh
cd native
make verify-candidates     # probes all 1,001; NEEDS NETWORK
```

That writes `docs/verified-sources-batch14.tsv` (the survivors) and
`docs/rejected-sources-batch14.tsv` (everything else, with the verdict and the
upstream's own error text, so the failures are inspectable data rather than a
lost log line).

The promotion step rewrites each surviving row's `kind` and `items` with what
the verifier actually observed, not what the generator predicted. This matters:
the generator infers a response shape from the API contract, and where that
guess is wrong the collector would parse nothing out of a perfectly live
endpoint.

Then regenerate from the verified manifest and retire the candidates in the
same commit, so every registered source traces to exactly one manifest:

```sh
python3 collectors/gen_verified_sources.py ../docs/verified-sources-batch14.tsv \
    --outdir collectors/sources --reserved <ids> --prefix vsrc14
rm collectors/sources/csrc14_*.c
make
```

To rebuild the candidate set itself (idempotent — it excludes its own ids from
the reserved list):

```sh
make regen-candidates
```

## State at the time of writing

- `make` — clean, no errors, no new warnings from the 20 generated files.
- `make hptest` — all passed.
- `make audit-sources` — 0 findings in `csrc14_*`; strict `hp*_*` set still 0.
- `tools/lint_sources.py` — 9,080 registered source_defs, up from 8,079. No
  check moved because of this batch: `dup-id` 0, `geo-precision` unchanged at
  118. The one check reported above baseline, `quarantine-empty` (13 vs 11), is
  **pre-existing** — it reproduces on a clean checkout with this batch stashed,
  and none of the 13 files it names is part of this batch.
