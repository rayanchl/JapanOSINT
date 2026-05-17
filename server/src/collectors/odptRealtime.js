/**
 * ODPT real-time feeds (odpt-train / odpt-bus / odpt-flight / odpt-station).
 *
 * One module, four registered collectors (mirrors the unified-* pattern in
 * collectors/index.js). Live ODPT v4 pulls when an ODPT token is set.
 * Without a token, train/bus/flight return an honest empty result (no
 * fabricated data); odpt-station has a real key-free fallback (OSM rail
 * stations via Overpass). Missing token surfaces via the apiCredentials map.
 */

import { fetchJson, fetchOverpass } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const BASES = ['https://api.odpt.org/api/v4/', 'https://api-challenge.odpt.org/api/v4/'];

function token() {
  return envFor('ODPT_TOKEN') || envFor('ODPT_CONSUMER_KEY') || envFor('ODPT_CHALLENGE_TOKEN') || null;
}

async function odptGet(rdfType, tok) {
  for (const base of BASES) {
    const url = `${base}${rdfType}?acl:consumerKey=${encodeURIComponent(tok)}`;
    const data = await fetchJson(url, { timeoutMs: 20000 });
    if (Array.isArray(data) && data.length > 0) return data;
  }
  return null;
}

function pointFeature(coords, props) {
  return { type: 'Feature', geometry: { type: 'Point', coordinates: coords }, properties: props };
}

function fc(features, source, description, hasToken) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      has_token: !!hasToken,
      description,
    },
  };
}

// ── odpt-train ──────────────────────────────────────────────────────────────
export async function collectOdptTrain() {
  const tok = token();
  if (!tok) {
    return fc([], 'odpt_no_token',
      'Real-time train positions/delays via ODPT (requires ODPT token)', tok);
  }
  const rows = await odptGet('odpt:Train', tok);
  const features = (rows || [])
    .filter((r) => r['geo:lat'] != null && r['geo:long'] != null)
    .map((r) => pointFeature([+r['geo:long'], +r['geo:lat']], {
      train_uid: r['owl:sameAs'] || r['@id'] || null,
      railway: r['odpt:railway'] || null,
      delay_sec: r['odpt:delay'] ?? null,
      from_station: r['odpt:fromStation'] || null,
      to_station: r['odpt:toStation'] || null,
      source: 'odpt_api',
    }));
  return fc(features, features.length ? 'odpt_api' : 'odpt_unavailable',
    'Real-time train positions/delays via ODPT', tok);
}

// ── odpt-bus ────────────────────────────────────────────────────────────────
export async function collectOdptBus() {
  const tok = token();
  if (!tok) {
    return fc([], 'odpt_no_token',
      'Real-time bus positions via ODPT (requires ODPT token)', tok);
  }
  const rows = await odptGet('odpt:Bus', tok);
  const features = (rows || [])
    .filter((r) => r['geo:lat'] != null && r['geo:long'] != null)
    .map((r) => pointFeature([+r['geo:long'], +r['geo:lat']], {
      bus_uid: r['owl:sameAs'] || r['@id'] || null,
      operator: r['odpt:operator'] || null,
      route: r['odpt:busroutePattern'] || null,
      source: 'odpt_api',
    }));
  return fc(features, features.length ? 'odpt_api' : 'odpt_unavailable',
    'Real-time bus positions via ODPT', tok);
}

// ── odpt-flight ─────────────────────────────────────────────────────────────
const AIRPORTS = {
  HND: { lat: 35.5494, lon: 139.7798 }, NRT: { lat: 35.7720, lon: 140.3929 },
  KIX: { lat: 34.4320, lon: 135.2304 }, CTS: { lat: 42.7752, lon: 141.6923 },
};

export async function collectOdptFlight() {
  const tok = token();
  if (!tok) {
    return fc([], 'odpt_no_token',
      'Flight arrival/departure info via ODPT (requires ODPT token)', tok);
  }
  const features = [];
  for (const rt of ['odpt:FlightInformationArrival', 'odpt:FlightInformationDeparture']) {
    const rows = await odptGet(rt, tok);
    for (const r of (rows || [])) {
      const apId = String(r['odpt:airport'] || '').replace(/^odpt\.Airport:/, '');
      const ap = AIRPORTS[apId];
      if (!ap) continue;
      features.push(pointFeature([ap.lon, ap.lat], {
        flight_uid: r['owl:sameAs'] || r['@id'] || null,
        flight_number: (r['odpt:flightNumber'] || []).join(',') || null,
        airport: apId,
        status: String(r['odpt:flightStatus'] || '').replace(/^odpt\.FlightStatus:/, '') || null,
        scheduled: r['odpt:scheduledDepartureTime'] || r['odpt:scheduledArrivalTime'] || null,
        source: 'odpt_api',
      }));
    }
  }
  return fc(features, features.length ? 'odpt_api' : 'odpt_unavailable',
    'Flight arrival/departure info via ODPT', tok);
}

// ── odpt-station ────────────────────────────────────────────────────────────
export async function collectOdptStation() {
  const tok = token();
  let features = [];
  let source = 'odpt_unavailable';

  if (tok) {
    const rows = await odptGet('odpt:Station', tok);
    features = (rows || [])
      .filter((r) => r['geo:lat'] != null && r['geo:long'] != null)
      .map((r) => pointFeature([+r['geo:long'], +r['geo:lat']], {
        station_uid: r['owl:sameAs'] || r['@id'] || null,
        station_name: r['odpt:stationTitle']?.en || r['dc:title'] || null,
        station_name_ja: r['odpt:stationTitle']?.ja || r['dc:title'] || null,
        railway: r['odpt:railway'] || null,
        operator: r['odpt:operator'] || null,
        source: 'odpt_api',
      }));
    if (features.length > 0) source = 'odpt_api';
  }

  if (features.length === 0) {
    // Real key-free fallback: every OSM rail station across Japan.
    const osm = await fetchOverpass(
      'node["railway"="station"](area.jp);node["railway"="halt"](area.jp);',
      (el, _i, coords) => pointFeature(coords, {
        station_uid: `OSM_${el.id}`,
        station_name: el.tags?.['name:en'] || el.tags?.name || 'Station',
        station_name_ja: el.tags?.name || null,
        operator: el.tags?.operator || null,
        railway: el.tags?.railway || null,
        source: 'osm_overpass',
      }),
      60_000, { limit: 0, queryTimeout: 180 },
    );
    if (osm && osm.length > 0) { features = osm; source = 'osm_overpass'; }
  }

  return fc(features, source,
    'Station locations/metadata via ODPT (OSM rail-station fallback when no ODPT token)', tok);
}
