/**
 * Maps each data source to the environment variables it needs in order to
 * call its upstream third-party API. Used by the /api/status endpoint to
 * tell the frontend which APIs are "configured" without ever leaking the
 * secret values themselves.
 *
 * Each entry can specify:
 *   required  – every var must be set for the source to be considered
 *               configured (e.g. an OAuth client id + secret pair).
 *   anyOf     – at least one of these vars must be set (e.g. ODPT
 *               accepts several token names).
 *   optional  – vars that improve the source if present but aren't
 *               strictly required (e.g. OpenSky anonymous access works).
 */
const CREDENTIALS = {
  // ── Cyber / OSINT ────────────────────────────────────────────────────
  'shodan-iot': { required: ['SHODAN_API_KEY'] },
  'shodan-japan': { required: ['SHODAN_API_KEY'] },
  // Camera-discovery Shodan channel. Without this entry the iOS source-detail
  // card wouldn't render an "API key" section because the server's
  // getCredentialStatus() returns requiresKey:false for unmapped ids.
  'shodan-cameras-jp': { required: ['SHODAN_API_KEY'] },
  'wifi-networks-wigle':  {
    required: ['WIGLE_API_KEY'],
    probeHeaders: (env) => ({ Authorization: `Basic ${env.WIGLE_API_KEY}` }),
  },
  'wifi-networks-shodan': { required: ['SHODAN_API_KEY'] },
  'wifi-networks-mls':    { required: ['MLS_API_KEY'] },
  'fofa-jp':         { required: ['FOFA_API_KEY'] },
  'greynoise-jp':    { required: ['GREYNOISE_API_KEY'] },
  'quake360-jp':     { required: ['QUAKE_API_KEY'] },
  'grayhat-buckets': { required: ['GRAYHAT_API_KEY'] },
  // GitHub-backed sources — token is optional. The collectors run anonymously
  // when unset, but the GitHub API drops them from 60/h to 5000/h once a
  // token is present, which matters at scale.
  'trickest-cve':    { optional: ['GITHUB_TOKEN'] },
  'poc-in-github':   { optional: ['GITHUB_TOKEN'] },
  'github-leaks-jp': { optional: ['GITHUB_TOKEN'] },
  'ghsa-advisories': { optional: ['GITHUB_TOKEN'] },

  // ── Infrastructure ─────────────────────────────────────────────────────
  'ev-charging': { optional: ['OPENCHARGEMAP_KEY'] },

  // ── Social media ─────────────────────────────────────────────────────
  'twitter-geo': { required: ['TWITTER_BEARER_TOKEN'] },
  'facebook-geo': { required: ['FACEBOOK_ACCESS_TOKEN'] },

  // ── Maritime / AIS ───────────────────────────────────────────────────
  'marine-traffic': { required: ['MARINETRAFFIC_API_KEY'] },
  'vessel-finder': { required: ['VESSELFINDER_API_KEY'] },
  'maritime-ais': { anyOf: ['MARINETRAFFIC_API_KEY', 'VESSELFINDER_API_KEY'] },
  'msil-umishiru': { required: ['UMISHIRU_API_KEY'] },

  // ── Aviation ─────────────────────────────────────────────────────────
  'flight-adsb': { optional: ['OPENSKY_CLIENT_ID', 'OPENSKY_CLIENT_SECRET', 'AERODATABOX_KEY'] },

  // ── Statistics ───────────────────────────────────────────────────────
  'estat-population': { anyOf: ['ESTAT_API_KEY', 'ESTAT_APP_ID'] },
  'resas-industry': { required: ['RESAS_API_KEY'] },
  'resas-tourism': { required: ['RESAS_API_KEY'] },
  'resas-municipality': { required: ['RESAS_API_KEY'] },

  // ── Transport ────────────────────────────────────────────────────────
  'odpt-train': { anyOf: ['ODPT_TOKEN', 'ODPT_CONSUMER_KEY', 'ODPT_CHALLENGE_TOKEN'] },
  'odpt-bus': { anyOf: ['ODPT_TOKEN', 'ODPT_CONSUMER_KEY', 'ODPT_CHALLENGE_TOKEN'] },
  'odpt-station': { anyOf: ['ODPT_TOKEN', 'ODPT_CONSUMER_KEY', 'ODPT_CHALLENGE_TOKEN'] },

  // ── Satellite ────────────────────────────────────────────────────────
  'sentinel-japan': { required: ['SENTINELHUB_CLIENT_ID', 'SENTINELHUB_CLIENT_SECRET'] },
  // USGS M2M token raises the daily quota; collector falls back to the
  // public-tier endpoint when unset, so it's optional rather than required.
  'satellite-imagery': { optional: ['USGS_M2M_TOKEN'] },

  // ── Marketplace / Tourism ────────────────────────────────────────────
  'tabelog-restaurants': { required: ['HOTPEPPER_API_KEY'] },
  'google-my-maps': { required: ['GOOGLE_MYMAPS_IDS'] },

  // ── Infrastructure / Datasets ────────────────────────────────────────
  'cell-towers': { required: ['OPENCELLID_KEY'] },
  'mlit-n02-stations': { optional: ['MLIT_N02_GEOJSON_URL'] },

  // ── Disclosure / filings ─────────────────────────────────────────────
  'edinet-filings': { required: ['EDINET_API_KEY'] },

  // ── Social ───────────────────────────────────────────────────────────
  'misskey-timeline': { required: ['MISSKEY_TOKEN'] },

  // ── Cameras (cameraDiscovery aggregator channel) ─────────────────────
  // Read directly by `fromWindy()` in collectors/cameraDiscovery.js. The
  // cameraDiscovery collector itself is a single source row but it pulls
  // from many channels — only Windy needs a key, so we attach the env-var
  // requirement to the discovery aggregator's source registry entry.
  'windy-webcams': {
    required: ['WINDY_API_KEY'],
    probeHeaders: (env) => ({ 'x-windy-api-key': env.WINDY_API_KEY }),
  },
};

