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
  'wifi-networks': { required: ['WIGLE_API_KEY'], optional: ['SHODAN_API_KEY', 'MLS_API_KEY'] },

  // ── Infrastructure ─────────────────────────────────────────────────────
  'ev-charging': { optional: ['OPENCHARGEMAP_KEY'] },

  // ── Social media ─────────────────────────────────────────────────────
  'twitter-geo': { required: ['TWITTER_BEARER_TOKEN'] },
  'facebook-geo': { required: ['FACEBOOK_ACCESS_TOKEN'] },

  // ── Maritime / AIS ───────────────────────────────────────────────────
  'marine-traffic': { required: ['MARINETRAFFIC_API_KEY'] },
  'vessel-finder': { required: ['VESSELFINDER_API_KEY'] },
  'maritime-ais': { anyOf: ['MARINETRAFFIC_API_KEY', 'VESSELFINDER_API_KEY'] },

  // ── Aviation ─────────────────────────────────────────────────────────
  'flight-adsb': { optional: ['OPENSKY_CLIENT_ID', 'OPENSKY_CLIENT_SECRET', 'AERODATABOX_KEY'] },
  'opensky-japan': { optional: ['OPENSKY_CLIENT_ID', 'OPENSKY_CLIENT_SECRET'] },

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

  // ── Marketplace / Tourism ────────────────────────────────────────────
  'tabelog-restaurants': { required: ['HOTPEPPER_API_KEY'] },
  'google-my-maps': { required: ['GOOGLE_MYMAPS_IDS'] },

  // ── Infrastructure / Datasets ────────────────────────────────────────
  'cell-towers': { required: ['OPENCELLID_KEY'] },
  'mlit-n02-stations': { optional: ['MLIT_N02_GEOJSON_URL'] },
};

function listVars(entry) {
  return [
    ...(entry.required || []),
    ...(entry.anyOf || []),
    ...(entry.optional || []),
  ];
}

function isSet(name) {
  const v = process.env[name];
  return typeof v === 'string' && v.trim().length > 0;
}

/**
 * Determine the credential state for a source.
 *
 * Returns an object that's safe to send to the browser: it never includes
 * the actual secret values, only the env-var names and a boolean for each.
 */
export function getCredentialStatus(sourceId) {
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
  const setMap = Object.fromEntries(allVars.map((v) => [v, isSet(v)]));

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

export default CREDENTIALS;
