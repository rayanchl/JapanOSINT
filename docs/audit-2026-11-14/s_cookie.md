# s_cookie — WEB-13 (httpOnly refresh cookie) and API-07 (lock-wait deadline)

Branch claude/peaceful-ritchie-i7e0hj worktree. Commits: 5272c08 (API-07), 74bd600 + 4ee4148 (WEB-13; head 4ee4148). Nothing pushed.

## Finding: the server has no refresh route of its own
Sessions come from Supabase GoTrue; core/auth.c only verifies the access JWT. The web client called GoTrue directly
(auth/supabase.js) and iOS does too (SupabaseAuth.swift, token in KeychainStore). So the cookie path is a small
token-exchange in front of GoTrue: core/authcookie.{c,h}, rows in core/routes.c, handler in core/httpd.c.
Needs SUPABASE_URL (already used for JWKS) and the NEW SUPABASE_ANON_KEY (publishable key); unset = 501
auth_exchange_not_configured. This is a deployment requirement: until the server has SUPABASE_ANON_KEY the web client cannot sign in or refresh.

## WEB-13
Routes (public rows, run on a worker thread, reply through the wakeup path, extended to carry status + Set-Cookie):
POST /api/auth/login | signup | oauth (PKCE code) | refresh | logout.
- Cookie: `jo_rt=<token>; Path=/api/auth/refresh; HttpOnly; SameSite=Strict` + `Secure` (TLS, or peer in JO_TRUSTED_PROXIES with
  right-most X-Forwarded-Proto https; new ratelimit_peer_trusted) + `Max-Age` (JO_REFRESH_COOKIE_MAX_AGE_SEC, default 30 d) only when
  the body said keep:true. Token with a byte a cookie cannot carry is refused (502), never emitted.
- JSON answers carry access_token/expires_in/user and never the refresh token. Upstream 400/401/422/429 pass through; 403 -> 401; transport/5xx -> 502.
  A refused cookie token on refresh also clears the cookie. Logout always clears the cookie; revokes upstream (scope=local) when given a Bearer.
- CSRF (ac_guard): X-JO-Refresh: 1 required; Origin, if present, must be this server's own host or on JO_CORS_ORIGINS ("null"/look-alikes refused);
  Sec-Fetch-Site same-site/cross-site only passes with a listed Origin, unknown values fail closed. Applied to login/signup/oauth/logout and to any
  refresh that presents a cookie (or asks {cookie:true}). Refresh with NO cookie and a body refresh_token (native) is unguarded and answered with the
  token in the JSON body, no cookie. Pre-auth throttle: RL_AUTH_EXCHANGE (fail-closed class), 20/min credential routes, 120/min refresh/logout, per client key.
- CORS: Access-Control-Allow-Credentials: true only on these routes and only for a listed origin (cors_credentials_headers, auth preflight variant);
  origin reflected, never *; X-JO-Refresh added to Allow-Headers. Other routes unchanged.
- iOS: verified by reading ios/ (SupabaseAuth.swift talks to GoTrue directly; refresh token in Keychain, Account.refreshToken). Not touched, unaffected.
- Web client (client/src/auth): no refresh token stored/read anywhere; `credentials: 'include'` only in authExchange (/api/auth/...); interceptor skips /api/auth/;
  access token in memory; bootstrap calls refresh when a non-secret session-hint flag exists; keep-me-signed-in only sends keep:true (cookie lifetime).
  Legacy migration: a refresh token left by the old build (localStorage or sessionStorage) is sent once with {cookie:true}, turned into the cookie, and
  deleted on success or refusal (kept for a retry only on a transient failure). Sign-out always calls /api/auth/logout so the cookie goes.
  supabase.js keeps only settings(), oauthAuthorizeURL and parseOAuthCallback (the code exchange is server-side now).
