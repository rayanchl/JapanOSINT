// /api/search/suggest?expand=1 also asks the suggest LLM (native/core/suggest.h):
// slow and paid. The Search page asks for it only on an explicit action ("More
// ideas", Tab) or ONE long typing pause — never per keystroke, never twice for
// one query, never recorded as search history — and drops a stale answer.
import React from 'react';
import { describe, it, expect, vi, afterEach, beforeEach } from 'vitest';
import { render, cleanup, fireEvent, screen, act, createEvent } from '@testing-library/react';
import { MemoryRouter } from 'react-router-dom';
import SearchPage from '../SearchPage.jsx';
import { SUGGEST_DEBOUNCE_MS, SUGGEST_EXPAND_PAUSE_MS } from '../../../hooks/useSearch.js';

const realFetch = globalThis.fetch;
beforeEach(() => { vi.useFakeTimers(); });
afterEach(() => { cleanup(); globalThis.fetch = realFetch; vi.restoreAllMocks(); vi.useRealTimers(); });

const RETRIEVED = (q) => ({
  suggestions: [`${q} history`],
  items: [{ text: `${q} history`, source: 'history', similarity: 0.9 }],
  meta: { retrieval: 'embedding', candidates: 1, shown: 1, cut_at: 9, truncated: false, llm: 'not_requested (expand=1 asks the model)' },
});
const EXPANDED = (q) => ({
  suggestions: [`${q} history`, `${q} ai idea`],
  items: [{ text: `${q} history`, source: 'history', similarity: 0.9 }, { text: `${q} ai idea`, source: 'llm' }],
  meta: { retrieval: 'embedding', candidates: 2, shown: 2, cut_at: 9, truncated: false, llm: 'included' },
});

/** fetch mock: `suggest(q, expand, init)` answers the suggest route (a value or
 *  a promise); everything else gets an empty object. Records every call. */
function mockFetch(suggest) {
  const calls = [];
  globalThis.fetch = vi.fn(async (input, init = {}) => {
    const url = typeof input === 'string' ? input : input.url;
    const method = (init.method || 'GET').toUpperCase();
    calls.push({ url, method, init });
    let out = {};
    if (url.includes('/api/search/suggest')) {
      const u = new URL(url, 'http://x');
      out = await suggest(u.searchParams.get('q'), u.searchParams.get('expand') === '1', init);
    }
    return new Response(JSON.stringify(out), { status: 200, headers: { 'content-type': 'application/json' } });
  });
  return calls;
}
const suggestCalls = (calls) => calls.filter((c) => c.url.includes('/api/search/suggest'));
const expandCalls = (calls) => suggestCalls(calls).filter((c) => c.url.includes('expand=1'));
const historyPosts = (calls) => calls.filter((c) => c.method === 'POST' && c.url.includes('/api/search-history'));

const box = () => screen.getByPlaceholderText(/email, IP, domain/);
async function type(v) { await act(async () => { fireEvent.change(box(), { target: { value: v } }); }); }
async function wait(ms) { await act(async () => { await vi.advanceTimersByTimeAsync(ms); }); }
function renderPage() { return render(<MemoryRouter><SearchPage /></MemoryRouter>); }

