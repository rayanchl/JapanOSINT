// The source pages (plan docs/source-pages-revamp/PLAN-2026-10-11.md):
//  - /console/sources searches and pages on the SERVER and keeps its filters
//    in the URL; the tiles come from /api/status?summary=1, not from a
//    client-side sum.
//  - /console/sources/:id renders every section from one detail body, and a
//    value the server did not measure says "not measured" (never 0 or a dash
//    that reads as a fact).
//  - the summary card shows the generated summary when there is one, and the
//    llm status code plus the facts when there is not — never prose that was
//    not generated.
//  - /console/collectors nests children under their parent and shows the
//    server's tallies.
import React from 'react';
import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, cleanup, screen, waitFor, fireEvent } from '@testing-library/react';
import { MemoryRouter, Routes, Route } from 'react-router-dom';
import SourcesListPage, { readFilters, writeFilters } from '../SourcesListPage.jsx';
import SourceDetailPage, { SummaryCard } from '../SourceDetailPage.jsx';
import CollectorsPage, { groupCollectors } from '../../collectors/CollectorsPage.jsx';
import CadenceCard, { describePolicy } from '../CadenceCard.jsx';
import { fmtInterval, fmtBytes, measured, kindLabel, splitTemplate, searchPath, NOT_MEASURED } from '../../../utils/sourceFormat.js';

vi.mock('../../panels/SourcesPanel.jsx', () => ({ default: () => null }));

const realFetch = globalThis.fetch;
afterEach(() => { cleanup(); globalThis.fetch = realFetch; vi.restoreAllMocks(); });

function mockFetch(route) {
  const calls = [];
  globalThis.fetch = vi.fn(async (input) => {
    const url = typeof input === 'string' ? input : input.url;
    calls.push(url);
    const r = route(url);
    const status = r && r.__status ? r.__status : 200;
    return new Response(JSON.stringify(r ?? {}), { status, headers: { 'content-type': 'application/json' } });
  });
  return calls;
}
const qp = (url, k) => new URL(url, 'http://x').searchParams.get(k);
const path = (url) => new URL(url, 'http://x').pathname;

describe('sourceFormat', () => {
  it('formats intervals, bytes and unmeasured values honestly', () => {
    expect(fmtInterval(0)).toBe('on demand');
    expect(fmtInterval(3600)).toBe('1 h');
    expect(fmtInterval(86400)).toBe('1 d');
    expect(fmtInterval(7344)).toBe('2 h 2 min');
    expect(fmtInterval(null)).toBe(NOT_MEASURED);
    expect(fmtBytes(512)).toBe('512 B');
    expect(fmtBytes(1536)).toBe('1.5 KB');
    expect(fmtBytes(null)).toBe(NOT_MEASURED);
    expect(measured(null)).toBe(NOT_MEASURED);
    expect(measured(0)).toBe('0');
    expect(kindLabel('unreachable')).toMatch(/never runs/);
    expect(splitTemplate('https://x/s?q={q}&d={qd}').filter((p) => p.token).map((p) => p.text)).toEqual(['{q}', '{qd}']);
  });
  it('round-trips the list filters through the URL and the API path', () => {
    const sp = writeFilters({ q: 'police', collector: 'jp_prefectural', has_items: true, desc: false, sort: 'items' });
    expect(sp.get('q')).toBe('police');
    expect(sp.get('has_items')).toBe('1');
    expect(sp.has('desc')).toBe(false);
    const f = readFilters(sp);
    expect(f).toEqual({ q: 'police', collector: 'jp_prefectural', has_items: true, sort: 'items' });
    expect(searchPath(f)).toBe('/api/sources/search?q=police&collector=jp_prefectural&has_items=1&sort=items');
    expect(searchPath({})).toBe('/api/sources/search');
  });
});

function searchBody(url, total = 120) {
  const limit = Number(qp(url, 'limit') || 50), offset = Number(qp(url, 'offset') || 0);
  const n = Math.max(0, Math.min(limit, total - offset));
  const data = Array.from({ length: n }, (_, i) => ({
    id: `src-${offset + i}`, name: `Source ${offset + i}`, collector: 'beat_a', category: 'government', engine: 'hpengine',
    kind: 'scheduled', interval: 3600, item_count: 10 + i, notices: 0, last_fetched: null, status: 'online', trust: { rated: false },
    sched: { effectiveInterval: null },
  }));
  return { data, page: { limit, offset, count: n, total, has_more: offset + n < total },
    meta: { registry_total: 18883, snapshot_at: '2026-10-12T00:00:00Z', q: qp(url, 'q'),
      facets: { collectors: { values: [{ key: 'beat_a', label: 'Beat A', count: total }], shown: 1, total: 1 }, categories: { values: [], shown: 0, total: 0 }, engines: { values: [], shown: 0, total: 0 }, kinds: { values: [], shown: 0, total: 0 }, statuses: { values: [], shown: 0, total: 0 } } } };
}

