/**
 * Unified Camera Discovery Collector
 *
 * Fans out across every known public-camera discovery channel for Japan and
 * fuses the results into a single deduplicated GeoJSON FeatureCollection:
 *
 *   • OSM Overpass (surveillance nodes, tourism viewpoints with webcam)
 *   • JMA volcano monitoring cameras
 *   • MLIT river / road monitoring cameras
 *   • Shutoko / Hanshin / NEXCO expressway CCTV
 *   • NHK + municipal + YouTube live-stream cameras
 *   • Ski / beach / port operator webcams
 *   • Insecam.org JP listing (scrape)
 *   • Windy / SkylineWebcams / EarthCam / livecam.asia aggregators (scrape)
 *   • DuckDuckGo HTML camera-specific dork results
 *   • Shodan API + Shodan InternetDB (camera-scoped queries)
 *
 * Every feature is tagged with `discovery_channel`, `camera_type`, and a stable
 * `camera_uid` built from lat/lon/url so layers can cross-reference findings.
 *
 * Designed to fail gracefully: each channel has its own timeout and any
 * failure just drops that channel from the fusion — the collector always
 * returns something useful.
 */

import { fetchOverpass, fetchJson, fetchText } from './_liveHelpers.js';
import {
  JMA_VOLCANO_CAMS,
  MLIT_RIVER_CAMS,
  EXPRESSWAY_CAMS,
  BROADCAST_LIVECAMS,
  TOURISM_CAMS,
  OVERPASS_CAMERA_QUERIES,
  CAMERA_DORKS,
  AGGREGATOR_URLS,
  SHODAN_CAMERA_QUERIES,
} from './_cameraSources.js';

const SHODAN_API_KEY = process.env.SHODAN_API_KEY || '';

// Utility: stable UID from lat/lon (~11m precision) + name or URL.
function cameraUid({ lat, lon, name, url }) {
  const lk = (Math.round((lat ?? 0) * 10000) / 10000).toFixed(4);
  const lnk = (Math.round((lon ?? 0) * 10000) / 10000).toFixed(4);
  const tail = (url || name || '').toString().toLowerCase().slice(0, 60);
  return `${lk}:${lnk}:${tail}`;
}

function makeFeature({ lat, lon, name, camera_type, discovery_channel, ...extra }) {
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [lon, lat] },
    properties: {
      camera_uid: cameraUid({ lat, lon, name, url: extra.url }),
      name: name || 'Unknown camera',
      camera_type: camera_type || 'unknown',
      discovery_channel,
      country: 'JP',
      ...extra,
    },
  };
}

// ─── Channel: OSM Overpass ───────────────────────────────────────────────────
async function fromOverpass() {
  const body = OVERPASS_CAMERA_QUERIES.join('');
  const els = await fetchOverpass(body, (el, i, coords) => {
    const t = el.tags || {};
    let camType = 'surveillance';
    if (t.tourism === 'viewpoint') camType = 'viewpoint';
    else if (t['surveillance:type']) camType = t['surveillance:type'];
    else if (t.surveillance) camType = t.surveillance;
    else if (t.webcam || t['contact:webcam']) camType = 'webcam';
    return makeFeature({
      lat: coords[1],
      lon: coords[0],
      name: t.name || t['name:en'] || t.operator || `OSM Camera ${el.id}`,
      camera_type: camType,
      discovery_channel: 'osm_overpass',
      operator: t.operator || null,
      url: t.webcam || t['contact:webcam'] || t.url || null,
      mount: t['camera:mount'] || null,
      direction: t['camera:direction'] || null,
      osm_id: el.id,
    });
  });
  return els || [];
}

// ─── Channel: JMA volcano cams ──────────────────────────────────────────────
function fromJMAVolcano() {
  return JMA_VOLCANO_CAMS.map((v) =>
    makeFeature({
      lat: v.lat,
      lon: v.lon,
      name: v.name,
      camera_type: 'volcano',
      discovery_channel: 'jma_volcano',
      operator: '気象庁',
      url: `https://www.data.jma.go.jp/svd/volcam/data/gazo/${v.vid}.html`,
    }),
  );
}

// ─── Channel: MLIT river cams ───────────────────────────────────────────────
function fromMLITRiver() {
  return MLIT_RIVER_CAMS.map((c) =>
    makeFeature({
      lat: c.lat,
      lon: c.lon,
      name: c.name,
      camera_type: 'river',
      discovery_channel: 'mlit_river',
      operator: c.office,
      url: 'https://www.river.go.jp/',
    }),
  );
}

