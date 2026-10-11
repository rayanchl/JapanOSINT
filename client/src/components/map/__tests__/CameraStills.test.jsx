// Camera stills timeline: the list route is fetched only when the panel is
// opened, a view target's '#' reaches the server percent-encoded, the raw
// frame comes through fetch (bearer) and is sniffed, and an operator-gate 403
// is shown as a reason rather than a broken image.
import React from 'react';
import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, screen, fireEvent, waitFor, cleanup } from '@testing-library/react';
import CameraStills from '../CameraStills.jsx';
import {
  stillsPath, sniffImageType, rawErrorMessage, cameraIdForProperties,
} from '../../../utils/cameraStills.js';

afterEach(() => { cleanup(); vi.restoreAllMocks(); });

const LIST = {
  data: [
    { id: 's2', captured_at: '2026-10-10 10:15:00', view: 'views/C0150201', scene_changed: true, blob_present: true },
    { id: 's1', captured_at: '2026-10-10 10:00:00', view: 'views/C0150202', scene_changed: null, blob_present: true },
  ],
  page: { limit: 24, count: 2, cursor: null },
  meta: {
    total: 2, changed: 1, unknown: 1, capture_stills: true,
    oldest: '2026-10-10 10:00:00', newest: '2026-10-10 10:15:00',
    targets_total: 2,
    targets: [
      { camera_id: 'x#view=views/C0150201', view: 'views/C0150201', last_error: null },
      { camera_id: 'x#view=views/C0150202', view: 'views/C0150202', last_error: 'http_404' },
    ],
  },
};

function jsonRes(status, body) {
  return {
    ok: status >= 200 && status < 300, status,
    json: async () => body,
    headers: new Headers({ 'content-type': 'application/json' }),
  };
}

describe('cameraStills utils', () => {
  it('percent-encodes a view target id', () => {
    const p = stillsPath('digitraffic-weathercam-stations|C01502#view=views/C0150201', { limit: 10 });
    expect(p).toContain('%23view%3Dviews%2FC0150201');
    expect(p).not.toContain('#');
  });
  it('prefers a full intel uid over a bare camera_uid', () => {
    expect(cameraIdForProperties({ uid: '511-alberta-cameras|AB--1', camera_uid: 'z' })).toBe('511-alberta-cameras|AB--1');
    expect(cameraIdForProperties({ camera_uid: 'windy_1' })).toBe('windy_1');
    expect(cameraIdForProperties({})).toBeNull();
  });
  it('sniffs only real image signatures', () => {
    expect(sniffImageType(new Uint8Array([0xff, 0xd8, 0xff, 0xe0]))).toBe('image/jpeg');
    expect(sniffImageType(new Uint8Array([0x89, 0x50, 0x4e, 0x47]))).toBe('image/png');
    expect(sniffImageType(new TextEncoder().encode('<html><script>'))).toBeNull();
  });
  it('explains the gated statuses', () => {
    expect(rawErrorMessage(403, 'Platform operator role required')).toMatch(/platform operators/);
    expect(rawErrorMessage(410, 'blob_not_preserved')).toMatch(/capture_evidence/);
  });
});

describe('<CameraStills>', () => {
  it('fetches nothing until opened, then lists every view', async () => {
    const f = vi.spyOn(globalThis, 'fetch').mockResolvedValue(jsonRes(200, LIST));
    render(<CameraStills cameraId="digitraffic-weathercam-stations|C01502" />);
    expect(f).not.toHaveBeenCalled();
    fireEvent.click(screen.getByRole('button', { name: /stills timeline/i }));
    await screen.findByText(/2 frames/);
    expect(String(f.mock.calls[0][0])).toContain('/api/cameras/digitraffic-weathercam-stations%7CC01502/stills');
    expect(screen.getByText(/views\/C0150202 \(http_404\)/)).toBeTruthy();
    expect(screen.getByText(/unknown/)).toBeTruthy();   // null scene_changed is not "same"
  });

  it('shows the operator gate as a reason', async () => {
    const f = vi.spyOn(globalThis, 'fetch');
    f.mockResolvedValueOnce(jsonRes(200, LIST));
    f.mockResolvedValueOnce(jsonRes(403, { error: 'Platform operator role required' }));
    render(<CameraStills cameraId="cam1" />);
    fireEvent.click(screen.getByRole('button', { name: /stills timeline/i }));
    fireEvent.click(await screen.findByText('2026-10-10 10:15:00'));
    const alert = await screen.findByRole('alert');
    expect(alert.textContent).toMatch(/restricted to platform operators/);
    await waitFor(() => expect(String(f.mock.calls[1][0])).toContain('/api/camera-stills/s2/raw'));
  });
});
