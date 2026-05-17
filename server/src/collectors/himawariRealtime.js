/**
 * Himawari real-time collector (himawari-realtime) — kind:'intel'.
 *
 * Live: NICT Himawari "real-time web" publishes a keyless latest-frame
 * pointer at https://himawari.asia/img/D531106/latest.json which returns
 * { date: "YYYY-MM-DD HH:MM:SS" } for the most recent full-disk geostationary
 * frame covering Japan / the western Pacific. We emit a single intel item
 * describing the latest available frame plus its preview/tile URL templates
 * (this is a raster product — one frame, not a point cloud).
 *
 * No credential required (NICT real-time web is public). Honest empty
 * ('himawari-realtime_unavailable') if the pointer is unreachable.
 * Never fabricated.
 */

import { fetchJson } from './_liveHelpers.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'himawari-realtime';
const LATEST = 'https://himawari.asia/img/D531106/latest.json';

function empty(source, description) {
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [],
    live: false,
    description,
    extraMeta: { source },
  });
}

export default async function collectHimawariRealtime() {
  const latest = await fetchJson(LATEST, { timeoutMs: 10000 }).catch(() => null);
  const date = latest?.date;
  if (!date || typeof date !== 'string') {
    return empty('himawari-realtime_unavailable',
      'Himawari-9 real-time full-disk frame (NICT) — latest pointer unavailable');
  }

  const iso = (() => {
    const d = new Date(date.replace(' ', 'T') + 'Z');
    return Number.isNaN(d.getTime()) ? new Date().toISOString() : d.toISOString();
  })();

  const y = date.slice(0, 4);
  const mo = date.slice(5, 7);
  const da = date.slice(8, 10);
  const hh = date.slice(11, 13);
  const mm = date.slice(14, 16);
  const previewUrl = `https://himawari.asia/img/D531106/8d/${y}/${mo}/${da}/${hh}${mm}00_3_3.png`;
  const tileTemplate = `https://himawari.asia/img/D531106/8d/${y}/${mo}/${da}/${hh}${mm}00_{x}_{y}.png`;

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, date),
      title: `Himawari-9 full-disk frame ${date} UTC`,
      summary: `Latest NICT Himawari-9 AHI full-disk true-color frame covering Japan, acquired ${date} UTC.`,
      body: `Himawari-9 (AHI sensor) geostationary full-disk frame published by NICT real-time web. Latest frame timestamp ${date} UTC. Covers Japan and the western Pacific at 10-minute cadence.`,
      link: 'https://himawari.asia/',
      published_at: iso,
      tags: ['satellite', 'himawari', 'nict', 'geostationary', 'real-time', 'raster'],
      properties: {
        bbox: [85.0, -60.0, 205.0, 60.0],
        acquired: iso,
        sensor: 'AHI',
        platform: 'Himawari-9',
        scene_id: date,
        preview_url: previewUrl,
        tile_url: tileTemplate,
        cadence_minutes: 10,
      },
    }],
    live: true,
    description: 'Himawari-9 real-time full-disk frame (NICT, latest available)',
    extraMeta: { source: 'himawari-realtime' },
  });
}
