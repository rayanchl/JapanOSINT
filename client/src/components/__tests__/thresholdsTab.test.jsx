// The operator's thresholds panel (Admin › Thresholds): it lists what
// GET /api/admin/thresholds reports — value, where it came from, default and
// range — and saves / resets one key with PUT /api/admin/thresholds/:key.
// The server is the authority on range and resolution; the panel must show
// its answer, not a value it assumed.
import React from 'react';
import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, cleanup, fireEvent, screen, waitFor, within } from '@testing-library/react';
import ThresholdsTab, { validateThreshold, stepFor } from '../console/ThresholdsTab.jsx';

const realFetch = globalThis.fetch;
afterEach(() => { cleanup(); globalThis.fetch = realFetch; vi.restoreAllMocks(); });

const row = (over = {}) => ({
  key: 'story_tau', value: 0.85, source: 'default', default: 0.85, min: 0.01, max: 1,
  env: 'JO_STORY_TAU', env_value: null, metric: 'cosine',
  description: 'Story joining.', updated_by: null, updated_at: null, ...over,
});

function mockServer(initial) {
  const state = initial.map((r) => ({ ...r }));
  const calls = [];
  globalThis.fetch = vi.fn(async (input, init = {}) => {
    const url = typeof input === 'string' ? input : input.url;
    const method = init.method || 'GET';
    calls.push({ url, method, body: init.body ? JSON.parse(init.body) : undefined });
    const json = (status, body) => new Response(JSON.stringify(body), { status, headers: { 'content-type': 'application/json' } });
    if (method === 'GET' && url.endsWith('/api/admin/thresholds')) {
      return json(200, { data: state, meta: { count: state.length, order: 'db > env > default' } });
    }
    const m = url.match(/\/api\/admin\/thresholds\/([^/?]+)$/);
    if (method === 'PUT' && m) {
      const r = state.find((x) => x.key === decodeURIComponent(m[1]));
      if (!r) return json(404, { error: 'unknown_threshold' });
      const b = JSON.parse(init.body);
      if (b.reset) Object.assign(r, { value: r.default, source: 'default', updated_by: null });
      else if (b.value < r.min || b.value > r.max) return json(400, { error: 'out_of_range', detail: `value must be a number in [${r.min}, ${r.max}]` });
      else Object.assign(r, { value: b.value, source: 'db', updated_by: 'op' });
      return json(200, { data: { ...r } });
    }
    return json(404, {});
  });
  return calls;
}

describe('threshold helpers', () => {
  it('validates against the server-declared range', () => {
    expect(validateThreshold('0.5', 0.01, 1)).toBeNull();
    expect(validateThreshold('1.2', 0.01, 1)).toMatch(/between/);
    expect(validateThreshold('', 0.01, 1)).toMatch(/number/);
    expect(validateThreshold('abc', 0.01, 1)).toMatch(/number/);
  });
  it('uses a finer slider step for a narrow range', () => {
    expect(stepFor(0.01, 1)).toBe(0.01);
    expect(stepFor(0.9, 1)).toBe(0.001);
  });
});

describe('ThresholdsTab', () => {
  it('lists each threshold with its source, default and env', async () => {
    mockServer([row(), row({ key: 'entity_vec_cos_floor', value: 0.7, source: 'env', env: 'JO_ENTITY_VEC_COS_FLOOR', env_value: 0.7, default: 0.8 })]);
    render(<ThresholdsTab />);
    const ent = await screen.findByTestId('threshold-entity_vec_cos_floor');
    expect(within(ent).getByText('environment')).toBeTruthy();
    expect(within(ent).getByText(/default 0\.8 .*JO_ENTITY_VEC_COS_FLOOR=0\.7/)).toBeTruthy();
    expect(within(screen.getByTestId('threshold-story_tau')).getByText('default', { selector: 'span' })).toBeTruthy();
  });

  it('saves a value with PUT {value} and shows the server-resolved source', async () => {
    const calls = mockServer([row()]);
    render(<ThresholdsTab />);
    const li = await screen.findByTestId('threshold-story_tau');
    const save = within(li).getByRole('button', { name: /save/i });
    expect(save.disabled).toBe(true);                     // nothing changed yet
    fireEvent.change(within(li).getByLabelText('story_tau value'), { target: { value: '0.9' } });
    expect(save.disabled).toBe(false);
    fireEvent.click(save);
    await waitFor(() => expect(within(li).getByText('runtime override')).toBeTruthy());
    const put = calls.find((c) => c.method === 'PUT');
    expect(put.url).toMatch(/\/api\/admin\/thresholds\/story_tau$/);
    expect(put.body).toEqual({ value: 0.9 });
  });

  it('refuses to send an out-of-range value, and resets with {reset:true}', async () => {
    const calls = mockServer([row({ value: 0.9, source: 'db', updated_by: 'op' })]);
    render(<ThresholdsTab />);
    const li = await screen.findByTestId('threshold-story_tau');
    fireEvent.change(within(li).getByLabelText('story_tau value'), { target: { value: '3' } });
    expect(within(li).getByText(/between 0\.01 and 1/)).toBeTruthy();
    expect(within(li).getByRole('button', { name: /save/i }).disabled).toBe(true);
    fireEvent.click(within(li).getByRole('button', { name: /reset/i }));
    await waitFor(() => expect(within(li).getByText('default', { selector: 'span' })).toBeTruthy());
    expect(calls.find((c) => c.method === 'PUT').body).toEqual({ reset: true });
  });

  it('shows the operator refusal on a 403', async () => {
    globalThis.fetch = vi.fn(async () => new Response(JSON.stringify({ error: 'Platform operator role required' }), { status: 403, headers: { 'content-type': 'application/json' } }));
    render(<ThresholdsTab />);
    expect(await screen.findByText(/platform-operator allowlist/)).toBeTruthy();
  });
});
