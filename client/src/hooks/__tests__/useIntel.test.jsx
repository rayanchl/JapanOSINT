// Regression (findings 4 and 8): the Intel "Recent" view and the per-source
// items page put sinceIso() — Date.now() at millisecond precision — straight
// into useIntelItems' params, which are keyed by JSON.stringify: every render
// refetched and every response re-rendered (84 COUNT requests per 500 ms).
// The same pages sent has_geom='1', which the server (yes|no) ignores.
import React from 'react';
import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, renderHook, cleanup, fireEvent, screen } from '@testing-library/react';
import { MemoryRouter, Routes, Route } from 'react-router-dom';
import { useIntelItems, sinceIso, useSinceIso } from '../useIntel.js';
import IntelPage from '../../components/intel/IntelPage.jsx';
import IntelSourceItemsPage from '../../components/intel/IntelSourceItemsPage.jsx';

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const realFetch = globalThis.fetch;

function mockFetch() {
  const calls = [];
  globalThis.fetch = vi.fn(async (input) => {
    const url = typeof input === 'string' ? input : input.url;
    calls.push(url);
    await sleep(2);
    const body = url.includes('/api/intel/sources') ? { data: [] } : { data: [], page: { next_cursor: null, total: 0 }, meta: {} };
    return new Response(JSON.stringify(body), { status: 200, headers: { 'content-type': 'application/json' } });
  });
  return calls;
}

afterEach(() => { cleanup(); globalThis.fetch = realFetch; vi.restoreAllMocks(); });

describe('sinceIso / useSinceIso', () => {
  it('gives the same value for calls a few ms apart (minute-floored)', () => {
    const t = Date.UTC(2026, 9, 2, 12, 34, 56, 789);
    expect(sinceIso('24h', t)).toBe(sinceIso('24h', t + 3));
    expect(sinceIso('24h', t)).toBe('2026-10-01T12:34:00.000Z');
    expect(sinceIso('', t)).toBeUndefined();
  });

  it('useSinceIso is stable across re-renders until the preset changes', () => {
    const { result, rerender } = renderHook(({ p }) => useSinceIso(p), { initialProps: { p: '24h' } });
    const first = result.current;
    rerender({ p: '24h' });
    expect(result.current).toBe(first);
    rerender({ p: '7d' });
    expect(result.current).not.toBe(first);
  });

  it('useIntelItems fed with useSinceIso fetches once, not in a loop', async () => {
    const calls = mockFetch();
    renderHook(() => useIntelItems('/api/intel/items', { since: useSinceIso('24h'), limit: 50 }));
    await sleep(400);
    expect(calls.filter((u) => u.includes('/api/intel/items')).length).toBe(1);
  });
});

describe('Intel pages', () => {
  it('Recent view requests a bounded number of times and sends has_geom=yes', async () => {
    const calls = mockFetch();
    render(<MemoryRouter initialEntries={['/intel?view=recent']}><IntelPage /></MemoryRouter>);
    await sleep(400);
    const items = calls.filter((u) => u.includes('/api/intel/items'));
    expect(items.length).toBeGreaterThanOrEqual(1);
    expect(items.length).toBeLessThanOrEqual(2);
    fireEvent.click(screen.getByText('geolocated only'));
    await sleep(300);
    const after = calls.filter((u) => u.includes('/api/intel/items'));
    expect(after.length).toBeLessThanOrEqual(items.length + 2);
    expect(after[after.length - 1]).toContain('has_geom=yes');
    expect(after.some((u) => u.includes('has_geom=1'))).toBe(false);
  });

  it('per-source items page does not refetch in a loop', async () => {
    const calls = mockFetch();
    render(
      <MemoryRouter initialEntries={['/intel/sources/SRC']}>
        <Routes><Route path="/intel/sources/:id" element={<IntelSourceItemsPage />} /></Routes>
      </MemoryRouter>,
    );
    fireEvent.click(screen.getByText('24 h'));
    await sleep(400);
    const items = calls.filter((u) => u.includes('/api/intel/items'));
    expect(items.length).toBeLessThanOrEqual(3);
  });

  it('Nearby mode offers no text filter and no translation toggle (the server applies neither)', async () => {
    mockFetch();
    render(<MemoryRouter initialEntries={['/intel?view=search&mode=near&near=35.68,139.76']}><IntelPage /></MemoryRouter>);
    await sleep(50);
    expect(screen.queryByPlaceholderText(/text filter/i)).toBeNull();
    expect(screen.queryByText('原文')).toBeNull();
    expect(screen.getByText(/Filters by distance only/)).toBeTruthy();
  });
});
