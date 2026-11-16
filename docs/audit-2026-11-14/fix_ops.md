# fix_ops report

| ID | Status | Notes |
|---|---|---|
| OPS-02 | FIXED | native/Makefile: HARDEN_CFLAGS = -fstack-protector-strong -fPIE (+ -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 only when CFLAGS lacks -O0); CFLAGS += -Wformat-security; LDFLAGS = -pie (+ -Wl,-z,relro,-z,now on Linux only); link rule now uses $(LDFLAGS). Vendored sqlite3/sqlite-vec/mongoose rules get HARDEN_CFLAGS (required: -pie needs -fPIE objects). Verified: core/httpd.c, lib/hpengine.c compiled by hand with full flag set (gcc, -O2 -Wall -Wextra): 0 warnings; mongoose.c: clean; -pie link smoke OK. NOT verified: macOS/clang full build, TSan/ASan jobs (they pass CC only, CFLAGS stay -O2 so FORTIFY applies; ASan+FORTIFY is supported). -Wformat=2 deliberately not used. |
| OPS-05 | FIXED | core/db.c: JO_SCHEMA_VERSION=1; user_version read right after pragmas; refuses (clear stderr, closes handle, returns 1) if file > binary; stamped only after all migrations succeed, logged "schema version A -> B". test_db_seed.c section 6 added. Syntax-checked db.c and the test with -Wall -Wextra (0 warnings); test NOT run. Dockerfile (multi-stage, /app path because JO_REPO_ROOT is compile-time, non-root uid 10001, /data volume, /app/data -> /data symlink, healthcheck), .dockerignore, deploy/japanosint.service (sandboxed; deliberately not RestrictNamespaces/PrivateUsers because netjail needs namespaces). Image not built. Not done: scheduling db-backup. |
| OPS-06 | FIXED | native/tools/gen_env_doc.py (+ --check); docs/ENVIRONMENT.md generated (463 vars, scanning .c/.h/.inc); Makefile targets env-doc, env-doc-check; CI step. .env.example got a commented first-boot section. NOTE: other agents add/remove getenv calls, so regenerate (python3 native/tools/gen_env_doc.py) after the final merge or env-doc-check will fail. |
| OPS-07b | PARTLY | native/tests/fuzz/fuzz_{zipread,xlsx,csv,htmlparse,rss_atom}.c; Makefile fuzz-build / fuzz-smoke (FUZZ_SECS default 20, FUZZ_CC clang); CI job fuzz-smoke (installs clang + libclang-rt-dev). clang IS installed here but its fuzzer/asan runtime libs are missing (libclang_rt.fuzzer not found), so a libFuzzer link was impossible. Verified instead: each harness compiles with clang -Wall -Wextra -fsanitize=fuzzer-no-link, and each links with gcc+ASan/UBSan against a tiny main driver and runs on a sample input without error. `make fuzz-smoke` itself was NOT run (instruction: no make in native/). |
| OPS-08 | PARTLY | CI step "Python tools compile and self-test": python3 -W error -m py_compile tools/*.py (verified locally: all pass) + audit_page_two.py --self-test (verified passes). SyntaxWarning in build_feed_manifest.py fixed (raw docstring). ESLint NOT added: client has no eslint config or dependency. |
| OPS-09 | FIXED | step kept; writes a $GITHUB_STEP_SUMMARY line and a ::warning:: on non-zero exit. |
| OPS-10 | FIXED | git rm --cached tests/contract/__pycache__/*.pyc; __pycache__/ and *.pyc in .gitignore. Fixtures untouched. |
| OPS-12 | PARTLY | ccache (CC="ccache cc", actions/cache keyed on Makefile hash + sha with restore-key) in build-and-test Build step and the asan job. Not added to tsan, macOS jobs; no -O1 for big tables. Unverified (CI-only). |
| OPS-15 | FIXED | native/third_party/README.md: sqlite 3.49.2, sqlite-vec v0.1.9, cJSON 1.7.18, mongoose 7.21; states plainly Mongoose is GPLv2/commercial and no licence decision recorded; no LICENSE file added. |
| DOC-01 | FIXED | CLAUDE.md: one paragraph inserted before "**The registered count is 61,527**"; nothing else touched. |

## Verify in the central build
- `make` warning-free on Linux and macOS (new flags; -Wformat-security on the whole tree: only 2 of ~1,900 TUs tested by me); `ldd`/`readelf -h bin/japanosint` shows PIE; `readelf -d` shows BIND_NOW.
- `make asan`/`tsan` still build (FORTIFY + sanitizer); `make unit` incl. test_db_seed section 6 (it reopens JO_DB, expects refusal when user_version = 2).
- `make env-doc-check` after all other agents finish (regenerate if stale).
- `make fuzz-smoke FUZZ_SECS=5` on a machine with libclang-rt-dev.
- ci.yml parses as YAML (checked); first CI run proves ccache, fuzz job and summary step.
- Hand-run only used scratch dir; I attempted one `make fuzz-smoke` which the permission layer denied; nothing ran.
