<!-- Dated history moved out of CLAUDE.md on 2026-11-16 (verbatim, unedited): native commands, unit-test link and seed work, CI timing and flakiness lessons. -->
<!-- CLAUDE.md holds the current rules; where this text contradicts it, CLAUDE.md wins. -->

```sh
cd native
make                 # full build (-Wall -Wextra); the tree is at 0 warnings, keep it there
make selftest        # boot self-test: DB integrity, schema objects, llm probe
make unit            # tests/unit/run.sh against a scratch DB
make hptest          # offline check of the engine's guarantees
make authtest        # JWT / tenant auth
make pagewalktest    # offline check of the paging + disclosure engine
make htmlparsetest   # the one anchor scanner
make rsstest         # a dead feed is a failed fetch, an empty feed is not
make lint-sources    # dup ids/endpoints, quarantine-empty, snprintf guards
make source-floor    # fails if a collector stopped registering (tools/source-floor.txt)
make registry-floor  # the same against the binary's registry (tools/registry-floor.txt)
make audit-sources   # scan every collector for discard patterns
tools/ci_concurrency_gate.sh ./bin/japanosint 1.0   # the event loop stays free under load
```

That is the native job in `.github/workflows/ci.yml`; CI also runs TSan and
ASan builds and the client's `vitest` and `vite build`. Run gates with `JO_DB`
pointing at a scratch file — several of them open the default DB otherwise.

**`make unit` is ONE PARTIAL LINK now, not 84 full ones** (2026-10-23). Every
test `#include`s the .c it tests, so the link line is every object except
`main.o` and that one — and at 1,860 objects that cost **35.688 s per test**,
about fifty minutes of pure linking a suite, which is what pushed the TSan job
through its 90-minute ceiling (the duration series is in `ci.yml`).
`tests/unit/run.sh` now `ld -r`s the 1,807 objects that NO test excludes into
one relocatable object once (1.2 s) and links each test against that plus the
53 objects some test does exclude: **2.674 s**, a 13x saving, for a binary
4 KB different from the old one. The whole suite runs end to end in **494 s**.

**An `ar` archive is the wrong tool here, and it passes the test while being
wrong** — worth knowing because it is the obvious thing to reach for and it
measured 0.274 s, another 10x. Archive members are pulled only to satisfy an
undefined symbol, and this tree's collectors self-register from constructors
that nothing references, so the archive link produced a **10.5 MB binary
against 128.3 MB** and the test ran against an EMPTY REGISTRY — `[db] seeded
sources from registry (0 new … 58,047 stale pruned)` — and still printed "all
passed", because the test it was checked with needs only *some* scheduled row.
A partial link keeps every section and every `.init_array` entry, so the
registry seeds the same rows it does without this. **A 130x speedup deserves
one suspicious look at what it stopped doing**; comparing the two binaries'
SIZES is what exposed it, before any assertion did.

It keeps the duplicate-symbol check `prune_orphans` exists for, verified rather
than assumed: `ld -r` over two objects defining one function reports "multiple
definition" and exits 1, exactly as the final link does — so a stale object
that redefines a symbol still fails the build, once instead of 84 times. And
run.sh now removes each test binary after running it: at 128 MB a test, the old
script left **~10 GB** in `$TMPDIR` until the suite ended.

**AND LINKING WAS NOT THE WHOLE BILL: THE JOB CAME IN AT 82.8 MIN, NOT THE ~45
THAT PREDICTED** (2026-10-24, CI run 554, all six jobs green). `ci.yml`'s own
note had set the test — "if a run still comes in over ~70 min, the ceiling is
the wrong lever a second time" — so the next thing was to find out where the
time goes rather than to raise it again. **A GitHub job log timestamps every
line, so `run.sh`'s own `--- test_x ---` markers ARE a profile**; nothing had to
be instrumented. The gaps say:

    compile 1,860 TSan objects + the one `ld -r`      3.1 min
    88 test slots, marker to first output (the LINK)  2.7 min, median 1.6 s
    test_alert_tenancy                               21.1 min
    test_source_category                             14.8 min
    test_service_vec                                  6.9 min
    test_sourceindexapi                               3.5 min
    the other 84 tests                               33.3 min, median 31.1 s

So the partial link worked exactly as measured, under TSan too — 2.7 min of
82.8, not worth another look. What remained was per-test EXECUTION, and inside
it ONE line repeats: `[db] seeded sources from registry`. **A seed that inserts
NOTHING costs 16 s under TSan** (30 s when it inserts all 58,055), because
`db_seed_sources()` is two prepared-statement steps per source and the
sanitiser instruments all 116,110 of them. test_alert_tenancy's 21 minutes is
82 of those lines; the median test's 31 s is mostly one.

Two changes, each measured:

* **`core/db.c` stamps a fingerprint of what the seed WOULD write** — a FNV-1a
  over the five values it writes (id, name, type, category, url) in registry
  order — into `_registry_seed`, beside the row count the table had when the
  last full pass finished, and skips the pass when both still match. A warm
  `db_open` goes **0.63 s → 0.07 s** natively; a fresh one is unchanged. The
  same shape as `_fts_meta`, which already keys the FTS rebuild on a
  fingerprint of the index it would rebuild.

  What it gives up is stated rather than hoped: a `name`/`type`/`category`/
  `url` edited BY HAND outside that function is no longer reverted on the next
  boot, and `JO_DB_RESEED=1` forces the full pass for exactly that. The row
  count is the half that catches drift on the database's side — a row inserted
  or deleted by anything else makes `count(*)` disagree and the next boot does
  the full pass, prune included. The count is stamped AFTER the prune, so an
  install holding a stale row for its history still reaches the fast path.
