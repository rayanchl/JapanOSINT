// Decided 2026-10-05: everything a workspace authors is visible to all of its
// members. Cases, areas of interest, watchlists, alert rules and breach
// monitors come back with `created_by`, and the client now names that author.
//
// What it may NOT do is invent an author-only rule. For these five resources
// the server gates changes on the caller's workspace ROLE (and, for a case,
// its roster) — never on who created the row — so a teammate's area is as
// editable as your own for an analyst, and neither is for a viewer:
//   aoiapi.c / alertsapi.c   create, edit, delete: analyst, admin or owner
//   breach_monitor.c         delete, rescan, domain proofs: analyst or above
//   casesapi.c               write: analyst+ OR on the roster;
//                            delete: owner/admin OR the case lead;
//                            roster: analyst+ OR the case lead
import React from 'react';
import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, cleanup, screen } from '@testing-library/react';
import { MemoryRouter, Routes, Route } from 'react-router-dom';
import CasesPage from '../cases/CasesPage.jsx';
import CaseDetailPage from '../cases/CaseDetailPage.jsx';
import { CasePickerSheet } from '../cases/CasePickerSheet.jsx';
import AOIPage from '../alerts/AOIPage.jsx';
import WatchlistsPage from '../alerts/WatchlistsPage.jsx';
import AlertsPage from '../alerts/AlertsPage.jsx';
import AlertInboxPage from '../alerts/AlertInboxPage.jsx';
import BreachMonitorsPage from '../alerts/BreachMonitorsPage.jsx';
import SearchHistoryDropdown from '../search/SearchHistoryDropdown.jsx';
import { caseCanWrite, caseCanDelete, caseCanManageRoster } from '../../hooks/useCases.js';
import { roleRank, canWriteWorkspace, isWorkspaceAdmin } from '../../auth/roles.js';

const mockAuth = vi.hoisted(() => ({ value: null }));
vi.mock('../../auth/AuthContext.jsx', () => ({ useAuth: () => mockAuth.value }));
function signedInAs(role) {
  mockAuth.value = { me: { user: { id: 'u-me' } }, role, canManageWorkspace: role === 'owner' || role === 'admin' };
}

const realFetch = globalThis.fetch;
afterEach(() => { cleanup(); globalThis.fetch = realFetch; vi.restoreAllMocks(); });

const MEMBERS = { members: [{ user_id: 'u-me', email: 'me@x.test', role: 'analyst' }, { user_id: 'u-mate', email: 'mate@x.test', role: 'analyst' }], invites: [] };

/** fetch mock keyed on the exact pathname; every URL is recorded. */
function mockFetch(routes) {
  const calls = [];
  globalThis.fetch = vi.fn(async (input) => {
    const url = typeof input === 'string' ? input : input.url;
    calls.push(url);
    const hit = routes[new URL(url, 'http://x').pathname];
    const body = typeof hit === 'function' ? hit(url) : hit;
    return new Response(JSON.stringify(body ?? {}), { status: 200, headers: { 'content-type': 'application/json' } });
  });
  return calls;
}
const inRouter = (ui) => render(<MemoryRouter>{ui}</MemoryRouter>);
const T = '2026-10-05 10:00:00';

describe('roles mirror the server ranking', () => {
  it('ranks owner > admin > analyst > viewer and fails closed on anything else', () => {
    expect(['owner', 'admin', 'analyst', 'viewer', 'superuser', undefined, null, 'constructor'].map(roleRank)).toEqual([4, 3, 2, 1, 0, 0, 0, 0]);
    expect(['owner', 'admin', 'analyst', 'viewer', undefined].map(canWriteWorkspace)).toEqual([true, true, true, false, false]);
    expect(['owner', 'admin', 'analyst', 'viewer'].map(isWorkspaceAdmin)).toEqual([true, true, false, false]);
  });

  it('case gates combine the workspace role with the case roster, exactly as casesapi.c does', () => {
    //            workspace role, my_case_role → [write, delete, roster]
    const table = [
      ['owner', null, [true, true, true]],
      ['admin', null, [true, true, true]],
      ['analyst', null, [true, false, true]],
      ['analyst', 'lead', [true, true, true]],
      ['viewer', null, [false, false, false]],
      ['viewer', 'viewer', [false, false, false]],   // a case viewer reads only (2026-10-09)
      ['viewer', 'contributor', [true, false, false]],
      ['viewer', 'lead', [true, true, true]],
      [undefined, null, [false, false, false]],
    ];
    for (const [role, mine, want] of table) {
      expect([caseCanWrite(role, mine), caseCanDelete(role, mine), caseCanManageRoster(role, mine)], `${role}/${mine}`).toEqual(want);
    }
  });
});