describe('search suggestions: expand=1', () => {
  it('the fast path is unchanged: one plain request after the debounce, and a "More ideas" button', async () => {
    const calls = mockFetch((q) => RETRIEVED(q));
    renderPage();
    await type('kobe');
    await wait(SUGGEST_DEBOUNCE_MS - 1);
    expect(suggestCalls(calls)).toHaveLength(0);
    await wait(1);
    expect(suggestCalls(calls)).toHaveLength(1);
    expect(suggestCalls(calls)[0].url).toBe('/api/search/suggest?q=kobe');
    expect(screen.getByText('kobe history')).toBeTruthy();
    expect(screen.getByRole('button', { name: /More ideas/ })).toBeTruthy();
    expect(expandCalls(calls)).toHaveLength(0);
  });

  it('"More ideas" asks once with expand=1 and marks the AI suggestions', async () => {
    const calls = mockFetch((q, expand) => (expand ? EXPANDED(q) : RETRIEVED(q)));
    renderPage();
    await type('kobe');
    await wait(SUGGEST_DEBOUNCE_MS);
    await act(async () => { fireEvent.click(screen.getByRole('button', { name: /More ideas/ })); });
    await wait(0);
    expect(expandCalls(calls)).toHaveLength(1);
    expect(expandCalls(calls)[0].url).toBe('/api/search/suggest?q=kobe&expand=1');
    const ai = screen.getByText('kobe ai idea').closest('button');
    expect(ai.getAttribute('title')).toMatch(/AI suggestion/);
    expect(ai.querySelector('[aria-label="AI suggestion"]')).toBeTruthy();
    expect(screen.getByText('kobe history').closest('button').getAttribute('title')).toBe('from history');
    // Asked once: the button is gone, and neither Tab nor the pause asks again.
    expect(screen.queryByRole('button', { name: /More ideas/ })).toBeNull();
    fireEvent.keyDown(box(), { key: 'Tab' });
    await wait(SUGGEST_EXPAND_PAUSE_MS * 2);
    expect(expandCalls(calls)).toHaveLength(1);
    expect(historyPosts(calls)).toHaveLength(0);
  });

  it('Tab in the box expands while suggestions are open, and is a plain Tab otherwise', async () => {
    const calls = mockFetch((q, expand) => (expand ? EXPANDED(q) : RETRIEVED(q)));
    renderPage();
    await type('kobe');
    // Nothing on show yet: Tab is not intercepted.
    const early = createEvent.keyDown(box(), { key: 'Tab' });
    fireEvent(box(), early);
    expect(early.defaultPrevented).toBe(false);
    await wait(SUGGEST_DEBOUNCE_MS);
    const tab = createEvent.keyDown(box(), { key: 'Tab' });
    await act(async () => { fireEvent(box(), tab); });
    await wait(0);
    expect(tab.defaultPrevented).toBe(true);
    expect(expandCalls(calls)).toHaveLength(1);
    expect(screen.getByText('kobe ai idea')).toBeTruthy();
    const shiftTab = createEvent.keyDown(box(), { key: 'Tab', shiftKey: true });
    fireEvent(box(), shiftTab);
    expect(shiftTab.defaultPrevented).toBe(false);
  });

  it('a long pause expands exactly once per query; keystrokes inside it never do', async () => {
    const calls = mockFetch((q, expand) => (expand ? EXPANDED(q) : RETRIEVED(q)));
    renderPage();
    // Steady typing, each gap shorter than the pause: no expand at all.
    for (const v of ['kob', 'kobe', 'kobe p', 'kobe po', 'kobe por', 'kobe port']) {
      await type(v);
      await wait(SUGGEST_DEBOUNCE_MS + 500);
    }
    expect(expandCalls(calls)).toHaveLength(0);
    await wait(SUGGEST_EXPAND_PAUSE_MS);
    expect(expandCalls(calls)).toHaveLength(1);
    expect(expandCalls(calls)[0].url).toBe(`/api/search/suggest?q=${encodeURIComponent('kobe port')}&expand=1`);
    expect(screen.getByText('kobe port ai idea')).toBeTruthy();
    await wait(SUGGEST_EXPAND_PAUSE_MS * 3);
    expect(expandCalls(calls)).toHaveLength(1);
    // Away and back to the same query: still the one request for it.
    await type('kobe portx');
    await type('kobe port');
    await wait(SUGGEST_EXPAND_PAUSE_MS * 2);
    expect(expandCalls(calls).filter((c) => c.url.includes(encodeURIComponent('kobe port') + '&'))).toHaveLength(1);
    expect(historyPosts(calls)).toHaveLength(0);
  });

  it('never expands when the server answered by generation only (no meta.retrieval)', async () => {
    const calls = mockFetch(() => ({ suggestions: ['kobe a', 'kobe b'] }));
    renderPage();
    await type('kobe');
    await wait(SUGGEST_DEBOUNCE_MS + SUGGEST_EXPAND_PAUSE_MS * 2);
    expect(screen.getByText('kobe a')).toBeTruthy();
    expect(screen.queryByRole('button', { name: /More ideas/ })).toBeNull();
    const tab = createEvent.keyDown(box(), { key: 'Tab' });
    fireEvent(box(), tab);
    expect(tab.defaultPrevented).toBe(false);
    expect(expandCalls(calls)).toHaveLength(0);
  });

  it('a changed query aborts the expand in flight and its late answer is ignored', async () => {
    let release;
    const late = new Promise((r) => { release = r; });
    const calls = mockFetch((q, expand) => {
      if (expand && q === 'kobe') return late.then(() => EXPANDED('kobe'));
      return RETRIEVED(q);
    });
    renderPage();
    await type('kobe');
    await wait(SUGGEST_DEBOUNCE_MS);
    await act(async () => { fireEvent.click(screen.getByRole('button', { name: /More ideas/ })); });
    expect(screen.getByRole('button', { name: /Asking the model/ })).toBeTruthy();
    const { signal } = expandCalls(calls)[0].init;
    expect(signal.aborted).toBe(false);
    await type('osaka');
    expect(signal.aborted).toBe(true);
    await act(async () => { release(); await vi.advanceTimersByTimeAsync(0); });
    expect(screen.queryByText('kobe ai idea')).toBeNull();
    await wait(SUGGEST_DEBOUNCE_MS);
    expect(screen.getByText('osaka history')).toBeTruthy();
    expect(screen.queryByText('kobe ai idea')).toBeNull();
  });

  it('a failed expand keeps the ordinary suggestions and says it failed', async () => {
    mockFetch((q, expand) => { if (expand) throw new Error('down'); return RETRIEVED(q); });
    renderPage();
    await type('kobe');
    await wait(SUGGEST_DEBOUNCE_MS);
    await act(async () => { fireEvent.click(screen.getByRole('button', { name: /More ideas/ })); });
    await wait(0);
    expect(screen.getByText('kobe history')).toBeTruthy();
    expect(screen.getByText(/could not be asked for more ideas/)).toBeTruthy();
  });
});
