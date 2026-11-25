// Regression (findings 5 and 13): social sign-in completes only a PKCE flow
// this tab started; a token in the URL fragment is never adopted; only a
// definitive refusal from the refresh endpoint ends a session; sign-out
// revokes the session server-side and clears per-account browser data.
// WEB-13: the refresh token is an httpOnly cookie the server owns; page script
// never stores, reads or sends one (apart from migrating a legacy value once).
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { createSupabaseAuth, AuthError } from '../supabase.js';
import {
  sessionStore, OAUTH_FLOW_TTL_MS, classifyRefreshError, coalescedRefresh, REFRESH,
  authExchange, signInWithPassword, signUpWithPassword, exchangeOAuthCode, serverLogout,
} from '../session.js';
import { claimLocalData, releaseLocalData, MAP_RECENT_SEARCHES_KEY } from '../localData.js';
import { savedStore } from '../../store/savedStore.js';

const realFetch = globalThis.fetch;
const auth = createSupabaseAuth({ projectURL: 'https://proj.supabase.test', anonKey: 'anon' });

beforeEach(() => { localStorage.clear(); sessionStorage.clear(); sessionStore.clearTokens(); });
afterEach(() => { globalThis.fetch = realFetch; vi.restoreAllMocks(); });

describe('parseOAuthCallback', () => {
  it('refuses implicit-flow tokens in the fragment instead of adopting them', () => {
    globalThis.fetch = vi.fn();
    expect(() => auth.parseOAuthCallback('https://app.test/#access_token=ATTACKER&refresh_token=R', 'verifier'))
      .toThrow(/implicit flow/);
    expect(globalThis.fetch).not.toHaveBeenCalled();
  });

  it('needs a verifier at all', () => {
    expect(() => auth.parseOAuthCallback('https://app.test/auth/callback?code=abc', null)).toThrow(/verifier/);
  });

  it('returns ?code= with the verifier (PKCE) for the server exchange', () => {
    expect(auth.parseOAuthCallback('https://app.test/auth/callback?code=abc', 'v1'))
      .toEqual({ auth_code: 'abc', code_verifier: 'v1' });
  });
});

describe('pending OAuth flow (tab-bound state nonce + TTL)', () => {
  it('is pending only in the tab that started it', () => {
    sessionStore.beginOAuthFlow('v1');
    expect(sessionStore.pendingOAuthFlow()?.verifier).toBe('v1');
    // Another tab: same localStorage, empty sessionStorage.
    sessionStorage.clear();
    expect(sessionStore.pendingOAuthFlow()).toBeNull();
  });

  it('expires after the TTL', () => {
    sessionStore.beginOAuthFlow('v1');
    expect(sessionStore.pendingOAuthFlow(Date.now() + OAUTH_FLOW_TTL_MS + 1000)).toBeNull();
  });

  it('a verifier left by the old code path (no nonce) is not a pending flow', () => {
    sessionStore.setPkceVerifier('legacy');
    expect(sessionStore.pendingOAuthFlow()).toBeNull();
  });

  it('clearOAuthFlow removes every trace', () => {
    sessionStore.beginOAuthFlow('v1');
    sessionStore.clearOAuthFlow();
    expect(sessionStore.pendingOAuthFlow()).toBeNull();
    expect(sessionStore.pkceVerifier).toBeNull();
  });
});

