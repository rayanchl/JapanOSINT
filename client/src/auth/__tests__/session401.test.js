// WEB-14: a 401 on a request that carried our token, with no refresh token to
// try, is the end of the session and is signalled (it used to be returned as a
// bare 401 that every page then rendered as "HTTP 401").
import { describe, it, expect, vi, afterEach } from 'vitest';
import { sessionStore, installFetchInterceptor, onSessionLost } from '../session.js';

const realFetch = window.fetch;
afterEach(() => { window.fetch = realFetch; localStorage.clear(); });

describe('401 without a refresh token', () => {
  it('signals session loss and clears the dead token', async () => {
    window.fetch = vi.fn(async () => new Response('{}', { status: 401 }));
    installFetchInterceptor();
    sessionStore.setTokens('expired-api-token', null);
    const lost = vi.fn();
    const off = onSessionLost(lost);
    const res = await window.fetch('/api/anything');
    expect(res.status).toBe(401);
    expect(lost).toHaveBeenCalledTimes(1);
    expect(sessionStore.accessToken).toBeNull();
    off();
  });

  it('an anonymous request answered 401 is not a lost session', async () => {
    window.fetch = vi.fn(async () => new Response('{}', { status: 401 }));
    installFetchInterceptor();
    const lost = vi.fn();
    const off = onSessionLost(lost);
    await window.fetch('/api/anything');
    expect(lost).not.toHaveBeenCalled();
    off();
  });
});