- Tests: tests/unit/test_authcookie.c (every accept shape and each rejection, cookie flags, jar parsing, Secure rules, 501/400/401 without network),
  test_cors.c (credentials), test_routes.c (8 public rows); client auth.test.js rewritten (32 auth tests).

Limitations stated plainly:
- XSS while the page is open can still call the API (same-origin JS) and holds the in-memory access token; it can no longer steal a refresh token.
- Logout revokes upstream only when an access token exists (after a reload before the first refresh finishes it only clears the cookie).
- OnboardingFlow still lets a user override the Supabase URL/anon key client-side; that now affects only settings() and the authorize URL, the
  exchange uses the server's SUPABASE_URL.
- The live exchange was tested against a local stub GoTrue, not a real Supabase project.

## API-07
core/loopdl.c: sqlite3_busy_handler on the loop connection (replaces busy_timeout there). Armed: gives up at the armed deadline (marks tripped, SQLITE_BUSY,
httpd answers the same 503 query_deadline_exceeded). Not armed: waits like the 5000 ms busy_timeout it replaces (LOOPDL_BUSY_FALLBACK_MS).
Test (test_loopdl.c): second connection holds an exclusive lock; armed 150 ms gives up in ~154 ms, reported; free lock acquired unarmed; lock released inside
a 4 s deadline succeeds. Passes.

## Verification (from native/, scratch DBs deleted after each gate)
- make -j6: 0 warnings. make authtest, hptest, lint-sources, env-doc-check: pass (docs/ENVIRONMENT.md regenerated, 480 variables: SUPABASE_ANON_KEY, JO_REFRESH_COOKIE_MAX_AGE_SEC).
- tools/ci_concurrency_gate.sh ./bin/japanosint 1.0: ok (worst health 0.0094 s).
- make unit: one uninterrupted run on the final tree, 127 test files, all passed. (An earlier run caught test_routes expecting exactly 3 public rows and the
  route-chain scan rejecting a literal /api/auth/ matcher; fixed in 4ee4148 and the full suite re-run.)
- client: npm ci; npx eslint . = 0 errors (57 warnings, none in src/auth); npx vitest run = 33 files / 238 tests pass; npx vite build ok.

## Live smoke (binary on 127.0.0.1:4391, stub GoTrue on 4392, JO_CORS_ORIGINS=https://app.example.com)
login keep=false      -> 200, Set-Cookie: jo_rt=rt1; Path=/api/auth/refresh; HttpOnly; SameSite=Strict
login keep=true       -> 200, Set-Cookie: jo_rt=rt2; Path=/api/auth/refresh; HttpOnly; SameSite=Strict; Max-Age=2592000
login wrong password  -> 400 {"error":"invalid_credentials",...}, no cookie
login no X-JO-Refresh -> 403 {"error":"csrf_header_missing"}
refresh cookie, no header     -> 403 csrf_header_missing
refresh cookie, evil Origin   -> 403 origin_not_allowed
refresh cookie + header       -> 200, Set-Cookie: jo_rt=rt3; ...; Max-Age=2592000 (rotated)
refresh replay of old cookie  -> 400 refresh_token_not_found, Set-Cookie: jo_rt=; Max-Age=0; Expires=1970 (cleared)
native body refresh_token, no cookie, no header -> 200, refresh_token in JSON body, no Set-Cookie
preflight listed origin -> 204, Allow-Origin: https://app.example.com, Allow-Credentials: true, Allow-Headers incl. X-JO-Refresh
login from listed origin -> Allow-Credentials: true + reflected origin; unlisted origin -> 403, no Allow-Credentials
logout with bearer    -> 200 {"ok":true,"revoked":true}, cookie cleared
GET /api/auth/login   -> 405 Allow: POST
Full output: /tmp/claude-0/-home-user-JapanOSINT/c20195ab-72c3-5bbf-bc8d-73ece67936f3/scratchpad/smoke_out.txt (script smoke.sh, stub stub_gotrue.py beside it).
