/**
 * JAMSTEC / Argo float collector (jamstec-argo).
 *
 * JAMSTEC's argo_compass site has no key-free JSON endpoint (the historical
 * sample paths now 404/redirect to an error page). The authoritative key-free
 * machine-readable source for Argo profiling-float positions — including the
 * JMA/JAMSTEC-operated DAC floats around Japan — is the public Argovis API:
 *   https://argovis-api.colorado.edu/argo
 * which returns GeoJSON-style profiles with `geolocation.coordinates`.
 *
 * We query the most recent ~14 days within a Japan-region polygon. Honest
 * empty FeatureCollection on failure — never fabricated floats.
 */

import { fetchJson } from './_liveHelpers.js';

const ARGOVIS = 'https://argovis-api.colorado.edu/argo';
// Japan-region polygon (lon,lat) closed ring: SW → SE → NE → NW → SW.
const POLYGON = '[[122,24],[146,24],[146,46],[122,46],[122,24]]';

function result(features, source) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Argo profiling-float positions around Japan (Argovis / JAMSTEC-JMA DAC)',
    },
  };
}

export default async function collectJamstecArgo() {
  const end = new Date();
  const start = new Date(end.getTime() - 14 * 24 * 3600 * 1000);
  const url = `${ARGOVIS}?startDate=${start.toISOString()}&endDate=${end.toISOString()}`
    + `&polygon=${encodeURIComponent(POLYGON)}`;

  const data = await fetchJson(url, { timeoutMs: 20000 }).catch(() => null);
  if (!Array.isArray(data)) return result([], 'jamstec_argo_unavailable');

  const features = [];
  for (const p of data) {
    const coords = p?.geolocation?.coordinates;
    if (!Array.isArray(coords) || coords.length < 2) continue;
    const lon = +coords[0];
    const lat = +coords[1];
    if (!Number.isFinite(lon) || !Number.isFinite(lat)) continue;
    const sources = Array.isArray(p.source) ? p.source.flatMap((s) => s.source || []) : [];
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [lon, lat] },
      properties: {
        profile_id: p._id ?? null,
        cycle_number: p.cycle_number ?? null,
        observed_at: p.timestamp ?? null,
        basin: p.basin ?? null,
        profile_direction: p.profile_direction ?? null,
        data_sources: sources,
        source: 'argovis_argo',
      },
    });
  }
  return result(features, features.length ? 'argovis_argo' : 'jamstec_argo_empty');
}
