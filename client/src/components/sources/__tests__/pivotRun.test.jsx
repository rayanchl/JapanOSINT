// WEB-01/07/02/20: a pivot is run WITH an entity or not at all; the server's
// 422 needs_entity is shown, never a "ran: 0 new rows"; the cadence "Change"
// button follows the flag AuthContext actually exports; and the Intel sources
// screen pages the server's search instead of downloading the registry.
import React from 'react';
import { describe, it, expect, vi, afterEach, beforeEach } from 'vitest';
import { render, cleanup, screen, waitFor, fireEvent } from '@testing-library/react';
import { MemoryRouter, Routes, Route } from 'react-router-dom';

vi.mock('../../../auth/AuthContext.jsx', () => ({ useAuthOptional: () => globalThis.__testAuth ?? null }));
vi.mock('../../panels/SourcesPanel.jsx', () => ({ default: () => null }));

import RunControl, { runSource, isPivotKind } from '../RunControl.jsx';
import SourcesListPage from '../SourcesListPage.jsx';
import IntelPage from '../../intel/IntelPage.jsx';

const realFetch = globalThis.fetch;
beforeEach(() => { globalThis.__testAuth = { isPlatformAdmin: true, canManageWorkspace: true }; });
afterEach(() => { cleanup(); globalThis.fetch = realFetch; delete globalThis.__testAuth; vi.restoreAllMocks(); });

function mockFetch(route) {
  const calls = [];
  globalThis.fetch = vi.fn(async (input, init) => {
    const url = typeof input === 'string' ? input : input.url;
    calls.push({ url, method: init?.method || 'GET', body: init?.body });
    const r = route(url, init);
    const status = r && r.__status ? r.__status : 200;
    return new Response(JSON.stringify(r ?? {}), { status, headers: { 'content-type': 'application/json' } });
  });
  return calls;
}
const path = (url) => new URL(url, 'http://x').pathname;
const posts = (calls) => calls.filter((c) => c.method === 'POST');

describe('pivot run flow', () => {
  it('classifies pivot kinds', () => {
    expect(isPivotKind('pivot')).toBe(true);
    expect(isPivotKind('scheduled-pivot')).toBe(true);
    expect(isPivotKind('scheduled')).toBe(false);
  });

  it('runSource refuses a pivot with no entity WITHOUT sending a request', async () => {
    const calls = mockFetch(() => ({ ingested: 0 }));
    const r = await runSource('p1', { kind: 'pivot', entity: '   ' });
    expect(r.ok).toBe(false);
    expect(r.needsEntity).toBe(true);
    expect(r.message).not.toMatch(/ran:/);
    expect(calls.length).toBe(0);
  });

  it('sends {entity} for a pivot and no body for a scheduled feed', async () => {
    const calls = mockFetch(() => ({ ingested: 3, duration_ms: 12 }));
    const r = await runSource('p1', { kind: 'pivot', entity: ' Toyota ' });
    expect(r.ok).toBe(true);
    expect(JSON.parse(posts(calls)[0].body)).toEqual({ entity: 'Toyota' });
    await runSource('f1', { kind: 'scheduled' });
    expect(posts(calls)[1].body).toBeUndefined();
  });

  it('a 422 needs_entity from the server is reported as such, not as a success', async () => {
    mockFetch(() => ({ __status: 422, error: 'needs_entity' }));
    const r = await runSource('p1', { kind: 'pivot', entity: 'x' });
    expect(r.ok).toBe(false);
    expect(r.needsEntity).toBe(true);
    expect(r.message).toMatch(/needs an entity/);
  });

  it('RunControl: the pivot Run button is disabled until an entity is typed, and a 422 shows inline', async () => {
    const calls = mockFetch(() => ({ __status: 422, error: 'needs_entity' }));
    render(<RunControl source={{ id: 'p1', kind: 'pivot' }} />);
    const btn = screen.getByRole('button', { name: /Run pivot p1/ });
    expect(btn.disabled).toBe(true);
    fireEvent.submit(screen.getByRole('form', { name: /Run pivot p1/ }));
    expect(posts(calls).length).toBe(0);
    fireEvent.change(screen.getByLabelText('Entity for p1'), { target: { value: 'Acme' } });
    expect(btn.disabled).toBe(false);
    fireEvent.click(btn);
    await waitFor(() => expect(screen.getByRole('alert').textContent).toMatch(/needs an entity/));
    expect(JSON.parse(posts(calls)[0].body)).toEqual({ entity: 'Acme' });
  });

  it('RunControl: a feed gets "Run now"; a viewer who may not run gets a read-only note; a pod gets nothing', () => {
    mockFetch(() => ({}));
    const { unmount } = render(<RunControl source={{ id: 'f1', kind: 'scheduled' }} />);
    expect(screen.getByRole('button', { name: /Run f1 now/ })).toBeTruthy();
    unmount();
    globalThis.__testAuth = { isPlatformAdmin: false, canManageWorkspace: false };
    const r2 = render(<RunControl source={{ id: 'f1', kind: 'scheduled' }} />);
    expect(screen.getByTestId('run-readonly')).toBeTruthy();
    expect(screen.queryByRole('button')).toBeNull();
    r2.unmount();
    const r3 = render(<RunControl source={{ id: 'pod', kind: 'internal' }} />);
    expect(r3.container.innerHTML).toBe('');
  });
});

