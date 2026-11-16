# Web client audit remainder (branch worktree-agent-a3c7e85b95d5956a4)

Base note: this worktree was cut from main, which lacks the earlier web fixes (RunControl etc.) that live on
claude/peaceful-ritchie-i7e0hj. Commit 1 imports client/ from that branch (main has no client/ changes since their
merge base); commit 2 is this work.

## Changes
- WEB-13 (auth/session.js, AuthContext.jsx, OnboardingFlow.jsx): access token in a module variable only. Refresh token in
  sessionStorage by default; localStorage only if "Keep me signed in on this device" is ticked (default off; copy beside the
  checkbox states a new tab / closed browser asks to sign in again). Legacy localStorage session migrated once on load: access
  token moved into memory and removed from storage, session treated as keep=true. clearTokens (sign-out, refresh refused)
  clears memory, both storages and the keep flag. Bootstrap now refreshes first when only a refresh token exists. OAuth passes the choice through the redirect.
- AdminPage "Re-run collector": uses RunControl (kind read from /api/sources/:id/detail): entity box for pivots, hidden for pods.
- WEB-05: see below. DOC-02: grep clean (only "~61k" in two code comments, no user-facing count; thresholds copy says measured 2026-10-23).

## WEB-13: protects / does not
Protects: no token at rest for the access token; with default mode nothing in localStorage, refresh token gone on tab/browser close;
a later XSS cannot read a stored access token or (default mode) a refresh token left behind by a closed session.
Does NOT protect: XSS while the tab is open still can read the in-memory/sessionStorage state or call the API (same-origin JS);
opt-in mode leaves a long-lived refresh token in localStorage, exactly as before; not httpOnly (needs server cookies); sign-out
revoke on server needs an access token, so after a reload before first refresh the server-side revoke is skipped (local clear still happens).

## Tests/build
npx vitest run: 32 files, 222 tests pass (6 new in auth/__tests__/auth.test.js: default writes nothing to localStorage, opt-in persists only refresh,
rotation, migration, sign-out clears both, new tab has no session). vite build to scratchpad/dist-web2: OK.

## WEB-05 server change needed (ApiKeysPage "used by")
No server way exists today. Search rows carry health.credentials_required only when the source's latest notice is needs_credential
(sourceapi.c ~465-479, h->env); no `key=` filter in /api/sources/search. The static key->source map is credtab.c (cred_get(id), req/any/opt
env names), surfaced only per source in /api/sources/:id/detail `credentials` (sourceapi.c:1599, statusapi_cred_json) and /api/status.
Needed: add to /api/sources/search (a) a filter `key=<ENV_NAME>` matching sources whose cred_get(id) req/any/opt contains it, and (b) a
per-row `credentials: {requires:[names], configured}` from cred_get, plus a facet count. Then ApiKeysPage can page `?key=NAME` and
show used-by for configured keys too. Page note kept as-is (honest).