describe('refresh failure classification', () => {
  it('only 400/401 from the refresh endpoint is definitive', () => {
    expect(classifyRefreshError(new AuthError(400, 'invalid_grant'))).toBe(REFRESH.invalid);
    expect(classifyRefreshError(new AuthError(401, 'x'))).toBe(REFRESH.invalid);
    expect(classifyRefreshError(new AuthError(-1, 'offline'))).toBe(REFRESH.transient);
    expect(classifyRefreshError(new AuthError(503, 'x'))).toBe(REFRESH.transient);
    expect(classifyRefreshError(new AuthError(429, 'x'))).toBe(REFRESH.transient);
    expect(classifyRefreshError(new TypeError('boom'))).toBe(REFRESH.transient);
  });

  it('a network failure during refresh keeps the session', async () => {
    sessionStore.setTokens('A', true);
    globalThis.fetch = vi.fn(async () => { throw new TypeError('Failed to fetch'); });
    expect(await coalescedRefresh()).toBe(REFRESH.transient);
    expect(sessionStore.hasSessionHint).toBe(true);
  });

  it('a refused refresh is invalid', async () => {
    sessionStore.setTokens('A', true);
    globalThis.fetch = vi.fn(async () => new Response('{"error":"refresh_token_not_found"}', { status: 400 }));
    expect(await coalescedRefresh()).toBe(REFRESH.invalid);
  });

  it('a 501 from the exchange (not configured) is transient', async () => {
    globalThis.fetch = vi.fn(async () => new Response('{"error":"auth_exchange_not_configured"}', { status: 501 }));
    expect(await coalescedRefresh()).toBe(REFRESH.transient);
  });
});

describe('server token exchange (WEB-13)', () => {
  const okSession = () => new Response(JSON.stringify({ access_token: 'ACC', expires_in: 3600, user: { id: 'u' } }), { status: 200 });

  it('every auth call is a POST with credentials: include and the X-JO-Refresh header', async () => {
    globalThis.fetch = vi.fn(async () => okSession());
    await signInWithPassword('a@b.c', 'pw', true);
    const [url, init] = globalThis.fetch.mock.calls[0];
    expect(String(url)).toBe('/api/auth/login');
    expect(init.method).toBe('POST');
    expect(init.credentials).toBe('include');
    expect(init.headers['X-JO-Refresh']).toBe('1');
    expect(JSON.parse(init.body)).toEqual({ email: 'a@b.c', password: 'pw', keep: true });
  });

  it('refresh sends keep only (the cookie rides along) and stores the access token in memory', async () => {
    sessionStore.setKeepSignedIn(true);
    globalThis.fetch = vi.fn(async () => okSession());
    expect(await coalescedRefresh()).toBe(REFRESH.ok);
    const [url, init] = globalThis.fetch.mock.calls[0];
    expect(String(url)).toBe('/api/auth/refresh');
    expect(JSON.parse(init.body)).toEqual({ keep: true });
    expect(init.credentials).toBe('include');
    expect(sessionStore.accessToken).toBe('ACC');
    expect(localStorage.getItem('osint:auth:refresh')).toBeNull();
    expect(sessionStorage.length).toBe(0);
  });

  it('nothing the server answers reaches storage but the non-secret flags', async () => {
    globalThis.fetch = vi.fn(async () => new Response(
      JSON.stringify({ access_token: 'ACC', refresh_token: 'LEAKED-RT', user: { id: 'u' } }), { status: 200 }));
    const s = await signInWithPassword('a@b.c', 'pw', false);
    sessionStore.setTokens(s.access_token, true);
    const all = JSON.stringify([Object.entries(localStorage), Object.entries(sessionStorage)]);
    expect(all).not.toContain('LEAKED-RT');
    expect(all).not.toContain('ACC');
    expect(s.refresh_token).toBeUndefined();
  });

  it('sign-up: a session, or confirmation required', async () => {
    globalThis.fetch = vi.fn(async () => new Response('{"confirmation_required":true}', { status: 200 }));
    expect(await signUpWithPassword('a@b.c', 'pw', false)).toEqual({ kind: 'confirmation_required' });
    globalThis.fetch = vi.fn(async () => okSession());
    expect((await signUpWithPassword('a@b.c', 'pw', false)).kind).toBe('session');
  });

  it('PKCE code exchange posts the code and verifier', async () => {
    globalThis.fetch = vi.fn(async () => okSession());
    const s = await exchangeOAuthCode('abc', 'v1', false);
    expect(s.access_token).toBe('ACC');
    expect(JSON.parse(globalThis.fetch.mock.calls[0][1].body)).toEqual({ auth_code: 'abc', code_verifier: 'v1', keep: false });
  });

  it('errors keep their HTTP status (400/401 are definitive)', async () => {
    globalThis.fetch = vi.fn(async () => new Response('{"error":"invalid_credentials","message":"Invalid login credentials"}', { status: 400 }));
    await expect(signInWithPassword('a@b.c', 'bad', false)).rejects.toMatchObject({ status: 400, message: 'Invalid login credentials' });
    globalThis.fetch = vi.fn(async () => { throw new TypeError('offline'); });
    await expect(authExchange('/api/auth/refresh', {})).rejects.toMatchObject({ status: -1 });
  });

  it('the fetch interceptor leaves /api/auth/ alone (no bearer, no refresh loop)', async () => {
    const { installFetchInterceptor } = await import('../session.js');
    const raw = vi.fn(async () => new Response('{}', { status: 401 }));
    window.fetch = raw;
    installFetchInterceptor();
    sessionStore.setTokens('TOK', true);
    await window.fetch('/api/auth/refresh', { method: 'POST' });
    expect(raw).toHaveBeenCalledTimes(1);
    expect(new Headers(raw.mock.calls[0][1]?.headers).get('Authorization')).toBeNull();
  });
});

