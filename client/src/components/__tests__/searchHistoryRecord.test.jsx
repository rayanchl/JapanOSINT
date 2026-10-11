// Decided 2026-10-09: every committed ad-hoc search lands in the workspace's
// search history (POST /api/search-history), not only saved-search runs.
// "Committed" must never mean a keystroke — the entity box searches as you
// type — so these pin when a record is written, and that it is written once.
import React from 'react';
import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, cleanup, fireEvent, screen, waitFor } from '@testing-library/react';
import { MemoryRouter } from 'react-router-dom';
import { recordSearch, intelHistoryParams, intelHistoryRoute } from '../../api/searchHistory.js';
import EntitiesPage from '../entities/EntitiesPage.jsx';
import IntelPage from '../intel/IntelPage.jsx';
import { rerunTarget } from '../search/SearchHistoryDropdown.jsx';
import { routeFor } from '../alerts/SavedSearchesPage.jsx';

vi.mock('../../auth/AuthContext.jsx', () => ({ useAuth: () => ({ isPlatformAdmin: false, role: 'viewer' }) }));

const realFetch = globalThis.fetch;
afterEach(() => { cleanup(); globalThis.fetch = realFetch; vi.restoreAllMocks(); vi.useRealTimers(); });

/** fetch mock recording method, url and JSON body; `route(url, method)` answers. */
function mockFetch(route) {
  const calls = [];
  globalThis.fetch = vi.fn(async (input, init = {}) => {
    const url = typeof input === 'string' ? input : input.url;
    const method = (init.method || 'GET').toUpperCase();
    let body = null;
    try { body = init.body ? JSON.parse(init.body) : null; } catch { body = init.body; }
    calls.push({ url, method, body });
    const out = route(url, method) ?? {};
    return new Response(JSON.stringify(out), { status: 200, headers: { 'content-type': 'application/json' } });
  });
  return calls;
}
const posts = (calls) => calls.filter((c) => c.method === 'POST' && c.url.includes('/api/search-history'));

describe('recordSearch', () => {
  it('posts the kind, the non-empty params and a whole result count', async () => {
    const calls = mockFetch(() => ({ data: {} }));
    await recordSearch('intel', { view: 'search', q: 'kobe port', near: '', sort: null }, 42.7);
    expect(posts(calls)).toHaveLength(1);
    expect(posts(calls)[0].body).toEqual({ kind: 'intel', params: { view: 'search', q: 'kobe port' }, result_count: 42 });
  });
  it('records nothing for empty params, and never throws on a failed write', async () => {
    const calls = mockFetch(() => ({}));
    await recordSearch('entity', { q: '' });
    expect(posts(calls)).toHaveLength(0);
    globalThis.fetch = vi.fn(async () => { throw new Error('offline'); });
    await expect(recordSearch('entity', { q: 'x' })).resolves.toBeNull();
  });
});

describe('EntitiesPage (search as you type)', () => {
  const entityRoute = (url) => url.includes('/api/entities/search')
    ? { results: [{ entity_id: 'e1', type: 'ORG', value: 'Kobe Port Authority', mention_count: 3 }] }
    : { data: {} };

  it('typing records nothing; Enter records the settled query once, with its count', async () => {
    const calls = mockFetch(entityRoute);
    render(<MemoryRouter><EntitiesPage /></MemoryRouter>);
    const box = screen.getByPlaceholderText(/search entities/);
    for (const v of ['k', 'ko', 'kob', 'kobe']) fireEvent.change(box, { target: { value: v } });
    await screen.findByText('Kobe Port Authority');
    expect(posts(calls)).toHaveLength(0);
    fireEvent.keyDown(box, { key: 'Enter' });
    await waitFor(() => expect(posts(calls)).toHaveLength(1));
    expect(posts(calls)[0].body).toEqual({ kind: 'entity', params: { q: 'kobe' }, result_count: 1 });
  });

  it('opening a result records the search that found it', async () => {
    const calls = mockFetch(entityRoute);
    render(<MemoryRouter><EntitiesPage /></MemoryRouter>);
    fireEvent.change(screen.getByPlaceholderText(/search entities/), { target: { value: 'kobe' } });
    fireEvent.click(await screen.findByText('Kobe Port Authority'));
    await waitFor(() => expect(posts(calls)).toHaveLength(1));
    expect(posts(calls)[0].body.params).toEqual({ q: 'kobe' });
  });

  it('arriving with ?q= (a link, a history re-run) records once its results are in', async () => {
    const calls = mockFetch(entityRoute);
    render(<MemoryRouter initialEntries={['/entities?q=kobe']}><EntitiesPage /></MemoryRouter>);
    await screen.findByText('Kobe Port Authority');
    await waitFor(() => expect(posts(calls)).toHaveLength(1));
    expect(posts(calls)[0].body).toEqual({ kind: 'entity', params: { q: 'kobe' }, result_count: 1 });
  });
});

describe('IntelPage search', () => {
  it('a full-text search records once, with the server total, on the Search tab', async () => {
    const calls = mockFetch((url, method) => (method === 'GET' && url.includes('/api/intel/search'))
      ? { data: [{ uid: 'a|1', title: 'Kobe berth notice', source_id: 'a' }], page: { total: 37 }, meta: {} }
      : { data: {} });
    render(<MemoryRouter initialEntries={['/intel?view=search&q=kobe']}><IntelPage /></MemoryRouter>);
    await screen.findByText('Kobe berth notice');
    await waitFor(() => expect(posts(calls)).toHaveLength(1));
    expect(posts(calls)[0].body).toEqual({ kind: 'intel', params: { view: 'search', mode: 'fts', q: 'kobe' }, result_count: 37 });
  });
});

describe('re-running an intel entry opens the Search tab', () => {
  it('a legacy {q} entry gains view=search; near and mode survive', () => {
    expect(intelHistoryParams({ q: 'kobe' })).toEqual({ q: 'kobe', view: 'search' });
    expect(routeFor('intel', { q: 'kobe' })).toBe('/intel?q=kobe&view=search');
    const t = rerunTarget({ kind: 'intel', params: { view: 'search', mode: 'near', near: '34.68,135.19', radius_m: '2000' } });
    expect(t.to).toBe('/intel?view=search&mode=near&near=34.68%2C135.19&radius_m=2000');
  });
  it('a search made inside one source reopens that source with its query', () => {
    expect(intelHistoryRoute({ view: 'source', source: 'jp-mlit-ports', q: '神戸' }))
      .toBe('/intel/sources/jp-mlit-ports?q=%E7%A5%9E%E6%88%B8');
    expect(rerunTarget({ kind: 'intel', params: { view: 'source', source: 'a b', q: 'x' } }).to).toBe('/intel/sources/a%20b?q=x');
  });
});
