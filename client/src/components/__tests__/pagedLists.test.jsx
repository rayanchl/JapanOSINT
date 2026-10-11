// Regression (audit 2026-10-02, open item 11): evidence, saved searches,
// search history and the maintenance lists used to come back as one fixed
// slice with only its own size, so the screens could say no more than "N
// loaded · more on server" — and the history dropdown printed the response
// size as if it were the total. The server now pages them and reports
// page.total / page.has_more; these screens page through and show the real
// total.
import React from 'react';
import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, renderHook, cleanup, fireEvent, screen, waitFor, act } from '@testing-library/react';
import { MemoryRouter } from 'react-router-dom';
import { pageUrl, appendPage, nextOffset, pageBound, usePagedList } from '../../hooks/usePagedList.js';
import SavedSearchesPage from '../alerts/SavedSearchesPage.jsx';
import SearchHistoryDropdown from '../search/SearchHistoryDropdown.jsx';
import { EvidenceSection, evidencePresence } from '../intel/IntelItemPage.jsx';
import { MaintenanceTab, boundLabel, mergeBuckets, digestListUrl, sourceListUrl } from '../console/AdminPage.jsx';

vi.mock('../../auth/AuthContext.jsx', () => ({ useAuth: () => ({ isPlatformAdmin: false }) }));

const realFetch = globalThis.fetch;
afterEach(() => { cleanup(); globalThis.fetch = realFetch; vi.restoreAllMocks(); });

/** fetch mock: `route(url)` returns a JSON body; every URL is recorded. */
function mockFetch(route) {
  const calls = [];
  globalThis.fetch = vi.fn(async (input) => {
    const url = typeof input === 'string' ? input : input.url;
    calls.push(url);
    const body = route(url);
    return new Response(JSON.stringify(body ?? {}), { status: 200, headers: { 'content-type': 'application/json' } });
  });
  return calls;
}
const qp = (url, k) => new URL(url, 'http://x').searchParams.get(k);

/** A server list of `total` rows answering limit/offset like savedsearchapi.c. */
function serverList(total, make, extraMeta = {}) {
  return (url) => {
    const limit = Number(qp(url, 'limit') || 50);
    const offset = Number(qp(url, 'offset') || 0);
    const n = Math.max(0, Math.min(limit, total - offset));
    const data = Array.from({ length: n }, (_, i) => make(offset + i));
    return { data, page: { limit, offset, count: n, total, has_more: offset + n < total }, meta: { ...extraMeta } };
  };
}

describe('paging helpers', () => {
  it('pageUrl keeps the query string the path already carries', () => {
    expect(pageUrl('/api/saved-searches', 50, 0)).toBe('/api/saved-searches?limit=50&offset=0');
    expect(pageUrl('/api/saved-searches?kind=intel&mine=1', 20, 40)).toBe('/api/saved-searches?kind=intel&mine=1&limit=20&offset=40');
  });
  it('appendPage drops a row already held (offset drift), never a different one', () => {
    expect(appendPage([{ id: 1 }, { id: 2 }], [{ id: 2 }, { id: 3 }])).toEqual([{ id: 1 }, { id: 2 }, { id: 3 }]);
    expect(appendPage([{ source_id: 'a' }], [{ source_id: 'b' }], 'source_id')).toEqual([{ source_id: 'a' }, { source_id: 'b' }]);
  });
  it('the next offset is what the server says it served', () => {
    expect(nextOffset({ offset: 50, count: 50 })).toBe(100);
    expect(nextOffset(null, 7)).toBe(7);
  });
  it('a null total stays null — never replaced by the rows on screen', () => {
    expect(pageBound({ total: null, has_more: true })).toEqual({ total: null, hasMore: true });
    expect(pageBound({ total: 12, has_more: false })).toEqual({ total: 12, hasMore: false });
  });
  it('maintenance labels and URLs', () => {
    expect(boundLabel(50, 212)).toBe('50 of 212');
    expect(boundLabel(12, 12)).toBe('12');
    expect(boundLabel(50, null, true)).toBe('50+');
    expect(digestListUrl('needs_human', 24)(50)).toBe('/api/admin/maintenance/lists/needs_human?hours=24&limit=50&offset=50');
    expect(sourceListUrl('jma quake', 'fetch_log')(30)).toBe('/api/admin/maintenance/source/jma%20quake/fetch_log?limit=50&offset=30');
    const a = { rows: [1], total: 3, hasMore: true, loadMore: 'a' };
    const b = { rows: [2], total: 1, hasMore: false, loadMore: 'b' };
    expect(mergeBuckets(a, b)).toMatchObject({ rows: [1, 2], total: 4, hasMore: true, loadMore: 'a' });
    expect(mergeBuckets({ ...a, total: null }, b).total).toBeNull();
  });
  it('evidence presence is counted over the loaded rows', () => {
    expect(evidencePresence([{ blob_present: true }, { blob_present: false }, { blob_present: true }])).toEqual({ present: 2, evicted: 1 });
  });
});

