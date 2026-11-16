// The three writer surfaces added 2026-11-12 for tables the server read and
// nothing wrote: Admin › Cache TTLs (collector_ttls), Admin › Station merges
// (llm_station_merges review) and Workspace › Report template
// (tenant_report_templates). Each must show the SERVER's answer — its bounds,
// its can_write, its refusal — rather than one the page assumed.
import React from 'react';
import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, cleanup, fireEvent, screen, waitFor } from '@testing-library/react';
import CollectorTtlsTab, { validateTtl, humanMs } from '../console/CollectorTtlsTab.jsx';
import StationMergesTab from '../console/StationMergesTab.jsx';
import ReportTemplateTab, { validateTemplateField } from '../console/ReportTemplateTab.jsx';

const realFetch = globalThis.fetch;
afterEach(() => { cleanup(); globalThis.fetch = realFetch; vi.restoreAllMocks(); });

const json = (status, body) => new Response(JSON.stringify(body), { status, headers: { 'content-type': 'application/json' } });
function mock(handler) {
  const calls = [];
  globalThis.fetch = vi.fn(async (input, init = {}) => {
    const url = typeof input === 'string' ? input : input.url;
    const method = init.method || 'GET';
    const body = init.body ? JSON.parse(init.body) : undefined;
    calls.push({ url, method, body });
    return handler(url, method, body);
  });
  return calls;
}

describe('cache TTL helpers', () => {
  it('validates an integer inside the server bounds', () => {
    expect(validateTtl('600000', 60000, 86400000)).toBeNull();
    expect(validateTtl('59999', 60000, 86400000)).toMatch(/between/);
    expect(validateTtl('600000.5', 60000, 86400000)).toMatch(/whole/);
    expect(validateTtl('', 60000, 86400000)).toMatch(/milliseconds/);
  });
  it('prints durations', () => {
    expect(humanMs(3600000)).toBe('1 h');
    expect(humanMs(900000)).toBe('15 min');
  });
});

describe('CollectorTtlsTab', () => {
  it('lists stored rows, flags stale ones, and resets with DELETE', async () => {
    const meta = { min_ttl_ms: 60000, max_ttl_ms: 86400000, default_ttl_ms: 900000 };
    const calls = mock((url, method) => {
      if (method === 'GET') return json(200, { data: [
        { key: 'transport-cluster-runner', ttl_ms: 3600000, source: 'user', registered: true, in_range: true, updated_by: 'op' },
        { key: 'gone-source', ttl_ms: 5000, source: 'registry', registered: false, in_range: false },
      ], meta });
      if (method === 'DELETE') return json(200, { data: { key: 'gone-source', ttl_ms: 900000, source: 'default' }, meta });
      return json(404, {});
    });
    render(<CollectorTtlsTab />);
    await screen.findByTestId('ttl-transport-cluster-runner');
    expect(screen.getByText('not registered')).toBeTruthy();
    expect(screen.getByText('outside clamp')).toBeTruthy();
    const resets = screen.getAllByText('Reset');
    fireEvent.click(resets[1]);
    await waitFor(() => expect(screen.queryByTestId('ttl-gone-source')).toBeNull());
    expect(calls.some((c) => c.method === 'DELETE' && c.url.endsWith('/api/admin/collector-ttls/gone-source'))).toBe(true);
  });
});

describe('StationMergesTab', () => {
  it('shows pending verdicts and posts an approval with the note', async () => {
    const row = { id: 'abc123', uid_a: 'st-seibu', uid_b: 'st-shinjuku', name_a: '西武新宿', name_b: '新宿',
      same: true, confidence: 0.83, reason: 'short walk', status: 'pending', model: 'judge.gguf', dist_m: 100, applied: false };
    const calls = mock((url, method) => {
      if (method === 'GET') return json(200, { data: [row], page: { limit: 100, offset: 0, count: 1, total: 1, has_more: false },
        meta: { counts: { pending: 1, approved: 0, rejected: 0 }, judge_enabled: true, judge_budget_per_run: 20 } });
      if (method === 'POST') return json(200, { data: { ...row, status: 'approved', applied: true } });
      return json(404, {});
    });
    render(<StationMergesTab />);
    await screen.findByTestId('merge-abc123');
    expect(screen.getByText('judge: same')).toBeTruthy();
    fireEvent.change(screen.getByLabelText('note for abc123'), { target: { value: 'checked' } });
    fireEvent.click(screen.getByText('Approve'));
    await waitFor(() => expect(screen.queryByTestId('merge-abc123')).toBeNull());
    const post = calls.find((c) => c.method === 'POST');
    expect(post.url).toMatch(/\/api\/admin\/station-merges\/abc123\/approve$/);
    expect(post.body).toEqual({ note: 'checked' });
  });
});

describe('ReportTemplateTab', () => {
  const limits = { classification: 127, header_note: 255, footer_md: 511, logo_url: 511 };
  it('validates against the server limits and the https rule', () => {
    const f = { key: 'logo_url', multiline: false };
    expect(validateTemplateField(f, 'https://e.org/l.png', 511)).toBeNull();
    expect(validateTemplateField(f, 'http://e.org/l.png', 511)).toMatch(/https/);
    expect(validateTemplateField({ key: 'classification', multiline: false }, 'a\nb', 127)).toMatch(/One line/);
    expect(validateTemplateField({ key: 'footer_md', multiline: true }, 'a\nb', 511)).toBeNull();
    expect(validateTemplateField({ key: 'classification', multiline: false }, 'x'.repeat(128), 127)).toMatch(/127/);
  });
  it('is read-only when the server says can_write is false', async () => {
    mock(() => json(200, { data: { classification: 'SECRET', header_note: '', footer_md: '', logo_url: '', present: true },
      meta: { can_write: false, role: 'viewer', limits } }));
    render(<ReportTemplateTab />);
    const input = await screen.findByLabelText('Classification banner');
    expect(input.value).toBe('SECRET');
    expect(input.disabled).toBe(true);
    expect(screen.queryByText('Save template')).toBeNull();
  });
  it('saves every field with PUT for an owner/admin', async () => {
    const calls = mock((url, method, body) => {
      if (method === 'PUT') return json(200, { data: { ...body, present: true }, meta: { can_write: true, role: 'owner', limits } });
      return json(200, { data: { classification: '', header_note: '', footer_md: '', logo_url: '', present: false },
        meta: { can_write: true, role: 'owner', limits } });
    });
    render(<ReportTemplateTab />);
    const input = await screen.findByLabelText('Classification banner');
    fireEvent.change(input, { target: { value: 'TLP:AMBER' } });
    fireEvent.click(screen.getByText('Save template'));
    await waitFor(() => expect(calls.some((c) => c.method === 'PUT')).toBe(true));
    const put = calls.find((c) => c.method === 'PUT');
    expect(put.url).toMatch(/\/api\/reports\/template$/);
    expect(put.body).toEqual({ classification: 'TLP:AMBER', header_note: '', footer_md: '', logo_url: '' });
  });
});
