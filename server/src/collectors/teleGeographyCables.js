/**
 * TeleGeography Submarine Cable Map — JP-landing systems.
 * https://www.submarinecablemap.com/
 *
 * Companion to OSM-based `submarineCables.js`. TeleGeography is the
 * canonical source of cable IDs + RFS year + capacity, and their landing-
 * point JSON is what every news article quotes when a cable cut happens.
 * Their data is published under CC-BY at:
 *   https://www.submarinecablemap.com/api/v3/landing-point/landing-point-geo.json
 *   https://www.submarinecablemap.com/api/v3/cable/cable-geo.json
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchJson } from './_liveHelpers.js';

const SOURCE_ID = 'telegeography-cables';
const LANDING_URL = 'https://www.submarinecablemap.com/api/v3/landing-point/landing-point-geo.json';
const CABLE_URL = 'https://www.submarinecablemap.com/api/v3/cable/cable-geo.json';

const JP_BBOX = { latMin: 24, latMax: 46, lonMin: 122, lonMax: 154 };

function inJp(lon, lat) {
  return lat >= JP_BBOX.latMin && lat <= JP_BBOX.latMax
      && lon >= JP_BBOX.lonMin && lon <= JP_BBOX.lonMax;
}

export default async function collectTeleGeographyCables() {
  let landings = null;
  let cables = null;
  try { landings = await fetchJson(LANDING_URL, { timeoutMs: 12000 }); } catch { /* ignore */ }
  try { cables = await fetchJson(CABLE_URL, { timeoutMs: 12000 }); } catch { /* ignore */ }

  const items = [];

  for (const f of landings?.features || []) {
    const coords = f.geometry?.coordinates;
    if (!Array.isArray(coords) || coords.length < 2) continue;
    if (!inJp(coords[0], coords[1])) continue;
    items.push({
      uid: intelUid(SOURCE_ID, `landing|${f.properties?.id || f.properties?.name}`),
      title: f.properties?.name || `Landing point ${f.properties?.id}`,
      summary: `Submarine cable landing — ${f.properties?.name || ''}`,
      link: f.properties?.url || null,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['submarine-cable', 'landing', 'telegeography'],
      properties: { kind: 'landing', lat: coords[1], lon: coords[0], ...(f.properties || {}) },
    });
  }

  for (const f of cables?.features || []) {
    // Cable polylines: include if ANY coordinate falls inside JP bbox.
    const lines = f.geometry?.type === 'MultiLineString'
      ? f.geometry.coordinates
      : (f.geometry?.type === 'LineString' ? [f.geometry.coordinates] : []);
    let touches = false;
    let firstCoord = null;
    for (const seg of lines) {
      for (const c of seg) {
        if (Array.isArray(c) && c.length >= 2) {
          if (!firstCoord) firstCoord = c;
          if (inJp(c[0], c[1])) { touches = true; break; }
        }
      }
      if (touches) break;
    }
    if (!touches) continue;
    items.push({
      uid: intelUid(SOURCE_ID, `cable|${f.properties?.id || f.properties?.name}`),
      title: f.properties?.name || `Cable ${f.properties?.id}`,
      summary: `Submarine cable touching JP — ${f.properties?.name || ''} (RFS ${f.properties?.rfs || '?'})`,
      link: f.properties?.url || null,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['submarine-cable', 'cable', 'telegeography'],
      properties: {
        kind: 'cable',
        lat: firstCoord?.[1] ?? null,
        lon: firstCoord?.[0] ?? null,
        ...(f.properties || {}),
      },
    });
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: items.length > 0,
    description: 'TeleGeography submarine cable map (JP-landing subset)',
  });
}