describe('usePagedList', () => {
  it('pages through a list until has_more is false', async () => {
    const calls = mockFetch(serverList(5, (i) => ({ id: i })));
    const { result } = renderHook(() => usePagedList('/api/x?kind=a', { pageSize: 2 }));
    await waitFor(() => expect(result.current.rows.length).toBe(2));
    expect(result.current.total).toBe(5);
    expect(result.current.hasMore).toBe(true);
    await act(() => result.current.loadMore());
    await act(() => result.current.loadMore());
    expect(result.current.rows.map((r) => r.id)).toEqual([0, 1, 2, 3, 4]);
    expect(result.current.hasMore).toBe(false);
    expect(calls).toEqual(['/api/x?kind=a&limit=2&offset=0', '/api/x?kind=a&limit=2&offset=2', '/api/x?kind=a&limit=2&offset=4']);
  });
});

describe('SavedSearchesPage', () => {
  const ss = (i, mine) => ({ id: `s${i}`, name: `search ${i}`, kind: 'intel', params: { q: `q${i}` }, pinned: false, created_at: '2026-10-01 00:00:00', run_count: 0, mine });

  it('shows the measured total, loads the next page, and ?mine=1 narrows the total', async () => {
    const all = serverList(120, (i) => ss(i, i % 2 === 0), { scope: 'workspace' });
    const mine = serverList(60, (i) => ss(i, true), { scope: 'user' });
    const history = serverList(0, () => null);
    const calls = mockFetch((url) => {
      if (url.startsWith('/api/search-history')) return history(url);
      return qp(url, 'mine') === '1' ? mine(url) : all(url);
    });
    render(<MemoryRouter><SavedSearchesPage /></MemoryRouter>);
    expect(await screen.findByText('showing 50 of 120 saved searches')).toBeTruthy();
    fireEvent.click(screen.getByText('Load more'));
    expect(await screen.findByText('showing 100 of 120 saved searches')).toBeTruthy();
    expect(calls).toContain('/api/saved-searches?limit=50&offset=50');
    fireEvent.click(screen.getByText('mine'));
    expect(await screen.findByText('showing 50 of 60 saved searches')).toBeTruthy();
    expect(calls).toContain('/api/saved-searches?mine=1&limit=50&offset=0');
    expect(calls).toContain('/api/search-history?mine=1&limit=50&offset=0');
  });
});