describe('logout', () => {
  it('POSTs /api/auth/logout with the bearer so the server revokes and clears the cookie', async () => {
    globalThis.fetch = vi.fn(async () => new Response('{"ok":true,"revoked":true}', { status: 200 }));
    await serverLogout('A');
    const [url, init] = globalThis.fetch.mock.calls[0];
    expect(String(url)).toBe('/api/auth/logout');
    expect(init.headers.Authorization).toBe('Bearer A');
    expect(init.credentials).toBe('include');
  });

  it('with no access token it still calls the server, so the cookie is cleared', async () => {
    globalThis.fetch = vi.fn(async () => new Response('{"ok":true,"revoked":false}', { status: 200 }));
    await serverLogout(null);
    expect(globalThis.fetch).toHaveBeenCalledTimes(1);
    expect(globalThis.fetch.mock.calls[0][1].headers.Authorization).toBeUndefined();
  });
});

describe('per-account local data', () => {
  it('release wipes bookmarks and recent geocodes', () => {
    savedStore.save({ kind: 'intel_item', refId: 'x', displayName: 'X' });
    localStorage.setItem(MAP_RECENT_SEARCHES_KEY, '["tokyo"]');
    releaseLocalData();
    expect(savedStore.all()).toEqual([]);
    expect(localStorage.getItem(MAP_RECENT_SEARCHES_KEY)).toBeNull();
  });

  it('a different account claiming the browser wipes the previous account\'s data; the same account keeps it', () => {
    claimLocalData('alice');
    savedStore.save({ kind: 'intel_item', refId: 'a', displayName: 'A' });
    claimLocalData('alice');
    expect(savedStore.all().length).toBe(1);
    claimLocalData('bob');
    expect(savedStore.all()).toEqual([]);
  });
});