* **`tests/unit/run.sh` seeds ONE template database and copies the file** per
  test (`tests/unit/seed_template.c`, one `db_open` and nothing else), so no
  test derives it from scratch and each one's own `db_open` then matches the
  stamp. A failure to build the template is NOT fatal: every test creates its
  own database exactly as before, which is slow and correct.

Local suite **494 s → 455 s** (the saving is small here because a native seed
is only 0.75 s; the point is the 20-30x sanitiser multiplier), 86 tests, all
passing, with **129 of the 152 opens taking the fast path** and
test_alert_tenancy taking it 82 times out of 82. The 23 that still do a full
pass are tests that open a second database of their own.
`tests/unit/test_db_seed.c` pins the fingerprint field by field, the three ways
of not matching a stamp, the stale-row case and both readings of
`JO_DB_RESEED`.

**CI then measured it: the TSan job is ~40 min, from 82.8** — 36.0, 36.9 and
45.4 over the next three green runs (2026-10-24), and three readings rather
than one because this runner varies: `asan` on the same runs was 12.3, 10.2 and
14.4 min on work that did not change. **A single CI duration is a draw from a
distribution**, so quoting one is the same error as believing one probe. The
whole workflow got cheaper with it, because `build-and-test` runs the same
suite without the sanitiser: **34.7 min → ~10.5 min**. Both levers were found
by reading the job's own timestamps rather than by guessing, and the first (the
link) would have looked like the whole answer if the second had not been
measured against it.

**The next driver is already named, so nobody has to find it twice**:
`test_source_category` spends 765 s on a 15-nearest vote over 42,255 declared
sources and `test_service_vec` indexes all 58,055 source cards five times at
~65 s each. Those are O(registry) BY DESIGN, so making them cheap is a question
about what they should cover — not a build fix.

One trap met on the way, and it is the one `prune_orphans` exists for: linking
a single test by hand with `find obj -name '*.o'` picked up four stale
`hp3b4*_arcgislayers.o` objects whose `.c` files no longer exist, and the
binary **segfaulted in the registration constructors** after printing
`[hp] NOT REGISTERED … '(no id)'` for rows that read as garbage. Nothing was
wrong with the tree; the object list was. Build a one-off test binary through
run.sh's own pruning, not through a bare `find`.

**A macOS job the runner cancels at 15 minutes is the runner's, not the
PR's** (seen 2026-10-21 on `main`'s own run for PR #57 and on two PRs the
same day; the owner's rule of 2026-10-22): when the other five jobs are green,
the same cancellation shows on `main`'s own run and the native change was
macOS-green on a previous head of the PR, merge and say so in one comment on
the PR. Never an empty commit to re-trigger it; a real code change may.

**And treat "CI is red" as a question, not an answer.** `ci.yml` carried
`JO_DB: ${{ runner.temp }}/ci.db` as a JOB-LEVEL `env:` entry, where the
`runner` context does not exist yet. That is an unrecognised named-value, which
invalidates the whole FILE rather than the one job: GitHub created each run,
failed it immediately and scheduled ZERO jobs. **Every one of the first 74 runs
failed that way, from this file's first commit** — so for 74 runs "CI is red"
was never a test result, because nothing had ever been compiled or executed.
PR #28 moved it to a `$GITHUB_ENV` step and the runs after it are the first
real ones. A failing run with `total_count: 0` in its jobs list is this shape,
not a gate.

**And the inverse: "that job is just flaky" is also a question.** The `asan` job
failed on `test_export_stream`'s backpressure assertion about 40% of the time
for weeks, and runs #116 and #117 are the SAME SHA with opposite outcomes, which
is as flaky as a signal gets. It was not noise. `xs_pump` set `x->queued = 0`
before the bytes it had taken were counted into `x->unsent`, so between those
two statements the worker's gate (`queued + unsent >= XS_HIGH_WATER`) saw
neither, and a worker racing the first pump queued a SECOND full high-water mark
while the first was in flight: 1.5 MB held against a 1 MB mark, on the one code
path whose entire purpose is that the bound holds. An intermittent assertion
means the invariant is intermittently false. Reproduce it (`2 of 30` runs
locally without ASan) and instrument the quantity the assertion names — the peak
distribution was `38 x 1024 KB, 1 x 1280, 1 x 1536` and said what the bug was —
rather than widening the slack until the red goes away.

If `make unit` dies with `tests/unit/run.sh: No such file or directory` (exit
127) on a tree that came from a Windows checkout, the script has CRLF line
endings and the kernel is reading `#!/bin/bash\r` as the interpreter. The error
names a script that is sitting right there and is executable, so it reads as
"missing file". `.gitattributes` forces LF for `*.sh`/`*.py` on checkout, but it
cannot rewrite files that were checked out BEFORE it existed — and because git
normalises CRLF away on check-in, `git status` stays clean forever while the
working tree stays broken. Repair the tree, don't re-clone:

```sh
git ls-files -z '*.sh' '*.py' | xargs -0 sed -i 's/\r$//'   # content-identical to HEAD
```

`make source-count` (and the `source-floor` gate built on it) counts hp table
rows as well as `REGISTER_SOURCE`, and agrees with the binary. Both floors were
recorded at 18,170 on 2026-10-02, at 18,644 on 2026-10-05 and at 18,645 on
2026-10-06; they had been
left at 16,271 and 13,081, low
enough that batches 31 and 32 could both stop registering with CI green.
Re-record them when a batch lands (`make source-floor-record
registry-floor-record`).

