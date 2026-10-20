// The web client called three things the native server does not serve: a /ws
// WebSocket, /api/data/cameras/snapshot?url= and /api/data/flight-adsb/enrich.
// These pin the replacements: the socket is not dialled unless a build turns
// it on, camera images go through fetch() (so they carry the session) to the
// real proxy route, and the dead routes are not referenced at all.
import { describe, it, expect, vi, afterEach } from 'vitest';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { render } from '@testing-library/react';
import React from 'react';
import fetchImageObjectUrl from '../authedImage.js';
import useWebSocket from '../../hooks/useWebSocket.js';
import { WS_ENABLED } from '../wsUrl.js';

const here = dirname(fileURLToPath(import.meta.url));
const src = (rel) => readFileSync(join(here, '..', '..', rel), 'utf8');

afterEach(() => { vi.unstubAllGlobals(); vi.restoreAllMocks(); });

// Minimal Response stand-ins: jsdom's Blob and undici's Response do not
// interoperate, and only these four members are read.
const resp = (status, ct, body) => ({
  ok: status >= 200 && status < 300,
  status,
  headers: { get: (k) => (k.toLowerCase() === 'content-type' ? ct : null) },
  json: async () => JSON.parse(body),
  blob: async () => ({ fake: 'blob' }),
});

describe('fetchImageObjectUrl', () => {
  it('goes through fetch() and returns a blob URL for an image', async () => {
    const fetchMock = vi.fn(async () => resp(200, 'image/jpeg', ''));
    vi.stubGlobal('fetch', fetchMock);
    const had = URL.createObjectURL;
    URL.createObjectURL = vi.fn(() => 'blob:fake');
    try {
      const u = await fetchImageObjectUrl('/api/data/cameras/proxy?camera_uid=abc');
      expect(u).toBe('blob:fake');
      expect(URL.createObjectURL).toHaveBeenCalledWith({ fake: 'blob' });
    } finally {
      URL.createObjectURL = had;
    }
    expect(fetchMock).toHaveBeenCalledWith('/api/data/cameras/proxy?camera_uid=abc', undefined);
  });

  it("reports the server's own error instead of a broken image", async () => {
    vi.stubGlobal('fetch', vi.fn(async () => resp(502, 'application/json',
      JSON.stringify({ error: 'upstream is not an image' }))));
    await expect(fetchImageObjectUrl('/api/x')).rejects.toThrow('HTTP 502: upstream is not an image');
  });

  it('refuses a 2xx that is not an image', async () => {
    vi.stubGlobal('fetch', vi.fn(async () => resp(200, 'text/html', '<html></html>')));
    await expect(fetchImageObjectUrl('/api/x')).rejects.toThrow('not an image (text/html)');
  });
});

describe('useWebSocket without a /ws server', () => {
  it('is off by default and never dials', () => {
    expect(WS_ENABLED).toBe(false);
    const ctor = vi.fn();
    vi.stubGlobal('WebSocket', ctor);
    let out;
    function Probe() { out = useWebSocket('/ws', { onMessage: () => {} }); return null; }
    render(React.createElement(Probe));
    expect(ctor).not.toHaveBeenCalled();
    expect(out).toEqual({ connected: false, available: false });
  });
});

describe('no calls to routes the server does not have', () => {
  it('MapPopup uses the camera proxy and no enrich route', () => {
    const s = src('components/map/MapPopup.jsx');
    // Code, not the comments that record why the calls were removed.
    const code = s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
    expect(code).not.toMatch(/cameras\/snapshot/);
    expect(code).not.toMatch(/flight-adsb\/enrich/);
    expect(s).toMatch(/\/api\/data\/cameras\/proxy\?camera_uid=/);
  });
});
