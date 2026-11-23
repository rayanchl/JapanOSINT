# OPS-08 / OPS-12 / OPS-13 — finish pass

State found: commit fecea37 on the branch had already added client/eslint.config.js, the
`lint` script, the CI lint step, ccache on TSan + macOS, and a `jo_audit.py` dispatcher.
This pass verified those and did the real consolidation OPS-13 still lacked.

## OPS-08 (ESLint) — done, verified here
- `client/eslint.config.js` (flat; @eslint/js recommended + react + react-hooks), pinned devDeps and lockfile present;
  `npm run lint` = `eslint src`; CI step "Client lint (ESLint)" in web-client job.
- `npm ci && npx eslint .` -> `✖ 141 problems (0 errors, 141 warnings)`.
- Warnings per rule (all downgraded to warn, none edited because client/ app code is off-limits for this task):
  react-hooks/set-state-in-effect 59, no-unused-vars 31 (14 in src/utils/layerIcons.js, rest unused imports/vars in 13 components),
  react/no-unescaped-entities 13, react-hooks/refs 12, react/jsx-key 10, react-hooks/exhaustive-deps 7,
  react-hooks/use-memo 4, react-hooks/static-components 2, react-hooks/purity 2, 1 unattributed (--fix-able directive).
- `npx vite build`: `✓ built in 43.06s`.
- `npx vitest run`: full run under load gave 2 failed / 220 passed (first run) and 1 failed (second): `pagedLists.test.jsx > SavedSearchesPage ... Test timed out in 5000ms`
  (machine was also compiling). Re-run alone: `Tests 12 passed (12)`. Timeout flake under CPU load, not an assertion failure; not widened.
  Not re-run to a clean full pass on a quiet machine.

## OPS-12 (ccache) — already in place, verified by reading
- Distinct keys: `ccache-<os>-gcc-` (main), `ccache-tsan-…`, `ccache-asan-…`, `ccache-macos-…`; each with `CC: ccache cc`, own CCACHE_DIR/MAXSIZE; macOS installs via brew.
- Cannot verify hit rates: CI cannot run here.
- Workflow parses (`yaml.safe_load` ok). Every `make <target>` CI calls resolves with `make -n` (the only "missing" matches, `ccache`/`hw`, are English words in comments).

## OPS-13 (tool overlap) — consolidated
Merged (code, not just names):
- NEW `native/tools/jo_registry.py`: the single `warm_template` / `checkpoint` / `unlink_db` / `list_sources` / `parse_list_sources`.
  Removed the five private copies from audit_registry_emit, audit_batch_emit, audit_cycle_health, audit_page_ceiling
  (audit_cycle_advance now imports from jo_registry). Drift unified: template build now fails on non-zero rc OR missing file,
  has a timeout, and checkpoint raises on error by default (callers that tolerated errors previously were silently ignoring a failed fold).
- `manifest.py`: new `iter_rows(paths)`; audit_batch_pagination.rows and audit_batch_reachable.rows (each hand-rolled the
  CRLF/field-count/comment loop) now delegate to it / `iter_lines`. audit_batch_emit already used `iter_lines`.
Kept separate (and why):
- audit_page_param.py: scans C source text, not manifests or the binary — different input.
- audit_page_ceiling / audit_cycle_health / audit_cycle_advance / audit_page_two / audit_batch_pagination: different questions
  (live ceiling measurement, resume-cycle health, cycle advance, raw HTTP page-2 probe, static manifest paging lint); their verdict logic is not duplicated, only the plumbing above was.
- probe_hp_batch (network probe) vs audit_batch_emit (engine) vs audit_registry_emit (registry sweep): CLAUDE.md rule 4 makes them distinct gates; no names removed.
- `jo_audit.py` dispatcher (from fecea37) unchanged; all original script names still work.
Tests: NEW `native/tools/test_audit_tools.py` (offline, fake `japanosint` shell script) — 6 tests, `Ran 6 tests ... OK`:
manifest.iter_rows (CRLF, comment, malformed), jo_registry self-test + helpers, audit_batch_emit verdicts (OK / DROPS_EVERYTHING),
audit_registry_emit end-to-end on the fake binary, batch_reachable + batch_pagination report MALFORMED, jo_audit --self-test.
New `make tools-selftest` (py_compile -W error + that test) and a CI step in "Python tools compile and self-test".
`python3 -W error -m py_compile tools/*.py` passes (via the target).

## Not verified
- CI itself (ccache restore, lint step, new test step).
- Full `make -j8` from scratch: started, but ~1,900 objects was too slow here; killed. Gates were run against a copy of an already built
  binary (native/bin/japanosint from the main checkout, untracked) — valid for the Python tool changes, not for any C state of this branch.
- audit_cycle_health / audit_page_ceiling / audit_cycle_advance are exercised only via `--help` (import path) and jo_registry tests, not a fake-binary run.
- Gate results: see final reply.