import { getEnv } from './credentials.js';

function listVars(entry) {
  return [
    ...(entry.required || []),
    ...(entry.anyOf || []),
    ...(entry.optional || []),
  ];
}

/**
 * Read a credential value through the tenant-aware resolver.
 *   tenantId = null → platform default (process.env) only. Used by the
 *                     scheduler and any cron-context caller.
 *   tenantId = '<id>' → tenant BYOK first, then platform fallback (subject
 *                       to the per-secret fallback_to_platform flag).
 */
function isSet(name, tenantId = null) {
  const v = getEnv(tenantId, name);
  return typeof v === 'string' && v.trim().length > 0;
}

/**
 * Determine the credential state for a source.
 *
 * Returns an object that's safe to send to the browser: it never includes
 * the actual secret values, only the env-var names and a boolean for each.
 */
export function getCredentialStatus(sourceId, tenantId = null) {
  const entry = CREDENTIALS[sourceId];
  if (!entry) {
    // No credentials needed → consider "configured".
    return {
      requiresKey: false,
      configured: true,
      envVars: [],
      missingVars: [],
    };
  }

  const allVars = listVars(entry);
  const setMap = Object.fromEntries(allVars.map((v) => [v, isSet(v, tenantId)]));

  const required = entry.required || [];
  const anyOf = entry.anyOf || [];

  const missingRequired = required.filter((v) => !setMap[v]);
  const anyOfSatisfied = anyOf.length === 0 || anyOf.some((v) => setMap[v]);

  let configured;
  if (required.length === 0 && anyOf.length === 0) {
    // Only optional vars → always considered configured.
    configured = true;
  } else {
    configured = missingRequired.length === 0 && anyOfSatisfied;
  }

  const missingVars = [
    ...missingRequired,
    ...(anyOfSatisfied ? [] : anyOf),
  ];

  return {
    requiresKey: required.length > 0 || anyOf.length > 0,
    configured,
    envVars: allVars.map((name) => ({
      name,
      set: setMap[name],
      role: required.includes(name)
        ? 'required'
        : anyOf.includes(name)
        ? 'anyOf'
        : 'optional',
    })),
    missingVars,
  };
}

/**
 * Returns the auth headers a probe should send for the given source, reading
 * env vars at call time. Null when the source declares no `probeHeaders` or
 * any of the required vars are missing — in that case the probe runs without
 * auth (and likely returns 401/403, which is itself useful info).
 */
export function getProbeAuthHeaders(sourceId, tenantId = null) {
  const entry = CREDENTIALS[sourceId];
  if (!entry || typeof entry.probeHeaders !== 'function') return null;
  const required = entry.required || [];
  for (const name of required) {
    if (!isSet(name, tenantId)) return null;
  }
  // Build a synthetic env-shaped object containing only the vars this
  // source declares, with tenant-resolved values. The probeHeaders callback
  // receives this in place of process.env so BYOK keys flow through.
  const env = {};
  for (const name of listVars(entry)) {
    env[name] = getEnv(tenantId, name);
  }
  try {
    return entry.probeHeaders(env) || null;
  } catch {
    return null;
  }
}

/**
 * Flatten the CREDENTIALS map to the unique set of env-var names used by any
 * source, with each var's most-restrictive `role` ('required' > 'anyOf' >
 * 'optional'). Used by the /api/keys route so the iOS API-keys tab can list
 * exactly the vars the server actually consumes.
 */
export function getAllKnownVarNames() {
  const ROLE_RANK = { required: 0, anyOf: 1, optional: 2 };
  const byName = new Map();
  for (const entry of Object.values(CREDENTIALS)) {
    for (const role of ['required', 'anyOf', 'optional']) {
      for (const name of entry[role] || []) {
        const existing = byName.get(name);
        if (!existing || ROLE_RANK[role] < ROLE_RANK[existing.role]) {
          byName.set(name, { name, role });
        }
      }
    }
  }
  return [...byName.values()].sort((a, b) => {
    const r = ROLE_RANK[a.role] - ROLE_RANK[b.role];
    return r !== 0 ? r : a.name.localeCompare(b.name);
  });
}

export default CREDENTIALS;
