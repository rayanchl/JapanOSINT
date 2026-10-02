// Regression (findings 5 and 13): social sign-in completes only a PKCE flow
// this tab started; a token in the URL fragment is never adopted; only a
// definitive refusal from the refresh endpoint ends a session; sign-out
// revokes the refresh token and clears per-account browser data.
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { createSupabaseAuth, AuthError } from '../supabase.js';
import { sessionStore, OAUTH_FLOW_TTL_MS, classifyRefreshError, coalescedRefresh, REFRESH } from '../session.js';
import { claimLocalData, releaseLocalData, MAP_RECENT_SEARCHES_KEY } from '../localData.js';
import { savedStore } from '../../store/savedStore.js';

const realFetch = globalThis.fetch;
const auth = createSupabaseAuth({ projectURL: 'https://proj.supabase.test', anonKey: 'anon' });

beforeEach(() => { localStorage.clear(); sessionStorage.clear(); });
afterEach(() => { globalThis.fetch = realFetch; vi.restoreAllMocks(); });

describe('completeOAuth', () => {
  it('refuses implicit-flow tokens in the fragment instead of adopting them', async () => {
    globalThis.fetch = vi.fn();
    await expect(auth.completeOAuth('https://app.test/#access_token=ATTACKER&refresh_token=R', 'verifier'))
      .rejects.toThrow(/implicit flow/);
    expect(globalThis.fetch).not.toHaveBeenCalled();
  });

  it('needs a verifier at all', async () => {
    await expect(auth.completeOAuth('https://app.test/auth/callback?code=abc', null)).rejects.toThrow(/verifier/);
  });

  it('exchanges ?code= with the verifier (PKCE)', async () => {
    globalThis.fetch = vi.fn(async (url, init) => {
      expect(String(url)).toContain('/auth/v1/token?grant_type=pkce');
      expect(JSON.parse(init.body)).toEqual({ auth_code: 'abc', code_verifier: 'v1' });
      return new Response(JSON.stringify({ access_token: 'A', refresh_token: 'R', user: { id: 'u' } }), { status: 200 });
    });
    const s = await auth.completeOAuth('https://app.test/auth/callback?code=abc', 'v1');
    expect(s.access_token).toBe('A');
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

  it('a network failure during refresh keeps the tokens', async () => {
    sessionStore.setTokens('A', 'R');
    globalThis.fetch = vi.fn(async () => { throw new TypeError('Failed to fetch'); });
    expect(await coalescedRefresh()).toBe(REFRESH.transient);
    expect(sessionStore.refreshToken).toBe('R');
  });

  it('a refused refresh token is invalid', async () => {
    sessionStore.setTokens('A', 'R');
    globalThis.fetch = vi.fn(async () => new Response('{"error":"invalid_grant"}', { status: 400 }));
    expect(await coalescedRefresh()).toBe(REFRESH.invalid);
  });
});

describe('logout', () => {
  it('POSTs /logout with the bearer, and treats an already-dead token as done', async () => {
    globalThis.fetch = vi.fn(async (url, init) => {
      expect(String(url)).toBe('https://proj.supabase.test/auth/v1/logout?scope=local');
      expect(init.method).toBe('POST');
      expect(init.headers.Authorization).toBe('Bearer A');
      return new Response(null, { status: 401 });
    });
    await expect(auth.logout('A')).resolves.toBeUndefined();
    expect(globalThis.fetch).toHaveBeenCalledTimes(1);
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
