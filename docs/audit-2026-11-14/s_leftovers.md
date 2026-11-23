# Leftovers report (WEB-05, realtime policy, pagination probe, API-02 tests)

Branch: claude/peaceful-ritchie-i7e0hj (worktree agent-a1588d735f8da3661). Not pushed, no PR.

## 1. WEB-05  `GET /api/sources/search?key=<NAME>`

What changed
- `sourceapi_query` gains `credkey`; `core/httpd.c` parses `key=` (also in the response-cache hash, so two keys never share a cached body).
- `core/sourceapi.c`: `source_needs_key()` is the filter. It uses only the registry's own declarations: hpengine `hp_source.key_env` (via `hp_source_get`), the credtab `req` and `any` rows (`cred_get(id)`, the table `/api/keys` and statusapi read; `opt` is NOT "needs"), and what a run recorded (the gated notice's `env_vars_required`, i.e. `health.env`, which is how hand-written collectors that gate in code say so). Exact, case-sensitive match.
- In-band count (rule 2): `meta.credential = {name, sources, shown, basis}` plus `meta.filters.key`; `sources` equals `page.total`. The block is absent when no `key` filter is sent.
- Web: key sheet "Used by · N sources" is now a paged `/api/sources/search?key=NAME&sort=id` list with "showing X of N" (PagedFooter) and an "Open in Sources" link to `/console/sources?key=NAME`; `key` added to `SEARCH_KEYS`, and the Sources page shows a "needs NAME x" chip. The per-key "N sources waiting" tag in the key list is unchanged (still the health=gated query; one request per key would be 89 requests).
- iOS: `SourceSearchQuery.key`, `searchSources` sends it, `ApiKeyDetailView.loadUsing` is one request, header "Used by N sources", footer says "Showing X of N".
- Files: native/core/{httpd.c,sourceapi.c,sourceapi.h}; native/tests/unit/test_sourceapi_key_filter.c (new); client/src/components/console/ApiKeysPage.jsx, sources/SourcesListPage.jsx, utils/sourceFormat.js, components/__tests__/apiKeysUsedBy.test.jsx (new); iOS below.
- Edits in sourceapi.c/httpd.c are additive and local (one helper before `hit_matches`, one line in `hit_matches`, `filters`/`meta` additions in `search_obj`, 4 lines in httpd.c sa_*).

Test evidence (verbatim)
```
hp row kr-constitutional-court-decisions needs KR_LAW_GO_KR_OC; credtab row US_SEC_ALJ_ACTIONS needs SEC_CONTACT
test_sourceapi_key_filter: all passed
```
Covers: hpengine key_env row found; credtab req row found; optional var not a need; recorded notice adds a source no declaration names; exact not substring; unknown key -> page.total 0 and meta.credential.sources 0; 1-shown-of-N states N; no key -> no meta.credential; expected set computed independently from the same registry declarations equals the response total.
Web: `apiKeysUsedBy.test.jsx` (1 test) passes.

Unverified / caveats
- The `http_host_needs_credential` host-level credential (SEC_CONTACT via host_agents.def) is not a declaration read by the filter (that function only answers while the credential is unset). SEC rows are found through credtab (US_SEC_ALJ_ACTIONS) or a recorded gated notice only. Not every SEC row may be listed until it has run.
- Hand-written collectors that gate in code with no credtab row and have never run are not discoverable (nothing declares them).
- Live HTTP route not exercised (unit-level on `sourceapi_search`); httpd.c arg parsing is a 3-line change compiled and linked.

## 2. cadence policy `realtime`

- Web: `policyLabel()` + `POLICY_LABELS` in utils/sourceFormat.js (declared/global/clamped/realtime, unknown strings shown as-is); SourceDetailPage "policy interval" row uses it; CadenceCard: `describePolicy` says which sources a global interval leaves alone, and the edit form gets a "real-time limit (s)" field (`realtime_max_sec`, 0 = off) saved with the PUT (the other three cadence settings were editable there, so this one is too).
- iOS (read-only cadence there; edited on web): `CadencePolicy.realtime_max_sec`, `SourceFormat.policy(_:)`, schedule "Policy" row uses it, "Real-time limit" row in the System-wide cadence section. No iOS editor exists for the other cadence settings, so none added.
- Tests: sourcePages.test.jsx new describe "cadence policy: realtime" (labels, describePolicy, searchPath key, operator edits the real-time limit and the PUT body carries realtime_max_sec: 120). Full vitest: `Test Files 33 passed (33) / Tests 227 passed (227)`.
- Noticed, not changed (scheduler/sourceapi territory): `cadenceBasis` in sourceapi.c and `basis` in source_interval.c print "policy" for a realtime row although the policy exempted it (interval = declared).

## 3. pagination_probe uses the one pager list

