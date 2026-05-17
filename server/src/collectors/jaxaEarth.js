/**
 * JAXA Earth API collector (jaxa-earth) — kind:'intel'.
 *
 * Live: JAXA Earth API public data catalog (no auth). The JAXA Earth API
 * publishes a machine-readable collection/dataset index at
 * https://data.earth.jaxa.jp/api/ — we fetch the dataset list and emit one
 * intel item per dataset/collection so the catalog of available JAXA Earth
 * products (over Japan / global) is surfaced. These are gridded raster
 * products, not point clouds.
 *
 * Endpoint paths are best-effort (the API has versioned routes); on any
 * failure we return honest empty ('jaxa-earth_unavailable'). No credential
 * required (public catalog). Never fabricated.
 */

import { fetchJson } from './_liveHelpers.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'jaxa-earth';

// Unverified across API versions — JAXA Earth API catalog index candidates.
// First that returns a JSON list of datasets wins; otherwise honest empty.
const CATALOG_ENDPOINTS = [
  'https://data.earth.jaxa.jp/api/collection/v1/all/',
  'https://data.earth.jaxa.jp/api/collection/',
  'https://data.earth.jaxa.jp/api/datasets/',
];

function empty(source, description) {
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [],
    live: false,
    description,
    extraMeta: { source },
  });
}

function extractList(data) {
  if (Array.isArray(data)) return data;
  if (Array.isArray(data?.collections)) return data.collections;
  if (Array.isArray(data?.datasets)) return data.datasets;
  if (Array.isArray(data?.results)) return data.results;
  if (data && typeof data === 'object') {
    const vals = Object.values(data);
    if (vals.length && vals.every((v) => v && typeof v === 'object')) return vals;
  }
  return null;
}

export default async function collectJaxaEarth() {
  let list = null;
  for (const url of CATALOG_ENDPOINTS) {
    const data = await fetchJson(url, { timeoutMs: 15000 }).catch(() => null);
    const l = extractList(data);
    if (l && l.length) { list = l; break; }
  }

  if (!list || list.length === 0) {
    return empty('jaxa-earth_unavailable',
      'JAXA Earth API data catalog — upstream unavailable or empty');
  }

  const items = list.slice(0, 200).map((d, i) => {
    const id = d.id || d.identifier || d.collection_id || d.name || `dataset-${i}`;
    const title = d.title || d.name || d.id || `JAXA Earth dataset ${i}`;
    const desc = d.description || d.abstract || null;
    return {
      uid: intelUid(SOURCE_ID, id),
      title: `JAXA Earth: ${title}`,
      summary: desc ? String(desc).slice(0, 240)
        : `JAXA Earth API dataset ${id}.`,
      body: `JAXA Earth API catalog entry "${title}" (${id}).${desc ? ` ${desc}` : ''}`,
      link: 'https://data.earth.jaxa.jp/',
      published_at: d.updated || d.modified || new Date().toISOString(),
      tags: ['satellite', 'jaxa', 'earth-observation', 'catalog', 'raster'],
      properties: {
        dataset_id: id,
        bbox: d.bbox || d.extent || null,
        temporal: d.temporal || d.time_range || null,
        keywords: d.keywords || null,
      },
    };
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: true,
    description: 'JAXA Earth API public data catalog (datasets/collections)',
    extraMeta: { source: 'jaxa-earth' },
  });
}
