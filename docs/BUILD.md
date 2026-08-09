# Building, testing and CI

The product is a single C binary: `native/bin/japanosint`. Everything below is
about producing and trusting it.

---

## 1. Prerequisites

| Need | Debian/Ubuntu | macOS (Homebrew) |
|---|---|---|
| libcurl (**with WebSocket support**) | `libcurl4-openssl-dev` | `brew install curl` — Apple's shipped libcurl has no WS, `lib/ws.c` needs it, and the Makefile links keg-only Homebrew curl with an rpath when it finds it |
| OpenSSL 3 | `libssl-dev` | `brew install openssl@3` |
| MeCab + a dictionary | `libmecab-dev mecab mecab-ipadic-utf8` | `brew install mecab mecab-ipadic` |
| zlib | `zlib1g-dev` | system |

SQLite, cJSON and mongoose are vendored in `native/third_party/` — do not
install them. MeCab is only needed for Japanese FTS segmentation; without a
dictionary `core/fts.c` sets `g_init_failed` and Latin text still indexes, so
the build and the selftest pass but the Japanese search path is untested.

**No single machine runs everything in this repo.** The Makefile knows about
macOS/Homebrew, `launch.sh` sets `DYLD_LIBRARY_PATH` (macOS) while
`native/llama/` ships mostly `.so` (Linux), and `native/tests/audit/` assumes
WSL paths. A Windows checkout has no compiler at all; build under WSL.

---

## 2. Build

```sh
cd native
make -j              # -> bin/japanosint
make selftest        # DB integrity + schema object count + llama probe
make unit            # tests/unit/run.sh
make lint-sources    # mechanical checks over the collector tree
make source-count    # the real registered-source count
make clean           # removes obj/, bin/ AND every obj-* tree
```

Useful overrides:

```sh
make -j OBJ=/tmp/obj-mine BIN=/tmp/jo-mine    # private object tree (parallel agents)
make CFLAGS='-O0 -g'                          # optimisation only; see below
make asan  / make asan-test                   # AddressSanitizer build + tests
make tsan  / make tsan-test / make tsan-sched # ThreadSanitizer build + tests
```

`CFLAGS` is split deliberately. The tunable half is `?=`, so you can override
it. The correctness half — `-DJO_REPO_ROOT`, `-Ithird_party`, `-MMD -MP` — is
`override CFLAGS +=`, so a command-line `CFLAGS=` **cannot** silently delete
it. Before that split, `make CFLAGS=-O0` disabled the `.d` header-dependency
mechanism, which is the thing that guarantees a `source.h` change rebuilds
every translation unit; without it a `source_def` layout change leaves stale
objects linked against the old layout and you get garbage field reads at
runtime rather than a compile error.

The sanitizer targets each build into their **own** object tree (`obj-asan`,
`obj-tsan`) because every TU has to carry the sanitizer flag; a single object
compiled without it makes the report meaningless.

---

## 3. `JO_REPO_ROOT` — how the binary finds the repo

The binary reads files from the checkout at runtime: `core/schema.sql`, the
seven `grammars/*.gbnf` LLM grammars, `grammars/*.schema.json`, the default DB
under `data/`, and the upload/evidence/media directories. It locates them
through the compile-time constant `JO_REPO_ROOT`.

The Makefile derives it correctly and portably:

```make
REPO_ROOT := $(abspath ..)
override CFLAGS += -DJO_REPO_ROOT='"$(REPO_ROOT)"'
```

so any `make`-produced binary resolves against its own checkout. Verified: all
seven `.gbnf` files exist under `grammars/` and resolve.

**The unfixed half.** Twelve `.c` files carry a fallback for when
`-DJO_REPO_ROOT` is *not* passed:

```c
#ifndef JO_REPO_ROOT
#define JO_REPO_ROOT "/Users/rayan/JapanOSINT"
#endif
```

`core/breach_index.c:20`, `core/breach_jobs.c:14`, `core/breach_meta.c:10`,
`core/camera_stills.c:95`, `core/db.c:17`, `core/evidence.c:21`,
`core/keysapi.c:18`, `core/media.c:88`, `core/prompts.c:17`,
`core/statusapi.c:39`, `core/uploadapi.c:33`, `main.c:33`.

That path exists on exactly one machine in the world. It only bites when
something compiles a TU without the Makefile — a hand-rolled `cc` line, an IDE
index, an ad-hoc test harness — and then the failure is silent, because:

