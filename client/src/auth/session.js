/**
 * Session storage + the global `/api` fetch interceptor.
 *
 * The iOS app keeps the bearer in `AuthTokenBox` and every `API.request()`
 * reads it. The web client has ~30 call sites that `fetch(apiUrl('/api/…'))`
 * directly, so instead of threading a header through each one the token is
 * attached here, once, by wrapping `window.fetch` for same-origin `/api`
 * requests. A 401 mid-session triggers ONE coalesced refresh and a retry —
 * Supabase refresh tokens are single-use, so concurrent 401s must share the
 * same refresh promise rather than race it.
 *
 * WEB-13: the refresh token is NOT stored, read or sent by this module. It
 * lives only in an httpOnly cookie the server sets on /api/auth/... (core/
 * authcookie.h); page script cannot read it. The access token is a module
 * variable. "Keep me signed in" only picks the cookie's lifetime (the server
 * gives it a Max-Age; otherwise it is a session cookie).
 */

import { createSupabaseAuth, AuthError, DEFAULT_SUPABASE_URL, DEFAULT_SUPABASE_ANON_KEY } from './supabase.js';
import apiUrl from '../utils/apiUrl.js';

const KEYS = {
  access: 'osint:auth:access',
  /** LEGACY: older builds stored the refresh token here (localStorage and
   *  sessionStorage). Read once to migrate into the cookie, then deleted. */
  refresh: 'osint:auth:refresh',
  /** Non-secret '1': a cookie session probably exists, so a reload should try
   *  to restore it. (httpOnly means script cannot ask the cookie itself.) */
  hint: 'osint:auth:session-hint',
  tenant: 'osint:auth:tenant',
  onboarded: 'osint:auth:onboarded',
  supabaseURL: 'osint:auth:supabase-url',
  supabaseAnon: 'osint:auth:supabase-anon',
  pkce: 'osint:auth:pkce-verifier',
  pkceNonce: 'osint:auth:pkce-nonce',
  pkceStarted: 'osint:auth:pkce-started',
  oauthReturn: 'osint:auth:oauth-return',
  // "Keep me signed in on this device" (a non-secret '1' flag, localStorage).
  keep: 'osint:auth:keep',
};

/** A social sign-in must come back within this long to be completed. */
export const OAUTH_FLOW_TTL_MS = 10 * 60 * 1000;

/** The access token lives in this variable and nowhere else: not in
 *  localStorage, not in sessionStorage. A reload drops it and bootstrap obtains
 *  a fresh one by calling /api/auth/refresh (the cookie rides along). */
let accessInMemory = null;

function lsGet(k) { try { return window.localStorage.getItem(k); } catch { return null; } }
function ssGet(k) { try { return window.sessionStorage.getItem(k); } catch { return null; } }
function ssSet(k, v) {
  try {
    if (v == null || v === '') window.sessionStorage.removeItem(k);
    else window.sessionStorage.setItem(k, v);
  } catch { /* private mode */ }
}
function randomNonce() {
  const b = new Uint8Array(16);
  crypto.getRandomValues(b);
  return Array.from(b, (x) => x.toString(16).padStart(2, '0')).join('');
}
function lsSet(k, v) {
  try {
    if (v == null || v === '') window.localStorage.removeItem(k);
    else window.localStorage.setItem(k, v);
  } catch { /* private mode / quota */ }
}

function ssRemove(k) { try { window.sessionStorage.removeItem(k); } catch { /* private mode */ } }

/** A refresh token an older build left in storage (either one), or null. */
function readLegacyRefresh() { return lsGet(KEYS.refresh) || ssGet(KEYS.refresh) || null; }
function deleteLegacyRefresh() { lsSet(KEYS.refresh, null); ssRemove(KEYS.refresh); }