describe('token storage (WEB-13)', () => {
  const dump = (st) => JSON.stringify(Object.keys(st).map((k) => [k, st.getItem(k)]));

  it('the access token is in memory only; storage holds no token', () => {
    sessionStore.setTokens('ACCESS-1', true);
    expect(sessionStore.accessToken).toBe('ACCESS-1');
    expect(dump(localStorage)).not.toContain('ACCESS-1');
    expect(dump(sessionStorage)).not.toContain('ACCESS-1');
    expect(sessionStore.hasSessionHint).toBe(true);
    expect(sessionStorage.length).toBe(0);
  });

  it('"keep me signed in" is a non-secret flag that only selects the cookie lifetime', () => {
    sessionStore.setKeepSignedIn(true);
    sessionStore.setTokens('ACCESS-2', true);
    expect(sessionStore.keepSignedIn).toBe(true);
    expect(dump(localStorage)).not.toContain('ACCESS-2');
    expect(localStorage.getItem('osint:auth:refresh')).toBeNull();
  });

  it('there is no refresh token to read from script', () => {
    sessionStore.setTokens('A', true);
    expect(sessionStore.refreshToken).toBeUndefined();
  });

  it('migrates a legacy localStorage session: access to memory, refresh handed to the server once, then deleted', async () => {
    localStorage.setItem('osint:auth:access', 'OLD-ACCESS');
    localStorage.setItem('osint:auth:refresh', 'OLD-REFRESH');
    expect(sessionStore.migrateLegacySession()).toBe(true);
    expect(localStorage.getItem('osint:auth:access')).toBeNull();
    expect(sessionStore.accessToken).toBe('OLD-ACCESS');
    expect(sessionStore.keepSignedIn).toBe(true);        // it was a "keep me signed in" token
    expect(sessionStore.hasSessionHint).toBe(true);
    globalThis.fetch = vi.fn(async () => new Response(JSON.stringify({ access_token: 'NEW' }), { status: 200 }));
    expect(await coalescedRefresh()).toBe(REFRESH.ok);
    expect(JSON.parse(globalThis.fetch.mock.calls[0][1].body))
      .toEqual({ keep: true, refresh_token: 'OLD-REFRESH', cookie: true });
    expect(localStorage.getItem('osint:auth:refresh')).toBeNull();
    expect(dump(localStorage)).not.toContain('OLD-REFRESH');
    expect(sessionStore.migrateLegacySession()).toBe(false);
    // the next refresh sends no token at all
    await coalescedRefresh();
    expect(JSON.parse(globalThis.fetch.mock.calls[1][1].body)).toEqual({ keep: true });
  });

  it('a legacy sessionStorage token migrates the same way (not a keep session)', async () => {
    sessionStorage.setItem('osint:auth:refresh', 'TAB-REFRESH');
    expect(sessionStore.migrateLegacySession()).toBe(true);
    expect(sessionStore.keepSignedIn).toBe(false);
    globalThis.fetch = vi.fn(async () => new Response(JSON.stringify({ access_token: 'NEW' }), { status: 200 }));
    expect(await coalescedRefresh()).toBe(REFRESH.ok);
    expect(sessionStorage.getItem('osint:auth:refresh')).toBeNull();
  });

  it('a legacy token survives a transient failure for the retry, and dies with a refusal', async () => {
    localStorage.setItem('osint:auth:refresh', 'OLD');
    globalThis.fetch = vi.fn(async () => { throw new TypeError('offline'); });
    expect(await coalescedRefresh()).toBe(REFRESH.transient);
    expect(localStorage.getItem('osint:auth:refresh')).toBe('OLD');
    globalThis.fetch = vi.fn(async () => new Response('{"error":"refresh_token_not_found"}', { status: 400 }));
    expect(await coalescedRefresh()).toBe(REFRESH.invalid);
    expect(localStorage.getItem('osint:auth:refresh')).toBeNull();
  });

  it('sign-out / session death clears memory, the hint, the keep flag and any legacy key', () => {
    sessionStore.setKeepSignedIn(true);
    sessionStore.setTokens('A', true);
    sessionStorage.setItem('osint:auth:refresh', 'stray-tab-copy');
    localStorage.setItem('osint:auth:refresh', 'stray');
    sessionStore.clearTokens();
    expect(sessionStore.accessToken).toBeNull();
    expect(sessionStore.hasSessionHint).toBe(false);
    expect(sessionStore.keepSignedIn).toBe(false);
    expect(localStorage.getItem('osint:auth:refresh')).toBeNull();
    expect(sessionStorage.getItem('osint:auth:refresh')).toBeNull();
  });

  it('a fresh browser has no session hint, so bootstrap does not even call refresh', () => {
    expect(sessionStore.hasSessionHint).toBe(false);
  });
});