**`grammar_load()` (`core/prompts.c:610-650`) caches the empty string on
failure.** A missing grammar file is cached as `""` for the process lifetime,
`llm_chat` is then called with no grammar, and the LLM happily returns
unconstrained prose that fails to parse — with no message anywhere saying "I
could not find the grammar". A wrong root is therefore permanent, silent, and
presents as "the LLM got worse". This wants a one-time `fprintf(stderr, …)` on
the miss; it is a C-side change and is not fixed here.

`JO_DB`, `JO_SCHEMA` and `JO_BREACH_DIR` override individual paths at runtime
regardless.

---

## 4. Tests

| Harness | What it is | Runs where |
|---|---|---|
| `make selftest` | `--selftest`: SQLite integrity check, schema object count, llama-server reachability probe | anywhere; llama "down" is a pass |
| `make unit` → `native/tests/unit/run.sh` | each test `#include`s the `.c` it exercises so it can reach static functions; links every object except `main.o` and the file under test | needs `obj/` (run `make` first) |
| `make asan-test` / `make tsan-test` | the same unit tests under ASan / TSan | Linux |
| `make tsan-sched` | 90 s of the real scheduler worker pool under TSan | Linux (`setarch -R`) |
| `native/tests/contract/run.sh` | route/JSON parity of the C server against committed Node fixtures | needs a built binary |
| `native/tests/bench/run.sh` | per-route latency table | needs a built binary |

**The contract fixtures are irreplaceable.** They were captured from
`server/src/index.js`, deleted 2026-05-17 and not recoverable from git. Treat
`native/tests/contract/*.node.json` as read-only test data. The harness used to
contain a capture branch that truncated them (`curl … > fixture || true`
truncates before curl fails); it is gone, and every fetch now writes to a temp
file that is moved into place only on success.

---

## 5. `make lint-sources`

`native/tools/lint_sources.py` (python3, stdlib only) is the mechanical
counterpart to the audit's central finding: every fix already exists somewhere
in this tree, and the dominant failure mode is that nothing notices when it is
missing elsewhere. It checks:

| Check | What it catches |
|---|---|
| `dup-id` | two `source_def`s with the same `.id` — `registry_get()` returns the first, so the second is scheduled but never dispatchable |
| `unresolved-id` | a `REGISTER_SOURCE` whose `.id` the tool could not statically resolve (makes the count a lower bound) |
| `registry-orphan` | a row in `core/source_registry.gen.c` with no implementation — a source that can never run but still appears in `/api/sources` |
| `quarantine-empty` | `return n > 0 ? 0 : -1` and casted variants — see [`pipeline.md`](./pipeline.md) §1 |
| `snprintf-guard` | `off += snprintf(buf + off, sizeof buf - off, …)` with no clamp: `off` runs past the buffer and the size argument underflows |
| `rowid-unchecked` | `sqlite3_last_insert_rowid()` read after a `sqlite3_step()` whose status was discarded |
| `geo-precision` | a collector emitting geometry with no `geo_precision` property |

It **counts macro expansion**. `RSSX()` in `collectors/sources/arxiv_feeds.c`
expands to a whole `source_def` plus its own `REGISTER_SOURCE`, so one line
there is 41 sources; the tool inlines local `.inc` includes and expands the
file's own macros before counting. A `grep -c REGISTER_SOURCE` does not, which
is how six different wrong source counts ended up committed to this repo.

It is **baselined**, in `native/tools/lint_baseline.json`. Every count in that
file is a defect that still needs fixing, not an approved exception; the check
fails only when a count goes *up*, so CI is green today and can only ratchet
down. After a batch of fixes lands, re-record the floor:

```sh
make -C native lint-baseline     # python3 tools/lint_sources.py --write-baseline
```

---

## 6. CI

`.github/workflows/ci.yml` — the first CI this repository has had. On
ubuntu-latest: install the deps above, `make -j`, `make selftest`, `make unit`,
`make lint-sources`. It deliberately does not run collectors (they hit real
third-party APIs), does not start llama-server (an 11 GB model), and does not
run the contract suite (its fixtures need a DB with real intel rows). A CI that
is red for unrelated reasons is a CI nobody looks at.

---

## 7. Running it

`./launch.sh up` from the repo root: freeze stale processes → incremental build
→ server pod → llama pod → suggest-llama pod → status. `./launch.sh tags`
prints every knob. A failed build is now fatal there — it used to discard
`make`'s exit status and fall back to `[ -x "$BIN" ]`, and a *stale* binary is
still executable, so a broken compile booted the previous build under a green
"build green" line (an entire audit was once run against a 40-minute-stale
binary that way).
