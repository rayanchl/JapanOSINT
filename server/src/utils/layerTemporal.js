/**
 * Layer-level temporal disposition for the time-slider feature.
 *
 * /api/data/:layer accepts ?at=<iso>&window=<seconds>. Per-layer behaviour:
 *
 *   { liveOnly: true } → return an empty FC in replay (no historical data
 *                        exists; vehicle positions etc. are not archived)
 *   { field, fallbackField } → apply COALESCE(field, fallbackField) BETWEEN
 *                              (at − window) AND at against intel_items
 *   null → static layer: ignore at/window, return current data unchanged
 *
 * The slider is a single global control (one `at`, one `window`) shared
 * across every active layer — there is no per-layer mode toggle. Only the
 * column name varies per layer.
 */
const DEFAULT_TEMPORAL = Object.freeze({
  field: 'published_at',
  fallbackField: 'fetched_at',
});

const LIVE_ONLY = new Set([
  'unified-flights',
]);

const STATIC = new Set([
  'unified-trains', 'unified-subways', 'unified-buses',
  'unified-ais-ships', 'unified-port-infra', 'unified-stations',
  'unified-airports', 'unified-station-footprints',
]);

const OVERRIDES = Object.freeze({});

export function getTemporalForLayer(layerId) {
  if (LIVE_ONLY.has(layerId)) return { liveOnly: true };
  if (STATIC.has(layerId))    return null;
  if (OVERRIDES[layerId])     return OVERRIDES[layerId];
  return DEFAULT_TEMPORAL;
}

export function describeTemporal(layerId) {
  const t = getTemporalForLayer(layerId);
  if (t?.liveOnly) return { liveOnly: true };
  if (!t) return null;
  return { temporal: { field: t.field, fallbackField: t.fallbackField } };
}
