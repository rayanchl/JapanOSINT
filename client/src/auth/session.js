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
 */

import { createSupabaseAuth, DEFAULT_SUPABASE_URL, DEFAULT_SUPABASE_ANON_KEY } from './supabase.js';

const KEYS = {
  access: 'osint:auth:access',
  refresh: 'osint:auth:refresh',
  tenant: 'osint:auth:tenant',
  onboarded: 'osint:auth:onboarded',
  supabaseURL: 'osint:auth:supabase-url',
  supabaseAnon: 'osint:auth:supabase-anon',
  pkce: 'osint:auth:pkce-verifier',
  oauthReturn: 'osint:auth:oauth-return',
};

function lsGet(k) { try { return window.localStorage.getItem(k); } catch { return null; } }
function lsSet(k, v) {
  try {
    if (v == null || v === '') window.localStorage.removeItem(k);
    else window.localStorage.setItem(k, v);
  } catch { /* private mode / quota */ }
}

export const sessionStore = {
  get accessToken() { return lsGet(KEYS.access); },
  get refreshToken() { return lsGet(KEYS.refresh); },
  get tenantId() { return lsGet(KEYS.tenant); },
  get onboardingCompleted() { return lsGet(KEYS.onboarded) === '1'; },
  get supabaseURL() { return lsGet(KEYS.supabaseURL) || import.meta.env?.VITE_SUPABASE_URL || DEFAULT_SUPABASE_URL; },
  get supabaseAnonKey() { return lsGet(KEYS.supabaseAnon) || import.meta.env?.VITE_SUPABASE_ANON_KEY || DEFAULT_SUPABASE_ANON_KEY; },
  get pkceVerifier() { return lsGet(KEYS.pkce); },
  get oauthReturn() { return lsGet(KEYS.oauthReturn); },

  setTokens(access, refresh) { lsSet(KEYS.access, access); lsSet(KEYS.refresh, refresh); },
  clearTokens() { lsSet(KEYS.access, null); lsSet(KEYS.refresh, null); },
  setTenantId(id) { lsSet(KEYS.tenant, id); },
  setOnboardingCompleted(v) { lsSet(KEYS.onboarded, v ? '1' : null); },
  setSupabaseConfig(url, anon) { lsSet(KEYS.supabaseURL, url); lsSet(KEYS.supabaseAnon, anon); },
  setPkceVerifier(v) { lsSet(KEYS.pkce, v); },
  setOauthReturn(v) { lsSet(KEYS.oauthReturn, v); },
};

export function supabaseClient() {
  return createSupabaseAuth({ projectURL: sessionStore.supabaseURL, anonKey: sessionStore.supabaseAnonKey });
}

/** One in-flight refresh at a time; every concurrent 401 awaits the same one. */
let refreshInFlight = null;
export function coalescedRefresh() {
  if (refreshInFlight) return refreshInFlight;
  refreshInFlight = (async () => {
    const rt = sessionStore.refreshToken;
    if (!rt) return false;
    try {
      const s = await supabaseClient().refresh(rt);
      sessionStore.setTokens(s.access_token, s.refresh_token);
      return true;
    } catch {
      return false;
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
  if (url.startsWith('/api/')) return true;
  try {
    const u = new URL(url, window.location.href);
    if (!u.pathname.startsWith('/api/')) return false;
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

    const build = () => {
      const headers = new Headers(init?.headers || (input instanceof Request ? input.headers : undefined));
      const tok = sessionStore.accessToken;
      if (tok && !headers.has('Authorization')) headers.set('Authorization', `Bearer ${tok}`);
      const tid = sessionStore.tenantId;
      if (tid && !headers.has('X-Tenant-Id')) headers.set('X-Tenant-Id', tid);
      return { ...init, headers };
    };

    let res = await raw(input, build());
    if (res.status !== 401 || !sessionStore.refreshToken) return res;
    // Retry exactly once after a coalesced refresh. Only bodies that can be
    // re-sent are retried; a consumed stream is returned as the 401 it got.
    const bodyReplayable = !init?.body || typeof init.body === 'string' || init.body instanceof URLSearchParams
      || init.body instanceof FormData || init.body instanceof Blob || init.body instanceof ArrayBuffer;
    const ok = await coalescedRefresh();
    if (!ok) {
      sessionStore.clearTokens();
      deathListeners.forEach((fn) => { try { fn(); } catch { /* listener error */ } });
      return res;
    }
    if (!bodyReplayable) return res;
    res = await raw(input, build());
    return res;
  };
}