export const sessionStore = {
  get accessToken() { return accessInMemory; },
  /** True when a refresh is worth attempting (a cookie session was
   *  established here, or a legacy stored token still awaits migration). */
  get hasSessionHint() { return lsGet(KEYS.hint) === '1' || Boolean(readLegacyRefresh()); },
  get keepSignedIn() { return lsGet(KEYS.keep) === '1'; },
  setKeepSignedIn(v) { lsSet(KEYS.keep, v ? '1' : null); },
  get tenantId() { return lsGet(KEYS.tenant); },
  get onboardingCompleted() { return lsGet(KEYS.onboarded) === '1'; },
  get supabaseURL() { return lsGet(KEYS.supabaseURL) || import.meta.env?.VITE_SUPABASE_URL || DEFAULT_SUPABASE_URL; },
  get supabaseAnonKey() { return lsGet(KEYS.supabaseAnon) || import.meta.env?.VITE_SUPABASE_ANON_KEY || DEFAULT_SUPABASE_ANON_KEY; },
  get pkceVerifier() { return lsGet(KEYS.pkce); },
  get oauthReturn() { return lsGet(KEYS.oauthReturn); },

  /** Access token -> memory only. `cookieSession` (truthy once the server has
   *  set the refresh cookie) records the non-secret hint that a reload may
   *  try to restore a session. No refresh token passes through here. */
  setTokens(access, cookieSession) {
    accessInMemory = access || null;
    lsSet(KEYS.hint, cookieSession ? '1' : null);
  },
  /** Clears the memory copy, the hint, the keep flag and any legacy key. (The
   *  cookie itself is cleared by the server, on /api/auth/logout or when it
   *  refuses the token.) */
  clearTokens() {
    accessInMemory = null;
    lsSet(KEYS.access, null); lsSet(KEYS.hint, null); lsSet(KEYS.keep, null);
    deleteLegacyRefresh();
    ssRemove(KEYS.access);
  },
  /**
   * One-time upgrade of a session written by an older build, which kept the
   * access token (and the refresh token) in localStorage. The access token is
   * moved into memory and removed from storage. A stored refresh token is NOT
   * adopted: it stays where it is only until the first refresh hands it to the
   * server once (`cookie: true`), which turns it into the httpOnly cookie, and
   * is then deleted (coalescedRefresh). A legacy localStorage token means the
   * user had ticked "keep me signed in", so the cookie gets that lifetime.
   */
  migrateLegacySession() {
    const legacyAccess = lsGet(KEYS.access);
    const hadLegacyRefresh = Boolean(lsGet(KEYS.refresh));
    if (legacyAccess == null && !readLegacyRefresh()) return false;
    if (legacyAccess != null) {
      if (!accessInMemory) accessInMemory = legacyAccess;
      lsSet(KEYS.access, null);
    }
    if (hadLegacyRefresh) lsSet(KEYS.keep, '1');
    return true;
  },
  setTenantId(id) { lsSet(KEYS.tenant, id); },
  setOnboardingCompleted(v) { lsSet(KEYS.onboarded, v ? '1' : null); },
  setSupabaseConfig(url, anon) { lsSet(KEYS.supabaseURL, url); lsSet(KEYS.supabaseAnon, anon); },
  setPkceVerifier(v) { lsSet(KEYS.pkce, v); },
  setOauthReturn(v) { lsSet(KEYS.oauthReturn, v); },

  /**
   * Record a social sign-in this TAB is starting. The verifier stays in
   * localStorage; a random flow nonce goes into BOTH localStorage and
   * sessionStorage. sessionStorage is per-tab and survives the round trip
   * through the provider, so a callback is completed only in the tab that
   * started it, and only within OAUTH_FLOW_TTL_MS. A link someone else sends,
   * opened in any other tab (or after the window), is not a flow we started.
   */
  beginOAuthFlow(verifier) {
    const nonce = randomNonce();
    lsSet(KEYS.pkce, verifier);
    lsSet(KEYS.pkceNonce, nonce);
    lsSet(KEYS.pkceStarted, String(Date.now()));
    ssSet(KEYS.pkceNonce, nonce);
    return nonce;
  },
  /** The pending flow if THIS tab started it and it has not expired, else null. */
  pendingOAuthFlow(now = Date.now()) {
    const verifier = lsGet(KEYS.pkce);
    const nonce = lsGet(KEYS.pkceNonce);
    const tabNonce = ssGet(KEYS.pkceNonce);
    const started = Number(lsGet(KEYS.pkceStarted));
    if (!verifier || !nonce || !tabNonce || tabNonce !== nonce) return null;
    if (!Number.isFinite(started) || now - started < 0 || now - started > OAUTH_FLOW_TTL_MS) return null;
    return { verifier, nonce, startedAt: started };
  },
  clearOAuthFlow() {
    lsSet(KEYS.pkce, null);
    lsSet(KEYS.pkceNonce, null);
    lsSet(KEYS.pkceStarted, null);
    ssSet(KEYS.pkceNonce, null);
  },
};

