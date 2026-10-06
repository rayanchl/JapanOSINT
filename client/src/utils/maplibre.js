/**
 * The ONE place this client imports maplibre-gl. Everything else imports the
 * namespace from here, so the two bits of process-wide setup below cannot be
 * skipped by a module that happens to load first.
 *
 * 1. Worker URL. maplibre-gl v6 is ESM-only and finds its worker with
 *    `new URL('./maplibre-gl-worker.mjs', import.meta.url)`. Vite does not
 *    emit a file for that pattern inside a dependency, so a production build
 *    requested /assets/maplibre-gl-worker.mjs, got the SPA's index.html back,
 *    and the worker never started: no tile ever parsed, the map never fired
 *    `load`, and everything gated on it (overlays, AOI, isochrone, popups)
 *    silently did nothing. `?worker&url` makes Vite bundle the worker as a
 *    real asset and hands back its hashed URL.
 *
 * 2. deck.gl compatibility. maplibre v6 split the camera out of Map (Map no
 *    longer extends Camera), so `map.transform` is gone; its live transform
 *    is `map._camera.transform`. Every @deck.gl/mapbox release up to and
 *    including 9.4.0 reads `map.transform.{height,elevation,_nearZ,_farZ}` on
 *    every interleaved frame, which threw a TypeError per frame: no deck
 *    layers drew at all (re-channelled place labels, PLATEAU buildings).
 *    Downgrading maplibre is not an option — every 5.x (and 6.x up to 6.4.0)
 *    carries GHSA-jrc7-96c5-q579, a critical XSS in DOM.sanitize() reachable
 *    through a remote basemap style's attribution HTML. So the property is
 *    restored as a read-only alias on the instance. Remove this once a deck.gl
 *    release stops reading `map.transform` (or once maplibre restores it).
 */
import * as maplibregl from 'maplibre-gl';
import workerUrl from 'maplibre-gl/dist/maplibre-gl-worker.mjs?worker&url';

maplibregl.setWorkerUrl(workerUrl);

/**
 * Give a maplibre v6 Map the `transform` property deck.gl's MapboxOverlay
 * reads. No-op when the instance already has one (an older/newer maplibre
 * that kept it), so this never shadows a real implementation.
 */
export function installDeckTransformShim(map) {
  if (!map || map.transform !== undefined) return false;
  if (!map._camera || map._camera.transform === undefined) return false;
  Object.defineProperty(map, 'transform', {
    configurable: true,
    get() { return this._camera.transform; },
  });
  return true;
}

export default maplibregl;
