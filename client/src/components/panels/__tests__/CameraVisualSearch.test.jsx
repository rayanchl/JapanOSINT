// Camera visual search: the sentence reaches /api/cameras/visual-search, the
// server's 503 reason is shown verbatim, a thumbnail the operator gate refuses
// shows its reason instead of a broken image, a reference-frame hit says it has
// no still, and "find similar cameras" POSTs the still id to .../by-image.
import React from 'react';
import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor, cleanup } from '@testing-library/react';
import CameraVisualSearch from '../CameraVisualSearch.jsx';
import { visualSearchPath, visualSearchErrorMessage, coverageNote } from '../../../utils/visualSearch.js';

afterEach(() => { cleanup(); vi.restoreAllMocks(); });

function jsonRes(status, body) {
  return {
    ok: status >= 200 && status < 300, status,
    json: async () => body,
    headers: new Headers({ 'content-type': 'application/json' }),
  };
}

const HITS = {
  data: [
    {
      camera_id: 'camera-discovery|river1', item_uid: 'camera-discovery|river1', view: null,
      name: 'Tama River', lat: 35.6, lon: 139.6, distance: 0.4, cosine: 0.91,
      still: { id: 'st3', captured_at: '2026-10-12 01:30:00', sha256: 'a'.repeat(64) },
      frame: { kind: 'still', ref_id: 'st3', captured_at: '2026-10-12 01:30:00', sha256: 'a'.repeat(64), evidence_id: null },
      frames_matched: 3,
    },
    {
      camera_id: 'camera-discovery|road1', item_uid: 'camera-discovery|road1', view: null,
      name: 'Route 1', lat: 35.62, lon: 139.62, distance: 0.6, cosine: 0.8,
      still: null,
      frame: { kind: 'refframe', ref_id: 'camera-discovery|road1', captured_at: '2026-10-12 04:00:00', sha256: 'b'.repeat(64), evidence_id: 'ev1' },
      frames_matched: 1,
    },
  ],
  meta: { cameras: { note: '2 of 5 cameras indexed' }, candidates: { examined: 40, limit: 240, exhausted: true } },
};

describe('visualSearch utils', () => {
  it('builds the query string', () => {
    const p = visualSearchPath('snow on the road', { k: 5, bbox: [139, 35, 140, 36] });
    expect(p).toContain('/api/cameras/visual-search?');
    expect(p).toContain('q=snow+on+the+road');
    expect(p).toContain('bbox=139%2C35%2C140%2C36');
  });
  it('keeps the server reason for a 503', () => {
    expect(visualSearchErrorMessage(503, { error: 'visual_search_unavailable', reason: 'JO_VISION_EMBED_URL is unset' }))
      .toMatch(/unavailable: JO_VISION_EMBED_URL is unset/);
    expect(visualSearchErrorMessage(401, {})).toMatch(/Sign in/);
  });
  it('says when the candidate pool ran out first', () => {
    expect(coverageNote({ cameras: { note: '1 of 2 cameras indexed' }, candidates: { examined: 50, exhausted: false } }))
      .toMatch(/1 of 2 cameras indexed · best of the 50/);
  });
});

describe('<CameraVisualSearch>', () => {
  it('searches, shows gated thumbnails as reasons, and pivots to similar cameras', async () => {
    const f = vi.spyOn(globalThis, 'fetch').mockImplementation(async (url, init) => {
      const u = String(url);
      if (u.startsWith('/api/cameras/visual-search?')) return jsonRes(200, HITS);
      if (u.includes('/api/camera-stills/')) return jsonRes(403, { error: 'Platform operator role required' });
      if (u === '/api/cameras/visual-search/by-image') {
        expect(init.method).toBe('POST');
        expect(JSON.parse(init.body).still_id).toBe('st3');
        return jsonRes(200, { data: [{ ...HITS.data[1], camera_id: 'camera-discovery|road9', name: 'Route 9' }], meta: {} });
      }
      return jsonRes(404, {});
    });
    render(<CameraVisualSearch />);
    fireEvent.change(screen.getByLabelText(/search cameras by what they show/i), { target: { value: 'flooded road' } });
    fireEvent.click(screen.getByRole('button', { name: 'Search' }));
    await screen.findByText('Tama River');
    expect(String(f.mock.calls[0][0])).toContain('q=flooded+road');
    expect(screen.getByText(/2 of 5 cameras indexed/)).toBeTruthy();
    expect(screen.getByText(/reference frame — no still/)).toBeTruthy();
    await waitFor(() => expect(screen.getByText(/restricted to platform operators/)).toBeTruthy());
    // only the still hit can be pivoted on
    const pivots = screen.getAllByRole('button', { name: /find similar cameras/i });
    expect(pivots).toHaveLength(1);
    fireEvent.click(pivots[0]);
    await screen.findByText('Route 9');
    expect(screen.getByText(/Cameras that look like Tama River/)).toBeTruthy();
  });

  it('shows why the server cannot answer', async () => {
    vi.spyOn(globalThis, 'fetch').mockResolvedValue(jsonRes(503, {
      error: 'visual_search_unavailable',
      reason: 'the camera image index has not been built yet',
    }));
    render(<CameraVisualSearch />);
    fireEvent.change(screen.getByLabelText(/search cameras by what they show/i), { target: { value: 'snow' } });
    fireEvent.click(screen.getByRole('button', { name: 'Search' }));
    const alert = await screen.findByRole('alert');
    expect(alert.textContent).toMatch(/has not been built yet/);
  });
});
