// Regression (finding 2): deck.gl's MapboxOverlay reads map.transform, which
// maplibre v6 moved to map._camera.transform.
import { describe, it, expect, vi } from 'vitest';

vi.mock('maplibre-gl', () => ({ setWorkerUrl: vi.fn() }));
vi.mock('maplibre-gl/dist/maplibre-gl-worker.mjs?worker&url', () => ({ default: '/assets/maplibre-gl-worker-test.js' }));

describe('utils/maplibre', () => {
  it('sets the worker URL Vite emitted', async () => {
    const ml = await import('maplibre-gl');
    await import('../maplibre.js');
    expect(ml.setWorkerUrl).toHaveBeenCalledWith('/assets/maplibre-gl-worker-test.js');
  });

  it('aliases map.transform to the live camera transform', async () => {
    const { installDeckTransformShim } = await import('../maplibre.js');
    const t1 = { height: 800 };
    const map = { _camera: { transform: t1 } };
    expect(installDeckTransformShim(map)).toBe(true);
    expect(map.transform).toBe(t1);
    const t2 = { height: 600 };
    map._camera.transform = t2;
    expect(map.transform.height).toBe(600);
  });

  it('never shadows a real transform', async () => {
    const { installDeckTransformShim } = await import('../maplibre.js');
    const own = { height: 1 };
    const map = { transform: own, _camera: { transform: { height: 2 } } };
    expect(installDeckTransformShim(map)).toBe(false);
    expect(map.transform).toBe(own);
  });
});