const CASES = { data: [
  { id: 'c1', name: 'My case', status: 'open', priority: 0, created_by: 'u-me', created_at: T, updated_at: T, item_count: 0 },
  { id: 'c2', name: 'Their case', status: 'open', priority: 0, created_by: 'u-mate', created_at: T, updated_at: T, item_count: 2 },
], page: { next_cursor: null } };

describe('cases list', () => {
  it('names who created each case, and gives an analyst the status buttons on every case, not only their own', async () => {
    signedInAs('analyst');
    mockFetch({ '/api/members': MEMBERS, '/api/cases': CASES });
    inRouter(<CasesPage />);
    await screen.findByText('Their case');
    await screen.findByText(/created by mate@x\.test/);
    expect(screen.getByText(/created by you/)).toBeTruthy();
    expect(screen.getAllByText('Close')).toHaveLength(2);
    expect(screen.getByText('New case')).toBeTruthy();
  });

  it('offers a viewer no create or status change (403 unless on the roster, which the list does not carry), and says why', async () => {
    signedInAs('viewer');
    mockFetch({ '/api/members': MEMBERS, '/api/cases': CASES });
    inRouter(<CasesPage />);
    await screen.findByText(/created by mate@x\.test/);
    expect(screen.queryByText('Close')).toBeNull();
    expect(screen.queryByText('New case')).toBeNull();
    expect(screen.getByText(/you can read every case/)).toBeTruthy();
  });
});

describe('case detail', () => {
  const detail = (myCaseRole) => ({ data: {
    id: 'c1', name: 'Case one', summary: '', status: 'open', priority: 0, created_by: 'u-mate',
    created_at: T, updated_at: T, closed_at: null, item_counts: { total: 0 }, item_count: 0, members: [], my_case_role: myCaseRole,
  } });
  async function open(role, myCaseRole) {
    signedInAs(role);
    mockFetch({ '/api/members': MEMBERS, '/api/cases/c1': detail(myCaseRole), '/api/cases/c1/items': { data: [], page: { next_cursor: null } } });
    render(<MemoryRouter initialEntries={['/cases/c1']}><Routes><Route path="/cases/:id" element={<CaseDetailPage />} /></Routes></MemoryRouter>);
    await screen.findByText('mate@x.test');      // the author, by email, not a raw uuid
    return {
      edit: !screen.getByText('Edit').closest('button').disabled,
      del: !screen.getByLabelText('Delete case').disabled,
    };
  }

  it('an analyst off the roster may edit a teammate’s case but not delete it', async () => {
    expect(await open('analyst', null)).toEqual({ edit: true, del: false });
  });
  it('the case lead may delete it, whoever created it', async () => {
    expect(await open('analyst', 'lead')).toEqual({ edit: true, del: true });
  });
  it('a workspace admin may delete any case', async () => {
    expect(await open('admin', null)).toEqual({ edit: true, del: true });
  });
  it('a workspace viewer may edit only as the case\'s contributor or lead; a case viewer reads only', async () => {
    expect(await open('viewer', 'contributor')).toEqual({ edit: true, del: false });
    cleanup();
    expect(await open('viewer', 'viewer')).toEqual({ edit: false, del: false });
    cleanup();
    expect(await open('viewer', null)).toEqual({ edit: false, del: false });
  });
});

describe('case picker', () => {
  it('opens after rendering closed, fetches nothing while closed, and names each case’s author', async () => {
    signedInAs('analyst');
    const calls = mockFetch({ '/api/members': MEMBERS, '/api/cases': CASES });
    const { rerender } = render(<CasePickerSheet open={false} refType="intel_item" refId="x" onClose={() => {}} />);
    expect(calls).toEqual([]);
    rerender(<CasePickerSheet open refType="intel_item" refId="x" onClose={() => {}} />);
    await screen.findByText('Their case');
    await screen.findByText('mate@x.test');
    expect(screen.getByText('you')).toBeTruthy();
  });
});