// ─── Channel: expressway CCTV ───────────────────────────────────────────────
function fromExpressway() {
  return EXPRESSWAY_CAMS.map((c) =>
    makeFeature({
      lat: c.lat,
      lon: c.lon,
      name: c.name,
      camera_type: 'traffic',
      discovery_channel: 'expressway_cctv',
      operator: c.operator,
      url: c.url || null,
    }),
  );
}

// ─── Channel: broadcast / YouTube / municipal livecams ──────────────────────
function fromBroadcast() {
  return BROADCAST_LIVECAMS.map((c) =>
    makeFeature({
      lat: c.lat,
      lon: c.lon,
      name: c.name,
      camera_type: c.type,
      discovery_channel: 'broadcast_livecam',
      url: c.url || null,
    }),
  );
}

// ─── Channel: tourism operator webcams ──────────────────────────────────────
function fromTourism() {
  return TOURISM_CAMS.map((c) =>
    makeFeature({
      lat: c.lat,
      lon: c.lon,
      name: c.name,
      camera_type: c.type,
      discovery_channel: 'tourism_webcam',
    }),
  );
}

// ─── Channel: Insecam scrape ────────────────────────────────────────────────
async function fromInsecam() {
  const html = await fetchText('http://www.insecam.org/en/bycountry/JP/', { timeoutMs: 8000 });
  if (!html) return [];
  // Each <a href="/en/view/XXXXX/"> entry is a public camera with image preview
  const features = [];
  const entryRe = /<a[^>]+href="\/en\/view\/(\d+)\/"[^>]*>[\s\S]*?<img[^>]+src="([^"]+)"/g;
  let m;
  let i = 0;
  while ((m = entryRe.exec(html)) !== null && features.length < 60) {
    i += 1;
    const id = m[1];
    const img = m[2];
    // Insecam does not expose lat/lon in the listing — jitter around Tokyo
    // centroid as a placeholder with the image URL preserved for later geolocation.
    features.push(
      makeFeature({
        lat: 35.68 + ((i * 13) % 100) * 0.002 - 0.1,
        lon: 139.70 + ((i * 17) % 100) * 0.002 - 0.1,
        name: `Insecam #${id}`,
        camera_type: 'insecam',
        discovery_channel: 'insecam_scrape',
        url: `http://www.insecam.org/en/view/${id}/`,
        thumbnail_url: img,
        auth_required: false,
      }),
    );
  }
  return features;
}

// ─── Channel: aggregator directory scrapes (SkylineWebcams, livecam.asia…) ──
async function fromAggregators() {
  const features = [];
  for (const url of AGGREGATOR_URLS) {
    const html = await fetchText(url, { timeoutMs: 7000 });
    if (!html) continue;
    // Generic link extractor limited to a few per aggregator to avoid spam
    const linkRe = /<a[^>]+href="([^"]+)"[^>]*>([^<]{4,120})<\/a>/g;
    let m;
    let count = 0;
    while ((m = linkRe.exec(html)) !== null && count < 8) {
      const href = m[1];
      const label = m[2].replace(/<[^>]+>/g, '').trim();
      if (!/japan|tokyo|osaka|kyoto|fuji|okinawa|hokkaido|sapporo|sendai|nagoya|hiroshima|fukuoka|日本|東京|大阪/i.test(label + href)) continue;
      count += 1;
      features.push(
        makeFeature({
          lat: 36.0 + (features.length % 20) * 0.05,
          lon: 138.0 + (features.length % 20) * 0.08,
          name: label || 'Aggregator webcam',
          camera_type: 'aggregator',
          discovery_channel: `aggregator:${new URL(url).hostname}`,
          url: href.startsWith('http') ? href : new URL(href, url).href,
        }),
      );
    }
  }
  return features;
}

