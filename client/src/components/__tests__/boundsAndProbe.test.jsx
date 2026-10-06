// Regression (findings 9 and 11): bounded lists state only measured totals;
// partial probe/consent responses are merged into the status row.
import React from 'react';
import { describe, it, expect, afterEach } from 'vitest';
import { render, cleanup } from '@testing-library/react';
import { BoundNote } from '../ui/kit.jsx';
import { mergeStatusRow, probeSummary } from '../console/ApiKeysPage.jsx';

afterEach(() => cleanup());
const text = (el) => render(el).container.textContent;

describe('BoundNote', () => {
  it('never invents a total: a next cursor without a total says "more on the server"', () => {
    expect(text(<BoundNote shown={100} more noun="areas" />)).toBe('100 areas loaded · more on the server (no total reported)');
  });
  it('shows a measured total', () => {
    expect(text(<BoundNote shown={50} total={700} noun="items" />)).toBe('showing 50 of 700 items');
    expect(text(<BoundNote shown={7} total={7} noun="items" />)).toBe('7 items');
  });
  it('shows a measured lower bound', () => {
    expect(text(<BoundNote shown={50} atLeast={1000} noun="items" />)).toBe('showing 50 of at least 1000 items');
  });
  it('renders nothing when nothing was measured', () => {
    expect(text(<BoundNote shown={5} noun="items" />)).toBe('');
  });
});

describe('probe / consent responses', () => {
  const rows = [{ id: 'a', name: 'A', requiresKey: true, envVars: [{ name: 'K' }], probeConsent: false }, { id: 'b', name: 'B' }];

  it('a partial consent response keeps requiresKey/envVars on the row', () => {
    const out = mergeStatusRow(rows, { ok: true, id: 'a', probeConsent: true });
    expect(out[0]).toEqual({ id: 'a', name: 'A', requiresKey: true, envVars: [{ name: 'K' }], probeConsent: true });
    expect(out[1]).toBe(rows[1]);
  });

  it('toasts from reachable / error, not a `status` the server never sends', () => {
    expect(probeSummary({ id: 'a', reachable: true, probeResponseStatus: 200 })).toBe('Probe: reachable (HTTP 200)');
    expect(probeSummary({ id: 'a', reachable: false, error: 'transport_error', probeResponseStatus: null })).toBe('Probe: unreachable — transport_error');
  });
});
