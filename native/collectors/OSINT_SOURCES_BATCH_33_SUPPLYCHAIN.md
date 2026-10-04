# Batch 33 — supply-chain and package-registry intelligence, cross-ecosystem

43 rows, `collectors/pivot/table/hp3b33_supplychain.c`, manifest
`docs/candidate-sources-batch33.supplychain.txt`.

**This batch is measured, not merely probed.** Unlike batch 31, the environment
it was authored in reaches package-registry hosts, so every gate in CLAUDE.md
ran against the live upstream on 2026-10-04 (UTC): proof of life with
`--check-filter`, the emit audit, per-pivot `emitted` vs `stored`, and the
registry sweep for the scheduled rows. Every record count, field list and
`meta.total` quoted in a row description is what the body actually held at that
moment.

## Why this beat

The tree already carried package registries. It carried them almost entirely
as **name search** — "find me packages matching X" — which is one question out
of three. The three a supply-chain investigation actually asks were missing in
every ecosystem:

| question | what it needs | what the tree had |
| --- | --- | --- |
| who can publish this | owners, teams, maintainer identity and email | nothing, in any ecosystem |
| what has it published, when, by whom | per-version history with the publishing account and the artefact hash | a version list for RubyGems and NuGet; nothing elsewhere |
| what breaks if that account falls | reverse dependencies, namespace listings, the whole namespace | nothing |

Every row answers one of those three, across crates.io, RubyGems, Hex.pm, npm,
pub.dev, Docker Hub, Packagist, PyPI, Maven Central, Hackage, the Go module
proxy, NuGet and JSR. JSR — the registry Deno and modern TypeScript resolve
through — was absent from the tree entirely.

## What measurement caught that probing could not

Four rows were wrong when first written and are right because they were run.
None of the defects is visible to a gate that counts records.

**Six rows answer with a bare root array, and that has to be declared.** With
no `array_path` the engine takes the densest array of objects it can find,
which is not the root whenever a root record carries a longer nested array of
its own. hex.pm's package list is exactly that shape: 100 package objects per
page, each with its own `releases` array. On a 60-page walk the engine mined
`[33].releases` (133 records) instead of the root (100) on **34 of the 60
pages**, and `HEXPM_RECENT_UPDATES` emitted 9,643 records across pages that
hold 6,000 — keyed on a field the release entries do not carry, with 2,881 of
them collapsing at the sink. `array_path = "."` is the document root and took
the same row to 6,000 emitted, 6,000 stored.

The engine disclosed it, which is how it was found: a
`collector-shape-notice` naming the array it mined, the runner-up, and the
pages it happened on. But a notice is a row in the database, not a build
error, so a source nobody reads after its first run keeps doing it. `"."` is
now documented in `lib/hpengine.h` and pinned in `make hptest` **both ways** —
the hijack and the fix — so the reason the `"."` exists stays legible.

**`NUGET_REGISTRATION_INDEX` declared the wrong level.** `items[]` is the
catalog PAGES, not the versions: two records of 189 KB and 219 KB with all 86
versions flattened into them as `items.0.*`, `items.1.*`. Nothing was
discarded, and nothing was queryable either. `items.items` crosses the page
array — `hp_path` cannot, so `hp_run` retries with `hp_path_multi`, which takes
every node at the path — and gives 86 version records with each one's
`catalogEntry`, `published`, `listed` flag and `.nupkg` URL.

**`JSR_PACKAGE_META` needs `{Q}`, not `{q}`.** `{q}` percent-encodes the slash
in `scope/name`, and jsr.io answers `/@std%2Fpath/meta.json` with HTTP 400.
Maven Central and proxy.golang.org both normalise `%2F` back to a slash, which
is why the other two slash-bearing pivots in this beat work with `{q}` — this
was measured on all three, not assumed from one.

**One COLLISION verdict is upstream and is left alone.**
`RUBYGEMS_LATEST_GEMS` emits 50 and stores 32 because rubygems.org repeats the
same gem several times in that feed. Checked field by field the repeats are
byte-identical — `rphonetic 0.1.0` appears 6 times, `zxing_ffi` 7,
`corvus_json_schema` 8 — so no key can separate them and collapsing them is
real dedupe, which is exactly what the content-hash collision guard is for. Do
not "fix" it by widening `id_keys`.