// ─── Channel: DuckDuckGo HTML dork search ───────────────────────────────────
async function fromDorks() {
  const features = [];
  for (const dork of CAMERA_DORKS.slice(0, 10)) {
    const html = await fetchText(
      `https://html.duckduckgo.com/html/?q=${encodeURIComponent(dork)}`,
      { timeoutMs: 6000 },
    );
    if (!html) continue;
    const re = /<a[^>]*class="result__a"[^>]*href="([^"]+)"[^>]*>([^<]*)<\/a>/gi;
    let m;
    let found = 0;
    while ((m = re.exec(html)) !== null && found < 5) {
      let href = m[1];
      try {
        if (href.startsWith('//')) href = 'https:' + href;
        const u = new URL(href.startsWith('http') ? href : 'https://' + href);
        const target = u.searchParams.get('uddg') || u.href;
        const host = new URL(target).hostname;
        if (!host.endsWith('.jp') && !/(windy|earthcam|skyline|webcamtaxi|livecam)/.test(host)) continue;
        found += 1;
        features.push(
          makeFeature({
            lat: 35.68 + (features.length % 15) * 0.01,
            lon: 139.75 + Math.floor(features.length / 15) * 0.01,
            name: m[2].replace(/<[^>]+>/g, '').trim() || host,
            camera_type: 'dork_hit',
            discovery_channel: 'duckduckgo_dork',
            url: target,
            host,
            dork_query: dork,
          }),
        );
      } catch { /* skip malformed */ }
    }
  }
  return features;
}

// ─── Channel: Shodan API (camera-scoped) ────────────────────────────────────
async function fromShodanAPI() {
  if (!SHODAN_API_KEY) return [];
  const features = [];
  for (const q of SHODAN_CAMERA_QUERIES.slice(0, 3)) {
    const data = await fetchJson(
      `https://api.shodan.io/shodan/host/search?key=${SHODAN_API_KEY}&query=${encodeURIComponent(q)}`,
      { timeoutMs: 8000 },
    );
    if (!data || !Array.isArray(data.matches)) continue;
    for (const m of data.matches.slice(0, 20)) {
      const lat = m.location?.latitude;
      const lon = m.location?.longitude;
      if (lat == null || lon == null) continue;
      features.push(
        makeFeature({
          lat,
          lon,
          name: `${m.product || 'Camera'} @ ${m.ip_str}:${m.port}`,
          camera_type: 'ip_camera',
          discovery_channel: 'shodan_api',
          url: `http://${m.ip_str}:${m.port}`,
          ip: m.ip_str,
          port: m.port,
          product: m.product || null,
          org: m.org || null,
          city: m.location?.city || null,
          shodan_query: q,
        }),
      );
    }
  }
  return features;
}

// ─── Fusion + dedup ─────────────────────────────────────────────────────────
function dedupe(features) {
  const seen = new Map();
  for (const f of features) {
    const uid = f.properties.camera_uid;
    if (!seen.has(uid)) {
      seen.set(uid, f);
      continue;
    }
    // Merge discovery_channel list so downstream users can see all detectors
    // that surfaced the same camera.
    const existing = seen.get(uid);
    const prevChannels = existing.properties.discovery_channels
      || [existing.properties.discovery_channel].filter(Boolean);
    const next = new Set([...prevChannels, f.properties.discovery_channel].filter(Boolean));
    existing.properties.discovery_channels = Array.from(next);
    // Preserve thumbnail/url/ip/port opportunistically
    for (const k of ['url', 'thumbnail_url', 'ip', 'port', 'operator', 'product']) {
      if (!existing.properties[k] && f.properties[k]) existing.properties[k] = f.properties[k];
    }
  }
  return Array.from(seen.values());
}

export default async function collectCameraDiscovery() {
  const channels = await Promise.allSettled([
    fromOverpass(),
    fromJMAVolcano(),
    fromMLITRiver(),
    fromExpressway(),
    fromBroadcast(),
    fromTourism(),
    fromInsecam(),
    fromAggregators(),
    fromDorks(),
    fromShodanAPI(),
  ]);

  const channelNames = [
    'osm_overpass',
    'jma_volcano',
    'mlit_river',
    'expressway_cctv',
    'broadcast_livecam',
    'tourism_webcam',
    'insecam_scrape',
    'aggregators',
    'duckduckgo_dork',
    'shodan_api',
  ];

  const perChannelCounts = {};
  const all = [];
  channels.forEach((r, idx) => {
    const name = channelNames[idx];
    if (r.status === 'fulfilled' && Array.isArray(r.value)) {
      perChannelCounts[name] = r.value.length;
      all.push(...r.value);
    } else {
      perChannelCounts[name] = 0;
    }
  });

  const features = dedupe(all);
  const liveChannels = Object.entries(perChannelCounts)
    .filter(([, n]) => n > 0)
    .map(([k]) => k);

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'camera_discovery',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: liveChannels.length > 0,
      live_channels: liveChannels,
      channel_counts: perChannelCounts,
      description:
        'Unified Japan camera discovery: OSM + JMA volcano + MLIT river + expressway + broadcast + tourism + Insecam + aggregators + dorks + Shodan',
    },
    metadata: {},
  };
}
