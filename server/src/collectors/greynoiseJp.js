/**
 * GreyNoise Community — checks a small list of interesting IPs.
 *
 * GreyNoise free community tier is per-IP lookup only; bulk "IPs scanning
 * Japan" queries require a paid plan. This collector therefore polls a
 * short, configurable allowlist of IPs (default: a dozen well-known
 * Japanese university / ISP / cloud ranges) and surfaces per-IP classifier
 * output: noise status, classification (benign/malicious), first/last seen.
 *
 * Auth: GREYNOISE_API_KEY (free signup at https://viz.greynoise.io/).
 *       Community endpoint also tolerates anonymous requests at a lower
 *       rate; we send the key when present.
 */

const BASE = 'https://api.greynoise.io/v3/community';
const TIMEOUT_MS = 10000;

// Override via env GREYNOISE_IPS=ip1,ip2,ip3 at deploy time.
const DEFAULT_IPS = (process.env.GREYNOISE_IPS || [
  '8.8.8.8', '1.1.1.1',        // Control / non-JP
  '133.71.100.50',              // University of Tokyo range sample
  '210.152.11.100',             // IIJ range sample
  '203.104.130.1',              // Sakura Internet sample
].join(',')).split(',').map((s) => s.trim()).filter(Boolean);

async function lookupOne(ip) {
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const key = process.env.GREYNOISE_API_KEY;
    const res = await fetch(`${BASE}/${encodeURIComponent(ip)}`, {
      signal: controller.signal,
      headers: {
        'accept': 'application/json',
        ...(key ? { 'key': key } : {}),
      },
    });
    clearTimeout(timer);
    if (res.status === 429) return { ip, rate_limited: true };
    if (!res.ok) return { ip, error: `HTTP ${res.status}` };
    return await res.json();
  } catch (err) {
    return { ip, error: err?.message || 'fetch failed' };
  }
}

export default async function collectGreynoiseJp() {
  const results = await Promise.all(DEFAULT_IPS.map(lookupOne));

  const features = results.map((r) => ({
    type: 'Feature',
    geometry: null, // GeoJSON-first frameworks would IP-geolocate here
    properties: {
      id: `GN_${r.ip}`,
      ip: r.ip,
      noise: r.noise ?? null,
      classification: r.classification || null,
      name: r.name || null,
      last_seen: r.last_seen || null,
      first_seen: r.first_seen || null,
      rate_limited: r.rate_limited || false,
      error: r.error || null,
      source: 'greynoise_community',
    },
  }));

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'greynoise_community',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      ips_polled: DEFAULT_IPS.length,
      env_hint: 'Set GREYNOISE_API_KEY for higher rate limits; GREYNOISE_IPS=ip1,ip2,... to customise the lookup list',
      description: 'GreyNoise community classification for a curated IP list',
    },
    metadata: {},
  };
}