describe('SearchHistoryDropdown', () => {
  it('prints the server total, not the size of the response, and does not claim privacy', async () => {
    const calls = mockFetch(serverList(900, (i) => ({ id: i, kind: 'osint', params: { q: `t${i}` }, result_count: 1, ts: '2026-10-01 00:00:00', mine: i % 3 !== 0 }), { scope: 'workspace', retained_max: 1000 }));
    render(<MemoryRouter><SearchHistoryDropdown open onRun={() => {}} limit={20} /></MemoryRouter>);
    expect(await screen.findByText('showing 20 of 900 entries')).toBeTruthy();
    expect(screen.queryByText('Private to you')).toBeNull();
    expect(screen.getByText('Shared with your workspace')).toBeTruthy();
    fireEvent.click(screen.getByText('Load more'));
    expect(await screen.findByText('showing 40 of 900 entries')).toBeTruthy();
    // /api/members is fetched too (author names); the paging is what's pinned.
    expect(calls.filter((u) => u.startsWith('/api/search-history')))
      .toEqual(['/api/search-history?limit=20&offset=0', '/api/search-history?limit=20&offset=20']);
  });

  it('opens after first rendering closed (every hook runs before the early return)', async () => {
    mockFetch(serverList(3, (i) => ({ id: i, kind: 'osint', params: { q: `t${i}` }, result_count: 1, ts: '2026-10-01 00:00:00', mine: true }), { scope: 'workspace', retained_max: 1000 }));
    const { rerender } = render(<MemoryRouter><SearchHistoryDropdown open={false} onRun={() => {}} limit={20} /></MemoryRouter>);
    rerender(<MemoryRouter><SearchHistoryDropdown open onRun={() => {}} limit={20} /></MemoryRouter>);
    expect(await screen.findByText('Shared with your workspace')).toBeTruthy();
  });
});

describe('EvidenceSection', () => {
  it('pages custody records with the real total', async () => {
    const calls = mockFetch(serverList(130, (i) => ({ id: `ev${i}`, chain_seq: i, captured_at: '2026-10-01 00:00:00', response_status: 200, content_bytes: 10, blob_present: i < 3 })));
    render(<MemoryRouter><EvidenceSection uid="item:1" /></MemoryRouter>);
    expect(await screen.findByText('showing 100 of 130 custody records')).toBeTruthy();
    expect(screen.getByText('3 present · 97 evicted (of 100 loaded)')).toBeTruthy();
    fireEvent.click(screen.getByText('Load more'));
    expect(await screen.findByText('130 custody records')).toBeTruthy();
    expect(screen.queryByText('Load more')).toBeNull();
    expect(calls[1]).toBe('/api/intel/items/item%3A1/evidence?limit=100&offset=100');
  });
});

describe('MaintenanceTab', () => {
  const rep = (i, extra = {}) => ({ id: i, anomaly_id: 1, source_id: `S${i}`, status: 'needs_human', created_at: '2026-10-01 00:00:00', ...extra });
  it('labels each list with the server total and loads later pages from /lists/:name', async () => {
    const needsPage2 = serverList(53, (i) => rep(i));
    const calls = mockFetch((url) => {
      if (url.startsWith('/api/admin/maintenance/lists/needs_human')) return needsPage2(url);
      if (url.startsWith('/api/admin/maintenance?')) {
        return {
          generated_at: '2026-10-06T00:00:00.000Z', window_hours: 24,
          totals: { verified: 60, merged: 0, rejected: 0, needs_human: 53, error: 0 },
          success_by_class: [],
          needs_human: Array.from({ length: 50 }, (_, i) => rep(i)),
          awaiting_review: { awaiting_apply: Array.from({ length: 5 }, (_, i) => rep(100 + i, { status: 'verified', action: 'url_swap', patch: '{}' })), awaiting_pr: [] },
          auto_dismissed: [], auto_fixed: [], quarantined: [], url_overrides: [], worst_sources: [],
          pages: {
            needs_human: { limit: 50, offset: 0, count: 50, total: 53, has_more: true },
            awaiting_apply: { limit: 50, offset: 0, count: 5, total: 5, has_more: false },
            awaiting_pr: { limit: 50, offset: 0, count: 0, total: 0, has_more: false },
          },
        };
      }
      return { data: [] };
    });
    render(<MemoryRouter><MaintenanceTab onOpenSource={() => {}} /></MemoryRouter>);
    expect(await screen.findByText('Needs human · 50 of 53')).toBeTruthy();
    expect(screen.getByText('Awaiting your review · 5')).toBeTruthy();
    fireEvent.click(screen.getByText('Load more'));
    expect(await screen.findByText('Needs human · 53')).toBeTruthy();
    expect(calls).toContain('/api/admin/maintenance/lists/needs_human?hours=24&limit=50&offset=50');
  });
});
