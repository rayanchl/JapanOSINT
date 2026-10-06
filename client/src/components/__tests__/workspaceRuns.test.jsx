// Decided 2026-10-05: everything in a workspace — records, searches, query
// syntheses — is visible to every member. The Search page lists every
// member's OSINT runs (GET /api/search/runs), each naming who ran it.
import React from 'react';
import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, cleanup, screen, fireEvent, waitFor } from '@testing-library/react';
import { memberNameMap, authorLabel } from '../../hooks/useMembers.js';
import WorkspaceRuns, { synthesisLine, runStatusTone } from '../search/WorkspaceRuns.jsx';

const realFetch = globalThis.fetch;
afterEach(() => { cleanup(); globalThis.fetch = realFetch; vi.restoreAllMocks(); });

const MEMBERS = { members: [{ user_id: 'u-me', email: 'me@x.test', role: 'analyst' }, { user_id: 'u-mate', email: 'mate@x.test', role: 'analyst' }], invites: [] };

function mockFetch(routes) {
  const calls = [];
  globalThis.fetch = vi.fn(async (input) => {
    const url = typeof input === 'string' ? input : input.url;
    calls.push(url);
    const hit = Object.keys(routes).find((k) => url.startsWith(k));
    const body = hit ? (typeof routes[hit] === 'function' ? routes[hit](url) : routes[hit]) : {};
    return new Response(JSON.stringify(body), { status: 200, headers: { 'content-type': 'application/json' } });
  });
  return calls;
}

describe('author labels', () => {
  const names = memberNameMap(MEMBERS);
  it('maps the roster', () => {
    expect(names).toEqual({ 'u-me': 'me@x.test', 'u-mate': 'mate@x.test' });
  });
  it('says you, a teammate by email, a former member, or no author', () => {
    expect(authorLabel({ user_id: 'u-me', mine: true }, names)).toBe('you');
    expect(authorLabel({ user_id: 'u-mate', mine: false }, names)).toBe('mate@x.test');
    expect(authorLabel({ user_id: 'u-gone', mine: false }, names)).toBe('a former member');
    expect(authorLabel({ user_id: null }, names)).toBe('unknown author');
    expect(authorLabel({ user_id: 'u-mate', mine: false }, {})).toBe('a teammate');
  });
});

describe('workspace runs', () => {
  it('status tones and an honest synthesis preview', () => {
    expect(runStatusTone('running')).toBe('accent');
    expect(runStatusTone('unknown')).toBe('neutral');
    expect(synthesisLine({ synthesis_preview: 'short', synthesis_truncated: false })).toBe('short');
    expect(synthesisLine({ synthesis_preview: 'start of it ', synthesis_truncated: true, synthesis_bytes: 2048 }))
      .toBe('start of it… (preview of 2048 bytes — open the run to read it all)');
    expect(synthesisLine({ synthesis_preview: null })).toBeNull();
  });

  it("lists every member's runs with their author, and narrows to mine on request", async () => {
    const runs = {
      data: [
        { request_id: 'r2', query: 'osaka shipping', user_id: 'u-me', mine: true, created_at: '2026-10-05 10:00:00', status: 'running', phase: 'collecting', synthesis_preview: null },
        { request_id: 'r1', query: 'who owns kobe port', user_id: 'u-mate', mine: false, created_at: '2026-10-04 10:00:00', status: 'completed', phase: 'completed', synthesis_preview: 'Kobe Port Holdings', synthesis_truncated: false },
      ],
      page: { limit: 50, count: 2, total: 2, next_cursor: null },
      meta: { scope: 'workspace' },
    };
    const calls = mockFetch({ '/api/members': MEMBERS, '/api/search/runs': runs });
    const onOpen = vi.fn(async () => {});
    render(<WorkspaceRuns onOpen={onOpen} />);
    await screen.findByText('who owns kobe port');
    await screen.findByText('mate@x.test');
    expect(screen.getByText('you')).toBeTruthy();
    expect(screen.getByText('Kobe Port Holdings')).toBeTruthy();
    expect(screen.getByText('2 runs')).toBeTruthy();
    fireEvent.click(screen.getAllByText('Open')[1]);
    await waitFor(() => expect(onOpen).toHaveBeenCalledWith('r1'));
    fireEvent.click(screen.getByText('mine'));
    await waitFor(() => expect(calls.some((u) => u.includes('/api/search/runs') && u.includes('mine=1'))).toBe(true));
  });
});