## Two tool defects the batch exposed

Both are in `tools/probe_hp_batch.py`, both made it report a source dead that
the engine measurably handles, and both are the rule-4c shape: two tools
reading one declaration differently, with the probe being the one that decides
whether a row ships.

* **An href carrying a fragment matched no anchor at all.** The regex was
  `href\s*=\s*["\']([^"\'#]+)["\']` — the class stops at the `#` and then
  demands the closing quote right there. Every anchor in a PEP 503 simple
  index ends `…tar.gz#sha256=<64 hex>`, so `pypi.org/simple/requests/` scored
  **0 anchors on a page holding 244** and was reported UNPARSEABLE. hpengine
  reads the whole attribute and keeps them. The fix matches the full attribute
  and drops the fragment only from the counting key, which is what the old
  capture was trying to do; `href_must` is now tested against the whole
  attribute, as `hp_html_walk` tests it.
* **The declared-`array_path` resolver walked dict keys only.** It understood
  neither `"."` (the root) nor a path that crosses an array, so the three
  hex.pm rows and the NuGet row came back `PATH_UNRESOLVED` while running
  clean against the engine. `_resolve_array_path()` now mirrors `hp_path` and
  `hp_path_multi`, with the eight cases it has to get right checked directly.

## crates.io is paced at 1100 ms

`core/hostgate.c` now carries a per-host gap for crates.io. Its crawler policy
is one request per second and it enforces it: a 4-worker sweep was 429'd
part-way through a `reverse_dependencies` walk, and a later single run of the
same row was 429'd on its **first** request while the window was still hot.
The engine already handles a mid-walk 429 honestly — it stamps a truncation
notice naming the status and pointing at `hostgate.c` — but the remedy it
points at had to actually exist for this host.

The pacing is why `CRATESIO_CRATE_REVDEPS` takes 219 s to walk its 200 pages,
and why `page_max` on that row is 200 rather than the 1,400 that `serde`'s
127,327 dependents would need. serde is the most-depended-on crate in the
registry; 200 pages covers every crate but the few hundred largest outright,
and when the ceiling bites it is stamped on every record.

## Measured figures

Per-pivot `emitted` / `stored`, from `--run <id> <entity>`:

```
CRATESIO_CRATE_VERSIONS          316 / 316     serde
CRATESIO_CRATE_OWNERS              2 / 2       serde
CRATESIO_CRATE_OWNER_TEAMS         1 / 1       serde
CRATESIO_CRATE_REVDEPS         20000 / 20001   serde  (+1 truncation notice, page_max=200)
CRATESIO_CRATE_DOWNLOAD_SERIES   439 / 439     serde
RUBYGEMS_OWNER_GEMS              191 / 191     tenderlove
HEXPM_PACKAGE_OWNERS               5 / 5       phoenix
HEXPM_USER_PACKAGES               54 / 54      josevalim
NPM_MAINTAINER_PACKAGES         1066 / 1066    sindresorhus
NPM_KEYWORD_PACKAGES             252 / 252     cve
NPM_DIST_TAGS                      1 / 1       express
PUBDEV_PACKAGE_VERSIONS          130 / 130     http
PUBDEV_SEARCH                     62 / 62      osint
DOCKERHUB_NAMESPACE_REPOS        100 / 100     bitnami
PACKAGIST_PACKAGE_ADVISORIES       2 / 2       monolog/monolog
PACKAGIST_PACKAGE_RSS             40 / 40      monolog/monolog
PACKAGIST_VENDOR_RSS              40 / 40      symfony
PYPI_SIMPLE_PROJECT_FILES        244 / 244     requests
PYPI_PROJECT_RELEASES_RSS         40 / 40      requests
MAVENCENTRAL_ARTIFACT_METADATA     1 / 1       org/apache/commons/commons-lang3
MAVENCENTRAL_PATH_LISTING        157 / 157     org/apache/commons
GOPROXY_MODULE_LATEST              1 / 1       github.com/gorilla/mux
NUGET_REGISTRATION_INDEX          86 / 86      newtonsoft.json
JSR_PACKAGE_META                   1 / 1       std/path
JSR_NPM_COMPAT_PACKUMENT           1 / 1       std__path
```

