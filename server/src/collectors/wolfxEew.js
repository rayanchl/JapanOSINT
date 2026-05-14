/**
 * Wolfx JMA Earthquake Early Warning (EEW)
 * https://api.wolfx.jp/jma_eew.json
 * No API key. Also WebSocket: wss://ws-api.wolfx.jp/jma_eew
 */

import { fetchJson } from './_liveHelpers.js';

const API_URL = 'https://api.wolfx.jp/jma_eew.json';
const TIMEOUT_MS = 6000;

export default async function collectWolfxEew() {
  let features = [];
  let source = 'live';
  const d = await fetchJson(API_URL, { timeoutMs: TIMEOUT_MS });
  if (d?.Latitude != null && d?.Longitude != null) {
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [Number(d.Longitude), Number(d.Latitude)] },
      properties: {
        event_id: d.EventID ?? null,
        serial: d.Serial ?? null,
        magnitude: d.Magunitude ?? d.Magnitude ?? null,
        depth_km: d.Depth ?? null,
        max_intensity: d.MaxIntensity ?? null,
        hypocenter: d.Hypocenter ?? null,
        announced_time: d.AnnouncedTime ?? null,
        origin_time: d.OriginTime ?? null,
        is_final: d.isFinal ?? null,
        is_cancel: d.isCancel ?? null,
        is_warn: d.isWarn ?? null,
        source: 'wolfx_jma_eew',
      },
    });
  } else if (d == null) {
    source = 'seed';
    features = [{
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [141.0, 38.3] },
      properties: {
        event_id: 'seed_eew_1',
        magnitude: 4.0,
        depth_km: 50,
        max_intensity: '3',
        hypocenter: '宮城県沖',
        origin_time: '2026-04-12T08:15:00+09:00',
        is_final: true,
        is_cancel: false,
        is_warn: false,
        source: 'wolfx_seed',
      },
    }];
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Wolfx JMA Earthquake Early Warning (EEW)',
    },
  };
}
