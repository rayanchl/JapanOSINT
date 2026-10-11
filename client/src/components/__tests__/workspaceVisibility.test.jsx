// Decided 2026-10-05: everything in a workspace — records, searches, query
// syntheses — is visible to every member. Shared rows name their author, and
// only the author (or, for a note, a workspace owner/admin) is offered the
// buttons that change them.
import React from 'react';
import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, cleanup, screen } from '@testing-library/react';
import SearchHistoryDropdown from '../search/SearchHistoryDropdown.jsx';
import AnnotationsSection from '../intel/AnnotationsSection.jsx';
import { MemoryRouter } from 'react-router-dom';

const mockAuth = vi.hoisted(() => ({ value: { me: { user: { id: 'u-me' } }, canManageWorkspace: false } }));
vi.mock('../../auth/AuthContext.jsx', () => ({ useAuth: () => mockAuth.value }));

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

describe('search history dropdown', () => {
  it('says the history is shared with the workspace and names who ran each search', async () => {
    mockFetch({
      '/api/members': MEMBERS,
      '/api/search-history': { data: [{ id: 1, kind: 'intel', params: { q: 'tokyo' }, result_count: 3, ts: '2026-10-05 10:00:00', user_id: 'u-mate', mine: false }], page: { count: 1 }, meta: { scope: 'workspace', retained_max: 1000 } },
    });
    render(<MemoryRouter><SearchHistoryDropdown open onRun={() => {}} /></MemoryRouter>);
    await screen.findByText('tokyo');
    await screen.findByText('mate@x.test');
    expect(screen.getByText('Shared with your workspace')).toBeTruthy();
    expect(screen.queryByText(/Private to you/)).toBeNull();
    expect(screen.queryByText(/cannot read what you have been investigating/)).toBeNull();
  });
});

describe('notes', () => {
  const notes = { data: [
    { id: 'n1', body_md: 'my note', author_id: 'u-me', created_at: '2026-10-05 10:00:00', is_deleted: false },
    { id: 'n2', body_md: 'their note', author_id: 'u-mate', created_at: '2026-10-05 09:00:00', is_deleted: false },
  ], page: { next_cursor: null } };

  it('offers edit and delete only on your own note, and names the teammate', async () => {
    mockAuth.value = { me: { user: { id: 'u-me' } }, canManageWorkspace: false };
    mockFetch({ '/api/members': MEMBERS, '/api/annotations': notes });
    render(<AnnotationsSection refType="intel_item" refId="x" />);
    await screen.findByText('their note');
    await screen.findByText(/mate@x\.test/);
    expect(screen.getAllByText('Edit')).toHaveLength(1);
    expect(screen.getAllByText('Delete')).toHaveLength(1);
  });

  it('lets a workspace owner/admin delete (moderate) a teammate note but not edit it', async () => {
    mockAuth.value = { me: { user: { id: 'u-me' } }, canManageWorkspace: true };
    mockFetch({ '/api/members': MEMBERS, '/api/annotations': notes });
    render(<AnnotationsSection refType="intel_item" refId="x" />);
    await screen.findByText('their note');
    expect(screen.getAllByText('Edit')).toHaveLength(1);
    expect(screen.getAllByText('Delete')).toHaveLength(2);
  });
});
