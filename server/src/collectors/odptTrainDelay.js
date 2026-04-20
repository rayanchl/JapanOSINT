/**
 * ODPT Train Information (delay feed) — odpt:TrainInformation endpoint.
 *
 * Auth: requires ODPT_TOKEN (same env var as odptTransport.js — free key
 * from https://developer.odpt.org/ / https://api-challenge.odpt.org/).
 *
 * Each returned feature represents a railway operator's current service
 * status (delay text, status text, valid/expire times). ODPT publishes
 * these as line-level events, not station-level — we attach the line's
 * rough bounding-box centroid as the point geometry when available, or
 * null geometry otherwise (the framework stores it in the DB regardless).
 *
 * Operators that publish on ODPT Challenge: Tokyo Metro, Toei, TX, Rinkai,
 * TWR and a handful of private railways. JR East is NOT on ODPT.
 */

import { getOdptToken } from '../utils/odptAuth.js';

const API_BASE = 'https://api.odpt.org/api/v4/';
const CHALLENGE_API_BASE = 'https://api-challenge.odpt.org/api/v4/';
const TIMEOUT_MS = 15000;

// Rough centroids for well-known operator railway-line areas, used when
// ODPT's TrainInformation doesn't carry a specific coordinate. These are
// just display hints; the authoritative lng/lat comes from our existing
// station data when available.
const OPERATOR_CENTROIDS = {
  'TokyoMetro': [139.7671, 35.6812],  // Tokyo station
  'Toei': [139.7454, 35.6895],        // Shinjuku
  'TWR': [139.7764, 35.6197],         // Shinagawa area
  'Rinkai': [139.8179, 35.6312],      // Tennozu
  'TX': [139.7867, 35.8156],          // Akihabara-Tsukuba axis midpoint
};

function endpointFor(base) {
  return `${base}odpt:TrainInformation`;
}

async function tryEndpoint(base, token) {
  const url = `${endpointFor(base)}?acl:consumerKey=${encodeURIComponent(token)}`;
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
  try {
    const res = await fetch(url, {
      signal: controller.signal,
      headers: { 'accept': 'application/json' },
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    const data = await res.json();
    return Array.isArray(data) ? data : null;
  } catch {
    return null;
  }
}

function operatorShortName(opUrl) {
  if (!opUrl) return null;
  // "odpt.Operator:TokyoMetro" → "TokyoMetro"
  const m = String(opUrl).match(/odpt\.Operator:([^/]+)/);
  return m ? m[1] : null;
}

function railwayShortName(rwUrl) {
  if (!rwUrl) return null;
  const m = String(rwUrl).match(/odpt\.Railway:([^/]+)/);
  return m ? m[1] : null;
}

export default async function collectOdptTrainDelay() {
  const token = getOdptToken();
  let raw = null;
  let liveBase = null;

  if (token) {
    raw = await tryEndpoint(API_BASE, token);
    liveBase = 'odpt.org';
    if (!raw) {
      raw = await tryEndpoint(CHALLENGE_API_BASE, token);
      liveBase = 'api-challenge.odpt.org';
    }
  }

  const items = Array.isArray(raw) ? raw : [];
  const features = items.map((it, i) => {
    const operator = operatorShortName(it['odpt:operator']);
    const railway = railwayShortName(it['odpt:railway']);
    const centroid = OPERATOR_CENTROIDS[operator] || null;

    // Title / status text — prefer English when available
    const statusTitle = it['odpt:trainInformationText']?.en
      || it['odpt:trainInformationText']?.ja
      || (typeof it['odpt:trainInformationText'] === 'string' ? it['odpt:trainInformationText'] : null);
    const statusJa = it['odpt:trainInformationText']?.ja;

    // Delay text comes on some operators under different property names
    const delayText = it['odpt:trainInformationStatus']?.en
      || it['odpt:trainInformationStatus']?.ja
      || (typeof it['odpt:trainInformationStatus'] === 'string' ? it['odpt:trainInformationStatus'] : null);

    return {
      type: 'Feature',
      geometry: centroid ? { type: 'Point', coordinates: centroid } : null,
      properties: {
        id: `ODPT_TI_${operator || 'x'}_${railway || i}`,
        operator,
        railway,
        status: delayText,
        message: statusTitle,
        message_ja: statusJa,
        valid_from: it['dct:valid'] || null,
        updated: it['dc:date'] || null,
        source: liveBase ? `odpt:${liveBase}` : 'odpt',
      },
    };
  }).filter((f) => f.properties.message || f.properties.status);

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: liveBase ? 'odpt_train_information_live' : 'odpt_train_information_unavailable',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      has_token: !!token,
      endpoint: liveBase ? endpointFor(liveBase === 'odpt.org' ? API_BASE : CHALLENGE_API_BASE) : null,
      description: 'ODPT odpt:TrainInformation — current train service status / delays per railway',
    },
    metadata: {},
  };
}