sessionStore.migrateLegacySession();

export function supabaseClient() {
  return createSupabaseAuth({ projectURL: sessionStore.supabaseURL, anonKey: sessionStore.supabaseAnonKey });
}

/**
 * How a refresh ended:
 *   'ok'         new tokens stored.
 *   'invalid'    the refresh endpoint REFUSED the token (HTTP 400/401), or
 *                there is none — the session is definitively over.
 *   'transient'  anything else: offline, timeout, 5xx, 429, an unreadable
 *                body. The tokens are kept; the session is not over.
 * Treating every failure as 'invalid' signed people out whenever Supabase
 * hiccuped or the laptop changed networks.
 */
export const REFRESH = { ok: 'ok', invalid: 'invalid', transient: 'transient' };

export function classifyRefreshError(e) {
  return e instanceof AuthError && (e.status === 400 || e.status === 401) ? REFRESH.invalid : REFRESH.transient;
}

/**
 * One call to the server's token-exchange routes (core/authcookie.h). The
 * ONLY place `credentials: 'include'` is used: the refresh cookie is scoped to
 * /api/auth/refresh and the server also requires the custom X-JO-Refresh
 * header, which a cross-site form cannot send. Answers carry the access token
 * (never the refresh token); a failure is an AuthError with the HTTP status.
 */
export async function authExchange(path, body, { bearer } = {}) {
  const headers = { 'Content-Type': 'application/json', 'X-JO-Refresh': '1' };
  if (bearer) headers.Authorization = `Bearer ${bearer}`;
  let res;
  try {
    res = await fetch(apiUrl(path), {
      method: 'POST', headers, credentials: 'include', body: JSON.stringify(body || {}),
    });
  } catch (e) {
    throw new AuthError(-1, `Could not reach the server (${e.message || 'network error'})`);
  }
  const text = await res.text();
  let json = null;
  try { json = text ? JSON.parse(text) : null; } catch { /* non-JSON body */ }
  if (!res.ok) {
    throw new AuthError(res.status, json?.message || json?.error || text || `request failed (${res.status})`);
  }
  return json || {};
}

function decodeSession(j) {
  if (!j || typeof j.access_token !== 'string' || !j.access_token) {
    throw new AuthError(0, 'Could not read the sign-in response.');
  }
  return { access_token: j.access_token, expires_in: j.expires_in ?? null, user: j.user ?? null };
}

export async function signInWithPassword(email, password, keep) {
  return decodeSession(await authExchange('/api/auth/login', { email, password, keep: Boolean(keep) }));
}

/** A session only when email confirmation is OFF, else { kind: 'confirmation_required' }. */
export async function signUpWithPassword(email, password, keep) {
  const j = await authExchange('/api/auth/signup', { email, password, keep: Boolean(keep) });
  if (j.confirmation_required) return { kind: 'confirmation_required' };
  return { kind: 'session', session: decodeSession(j) };
}

export async function exchangeOAuthCode(authCode, verifier, keep) {
  return decodeSession(await authExchange('/api/auth/oauth',
    { auth_code: authCode, code_verifier: verifier, keep: Boolean(keep) }));
}

/** Revoke (when we hold an access token) and ALWAYS clear the cookie. */
export async function serverLogout(accessToken) {
  await authExchange('/api/auth/logout', {}, { bearer: accessToken || undefined });
}

