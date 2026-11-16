// Regressions for the client audit of 2026-11-07. Each block names the defect
// it pins; every one of them read as healthy in the UI while being wrong.
import React from 'react';
import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, renderHook, cleanup, screen, waitFor, act, fireEvent } from '@testing-library/react';
import { MemoryRouter, Routes, Route } from 'react-router-dom';
import { pageBound } from '../../hooks/usePagedList.js';
import { usePaged, nextCursorOf } from '../../hooks/useCases.js';
import { rejectBodyError, ApiError } from '../../api/client.js';
import ErrorBoundary from '../ui/ErrorBoundary.jsx';
import StationPopup from '../map/popups/StationPopup.jsx';
import VehiclePopup from '../map/popups/VehiclePopup.jsx';
import SourceDetailPage, { describeEffective, describePaging, RunsLog } from '../sources/SourceDetailPage.jsx';
import CadenceCard from '../sources/CadenceCard.jsx';

const realFetch = globalThis.fetch;
afterEach(() => { cleanup(); globalThis.fetch = realFetch; vi.restoreAllMocks(); });

function mockFetch(route) {
  const calls = [];
  globalThis.fetch = vi.fn(async (input) => {
    const url = typeof input === 'string' ? input : input.url;
    calls.push(url);
    const r = await route(url);
    const status = r && r.__status ? r.__status : 200;
    return new Response(JSON.stringify(r ?? {}), { status, headers: { 'content-type': 'application/json' } });
  });
  return calls;
}
const path = (url) => new URL(url, 'http://x').pathname;
const qp = (url, k) => new URL(url, 'http://x').searchParams.get(k);

describe('a body error is a failure whatever the status', () => {
  it('rejectBodyError throws an ApiError carrying the server text', () => {
    expect(() => rejectBodyError({ error: 'station not found' })).toThrow(ApiError);
    expect(rejectBodyError({ station: { name: 'x' } })).toEqual({ station: { name: 'x' } });
    expect(rejectBodyError([1, 2])).toEqual([1, 2]);
  });
  it('StationPopup shows a 200 {"error"} body as an error instead of throwing', async () => {
    mockFetch(() => ({ error: 'station not found' }));
    render(<StationPopup properties={{ station_uid: 'nope' }} />);
    await waitFor(() => expect(screen.getByText(/station not found/)).toBeTruthy());
  });
  it('VehiclePopup does the same for a body with no trip', async () => {
    mockFetch(() => ({ error: 'trip not found' }));
    render(<VehiclePopup properties={{ trip_id: 'nope' }} />);
    await waitFor(() => expect(screen.getByText(/trip not found/)).toBeTruthy());
  });
});

describe('ErrorBoundary', () => {
  it('contains a throwing child and resets on a new key', () => {
    vi.spyOn(console, 'error').mockImplementation(() => {});
    const Boom = ({ on }) => { if (on) throw new Error('kaboom'); return <span>fine</span>; };
    const { rerender } = render(<ErrorBoundary label="Map popup" resetKey="a"><Boom on /></ErrorBoundary>);
    expect(screen.getByText(/Map popup failed to render/)).toBeTruthy();
    expect(screen.getByText('kaboom')).toBeTruthy();
    rerender(<ErrorBoundary label="Map popup" resetKey="b"><Boom on={false} /></ErrorBoundary>);
    expect(screen.getByText('fine')).toBeTruthy();
  });
});

describe('pageBound without has_more', () => {
  it('reads shown against the measured total (the /logs envelope)', () => {
    expect(pageBound({ limit: 50, offset: 0, total: 120 }, 50)).toEqual({ total: 120, hasMore: true });
    expect(pageBound({ limit: 50, offset: 100, total: 120 }, 120)).toEqual({ total: 120, hasMore: false });
    // an unknown total stays unknown, never "no more" by inference
    expect(pageBound({ limit: 50, offset: 0, total: null }, 50)).toEqual({ total: null, hasMore: false });
    // has_more, when sent, still decides
    expect(pageBound({ total: 12, has_more: true }, 12)).toEqual({ total: 12, hasMore: true });
  });
});

describe('usePaged — the one keyset hook', () => {
  it('accepts page.cursor as well as page.next_cursor', () => {
    expect(nextCursorOf({ next_cursor: 'a' })).toBe('a');
    expect(nextCursorOf({ cursor: 'b' })).toBe('b');
    expect(nextCursorOf({})).toBeNull();
  });
  it('surfaces the total, keeps a failed load-more apart from the list error', async () => {
    mockFetch((url) => (qp(url, 'cursor')
      ? { __status: 500, error: 'boom' }
      : { data: [{ id: 1 }, { id: 2 }], page: { next_cursor: 'c2', total: 9 } }));
    const { result } = renderHook(() => usePaged('/api/aoi', { limit: 2 }));
    await waitFor(() => expect(result.current.rows.length).toBe(2));
    expect(result.current.total).toBe(9);
    expect(result.current.hasMore).toBe(true);
    await act(async () => { await result.current.loadMore(); });
    expect(result.current.error).toBeNull();
    expect(result.current.moreError).toBeTruthy();
    expect(result.current.rows.length).toBe(2);
  });
  it('drops a stale first page that lands after a newer one', async () => {
    let n = 0;
    const gates = [];
    globalThis.fetch = vi.fn(async (input) => {
      const url = typeof input === 'string' ? input : input.url;
      const mine = ++n;
      await new Promise((r) => gates.push(r));
      const body = { data: [{ id: `${path(url)}-${mine}` }], page: {} };
      return new Response(JSON.stringify(body), { status: 200, headers: { 'content-type': 'application/json' } });
    });
    const { result, rerender } = renderHook(({ p }) => usePaged(p), { initialProps: { p: '/api/a' } });
    rerender({ p: '/api/b' });
    await waitFor(() => expect(gates.length).toBe(2));
    await act(async () => { gates[1](); });           // the newer request answers first
    await waitFor(() => expect(result.current.rows[0]?.id).toBe('/api/b-2'));
    await act(async () => { gates[0](); });           // then the stale one
    await new Promise((r) => setTimeout(r, 10));
    expect(result.current.rows[0].id).toBe('/api/b-2');
  });
});