describe('SourcesListPage', () => {
  it('searches on the server, pages with the real total, and keeps q in the URL', async () => {
    const calls = mockFetch((url) => {
      if (path(url) === '/api/sources/search') return searchBody(url);
      if (path(url) === '/api/status') return { summary: { total: 18883, online: 5, degraded: 2, offline: 1, pending: 18875, requiresKey: 40, missingKey: 12 } };
      if (path(url) === '/api/collectors') return { data: [], meta: { total: 525 } };
      return {};
    });
    render(<MemoryRouter initialEntries={['/console/sources?q=police']}><Routes><Route path="/console/sources" element={<SourcesListPage />} /></Routes></MemoryRouter>);
    await waitFor(() => expect(screen.getByText('Source 0')).toBeTruthy());
    const search = calls.find((u) => path(u) === '/api/sources/search');
    expect(qp(search, 'q')).toBe('police');
    expect(qp(search, 'limit')).toBe('50');
    // the bound and the summary tile are the server's numbers
    expect(screen.getAllByText(/50 of 120 sources/).length).toBeGreaterThan(0);
    expect(screen.getByText('18,883')).toBeTruthy();
    expect(screen.getByText('18,875')).toBeTruthy();
    // load more asks for the next offset and appends
    fireEvent.click(screen.getByRole('button', { name: /load more|more/i }));
    await waitFor(() => expect(screen.getByText('Source 50')).toBeTruthy());
    const second = calls.filter((u) => path(u) === '/api/sources/search')[1];
    expect(qp(second, 'offset')).toBe('50');
    expect(qp(second, 'q')).toBe('police');
  });
  it('states an honest empty when the server finds nothing', async () => {
    mockFetch((url) => path(url) === '/api/sources/search' ? searchBody(url, 0) : path(url) === '/api/status' ? { summary: {} } : { data: [], meta: {} });
    render(<MemoryRouter initialEntries={['/console/sources?q=zz']}><Routes><Route path="/console/sources" element={<SourcesListPage />} /></Routes></MemoryRouter>);
    await waitFor(() => expect(screen.getByText(/No source matches these filters/)).toBeTruthy());
  });
});

const DETAIL = {
  registry: { id: 'T_JSON', name: 'json list', name_ja: null, collector: 'hp_test', collector_meta: { label: 'HP test' }, category: 'corporate', type: 'api', engine: 'hpengine', kind: 'pivot', interval: 0, url: 'https://x.test', host: 'x.test', description: 'A test row', license: null, layer: null, free_tier: true, is_entity_pivot: true },
  engine: { engine: 'hpengine', mode: 'json', want: 'any', kind: 'pivot', uses_entity: true, entity_tokens: ['{q}', '{qd}'], uses_date_tokens: false,
    request: { method: 'GET', url: 'https://x.test/s?q={q}&d={qd}', headers: [] }, parsing: { title_keys: 'legalName', id_keys: 'orgno+zip', record_type: 't-company' },
    detail_hop: { declared: false }, paging: { strategy: 'single_request' } },
  schedule: { tracked: false, declaredInterval: 0, effectiveInterval: null, consecutiveFailures: 0, consecutiveEmpties: 0, backedOff: false, backoffUntil: null, quarantined: false, schedule_mode: null, kind: 'pivot', stats_30d: { runs_30d: 0, ok_30d: 0, success_rate_30d: null, avg_duration_ms_30d: null, last_run_at: null }, next_due: null, overdue: null },
  status: { status: null, last_check: null, last_success: null, response_time_ms: null, records_last_run: null, error_message: null, trust: { rated: false, reliability: null, grade: null } },
  data: { item_count: 3, geocoded: 1, notices: 0, last_fetched: '2026-10-03T00:00:00Z', awaiting_geo: null, first_fetched: '2026-10-01T00:00:00Z', first_published: null, last_published: null, db_text_bytes: 2048, bbox: null,
    record_types: { values: [{ record_type: 't-company', count: 2 }, { record_type: 't-person', count: 1 }], shown: 2, total: 2 },
    languages: [{ language: 'en', count: 3 }], field_inventory: { sample_size: 3, fields: { values: [{ key: 'name', count: 3 }, { key: 'orgno', count: 2 }], shown: 2, total: 2 } } },
  notices: { counts: [], latest: [] },
  runs: { recent: [], shown: 0, total: 0 },
  credentials: { requiresKey: false, configured: true, envVars: [], missingVars: [] },
  pacing: { host: 'x.test', min_gap_ms: 150 },
  provenance: { files_scanned: 3, rows_indexed: 10, rows: [{ file: 'docs/x.tsv', line: 4, text: 'T_JSON\tPASS' }], shown: 1, total: 1 },
  meta: { fetched_at: '2026-10-12T00:00:00Z' },
};

