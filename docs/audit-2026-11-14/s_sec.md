# SEC_CONTACT as a credential, personal address removed

Branch: worktree-agent-ad8c3113c4f3129d0 (commit hash in the final reply). Merged `claude/peaceful-ritchie-i7e0hj` (2f2e3ba) per coordinator; conflicts in PlatformAdmin.swift (took audit side: `isOperator`), keysapi.c (kept audit's checked 0600 overlay write + my env mirroring) and scheduler.c (kept both `hostgate_set_cancel`/body ceiling and the refusal reset).

## Mechanism
- `core/host_agents.def`: HA_SEC string is a template `JapanOSINT/1.0 research (%CONTACT%)`; no address in the tree.
- `core/httpclient.{c,h}`: `http_contact_ua()` resolves `$SEC_CONTACT_UA` (full string, wins) else `SEC_CONTACT` (bare address -> template, or full UA string), trimmed and validated by `http_contact_check()` (an `@` address with local part and dotted domain, no control chars incl. CR/LF, <=256). `host_agent_ua()` is still the single resolver (dump, builder, override accessor). Unset/invalid -> NULL: `http_request()` refuses SEC hosts with no network, sets a sticky thread-local `http_credential_refusal()` = "SEC_CONTACT". `--host-agents` prints `(unset: SEC_CONTACT)`.
- Keys: `core/credtab.c` lists SEC_CONTACT (required) on 15 SEC source ids (so it appears in GET /api/keys and the sources' credential blocks) and UNPAYWALL_EMAIL on the two Unpaywall rows. `core/keysapi.c`: `PUT /api/keys/SEC_CONTACT` validates (400 with reason), writes the overlay and mirrors set/clear into the process environment so it applies without restart (boot already applied overlay -> env); `$JO_API_KEYS_FILE` relocates the overlay (tests).
- Gating: hpengine files `jo_needs_credential` (collector-status-notice, status needs_credential, names SEC_CONTACT) instead of a fetch-failure when the client refused, rc 0; the 3 bespoke SEC collectors (sec_edgar, sec_filing_diff, osint_extras2 SEC_FULLTEXT) gate at the top like key-gated siblings; `core/scheduler.c` is the catch-all (any run that left nothing behind after a refusal -> same notice, rc 0). No `key_env` change was needed (key_env needs `{key}` in the URL).
- `tools/audit_registry_emit.py`: FETCH_FAILED test now `not gated` (the engine still prints its `[hp:] transport failure` stderr line); verdict GATED.
- `tools/probe_hp_batch.py`: builds the SEC UA from SEC_CONTACT/SEC_CONTACT_UA the same way; unset -> raises "needs SEC_CONTACT".
- Other personal-address uses: Crossref/OpenAlex `mailto=` dropped (optional); the two Unpaywall rows now `key_env = UNPAYWALL_EMAIL` with `?email={key}` (the API requires an email); iOS operator gate reads Info.plist `JOPlatformAdminEmail` (audit branch already replaced it with `isOperator`); 16+2 docs files replaced with `<contact address>`.
- Docs: CLAUDE.md SEC paragraph updated in place; `.env.example` has SEC_CONTACT/UNPAYWALL_EMAIL; `docs/ENVIRONMENT.md` regenerated (`make env-doc-check` current, 466 vars).

## Tests (native/, JO_DB scratch)
- `JO_UNIT_ONLY='host_agents|credtab_known|http_tls|sched_runline|source_settings' tests/unit/run.sh`: all passed. New cases in `tests/unit/test_host_agents.c`: unset -> needs SEC_CONTACT + http_request refused with 0 requests/0 hosts; env bare/trimmed/full-string; `$SEC_CONTACT_UA` wins and works alone; validation incl. CR/LF and bad env = unset; Keys API PUT (400s, listing in GET, set -> UA, fresh boot via keysapi_apply_overlay_env, clear); dump shows `(unset: SEC_CONTACT)`.
- `make hptest rsstest`: all passed (new 53-cred; stubs added to hpengine_test, rss_atom_test, fuzz_rss_atom).
- `make registry-floor`: ok 105229, 0 dup/dropped/never-runs. `make env-doc-check`: ok. `make lint-sources`: see final note in the reply (long-running).
- Build: `make -j2`; the 11 files I changed were rebuilt with the normal -O2 flags, 0 `warning:`. Most other objects of this fresh tree were produced with `-O0` (machine contention: 6 concurrent builds) so a clean from-scratch -O2 build was not repeated.

## Live
Unset (`env -u SEC_CONTACT -u SEC_CONTACT_UA ./bin/japanosint --run ...`):
- SEC_EDGAR_FRAMES: `[http] refused ...: SEC_CONTACT is not configured`, `[SEC_EDGAR_FRAMES] gated (SEC_CONTACT)`, `run rc=0 records=0 stored=1 notices=1 bytes=0 requests=0`; DB row `collector-status-notice` status needs_credential env_vars_required ["SEC_CONTACT"].
- SEC_EDGAR_SEARCH apple and SEC_RISK_FACTOR_DIFF apple: gated, rc=0, requests=0, notice stored.
- `audit_registry_emit.py --only SEC_EDGAR_FRAMES`: GATED ("needs SEC_CONTACT").
Set (`SEC_CONTACT=ops@example.org`): `--host-agents` -> `JapanOSINT/1.0 research (ops@example.org)` on www/data/efts.sec.gov. SEC_EDGAR_FRAMES: HTTP ok, 2681 of 2681 stored, 1 request. SEC_EDGAR_SEARCH apple: 1202 records, 4 requests. So SEC accepted the placeholder domain today.

## Operator after upgrading
Set SEC_CONTACT to an address you read (Keys screen -> platform key, or `SEC_CONTACT=` in .env / environment; or keep `SEC_CONTACT_UA` as a full string). Until then the 15 SEC sources show "needs SEC_CONTACT". Set UNPAYWALL_EMAIL for the two Unpaywall rows. Crossref/OpenAlex rows run without the polite-pool `mailto` (add it back per deployment if wanted). iOS: nothing needed (server decides operator). `git grep` finds the personal address nowhere; history is untouched.

Note: make lint-sources (tools/lint_sources.py) takes over an hour on this contended machine; its first run flagged dup-endpoint data.sec.gov +1 (my gate URLs duplicated a root URL). The three gate URLs were then made distinct paths (commit c096bad); the re-run had not finished at report time. All other checks at baseline.
