# One environment reader (lib/joenv.{c,h})

## What was added
- `native/lib/joenv.{c,h}`: `jo_env_ll/long/int/double` (clamp out of range) and the `_strict` forms (out of range -> default), `jo_env_bool`, `jo_env_str`. Unset/empty -> default silently; garbage, trailing garbage (trailing whitespace tolerated), NaN/inf, overflow -> default plus ONE stderr line per variable name (mutex-guarded name table); out of range -> clamped (or default for `_strict`) plus one line. No value caching (getenv each call), dependency-free, thread-safe.
- `native/tests/unit/test_joenv.c` (unset, empty, garbage, trailing garbage, fraction, bare sign, below min, above max, overflow both ways, _strict, ll/int width, double incl. nan/inf, bool spellings, str, once-per-name stderr count, setenv-between-calls). Passes in `make unit`.
- `native/tools/lint_env_readers.py`, wired into `make lint-sources`: fails on a new `static ... env_int/env_long/env_ll/env_double/env_bool/env_pos/env_truthy/env_nonempty/<prefix>_env_*` private helper, on `atoi/strtol/...(getenv(...))`, and on `const char *e = getenv(..)` followed within 5 lines by a numeric parse of `e`. Marker `/* env-reader-ok: <reason> */`. 0 findings.
- `tools/gen_env_doc.py` understands the reader (name, default literal, and a new Range column from min/max), plus string-passed `JO_*_PAGE_MAX` legacy knobs that lib/pagewalk.c reads. `docs/ENVIRONMENT.md` regenerated: 475 variables.
- Makefile: `lib/joenv.c` added to the standalone link lines (hptest, rsstest, pagewalktest, FUZZ_RSS_SRCS) because lib/pagewalk.c and lib/rss_atom.c now call it.

## Converted
- Private numeric/bool helper copies deleted: 29 (+ cadence's one-arg `env_long`, ffmpeg `env_nonempty`, collector_repair `env_truthy`, retention `env_pos`, github-leaks `env_int`, sanc `sanc_env_int` are all in that count).
- Call sites now on the reader: 237 (`grep jo_env_(ll|long|int|double|bool|str)` over core/lib/collectors/main.c).
- Inline `getenv`+`atoi/strtol/atol/atoll/strtod` idioms (about 90 sites: hostgate, httpd, httpclient, pipeline, xlsx, zipread, ws, pagewalk, rss_atom, jsonstream, overpass, thresholds env step, ~35 collectors incl. 3 generated vsrc files, main.c PORT) converted too.
- `core/thresholds.c`: resolution order untouched (DB > env > default); only `env_value()` now calls `jo_env_double_strict(name, NAN, min, max)` (NAN = no usable value, same "ignored if unparsable or out of range" rule).
- `core/retention_pod.c` keeps `env_days()`: a domain wrapper (<=0 = keep forever, floor with message) built on `jo_env_int`, not a parsing copy.

## getenv left (261 occurrences, was 387 incl. comments)
- Credential/key/token/contact lookups (about 220 incl. `jo_env()`): deliberately untouched (key_env, credtab, SEC_CONTACT).
- Presence flags where `=0` still means ON (`JO_NO_EVIDENCE`, `JO_NO_CONTENT_CHANGE`, `MEDIA_NO_TOOLS`, `JO_NO_FFMPEG`/`CAM_STILLS_NO_FFMPEG`, `JO_NO_SCHED`, `JO_NO_ALERT_*`, `JO_NO_EVIDENCE_GC`, `JO_SINK_TRACE`): converting to `jo_env_bool` would turn `=0` from "disabled" into "enabled"; behavior kept.
- Plain strings (URLs, file paths, model names, prefixes, dynamic names in statusapi/keysapi/threatintel/hpengine key_env): the reader returns the same pointer; left as is.

## Behaviour differences I could not avoid (all are garbage-input only, valid values behave identically)
- `atoi`/`strtol` prefix parsing ("12abc" -> 12) is gone: trailing garbage now means the default plus a one-time stderr line.
- Sites that mapped garbage to 0 or to a clamp bound now get the default: breach_monitor `JO_BREACH_MONITOR_MAX_HITS` (garbage was 1), ffmpeg `JO_FFMPEG_MAX_BYTES`/`_TIMEOUT_MS` (garbage was the floor), `JO_OSINT_FANOUT`, `JO_HOST_*`, `PORT` (garbage was 0 -> ephemeral port; now 4000).
- `strtol` overflow used to saturate silently; now clamped (or default for `_strict`) with a line. `long`-width reads clamp to int where the site cast to int.
- Sites that had NO bound and now have one (widest sane): github-leaks `GITHUB_LEAK_PER_QUERY` 1..100 (GitHub per_page cap), `THREATFOX_MIN_CONFIDENCE` 0..100, `JO_SOURCEAPI_SNAP_SEC` <= INT_MAX/1000 (the *1000 overflowed int), `PORT` 0..65535. Everything else unbounded keeps LONG_MAX/INT_MAX and, where the old site rejected <=0, uses `_strict` min 1 so "0 -> default" is preserved exactly.
- Doc count: the checked-in docs/ENVIRONMENT.md (467) was already stale at HEAD (`make env-doc-check` was failing before this change); the real HEAD scan is 474. New doc is 475 (474 + `JO_GEOJSON_PAGE_MAX`, now recognised as a string-passed knob). No variable was lost.
- The generated vsrc*.c collectors were edited in C (the C is the maintained copy), not regenerated.

## Gates (from native/, JO_DB scratch file)
- `make -j8`: exit 0, 0 `: warning:` / `: error:` lines in the log (both full builds).
- `make unit`: UNIT_EXIT 0, "all passed" (incl. new test_joenv; 111 test files).
- `make hptest`: rc=0. `make pagewalktest`: rc=0 ("all passed"). `make authtest`: rc=0. `make rsstest`: rc=0. `make selftest`: rc=0.
- `make lint-sources`: rc=0 ("lint_env_readers: 0 findings (one environment reader: lib/joenv.h)").
- `make env-doc-check`: "docs/ENVIRONMENT.md is current (475 variables)".
- `make source-floor`: "source floor ok: 105229 >= 105229".
- `make registry-floor`: "registry ok: 105229 >= 105229, 0 duplicates, 0 dropped, 0 never-runs".
- Not run: TSan/ASan builds, `make audit-sources`, `tools/ci_concurrency_gate.sh`, macOS.