function listBody(url, n = 0) {
  return { data: Array.from({ length: n }, (_, i) => ({ id: `s${i}`, name: `Source ${i}`, kind: 'scheduled', category: 'x', item_count: 5 })), page: { limit: 50, offset: 0, count: n, total: n, has_more: false }, meta: { registry_total: n, facets: {} } };
}

describe('cadence edit gate', () => {
  const routeAll = (url) => {
    const p = path(url);
    if (p === '/api/sources/cadence') return { policy: { mode: 'declared', global_interval_sec: 0, min_interval_sec: 0, max_interval_sec: 0 }, estimate: null, source: 'default' };
    if (p === '/api/sources/search') return listBody(url, 0);
    if (p === '/api/status') return { summary: {} };
    return { data: [], meta: {} };
  };
  const mount = () => render(<MemoryRouter initialEntries={['/console/sources']}><Routes><Route path="/console/sources" element={<SourcesListPage />} /></Routes></MemoryRouter>);

  it('a platform admin sees Change', async () => {
    mockFetch(routeAll);
    globalThis.__testAuth = { isPlatformAdmin: true };
    mount();
    await waitFor(() => expect(screen.getByRole('button', { name: 'Change' })).toBeTruthy());
  });
  it('a workspace owner who is NOT a platform admin does not (the server would 403)', async () => {
    mockFetch(routeAll);
    globalThis.__testAuth = { isPlatformAdmin: false, canManageWorkspace: true, isPlatformOperator: true };
    mount();
    await waitFor(() => expect(screen.getByTestId('cadence-card')).toBeTruthy());
    expect(screen.queryByRole('button', { name: 'Change' })).toBeNull();
  });
});

describe('IntelPage sources view (formerly the whole registry)', () => {
  it('pages /api/sources/search with has_items and never fetches /api/intel/sources', async () => {
    const calls = mockFetch((url) => (path(url) === '/api/sources/search'
      ? { ...listBody(url, 50), page: { limit: 50, offset: 0, count: 50, total: 120, has_more: true } }
      : {}));
    render(<MemoryRouter initialEntries={['/intel?view=sources']}><Routes><Route path="/intel" element={<IntelPage />} /></Routes></MemoryRouter>);
    await waitFor(() => expect(screen.getByText('Source 49')).toBeTruthy());
    const first = calls.find((c) => path(c.url) === '/api/sources/search');
    expect(new URL(first.url, 'http://x').searchParams.get('has_items')).toBe('1');
    expect(new URL(first.url, 'http://x').searchParams.get('limit')).toBe('50');
    expect(calls.some((c) => path(c.url) === '/api/intel/sources')).toBe(false);
    expect(screen.getAllByText(/showing 50 of 120 sources/).length).toBeGreaterThan(0);
    fireEvent.click(screen.getAllByRole('button', { name: /Load more/ })[0]);
    await waitFor(() => expect(calls.filter((c) => path(c.url) === '/api/sources/search').length).toBe(2));
    expect(new URL(calls.filter((c) => path(c.url) === '/api/sources/search')[1].url, 'http://x').searchParams.get('offset')).toBe('50');
  });
});