describe('search history dropdown', () => {
  it('reads the member roster only once it is open', async () => {
    signedInAs('analyst');
    const calls = mockFetch({
      '/api/members': MEMBERS,
      '/api/search-history': { data: [{ id: 1, kind: 'intel', params: { q: 'tokyo' }, result_count: 3, ts: T, user_id: 'u-mate', mine: false }], page: { count: 1, total: 1 }, meta: { scope: 'workspace' } },
    });
    const { rerender } = inRouter(<SearchHistoryDropdown open={false} onRun={() => {}} />);
    expect(calls).toEqual([]);
    rerender(<MemoryRouter><SearchHistoryDropdown open onRun={() => {}} /></MemoryRouter>);
    await screen.findByText('mate@x.test');
  });
});

const AOIS = { data: [
  { id: 'a1', name: 'My area', kind: 'bbox', geometry: [139, 35, 140, 36], bbox: [139, 35, 140, 36], created_by: 'u-me', created_at: T },
  { id: 'a2', name: 'Their area', kind: 'bbox', geometry: [139, 35, 140, 36], bbox: [139, 35, 140, 36], created_by: 'u-mate', created_at: T },
], page: { next_cursor: null } };

describe('areas of interest', () => {
  it('names the author and lets an analyst rename and delete any area (role-gated, not author-gated)', async () => {
    signedInAs('analyst');
    mockFetch({ '/api/members': MEMBERS, '/api/aoi': AOIS });
    inRouter(<AOIPage />);
    await screen.findByText('mate@x.test');
    expect(screen.getByText('you')).toBeTruthy();
    expect(screen.getAllByTitle('Rename')).toHaveLength(2);
    expect(screen.getAllByTitle('Delete')).toHaveLength(2);
    expect(screen.getByText('New area')).toBeTruthy();
  });

  it('gives a viewer no create, rename or delete — not even on an area they drew', async () => {
    signedInAs('viewer');
    mockFetch({ '/api/members': MEMBERS, '/api/aoi': AOIS });
    inRouter(<AOIPage />);
    await screen.findByText('mate@x.test');
    expect(screen.queryAllByTitle('Rename')).toHaveLength(0);
    expect(screen.queryAllByTitle('Delete')).toHaveLength(0);
    expect(screen.queryByText('New area')).toBeNull();
    expect(screen.getByText(/you can read every area here/)).toBeTruthy();
  });
});

const WATCHLISTS = { data: [
  { id: 'w1', name: 'Mine', entity_ids: ['e1'], rule_id: 'r1', created_by: 'u-me', enabled: true, muted_until: null, created_at: T },
  { id: 'w2', name: 'Theirs', entity_ids: ['e2'], rule_id: 'r2', created_by: 'u-mate', enabled: true, muted_until: null, created_at: T },
], page: { next_cursor: null } };

describe('watchlists', () => {
  it('names the author; an analyst may edit, toggle and delete every watchlist', async () => {
    signedInAs('analyst');
    mockFetch({ '/api/members': MEMBERS, '/api/watchlists': WATCHLISTS });
    inRouter(<WatchlistsPage />);
    await screen.findByText('mate@x.test');
    expect(screen.getAllByTitle('Edit')).toHaveLength(2);
    expect(screen.getAllByTitle('Delete')).toHaveLength(2);
    expect(screen.getAllByRole('switch').every((s) => !s.disabled)).toBe(true);
  });

  it('a viewer reads them, with the switches shown but disabled', async () => {
    signedInAs('viewer');
    mockFetch({ '/api/members': MEMBERS, '/api/watchlists': WATCHLISTS });
    inRouter(<WatchlistsPage />);
    await screen.findByText('mate@x.test');
    expect(screen.queryAllByTitle('Edit')).toHaveLength(0);
    expect(screen.queryAllByTitle('Delete')).toHaveLength(0);
    expect(screen.queryByText('New watchlist')).toBeNull();
    expect(screen.getAllByRole('switch').every((s) => s.disabled)).toBe(true);
  });
});

const rule = (over) => ({ id: 'r1', name: 'Tokyo', enabled: true, predicate: { q: 'tokyo' }, channels: [], dedup_window_sec: 3600, storm_cap_per_hour: 100, muted_until: null, created_at: T, updated_at: T, ...over });