describe('SourceDetailPage', () => {
  it('renders every section from the detail body and says not measured for nulls', async () => {
    mockFetch((url) => {
      if (/\/detail$/.test(path(url))) return DETAIL;
      if (/\/summary$/.test(path(url))) return { summary: 'A test row that yields companies.', result_type: 'company records', caveats: ['never run'], model: 'tiny.gguf', facts_used: 20, generated_at: '2026-10-12T00:00:00Z', cached: false };
      if (path(url) === '/api/intel/items') return { data: [{ uid: 'T_JSON|1', title: 'Acme AS', record_type: 't-company' }], page: {} };
      return {};
    });
    render(<MemoryRouter initialEntries={['/console/sources/T_JSON']}><Routes><Route path="/console/sources/:id" element={<SourceDetailPage />} /></Routes></MemoryRouter>);
    await waitFor(() => expect(screen.getByText('A test row that yields companies.')).toBeTruthy());
    for (const label of ['What it is', 'Engine and mechanism', 'Parsing', 'Frequency', 'Address and location', 'Obtained data and its form', 'Testing', 'Status', 'Size']) {
      expect(screen.getByText(label)).toBeTruthy();
    }
    expect(screen.getByText('yields: company records')).toBeTruthy();
    expect(screen.getAllByText('{q}').length).toBeGreaterThan(0);          // tokens highlighted in the template
    expect(screen.getByText(/orgno\+zip/)).toBeTruthy();                    // id_keys shown verbatim
    expect(screen.getByText(/≥ 150 ms between requests/)).toBeTruthy();    // pacing is a fact
    expect(screen.getAllByText('t-company').length).toBeGreaterThan(0);     // record types histogram (+ record_type stamped)
    expect(screen.getByText('Acme AS')).toBeTruthy();                       // sample record
    expect(screen.getByText(/docs\/x.tsv:4/)).toBeTruthy();                 // provenance row
    expect(screen.getAllByText(/2.0 KB/).length).toBeGreaterThan(0);         // db text size (data + size sections)
    expect(screen.getAllByText(new RegExp(NOT_MEASURED)).length).toBeGreaterThan(0);   // nulls say so
    expect(screen.getByText(/this source has never run/)).toBeTruthy();
    expect(screen.getAllByText(/never run|no runs in 30 days/).length).toBeGreaterThan(0);   // transfer rows say so for a source that never ran
  });
  it('shows the llm status and the facts, never prose, when the model is unreachable', async () => {
    mockFetch((url) => {
      if (/\/detail$/.test(path(url))) return DETAIL;
      if (/\/summary$/.test(path(url))) return { error: 'llm_unreachable', http_status: 0, model: 'unknown', llm_base_url: 'http://127.0.0.1:9', facts_used: 20, facts: { registry: { id: 'T_JSON' } } };
      if (path(url) === '/api/intel/items') return { data: [], page: {} };
      return {};
    });
    render(<MemoryRouter initialEntries={['/console/sources/T_JSON']}><Routes><Route path="/console/sources/:id" element={<SourceDetailPage />} /></Routes></MemoryRouter>);
    await waitFor(() => expect(screen.getByText(/No summary: model unreachable/)).toBeTruthy());
    expect(screen.getByText(/llm_unreachable/)).toBeTruthy();
    expect(screen.getByText(/20 facts the model would have been given/)).toBeTruthy();
  });
  it('404 is said, not rendered as an empty source', async () => {
    mockFetch((url) => /\/detail$/.test(path(url)) ? { __status: 404, error: 'not_found' } : { data: [] });
    render(<MemoryRouter initialEntries={['/console/sources/NOPE']}><Routes><Route path="/console/sources/:id" element={<SourceDetailPage />} /></Routes></MemoryRouter>);
    await waitFor(() => expect(screen.getByText(/No registered source has this id/)).toBeTruthy());
  });
});

describe('SummaryCard', () => {
  it('renders nothing fabricated for an empty answer', () => {
    const { container } = render(<SummaryCard summary={null} loading={false} error={null} onRegenerate={() => {}} />);
    expect(container.textContent).toBe('');
  });
});