describe('source detail', () => {
  it('describes the effective interval from the basis the server sent', () => {
    expect(describeEffective({ declaredInterval: 3600, effectiveInterval: null, policyInterval: 21600, cadenceBasis: 'policy' }))
      .toMatch(/6 ?h \(policy\).*not recorded/);
    expect(describeEffective({ declaredInterval: 3600, effectiveInterval: 900, cadenceBasis: 'override' })).toMatch(/15 ?min \(override\)/);
    expect(describeEffective({ declaredInterval: 0, effectiveInterval: null, cadenceBasis: 'on demand' })).toMatch(/on demand/);
    // never "as declared" for a row the policy holds
    expect(describeEffective({ declaredInterval: 3600, effectiveInterval: null, policyInterval: 21600, cadenceBasis: 'policy' })).not.toMatch(/as declared/);
  });
  it('a cycling row is not described as stopping at page_max', () => {
    expect(describePaging({ strategy: 'page_param', page_param: 'offset', page_size: 100, page_max: 50, page_resume: 1 })).toMatch(/50 pages per run/);
    expect(describePaging({ strategy: 'page_param', page_param: 'offset', page_max: 50 })).toMatch(/up to 50 pages/);
    expect(describePaging({ strategy: 'single_request', window_days: 7 })).toMatch(/per window/);
  });
  it('the run log loads in the page, through the API client, with its total', async () => {
    const calls = mockFetch((url) => {
      const offset = Number(qp(url, 'offset') || 0);
      const limit = Number(qp(url, 'limit') || 50);
      const n = Math.max(0, Math.min(limit, 70 - offset));
      return { data: Array.from({ length: n }, (_, i) => ({ id: offset + i, timestamp: '2026-11-01T00:00:00Z', status: 'ok', records_fetched: 1, stored: 1 })),
        page: { limit, offset, total: 70 } };
    });
    render(<MemoryRouter><RunsLog sourceId="S1" /></MemoryRouter>);
    await waitFor(() => expect(screen.getByText(/50 of 70/)).toBeTruthy());
    fireEvent.click(screen.getByRole('button', { name: /Load more/ }));
    await waitFor(() => expect(calls.some((u) => qp(u, 'offset') === '50')).toBe(true));
    expect(calls.every((u) => path(u) === '/api/sources/S1/logs')).toBe(true);
  });
  it('reads avg_duration_ms_30d, the field the server writes', async () => {
    const DETAIL = {
      registry: { id: 'S1', name: 'S one', collector: 'c', kind: 'scheduled', engine: 'vjson' },
      engine: { engine: 'vjson' },
      schedule: { declaredInterval: 3600, effectiveInterval: null, policyInterval: 21600, cadenceBasis: 'policy', consecutiveFailures: 0, consecutiveEmpties: 0,
        stats_30d: { runs_30d: 4, success_rate_30d: 1, avg_duration_ms_30d: 1234, last_run_at: null } },
      status: { status: 'online', trust: { rated: false } },
      data: { item_count: 0, geocoded: 0, record_types: { values: [] }, languages: [], field_inventory: { fields: { values: [] } } },
      notices: { latest: [], counts: [] }, runs: { recent: [], shown: 0, total: 0 }, credentials: {}, pacing: {}, provenance: {}, meta: {},
    };
    mockFetch((url) => (/\/detail$/.test(path(url)) ? DETAIL : /\/summary$/.test(path(url)) ? { __status: 503, error: 'x', facts: {} } : { data: [] }));
    render(<MemoryRouter initialEntries={['/console/sources/S1']}><Routes><Route path="/console/sources/:id" element={<SourceDetailPage />} /></Routes></MemoryRouter>);
    await waitFor(() => expect(screen.getByText('1234 ms')).toBeTruthy());
    expect(screen.queryByText(/as declared/)).toBeNull();
    // no AuthProvider here: the reader is not known to be allowed to run it
    expect(screen.queryByText(/Run now/)).toBeNull();
  });
});

describe('CadenceCard', () => {
  it("the server's can_write decides over the caller's guess", async () => {
    mockFetch(() => ({ policy: { global_interval_sec: 21600, mode: 'global' }, can_write: true, source: 'default' }));
    render(<MemoryRouter><CadenceCard canEdit={false} /></MemoryRouter>);
    await waitFor(() => expect(screen.getByText('Change')).toBeTruthy());
    cleanup();
    mockFetch(() => ({ policy: { global_interval_sec: 21600, mode: 'global' }, can_write: false, source: 'default' }));
    render(<MemoryRouter><CadenceCard canEdit /></MemoryRouter>);
    await waitFor(() => expect(screen.getByText(/every scheduled source/)).toBeTruthy());
    expect(screen.queryByText('Change')).toBeNull();
  });
});