describe('alert rules', () => {
  it('shows no author when the server sends none (GET /api/alerts does not emit created_by), and invents none', async () => {
    signedInAs('analyst');
    const calls = mockFetch({ '/api/members': MEMBERS, '/api/alerts': { data: [rule()] } });
    inRouter(<AlertsPage />);
    await screen.findByText('Tokyo');
    expect(screen.queryByText(/· by /)).toBeNull();
    expect(calls.some((u) => u.startsWith('/api/members'))).toBe(false);
    expect(screen.getByTitle('Edit')).toBeTruthy();
    expect(screen.getByTitle('Delete')).toBeTruthy();
  });

  it('names the author once the server sends created_by', async () => {
    signedInAs('analyst');
    mockFetch({ '/api/members': MEMBERS, '/api/alerts': { data: [rule({ created_by: 'u-mate' })] } });
    inRouter(<AlertsPage />);
    await screen.findByText(/· by mate@x\.test/);
  });

  it('a viewer may read rules and their history but change none', async () => {
    signedInAs('viewer');
    mockFetch({ '/api/members': MEMBERS, '/api/alerts': { data: [rule({ created_by: 'u-me' })] } });
    inRouter(<AlertsPage />);
    await screen.findByText(/· by you/);
    expect(screen.queryByTitle('Edit')).toBeNull();
    expect(screen.queryByTitle('Delete')).toBeNull();
    expect(screen.queryByText('Test')).toBeNull();
    expect(screen.queryByText('New alert')).toBeNull();
    expect(screen.getByText('History')).toBeTruthy();
    expect(screen.getByRole('switch').disabled).toBe(true);
  });
});

describe('alert inbox', () => {
  const events = { data: [{ id: 'e1', rule_id: 'r1', rule_name: 'Tokyo', item_uid: 'i1', item_title: 'Item one', matched_at: T, unread: true, delivered_channels: [] }], page: { next_cursor: null, total: 1 } };
  it('says read state is shared, and offers mute (a rule change) to analysts only', async () => {
    signedInAs('viewer');
    mockFetch({ '/api/alert-events': events });
    inRouter(<AlertInboxPage />);
    await screen.findByText('Item one');
    expect(screen.getByText(/marks it read for every member/)).toBeTruthy();
    expect(screen.getByText('Read')).toBeTruthy();
    expect(screen.queryByText('Rule…')).toBeNull();
    cleanup();
    signedInAs('analyst');
    mockFetch({ '/api/alert-events': events });
    inRouter(<AlertInboxPage />);
    await screen.findByText('Item one');
    expect(screen.getByText('Rule…')).toBeTruthy();
  });
});

const MONITORS = { data: [
  { id: 'm1', kind: 'email', label: 'CEO', rule_id: null, value_domain: 'x.test', created_by: 'u-me', created_at: T, last_checked_at: null, hash_prefix: 'abcdef1234', delivers: false },
  { id: 'm2', kind: 'domain', label: 'Corp', rule_id: null, value_domain: 'corp.test', created_by: 'u-mate', created_at: T, last_checked_at: null, hash_prefix: '0123456789', delivers: false },
], page: { next_cursor: null } };

describe('breach monitors', () => {
  it('names who added each monitor; an analyst may delete any of them', async () => {
    signedInAs('analyst');
    mockFetch({ '/api/members': MEMBERS, '/api/breach-monitors': MONITORS, '/api/breach-monitors/domains': { data: [] } });
    inRouter(<BreachMonitorsPage />);
    await screen.findByText('mate@x.test');
    expect(screen.getByText('you')).toBeTruthy();
    expect(screen.getAllByTitle('Delete')).toHaveLength(2);
    expect(screen.getByText('Add monitor')).toBeTruthy();
    expect(screen.getByText('Claim domain')).toBeTruthy();
  });

  it('a viewer may not add, delete or claim a domain', async () => {
    signedInAs('viewer');
    mockFetch({ '/api/members': MEMBERS, '/api/breach-monitors': MONITORS, '/api/breach-monitors/domains': { data: [] } });
    inRouter(<BreachMonitorsPage />);
    await screen.findByText('mate@x.test');
    expect(screen.queryAllByTitle('Delete')).toHaveLength(0);
    expect(screen.queryByText('Add monitor')).toBeNull();
    expect(screen.queryByText('Claim domain')).toBeNull();
  });
});
