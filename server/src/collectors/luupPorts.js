/**
 * LUUP — e-scooter / e-bike share live station status (ports).
 *
 * LUUP runs the largest e-scooter / e-bike share in Japan (Tokyo, Osaka,
 * Kyoto, Yokohama, etc.). They do NOT publish GBFS, so this is distinct
 * from `bike-share-gbfs` (which fans out HelloCycling + DOCOMO Cycle).
 *
 * Endpoints we try, in order:
 *   1. `https://luup.sc/api/...`              — public web app sometimes proxies
 *   2. `https://api.luup.sc/v1/ports`         — mobile-app API (token required)
 *   3. `https://luup.sc/`                     — homepage probe (always-reachable
 *                                                signal vs. API outage)
 *
 * Auth: the mobile API authenticates with a bearer token issued by LUUP's
 * sign-up flow. Set `LUUP_MOBILE_TOKEN` in `.env` to enable the per-port
 * read. We DO NOT impersonate accounts, brute-force endpoints, or write —
 * this is a read-only public-data probe.
 *
 * Output: per-port intel item with lat/lon + vehicle counts when authed;
 * portal status only when token is absent.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchJson, fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'luup-private';
const KEY_ENV = 'LUUP_MOBILE_TOKEN';

const HOMEPAGE = 'https://luup.sc/';
const PORTS_API = 'https://api.luup.sc/v1/ports';

// Tokyo / Osaka / Kyoto / Yokohama bboxes — LUUP currently operates
// inside these. We loop the API per bbox because the unbounded call
// occasionally truncates to 500.
const BBOXES = [
  { city: 'tokyo',    minLat: 35.50, maxLat: 35.80, minLon: 139.55, maxLon: 139.90 },
  { city: 'osaka',    minLat: 34.60, maxLat: 34.78, minLon: 135.40, maxLon: 135.60 },
  { city: 'kyoto',    minLat: 34.95, maxLat: 35.07, minLon: 135.69, maxLon: 135.82 },
  { city: 'yokohama', minLat: 35.40, maxLat: 35.52, minLon: 139.55, maxLon: 139.70 },
];

async function fetchBbox(token, bbox) {
  const qs = `lat_min=${bbox.minLat}&lat_max=${bbox.maxLat}&lng_min=${bbox.minLon}&lng_max=${bbox.maxLon}`;
  try {
    return await fetchJson(`${PORTS_API}?${qs}`, {
      timeoutMs: 10000,
      headers: {
        'Authorization': `Bearer ${token}`,
        'User-Agent': 'JapanOSINT/1.0 (defensive-infra-research; read-only)',
        'Accept': 'application/json',
      },
    });
  } catch {
    return null;
  }
}

export default async function collectLuupPorts() {
  const token = process.env[KEY_ENV] || '';
  const hasKey = token.length > 0;
  const portalLive = await fetchHead(HOMEPAGE).catch(() => false);

  // Without a token we just emit the portal pointer.
  if (!hasKey) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [{
        uid: intelUid(SOURCE_ID, 'portal'),
        title: 'LUUP — e-scooter / e-bike share (token required)',
        summary: `Set ${KEY_ENV} (bearer from LUUP mobile sign-up) to enable per-port reads`,
        link: HOMEPAGE,
        language: 'ja',
        published_at: new Date().toISOString(),
        tags: ['mobility', 'luup', 'scooter', portalLive ? 'reachable' : 'unreachable', 'key-missing'],
        properties: { operator: 'Luup, Inc.', reachable: portalLive, requires_key: true, has_key: false },
      }],
      live: portalLive,
      description: 'LUUP e-scooter / e-bike share — portal probe (token absent)',
    });
  }

  const items = [];
  let anyApiLive = false;

  for (const bbox of BBOXES) {
    const j = await fetchBbox(token, bbox);
    const ports = Array.isArray(j) ? j : (j?.ports || j?.data || []);
    if (ports.length > 0) anyApiLive = true;

    for (const p of ports.slice(0, 500)) {
      // Field-name normalization — LUUP's mobile schema has used `lat`/`lng`
      // and `latitude`/`longitude` in different builds; accept either.
      const lat = Number(p.lat ?? p.latitude);
      const lon = Number(p.lng ?? p.longitude ?? p.lon);
      if (!Number.isFinite(lat) || !Number.isFinite(lon)) continue;
      items.push({
        uid: intelUid(SOURCE_ID, `${bbox.city}|${p.id ?? p.port_id ?? `${lat},${lon}`}`),
        title: p.name || `LUUP port ${p.id ?? ''}`,
        summary: `${p.vehicle_count ?? p.available ?? '?'} vehicles @ ${p.name || bbox.city}`,
        link: null,
        language: 'ja',
        published_at: new Date().toISOString(),
        tags: ['mobility', 'luup', 'scooter', bbox.city],
        properties: {
          operator: 'Luup, Inc.',
          city: bbox.city,
          port_id: p.id ?? p.port_id ?? null,
          lat, lon,
          vehicle_count: p.vehicle_count ?? p.available ?? null,
          capacity: p.capacity ?? null,
          status: p.status ?? null,
        },
      });
    }
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: anyApiLive,
    description: 'LUUP e-scooter / e-bike share — per-port real-time vehicle counts',
    extraMeta: { bboxes: BBOXES.length, ports: items.length },
  });
}
