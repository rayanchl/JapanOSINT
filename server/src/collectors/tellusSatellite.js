/**
 * Tellus satellite collector (tellus-satellite) — kind:'intel'.
 *
 * Tellus (さくらインターネット / 経産省) is Japan's national satellite-data
 * platform. Its data API (catalog / scene search) requires an API token.
 * We read TELLUS_TOKEN via envFor (BYOK). When absent we return honest empty
 * ('tellus-satellite_no_key'); when present we query the Tellus data-search
 * API for the latest scenes over Japan and emit one intel item per scene
 * (raster product, not a point cloud).
 *
 * The Tellus Traveler / data-search API path is unverified across versions;
 * any auth/fetch failure yields honest empty ('tellus-satellite_unavailable').
 * Never fabricated.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'tellus-satellite';
const JP_BBOX = [122.0, 24.0, 146.0, 46.0];

function empty(source, description) {
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [],
    live: false,
    description,
    extraMeta: { source },
  });
}

export default async function collectTellusSatellite() {
  const token = envFor('TELLUS_TOKEN');
  if (!token) {
    return empty('tellus-satellite_no_key',
      'Tellus satellite scene catalog over Japan (requires TELLUS_TOKEN)');
  }

  const [w, s, e, n] = JP_BBOX;
  // Tellus Traveler data-search API (STAC-like). Endpoint/route unverified
  // across Tellus API versions; honest empty on any failure.
  const reqBody = {
    intersects: {
      type: 'Polygon',
      coordinates: [[[w, s], [e, s], [e, n], [w, n], [w, s]]],
    },
    limit: 50,
    sortby: [{ field: 'properties.start_datetime', direction: 'desc' }],
  };
  const data = await fetchJson(
    'https://www.tellusxdp.com/api/traveler/v1/data-search/',
    {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${token}`,
      },
      body: JSON.stringify(reqBody),
      timeoutMs: 20000,
    },
  ).catch(() => null);

  const feats = Array.isArray(data?.features) ? data.features
    : Array.isArray(data?.results) ? data.results
      : Array.isArray(data?.items) ? data.items : null;
  if (!feats || feats.length === 0) {
    return empty('tellus-satellite_unavailable',
      'Tellus satellite scene catalog over Japan — API returned no scenes');
  }

  const items = feats.slice(0, 50).map((f, i) => {
    const props = f.properties || f;
    const sid = f.id || props.scene_id || props.id || `scene-${i}`;
    const acquired = props.start_datetime || props.datetime
      || props.acquired || null;
    return {
      uid: intelUid(SOURCE_ID, sid),
      title: `Tellus scene ${sid}`,
      summary: `Tellus satellite scene acquired ${acquired || 'unknown'} over Japan.`,
      body: `Tellus platform satellite scene ${sid} intersecting the Japan AOI. Acquired ${acquired || 'unknown'}. Sensor ${props.sensor || props.platform || 'unknown'}.`,
      link: 'https://www.tellusxdp.com/',
      published_at: acquired || new Date().toISOString(),
      tags: ['satellite', 'tellus', 'japan', 'raster'],
      properties: {
        bbox: f.bbox || JP_BBOX,
        geometry: f.geometry || null,
        acquired,
        sensor: props.sensor || null,
        platform: props.platform || null,
        scene_id: sid,
      },
    };
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: true,
    description: 'Latest Tellus platform satellite scenes over Japan',
    extraMeta: { source: 'tellus-satellite' },
  });
}
