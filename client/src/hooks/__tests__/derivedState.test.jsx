// The set-state-in-effect restructuring (reset-on-prop-change derived while
// rendering; loaders started from an async callback) must keep the behaviour
// the effects had. One test per pattern.
import React, { useCallback, useRef, useState } from 'react';
import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, renderHook, act, cleanup, screen, waitFor } from '@testing-library/react';
import useLoadOnChange from '../useLoadOnChange.js';
import useMapProjection from '../useMapProjection.js';
import { useMoreRows } from '../usePagedList.js';
import { useEntitySearch } from '../useSearch.js';
import { ThresholdRow } from '../../components/console/ThresholdsTab.jsx';

afterEach(() => { cleanup(); vi.restoreAllMocks(); });

describe('useLoadOnChange', () => {
  it('runs the loader once per identity and not after unmount', async () => {
    const load = vi.fn();
    const { rerender, unmount } = renderHook(({ l }) => useLoadOnChange(l), { initialProps: { l: load } });
    await act(async () => { await Promise.resolve(); });
    expect(load).toHaveBeenCalledTimes(1);
    const load2 = vi.fn();
    rerender({ l: load2 });
    await act(async () => { await Promise.resolve(); });
    expect(load2).toHaveBeenCalledTimes(1);
    const load3 = vi.fn();
    rerender({ l: load3 });
    unmount();
    await act(async () => { await Promise.resolve(); });
    expect(load3).not.toHaveBeenCalled();
  });

  it('lets the loader set state (including a leading loading flag)', async () => {
    function C() {
      const [s, setS] = useState('idle');
      const load = useCallback(async () => { setS('loading'); await Promise.resolve(); setS('done'); }, []);
      useLoadOnChange(load);
      return <div data-testid="s">{s}</div>;
    }
    render(<C />);
    await waitFor(() => expect(screen.getByTestId('s').textContent).toBe('done'));
  });
});

describe('useMoreRows', () => {
  it('starts over when the parent hands it a new first page', () => {
    const a = [{ id: 1 }];
    const b = [{ id: 2 }, { id: 3 }];
    const { result, rerender } = renderHook(({ rows, page }) => useMoreRows(rows, page, () => '/x'), {
      initialProps: { rows: a, page: { has_more: true, offset: 0, count: 1, total: 5 } },
    });
    expect(result.current.rows).toEqual(a);
    rerender({ rows: b, page: { has_more: false, offset: 0, count: 2, total: 2 } });
    expect(result.current.rows).toEqual(b);
    expect(result.current.hasMore).toBe(false);
  });
});

describe('useEntitySearch', () => {
  it('drops results and the loading flag when the box is emptied', async () => {
    const realFetch = globalThis.fetch;
    globalThis.fetch = vi.fn(async () => new Response(JSON.stringify({ results: [{ entity_id: 'e1', value: 'v' }] }), { status: 200 }));
    try {
      const { result, rerender } = renderHook(({ q }) => useEntitySearch(q), { initialProps: { q: 'abc' } });
      expect(result.current.loading).toBe(true);               // flagged in the render that saw the query
      await waitFor(() => expect(result.current.results.length).toBe(1), { timeout: 2000 });
      expect(result.current.resolved).toBe('abc');
      rerender({ q: '  ' });
      expect(result.current.results).toEqual([]);
      expect(result.current.resolved).toBeNull();
      expect(result.current.loading).toBe(false);
    } finally { globalThis.fetch = realFetch; }
  });
});

describe('useMapProjection', () => {
  it('never shows the previous point\'s pixels for a changed or invalid point', async () => {
    const map = { project: ([lng, lat]) => ({ x: lng, y: lat }), on() {}, off() {} };
    const { result, rerender } = renderHook(({ ll }) => useMapProjection(useRef(map), ll), { initialProps: { ll: [139, 35] } });
    await waitFor(() => expect(result.current).toEqual({ x: 139, y: 35 }));
    rerender({ ll: [140, 36] });
    expect(result.current).toBeNull();                          // stale projection is not served
    await waitFor(() => expect(result.current).toEqual({ x: 140, y: 36 }));
    rerender({ ll: [NaN, 1] });
    expect(result.current).toBeNull();
  });
});

describe('ThresholdRow draft', () => {
  const base = { key: 'k', value: 0.9, min: 0, max: 1, default: 0.9, source: 'default', env: 'E', metric: 'm', description: 'd' };
  it('follows the server value after a save lands, keeps edits otherwise', () => {
    const { rerender } = render(<ThresholdRow t={base} onSaved={() => {}} />);
    const input = screen.getByLabelText('k value');
    expect(input.value).toBe('0.9');
    act(() => { input.focus(); });
    rerender(<ThresholdRow t={{ ...base }} onSaved={() => {}} />);   // same value: nothing re-seeds
    expect(screen.getByLabelText('k value').value).toBe('0.9');
    rerender(<ThresholdRow t={{ ...base, value: 0.95, source: 'db' }} onSaved={() => {}} />);
    expect(screen.getByLabelText('k value').value).toBe('0.95');
  });
});
