// Since 2026-11-07 the server answers an X-Tenant-Id naming a workspace the
// caller is not a member of with 403 `not_a_member` (it used to fall back to
// the first workspace in silence). The interceptor clears the stored id, tells
// its listeners (AuthContext re-reads /api/me once) and does NOT replay the
// refused request against another workspace.
import { describe, it, expect, vi } from 'vitest';
import { sessionStore, installFetchInterceptor, onTenantRejected } from '../session.js';

describe('403 not_a_member', () => {
  it('clears the stored tenant, notifies once per refusal, never retries', async () => {
    const calls = [];
    window.fetch = vi.fn(async (input, init) => {
      calls.push(new Headers(init?.headers).get('X-Tenant-Id'));
      if (String(input).endsWith('/other')) {
        return new Response('{"error":"forbidden"}', { status: 403, headers: { 'content-type': 'application/json' } });
      }
      return new Response('{"error":"not_a_member"}', { status: 403, headers: { 'content-type': 'application/json' } });
    });
    installFetchInterceptor();
    const heard = vi.fn();
    const off = onTenantRejected(heard);

    // An ordinary 403 is not this refusal: the stored id stays.
    sessionStore.setTenantId('stale-tenant');
    let res = await window.fetch('/api/other');
    expect(res.status).toBe(403);
    expect(sessionStore.tenantId).toBe('stale-tenant');
    expect(heard).not.toHaveBeenCalled();

    res = await window.fetch('/api/cases');
    expect(res.status).toBe(403);
    expect(await res.json()).toEqual({ error: 'not_a_member' });   // body still readable
    expect(sessionStore.tenantId).toBe(null);
    expect(heard).toHaveBeenCalledTimes(1);
    expect(calls).toEqual(['stale-tenant', 'stale-tenant']);       // one attempt each, no replay

    // A caller-supplied header is the caller's own choice: the store is untouched.
    sessionStore.setTenantId('kept');
    await window.fetch('/api/cases', { headers: { 'X-Tenant-Id': 'explicit' } });
    expect(sessionStore.tenantId).toBe('kept');
    expect(heard).toHaveBeenCalledTimes(1);
    off();
  });
});
