/**
 * GCOM-W / AMSR2 collector (gcom-w) — kind:'intel'.
 *
 * JAXA GCOM-W "SHIZUKU" carries the AMSR2 microwave radiometer. The Level-1/2
 * product archive lives on JAXA G-Portal which requires an authenticated
 * account. AMSR2 standard products are, however, also mirrored without auth
 * by the NASA NSIDC / GHRC DAAC and exposed through NASA CMR (Common Metadata
 * Repository) — a public, keyless granule search. We query CMR for the most
 * recent AMSR2 granules and emit one intel item per granule (microwave raster
 * swath product, not a point cloud).
 *
 * Optional JAXA_GPORTAL_TOKEN is read via envFor (note only — CMR path is
 * keyless). Honest empty ('gcom-w_unavailable') on failure. Never fabricated.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'gcom-w';
// minLon, minLat, maxLon, maxLat
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

export default async function collectGcomW() {
  // Optional — JAXA G-Portal token (not required; NASA CMR path is keyless).
  const gportalToken = envFor('JAXA_GPORTAL_TOKEN');

  const [w, s, e, n] = JP_BBOX;
  // NASA CMR granule search (public, no auth). AMSR2 L3 sea-surface params.
  // Short name unverified across versions; we search by AMSR2 keyword + bbox.
  const url = 'https://cmr.earthdata.nasa.gov/search/granules.json'
    + '?keyword=AMSR2'
    + `&bounding_box=${w},${s},${e},${n}`
    + '&sort_key=-start_date&page_size=50';
  const data = await fetchJson(url, { timeoutMs: 20000 }).catch(() => null);

  const rows = data?.feed?.entry;
  if (!Array.isArray(rows) || rows.length === 0) {
    return empty('gcom-w_unavailable',
      'JAXA GCOM-W AMSR2 granules over Japan — NASA CMR returned no granules');
  }

  const items = rows.slice(0, 50).map((g, i) => {
    const sid = g.producer_granule_id || g.title || g.id || `granule-${i}`;
    const acquired = g.time_start || g.updated || null;
    return {
      uid: intelUid(SOURCE_ID, sid),
      title: `GCOM-W AMSR2 granule ${sid}`,
      summary: `JAXA GCOM-W AMSR2 microwave granule acquired ${acquired || 'unknown'} over Japan.`,
      body: `GCOM-W "SHIZUKU" AMSR2 microwave radiometer granule ${sid} (NASA CMR mirror) intersecting the Japan AOI. Acquired ${acquired || 'unknown'}.${gportalToken ? ' JAXA G-Portal token present (native products via G-Portal).' : ''}`,
      link: g.links?.[0]?.href || 'https://gportal.jaxa.jp/',
      published_at: acquired || new Date().toISOString(),
      tags: ['satellite', 'gcom-w', 'amsr2', 'microwave', 'jaxa', 'raster'],
      properties: {
        bbox: JP_BBOX,
        acquired,
        time_end: g.time_end || null,
        sensor: 'AMSR2',
        platform: 'GCOM-W',
        scene_id: sid,
        dataset: g.dataset_id || null,
      },
    };
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: true,
    description: 'JAXA GCOM-W AMSR2 microwave granules over Japan (NASA CMR public mirror)',
    extraMeta: { source: 'gcom-w' },
  });
}