Scheduled rows, from `tools/audit_registry_emit.py` against a fresh copy of a
warm template DB, plus the two long walks measured separately:

```
CRATESIO_NEW_CRATES             4000 / 4000
CRATESIO_RECENT_UPDATES         4000 / 3999    one page-boundary repeat
CRATESIO_TOP_DOWNLOADS         10000 / 10001
CRATESIO_CATEGORIES               58 / 58      = meta.total, the whole taxonomy
CRATESIO_KEYWORDS              58610 / 58609   588 pages, 646 s at 1100 ms pacing;
                                               meta.total was 58,604 when the walk started
                                               and 6 keywords were added during it
RUBYGEMS_JUST_UPDATED             50 / 50
RUBYGEMS_LATEST_GEMS              50 / 32      upstream byte-identical repeats
HEXPM_RECENT_UPDATES            6000 / 6001
HEXPM_NEW_PACKAGES              6000 / 6001
PUBDEV_ALL_PACKAGES           91326 / 91101   914 pages, the upstream's next_url running
                                               out — not the ceiling; 225 page-boundary
                                               repeats over a 144 s walk
PUBDEV_RECENT_ATOM               100 / 100
DOCKERHUB_OFFICIAL_IMAGES        100 / 100
PACKAGIST_RELEASES_RSS            40 / 40
PACKAGIST_POPULAR               3000 / 3001
PYPI_RECENT_UPDATES_RSS          100 / 100
HACKAGE_RECENT_RSS                20 / 20
HACKAGE_ALL_PACKAGES           19515 / 19515   6,929 duplicate anchors collapsed
PYPI_SIMPLE_INDEX                 see below
```

`PYPI_SIMPLE_INDEX` is the whole PyPI namespace in one PEP 503 document:
46.7 MB of project anchors, and the only row in the beat whose cost is worth
stating separately. A complete pass was measured end to end:

```
[hp:PYPI_SIMPLE_INDEX] emitted 904059 of 904059 available across 1 page(s)
[sched] PYPI_SIMPLE_INDEX run rc=0 records=904059 6818250ms stored=904059
```

904,059 emitted, 904,059 stored, no uid collision, **1 h 54 m** of wall clock
and **2,269,962,240 bytes** of database — 2.51 KB per record. A separate fetch
of the same URL four hours earlier held 904,857 anchors; the namespace moves
continuously, and the run's own emitted-of-available is the figure that
matters, because it says the engine used every anchor it was given. Every
other namespace row here is an order of magnitude smaller. It is therefore scheduled **weekly**, which the manifest
originally had as daily. The per-project hops
(`PYPI_SIMPLE_PROJECT_FILES`, `PYPI_PROJECT_RELEASES_RSS`) are the targeted
reads and `PYPI_RECENT_UPDATES_RSS` is the change feed; this row is only the
denominator, and a denominator has to be fresh to the week, not the hour.

## What this still does not cover

`docs/rejected-sources-batch33.tsv` carries every candidate that did not ship
and why. Two rows were dropped after measurement; a handful were ruled out by
engine shape (NDJSON has no mode, a string array has no fields to key); and
the rest are hosts this environment's network policy refuses at CONNECT. The
four aggregate supply-chain APIs — OSV, deps.dev, libraries.io, ecosyste.ms —
are all in that last group, as are CPAN, CRAN, Swift, CocoaPods and every
distro security tracker except Ubuntu's, which the tree already carries in
full. A session with wider egress should start there rather than re-deriving
the list.

The Go module index (`index.golang.org/index?since=`) is the one rejection
worth fixing in the engine rather than working around: it is the global Go
publish firehose and it is newline-delimited JSON, which `hpengine` has no
mode for. `HP_JSON` would read the first object and silently drop the rest, so
no row was written.