- `lib/pagewalk.{c,h}`: new accessors `pw_declared_size_param(url,&size)` (first PW_PAGERS size spelling with PWP_DECL in the URL) and `pw_url_has_cursor(url)` (PW_CURSORS rows flagged PWC_SEED).
- `core/pagination_probe.c`: `SIZE_PARAMS`, `OFF_PARAMS`, `find_num_param` deleted; the "page-numbered?" test (`per_page/page_size/pageSize/itemsPerPage`) now reads `pw_pager_find(sp)->page_numbered`. The spec-family table (`FAMILIES`) and the GENERIC candidate parameter lists remain: they are probe *proposals* (what to try next), not the walker's size/cursor table.
- `tests/paging_core_single.py`: the EXEMPT entry for the probe removed (EXEMPT = {}), new "probe size list" check (6 of the old SIZE_PARAMS spellings in a non-pagewalk file fails) with a selftest fixture of the old array.
- Evidence: `paging core: one declared-total list, one pager table, one next-link list` (ran inside make pagewalktest). Before the probe edit the gate printed `FAIL: ./core/pagination_probe.c carries a second pager table`, so the new rule fails on the old copy.
- Behaviour change (unmeasured): the probe's candidate set is now the walker's PWP_DECL size spellings (adds `num`, `length`, `rpp`, drops `size`-only differences none; `from` is no longer treated as an existing cursor, per pagewalk's own comment). No unit test exercises `pagination_probe_candidates()`; candidate count before/after not measured.

## 4. API-02 follow-up tests  (tests/unit/test_run_gate_and_user_cap.c, new, includes core/httpd.c)

(a) `collector_run_gate`: personal-workspace owner refused (403 naming the personal workspace); team owner/admin pass; viewer refused for role; pod operator-only even for a team owner; `tenants.personal=1` with 2 members is a team workspace and with 1 member is personal again; unknown/empty tenant fails closed; foreign X-Tenant-Id is not_a_member; per-key budget 429 on the 11th; platform operator passes from a personal workspace and for a pod, per user.
(b) per-user worker cap: default 4; 5th admit refused with the per-user flag; `reply_busy` writes a 429 `too_many_requests_in_flight` (not 503); another user unaffected; slots held by real threads via `worker_spawn` and returned through `worker_release`; capacity reusable; no-uid request charged to pool only.
- Both behaved correctly: no behaviour fix needed.
- Mutation check (test can fail): disabling the personal gate -> `Assertion gate(..."sub-p"...) == 403 failed`; changing the cap comparison -> `Assertion worker_admit() == 0 && "the fifth worker of one user is refused" failed`. Source restored afterwards (`git diff` of httpd.c shows only the 4 credkey lines).
- Evidence: `test_run_gate_and_user_cap: all passed`.
- Not tested: `JO_HTTP_USER_WORKERS=0` (cap is read once into a static), the live HTTP 429 path end to end, the global 503 path.

## Verification (from native/, JO_DB on a scratch file)
- `make -j4`: exit 0, 0 warnings in the files built in this worktree. (An earlier shared-log reading showed a camera_feed.c format-truncation warning; the shared scratchpad log was being written by a concurrent agent's build, so I do not attribute it. My own rebuild log lo_build.log had 0 `warning:` lines, but it only recompiled what was stale.)
- `make unit hptest pagewalktest authtest lint-sources source-floor registry-floor audit-sources`: `VERIFY_EXIT 0`.
  - `lint-sources: OK`; `source floor ok: 105229 >= 105229`; `registry ok: 105229 >= 105229, 0 duplicates, 0 dropped, 0 never-runs`.
  - `audit-sources`: `strict set ...: 397 files, 0 finding(s)`, BUT the tree-wide scan reports `findings: 1` (`collectors/sources/bird_flu_outbreaks.c: record-cap`), a file this work did not touch. CLAUDE.md expects 0; that one is pre-existing and left alone.
- client: `npx vitest run` 33 files / 227 tests passed; `npx eslint .` 0 errors, 141 warnings (pre-existing react-hooks/unused-var kinds); `npx vite build` built in 1m10s (chunk-size notice only). `npm ci` was run.
- A first full vitest run while the C build was saturating the 4 CPUs showed 2 timeouts (SourcesListPage, SavedSearchesPage); both passed on rerun with the CPU free.

## Swift files touched: written, NOT compiled (no Swift toolchain)
- ios/JapanOsintApp/API+Sources.swift (SourceSearchQuery.key, query item, isFiltered)
- ios/JapanOsintApp/ApiKeys/ApiKeyDetailView.swift (loadUsing rewritten, section texts)
- ios/JapanOsintApp/SourceModels.swift (CadencePolicy.realtime_max_sec, SourceFormat.policy)
- ios/JapanOsintApp/Dashboard/SourceDetailView.swift (policy label, Real-time limit row)

## Left undone
- No live-server smoke of `/api/sources/search?key=` over HTTP.
- No web "N sources" count per row of the key list (would be one request per key).
- iOS has no link into a filtered Sources list (the sheet lists the first 50 with "Showing X of N").
