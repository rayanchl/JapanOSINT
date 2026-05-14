/**
 * Bike-share GBFS feeds (HelloCycling + adjacent operators).
 * GBFS (General Bikeshare Feed Specification) is the cross-operator
 * standard JSON. JP operators publishing GBFS include:
 *   - HelloCycling (OpenStreet — DOCOMO Bike Share)
 *   - LUUP (e-scooter / e-bike — partial)
 * For each operator we pull station_information.json (lat/lon, capacity)
 * and station_status.json (num_bikes_available, num_docks_available).
 *
 * Bike-flux gradient between stations ≈ live commute heatmap, leading
 * indicator vs the rail layer for last-mile demand.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchJson } from './_liveHelpers.js';

const SOURCE_ID = 'bike-share-gbfs';
const OPERATORS = [
  {
    operator: 'HelloCycling',
    discovery: 'https://api-public.odpt.org/api/v4/gbfs/hellocycling/gbfs.json',
  },
  {
    operator: 'docomo-bike-share-tokyo',
    discovery: 'https://api-public.odpt.org/api/v4/gbfs/docomo-cycle-tokyo/gbfs.json',
  },
];

async function loadGbfs(disc) {
  try {
    const root = await fetchJson(disc.discovery, { timeoutMs: 8000 });
    const feeds = root?.data?.ja?.feeds || root?.data?.en?.feeds || [];
    const byName = Object.fromEntries(feeds.map((f) => [f.name, f.url]));
    const info = byName.station_information ? await fetchJson(byName.station_information, { timeoutMs: 8000 }) : null;
    const status = byName.station_status ? await fetchJson(byName.station_status, { timeoutMs: 8000 }) : null;
    return { info, status };
  } catch {
    return { info: null, status: null };
  }
}

export default async function collectBikeShareGbfs() {
  const items = [];
  let anyLive = false;

  for (const op of OPERATORS) {
    const { info, status } = await loadGbfs(op);
    const stations = info?.data?.stations || [];
    const statusByStation = new Map(
      (status?.data?.stations || []).map((s) => [s.station_id, s]),
    );
    if (stations.length > 0) anyLive = true;
    for (const s of stations.slice(0, 500)) {
      const st = statusByStation.get(s.station_id);
      items.push({
        uid: intelUid(SOURCE_ID, `${op.operator}|${s.station_id}`),
        title: s.name || `${op.operator} station ${s.station_id}`,
        summary: st
          ? `${st.num_bikes_available ?? '?'} bikes / ${st.num_docks_available ?? '?'} docks`
          : 'GBFS station',
        link: null,
        language: 'ja',
        published_at: new Date().toISOString(),
        tags: ['mobility', 'bike-share', 'gbfs', op.operator],
        properties: {
          operator: op.operator,
          station_id: s.station_id,
          lat: s.lat,
          lon: s.lon,
          capacity: s.capacity || null,
          num_bikes_available: st?.num_bikes_available ?? null,
          num_docks_available: st?.num_docks_available ?? null,
          is_renting: st?.is_renting ?? null,
          is_returning: st?.is_returning ?? null,
        },
      });
    }
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: anyLive,
    description: 'Bike-share GBFS feeds (HelloCycling, DOCOMO Cycle Tokyo)',
  });
}
