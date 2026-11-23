// WEB-05: the key sheet's "Used by" is the server's own answer to
// GET /api/sources/search?key=<NAME>, with the matched total stated, not a
// scan of the sources that happen to be gated right now.
import React from 'react';
import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, cleanup, screen, waitFor } from '@testing-library/react';
import { MemoryRouter } from 'react-router-dom';
import ApiKeysPage from '../console/ApiKeysPage.jsx';

vi.mock('../../auth/AuthContext.jsx', () => ({ useAuth: () => ({ isPlatformAdmin: true, role: 'owner' }) }));

const realFetch = globalThis.fetch;
afterEach(() => { cleanup(); globalThis.fetch = realFetch; vi.restoreAllMocks(); });

describe('API keys: used by', () => {
  it('asks the server for the sources that need the key and says how many exist', async () => {
    const calls = [];
    globalThis.fetch = vi.fn(async (input) => {
      const url = typeof input === 'string' ? input : input.url;
      calls.push(url);
      const u = new URL(url, 'http://x');
      let body = {};
      if (u.pathname === '/api/keys') body = [{ name: 'SHODAN_API_KEY', role: 'required', set: false, hasOverlay: false }];
      else if (u.pathname === '/api/sources/search' && u.searchParams.get('key') === 'SHODAN_API_KEY') {
        body = {
          data: [{ id: 'shodan-iot', name: 'Shodan IoT', category: 'cyber', status: 'pending', health: { state: 'gated' } },
                 { id: 'shodan-japan', name: 'Shodan Japan', category: 'cyber', status: 'pending', health: { state: 'ok' } }],
          page: { limit: 25, offset: 0, count: 2, total: 30, has_more: true },
          meta: { credential: { name: 'SHODAN_API_KEY', sources: 30, shown: 2 } },
        };
      } else if (u.pathname === '/api/sources/search') body = { data: [], page: { limit: 200, offset: 0, count: 0, total: 0, has_more: false } };
      return new Response(JSON.stringify(body), { status: 200, headers: { 'content-type': 'application/json' } });
    });
    render(<MemoryRouter initialEntries={['/console/keys?scope=platform&key=SHODAN_API_KEY']}><ApiKeysPage /></MemoryRouter>);
    await waitFor(() => expect(screen.getByText('Used by · 30 sources')).toBeTruthy());
    expect(calls.some((c) => c.includes('/api/sources/search?key=SHODAN_API_KEY'))).toBe(true);
    expect(screen.getByTestId('key-used-by').textContent).toMatch(/30 sources need this key.*showing 2/);
    expect(screen.getByText('Shodan IoT')).toBeTruthy();
    expect(screen.getByText('Open in Sources').getAttribute('href')).toBe('/console/sources?key=SHODAN_API_KEY');
  });
});