/** One in-flight refresh at a time; every concurrent 401 awaits the same one. */
let refreshInFlight = null;
export function coalescedRefresh() {
  if (refreshInFlight) return refreshInFlight;
  refreshInFlight = (async () => {
    // A token an older build stored is handed over exactly once so the server
    // can turn it into the cookie; it is deleted as soon as the server has
    // answered definitively (a transient failure keeps it for the retry).
    const legacy = readLegacyRefresh();
    try {
      const body = { keep: sessionStore.keepSignedIn };
      if (legacy) { body.refresh_token = legacy; body.cookie = true; }
      const s = decodeSession(await authExchange('/api/auth/refresh', body));
      sessionStore.setTokens(s.access_token, true);
      if (legacy) deleteLegacyRefresh();
      return REFRESH.ok;
    } catch (e) {
      const outcome = classifyRefreshError(e);
      if (legacy && outcome === REFRESH.invalid) deleteLegacyRefresh();
      return outcome;
    } finally {
      refreshInFlight = null;
    }
  })();
  return refreshInFlight;
}

/** Listeners told when the session dies (refresh failed after a 401). */
const deathListeners = new Set();
export function onSessionLost(fn) { deathListeners.add(fn); return () => deathListeners.delete(fn); }

function isApiRequest(input) {
  const url = typeof input === 'string' ? input : input?.url;
  if (!url) return false;
  // /api/auth/... is the token exchange itself: it must neither carry a bearer
  // nor trigger a 401->refresh retry (that would recurse).
  if (url.startsWith('/api/auth/')) return false;
  if (url.startsWith('/api/')) return true;
  try {
    const u = new URL(url, window.location.href);
    if (!u.pathname.startsWith('/api/') || u.pathname.startsWith('/api/auth/')) return false;
    // Same origin, or the configured API host override.
    const override = import.meta.env?.VITE_API_HOST;
    return u.origin === window.location.origin || (override && u.host === override);
  } catch { return false; }
}

let installed = false;
/** Idempotent. Wraps `window.fetch` so `/api` calls carry the session. */
export function installFetchInterceptor() {
  if (installed || typeof window === 'undefined') return;
  installed = true;
  const raw = window.fetch.bind(window);
  window.__joRawFetch = raw;

  window.fetch = async function joFetch(input, init) {
    if (!isApiRequest(input)) return raw(input, init);

    let sentToken = false;
    const build = () => {
      const headers = new Headers(init?.headers || (input instanceof Request ? input.headers : undefined));
      const tok = sessionStore.accessToken;
      if (tok && !headers.has('Authorization')) { headers.set('Authorization', `Bearer ${tok}`); sentToken = true; }
      const tid = sessionStore.tenantId;
      if (tid && !headers.has('X-Tenant-Id')) headers.set('X-Tenant-Id', tid);
      return { ...init, headers };
    };

    let res = await raw(input, build());
    if (res.status !== 401) return res;
    if (!sessionStore.hasSessionHint) {
      // A 401 with nothing to refresh with (an expired break-glass / API
      // token, or tokens cleared in another tab) is the end of the session
      // too: say so, so the app can ask for a sign-in instead of every page
      // showing "HTTP 401". Only when this request actually carried a token
      // of ours — an anonymous request answered 401 says nothing about a
      // session. (The check is on what WE attached, not a caller header.)
      if (sentToken) {
        sessionStore.clearTokens();
        deathListeners.forEach((fn) => { try { fn(); } catch { /* listener error */ } });
      }
      return res;
    }
    // Retry exactly once after a coalesced refresh. Only bodies that can be
    // re-sent are retried; a consumed stream is returned as the 401 it got.
    const bodyReplayable = !init?.body || typeof init.body === 'string' || init.body instanceof URLSearchParams
      || init.body instanceof FormData || init.body instanceof Blob || init.body instanceof ArrayBuffer;
    const outcome = await coalescedRefresh();
    if (outcome === REFRESH.invalid) {
      sessionStore.clearTokens();
      deathListeners.forEach((fn) => { try { fn(); } catch { /* listener error */ } });
      return res;
    }
    // Transient refresh failure: keep the session and hand back the 401; the
    // caller shows a failed request, and the next request tries again.
    if (outcome !== REFRESH.ok) return res;
    if (!bodyReplayable) return res;
    res = await raw(input, build());
    return res;
  };
}