describe('CollectorsPage', () => {
  it('nests collectors under their server-side groups and shows server tallies', async () => {
    const rows = [
      { key: 'jp_police', label: 'Prefectural police', parent: 'grp_safety', group: 'grp_safety', sources: 47, scheduled: 47, pivots: 0, internal: 0, unreachable: 0, items: 1200, notices: 0, last_fetched: null, status: { online: 40, degraded: 5, offline: 2, pending: 0 }, categories: { values: [{ key: 'crime', count: 47 }] }, engines: { values: [{ key: 'vrss', count: 47 }] } },
      { key: 'solo_beat', label: null, parent: 'grp_osint', group: 'grp_osint', sources: 1, scheduled: 0, pivots: 1, internal: 0, unreachable: 0, items: 0, notices: 0, last_fetched: null, status: { online: 0, degraded: 0, offline: 0, pending: 1 }, categories: { values: [] }, engines: { values: [] } },
    ];
    const groups = [
      { key: 'grp_safety', label: 'Crime, policing, defense & conflict', is_group: true, collectors: ['jp_police'], collector_count: 1, sources: 47, scheduled: 47, pivots: 0, internal: 0, unreachable: 0, items: 1200, notices: 0, last_fetched: null, status: { online: 40, degraded: 5, offline: 2, pending: 0 }, categories: { values: [] }, engines: { values: [] } },
      { key: 'grp_osint', label: 'Investigation pivots & reference', is_group: true, collectors: ['solo_beat'], collector_count: 1, sources: 1, scheduled: 0, pivots: 1, internal: 0, unreachable: 0, items: 0, notices: 0, last_fetched: null, status: { online: 0, degraded: 0, offline: 0, pending: 1 }, categories: { values: [] }, engines: { values: [] } },
    ];
    const g = groupCollectors(groups, rows);
    expect(g.map((x) => x.key)).toEqual(['grp_safety', 'grp_osint']);
    expect(g[0].members.map((m) => m.key)).toEqual(['jp_police']);
    expect(groupCollectors(groups, rows, (r) => r.key === 'solo_beat').map((x) => x.key)).toEqual(['grp_osint']);
    mockFetch(() => ({ data: rows, groups, meta: { total: 2, sources: 48, labelled: 2, groups: 2, unassigned_collectors: 0, snapshot_at: '2026-10-12T00:00:00Z' } }));
    render(<MemoryRouter><CollectorsPage /></MemoryRouter>);
    await waitFor(() => expect(screen.getByText('Prefectural police')).toBeTruthy());
    expect(screen.getByText('Crime, policing, defense & conflict')).toBeTruthy();
    // a one-source collector is folded, with its count stated, and opens in place — never dropped
    expect(screen.queryByText('solo_beat')).toBeNull();
    const fold = screen.getByTestId('singletons-grp_osint');
    expect(fold.textContent).toMatch(/1 single-source collector/);
    fireEvent.click(fold);
    expect(screen.getAllByText('solo_beat').length).toBeGreaterThan(0);
    expect(screen.getByText('pending')).toBeTruthy();
    expect(screen.getByText(/48 sources · 2 collectors · 2 groups/)).toBeTruthy();
    expect(screen.getAllByText('1,200 items').length).toBeGreaterThan(0);
  });
});

describe('CadenceCard', () => {
  it('states the policy and what it costs, and only an operator can change it', async () => {
    expect(describePolicy({ global_interval_sec: 21600 })).toMatch(/every 6h|every 6 h|21600/);
    expect(describePolicy({ global_interval_sec: 0, min_interval_sec: 900, max_interval_sec: 0 })).toMatch(/never more often/);
    expect(describePolicy({ global_interval_sec: 0, min_interval_sec: 0, max_interval_sec: 0 })).toMatch(/own declared interval/);
    mockFetch(() => ({ policy: { global_interval_sec: 0, min_interval_sec: 900, max_interval_sec: 86400, mode: 'clamped' }, source: 'db', updated_at: '2026-10-20T10:00:00Z', updated_by: 'ops@example.test',
      estimate: { scheduled_rows: 19954, runs_per_day_declared: 240318, runs_per_day_effective: 120000, fastest_effective_sec: 900, slowest_effective_sec: 86400, basis: 'measured' } }));
    render(<MemoryRouter><CadenceCard canEdit={false} /></MemoryRouter>);
    await waitFor(() => expect(screen.getByText(/never more often than every/)).toBeTruthy());
    expect(screen.getByText('clamped')).toBeTruthy();
    expect(screen.getByText(/120,000 runs\/day under this policy \(240,318 as declared\)/)).toBeTruthy();
    expect(screen.queryByText('Change')).toBeNull();
    cleanup();
    render(<MemoryRouter><CadenceCard canEdit /></MemoryRouter>);
    await waitFor(() => expect(screen.getByText('Change')).toBeTruthy());
    fireEvent.click(screen.getByText('Change'));
    expect(screen.getByText('Save')).toBeTruthy();
  });
});
