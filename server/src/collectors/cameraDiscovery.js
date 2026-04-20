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
 *   • Shodan API + Shodan InternetDB (camera-scoped queries)
 *
 * Every feature is tagged with `discovery_channel`, `camera_type`, and a stable
 * `camera_uid` built from lat/lon/url so layers can cross-reference findings.
 *
 * Designed to fail gracefully: each channel has its own timeout and any
 * failure just drops that channel from the fusion — the collector always
 * returns something useful.
 */

import { fetchOverpass, fetchOverpassTiled, fetchJson, fetchText } from './_liveHelpers.js';
import {
  JMA_VOLCANO_CAMS,
  MLIT_RIVER_CAMS,
  EXPRESSWAY_CAMS,
  BROADCAST_LIVECAMS,
  TOURISM_CAMS,
  OVERPASS_CAMERA_QUERIES,
  SHODAN_CAMERA_QUERIES,
  EARTHCAM_JP,
  PREFECTURE_CENTROIDS,
  NEW_AGGREGATOR_INDEX,
} from './_cameraSources.js';
import { renderHtml, extractYouTubeEmbed } from '../utils/screenshot.js';
import { geocodeFeatures } from '../utils/cameraGeocode.js';

const SHODAN_API_KEY = process.env.SHODAN_API_KEY || '';

const BROWSER_UA =
  'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36';

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
// Tiled across 12 bboxes: a single nationwide query for every surveillance +
// webcam + viewpoint node in Japan routinely times out at Overpass (tens of
// thousands of elements).
async function fromOverpass() {
  // Rewrite each `(area.jp)` selector to `(bbox)` for tile mode.
  const bodyFn = (bbox) =>
    OVERPASS_CAMERA_QUERIES.map((q) => q.replaceAll('(area.jp)', `(${bbox})`)).join('');
  const els = await fetchOverpassTiled(bodyFn, (el, i, coords) => {
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
  return MLIT_RIVER_CAMS.map((c) => {
    const portalUrl = `https://www.river.go.jp/portal/?region=80&contents=multi&pointLat=${c.lat}&pointLng=${c.lon}`;
    return makeFeature({
      lat: c.lat,
      lon: c.lon,
      name: c.name,
      camera_type: 'river',
      discovery_channel: 'mlit_river',
      operator: c.office,
      url: portalUrl,
    });
  });
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
      thumbnail_url: c.thumbnail || null,
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
      thumbnail_url: c.thumbnail || null,
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

// ─── Channel: SkylineWebcams Japan ──────────────────────────────────────────
// The index sits behind a Funding Choices consent wall and the camera grid is
// lazy-loaded. We render with Chromium, click "Accept all", scroll to force
// lazy-load, then pull out `(fr/)?webcam/japan/.../*.html` anchors (which land
// in the DOM as relative URLs, not rooted paths).
// Skyline uses a proprietary HLS player — no YouTube IDs to upgrade, and
// detail pages don't expose lat/lon, so we centroid-jitter by city slug.
async function fromSkyline() {
  const base = NEW_AGGREGATOR_INDEX.skyline;
  const html = await renderHtml(base, {
    timeoutMs: 30000,
    settleMs: 3000,
    acceptCookies: true,
    scrollPasses: 6,
    userAgent: BROWSER_UA,
  });
  if (!html) return [];

  const linkRe = /href="((?:\/|)?fr\/webcam\/japan\/[^"#?\s]+\.html)"[^>]*>([\s\S]*?)<\/a>/gi;
  const seen = new Map();
  let m;
  while ((m = linkRe.exec(html)) !== null) {
    const path = m[1].startsWith('/') ? m[1] : `/${m[1]}`;
    if (seen.has(path)) continue;
    const label = m[2]
      .replace(/<[^>]+>/g, ' ')
      .replace(/\s+/g, ' ')
      .trim();
    // path looks like /fr/webcam/japan/<region>/<city>/<slug>.html — use the
    // <city> segment for centroid lookup.
    const parts = path.split('/').filter(Boolean);
    const citySlug = (parts[4] || parts[3] || '').replace(/-prefecture$/, '').replace(/-/g, '');
    seen.set(path, { label, citySlug });
  }

  const features = [];
  for (const [path, meta] of seen) {
    const centroid = PREFECTURE_CENTROIDS[meta.citySlug]
      || guessCentroidFromText(meta.label)
      || TOKYO_CENTROID;
    const { lat, lon } = jitterAround(centroid, features.length);
    features.push(
      makeFeature({
        lat, lon,
        name: meta.label || 'SkylineWebcams feed',
        camera_type: 'aggregator_skyline',
        discovery_channel: 'skylinewebcams',
        url: `https://www.skylinewebcams.com${path}`,
        city: meta.citySlug,
      }),
    );
  }
  // Detail pages server-render a YouTube iframe for most Japan feeds, so
  // upgrade url → youtube.com/watch?v=<id> when possible. The popup
  // (MapPopup.jsx) auto-embeds any YouTube URL as an inline player.
  await upgradeYouTubeStreamUrls(features, 4);
  return features;
}

// YT-id extraction via plain HTTP — aggregator detail pages usually inline
// the video in server-rendered HTML (youtube/youtube-nocookie iframe src or a
// JSON-LD VideoObject.embedUrl). Much cheaper than spinning up Chromium.
const YT_ID_RE = /(?:youtube(?:-nocookie)?\.com\/(?:embed\/|watch\?[^"'\s]*?v=)|youtu\.be\/)([\w-]{11})/;

async function extractYouTubeIdFast(url) {
  try {
    const html = await fetchText(url, {
      timeoutMs: 6000,
      headers: { 'User-Agent': BROWSER_UA },
    });
    if (!html) return null;
    const m = html.match(YT_ID_RE);
    return m ? m[1] : null;
  } catch {
    return null;
  }
}

// Resolve each feature's `url` to a YouTube watch link when the source page
// embeds a YouTube live stream. Runs plain fetch first (instant, no Chromium
// contention); falls back to Chromium only when the cheap path misses.
async function upgradeYouTubeStreamUrls(features, concurrency = 6) {
  const queue = features.slice();
  async function worker() {
    while (queue.length) {
      const f = queue.shift();
      const url = f.properties?.url;
      if (!url) continue;
      try {
        let ytId = await extractYouTubeIdFast(url);
        if (!ytId) {
          // Page probably renders the iframe via JS — pay the Chromium cost.
          ytId = await extractYouTubeEmbed(url);
        }
        if (ytId) {
          f.properties.original_page_url = url;
          f.properties.url = `https://www.youtube.com/watch?v=${ytId}`;
          f.properties.youtube_id = ytId;
          // Canonicalize camera_uid on the YouTube video id so two aggregators
          // pointing at the same stream converge to the SAME row. The original
          // uid (built from the aggregator URL at makeFeature time) would
          // otherwise let the DB upsert create a fresh row per aggregator.
          f.properties.camera_uid = `yt:${ytId}`;
        }
      } catch { /* skip */ }
    }
  }
  await Promise.all(Array.from({ length: concurrency }, worker));
}

// ─── Channel: EarthCam curated JP feeds ─────────────────────────────────────
function fromEarthCam() {
  return EARTHCAM_JP.map((c) =>
    makeFeature({
      lat: c.lat,
      lon: c.lon,
      name: c.name,
      camera_type: 'aggregator_earthcam',
      discovery_channel: 'earthcam',
      url: c.url,
      city: c.city,
    }),
  );
}

// ─── Channel: Insecam scrape ────────────────────────────────────────────────
// Japan has ~60 pages × 6 cams/page (~360 cams total). Each card's <a> tag
// carries `title="Live camera in Japan, <City>"` and the detail page exposes
// real Latitude/Longitude fields (insecam's own coarse geolocation — usually
// city-centroid-accurate). We fetch every detail page with bounded concurrency
// so markers land in the right region instead of jittered around Tokyo, and
// memo coords across collector runs so repeat calls only pay for new cams.
const INSECAM_COORD_CACHE = new Map();

async function fromInsecam() {
  const BASE = 'http://www.insecam.org/en/bycountry/JP/';
  const MAX_PAGES = 60;
  const LIST_CONCURRENCY = 6;
  const DETAIL_CONCURRENCY = 10;

  const firstHtml = await fetchText(BASE, { timeoutMs: 8000 });
  if (!firstHtml) return [];
  const pageMatch = firstHtml.match(/pagenavigator\("\?page=",\s*(\d+)/);
  const totalPages = Math.min(
    pageMatch ? parseInt(pageMatch[1], 10) || 1 : 1,
    MAX_PAGES,
  );

  const pageQueue = [];
  for (let p = 2; p <= totalPages; p++) pageQueue.push(p);
  const htmlByPage = new Map([[1, firstHtml]]);
  await Promise.all(Array.from({ length: LIST_CONCURRENCY }, async () => {
    while (pageQueue.length) {
      const p = pageQueue.shift();
      const html = await fetchText(`${BASE}?page=${p}`, { timeoutMs: 8000 });
      if (html) htmlByPage.set(p, html);
    }
  }));

  // First pass: extract every card from every page.
  const cards = [];
  const seen = new Set();
  const entryRe = /<a[^>]+href="\/en\/view\/(\d+)\/"[^>]+title="Live camera in Japan,\s*([^"]+)"[^>]*>[\s\S]*?<img[^>]+src="([^"]+)"/g;
  for (const html of htmlByPage.values()) {
    entryRe.lastIndex = 0;
    let m;
    while ((m = entryRe.exec(html)) !== null) {
      const id = m[1];
      if (seen.has(id)) continue;
      seen.add(id);
      cards.push({ id, city: m[2].trim(), img: m[3].replace(/&amp;/g, '&') });
    }
  }

  // Second pass: resolve real coords from detail pages (memoized).
  const detailQueue = cards.filter((c) => !INSECAM_COORD_CACHE.has(c.id));
  async function resolveDetail(card) {
    const html = await fetchText(
      `http://www.insecam.org/en/view/${card.id}/`,
      { timeoutMs: 6000 },
    );
    if (!html) return;
    const latM = html.match(/Latitude:[\s\S]{0,200}?([\-\d]+\.\d+)/);
    const lonM = html.match(/Longitude:[\s\S]{0,200}?([\-\d]+\.\d+)/);
    if (latM && lonM) {
      const lat = parseFloat(latM[1]);
      const lon = parseFloat(lonM[1]);
      if (Number.isFinite(lat) && Number.isFinite(lon)) {
        INSECAM_COORD_CACHE.set(card.id, { lat, lon });
      }
    }
  }
  await Promise.all(Array.from({ length: DETAIL_CONCURRENCY }, async () => {
    while (detailQueue.length) {
      const card = detailQueue.shift();
      try { await resolveDetail(card); } catch { /* skip */ }
    }
  }));

  // Final pass: build features, preferring real coords, falling back to
  // city-centroid + jitter when insecam didn't return usable numbers.
  const features = [];
  for (const card of cards) {
    const real = INSECAM_COORD_CACHE.get(card.id);
    let lat, lon;
    if (real) {
      // Tiny jitter so cams sharing a city centroid don't fully overlap.
      const dx = ((parseInt(card.id, 10) * 17) % 97) * 0.0003 - 0.015;
      const dy = ((parseInt(card.id, 10) * 13) % 89) * 0.0003 - 0.013;
      lat = real.lat + dy;
      lon = real.lon + dx;
    } else {
      const centroid = guessCentroidFromText(card.city) || TOKYO_CENTROID;
      const j = jitterAround(centroid, features.length);
      lat = j.lat; lon = j.lon;
    }
    features.push(
      makeFeature({
        lat, lon,
        name: `Insecam ${card.city} #${card.id}`,
        camera_type: 'insecam',
        discovery_channel: 'insecam_scrape',
        url: `http://www.insecam.org/en/view/${card.id}/`,
        thumbnail_url: card.img,
        city: card.city,
        coord_source: real ? 'insecam_detail' : 'city_centroid',
        auth_required: false,
      }),
    );
  }
  return features;
}

// ─── Channel: Shodan API (camera-scoped) ────────────────────────────────────
async function fromShodanAPI() {
  if (!SHODAN_API_KEY) return [];
  const query = 'country:JP';
  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 10000);
    const res = await fetch(
      `https://api.shodan.io/shodan/host/search?key=${SHODAN_API_KEY}&query=${encodeURIComponent(query)}`,
      { signal: controller.signal },
    );
    clearTimeout(timeout);
    if (!res.ok) return [];
    const data = await res.json();
    if (!Array.isArray(data.matches)) return [];
    const features = [];
    for (const m of data.matches) {
      const lat = m.location?.latitude;
      const lon = m.location?.longitude;
      if (!Number.isFinite(lat) || !Number.isFinite(lon)) continue;
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
          banner: (m.data || '').substring(0, 200),
          shodan_query: query,
        }),
      );
    }
    return features;
  } catch {
    return [];
  }
}

// ─── Helpers shared by the new aggregator channels ─────────────────────────
const TOKYO_CENTROID = { lat: 35.6762, lon: 139.6503 };

function guessCentroidFromText(text) {
  if (!text) return null;
  const s = text.toLowerCase();
  for (const [key, coords] of Object.entries(PREFECTURE_CENTROIDS)) {
    if (s.includes(key.replace(/_city$/, ''))) return coords;
  }
  return null;
}

function jitterAround({ lat, lon }, idx) {
  // Small deterministic jitter so stacked markers don't fully overlap.
  const dx = ((idx * 17) % 31) * 0.003 - 0.045;
  const dy = ((idx * 13) % 29) * 0.003 - 0.04;
  return { lat: lat + dy, lon: lon + dx };
}

function absUrl(href, base) {
  try { return new URL(href, base).href; } catch { return null; }
}

// ─── Channel: webcamtaxi.com Japan listing ──────────────────────────────────
async function fromWebcamTaxi() {
  const base = NEW_AGGREGATOR_INDEX.webcamtaxi;
  // Site is Cloudflare-protected — plain fetch 403s. Playwright with a real
  // User-Agent passes the challenge.
  const html = await renderHtml(base, {
    timeoutMs: 25000,
    settleMs: 3000,
    userAgent: BROWSER_UA,
  });
  if (!html) return [];
  const features = [];
  const re = /<a[^>]+href="(\/en\/japan\/([a-z-]+)\/[^"]+\.html)"[^>]*>([^<]{3,120})<\/a>/gi;
  let m;
  while ((m = re.exec(html)) !== null && features.length < 60) {
    const href = m[1];
    const prefSlug = m[2].replace(/-/g, '');
    const label = m[3].replace(/<[^>]+>/g, '').trim();
    const centroid = PREFECTURE_CENTROIDS[prefSlug]
      || guessCentroidFromText(label)
      || TOKYO_CENTROID;
    const { lat, lon } = jitterAround(centroid, features.length);
    features.push(
      makeFeature({
        lat, lon,
        name: label || 'Webcamtaxi feed',
        camera_type: 'aggregator_webcamtaxi',
        discovery_channel: 'webcamtaxi',
        url: absUrl(href, base),
        city: prefSlug,
      }),
    );
  }
  await geocodeFeatures(features);
  return features;
}

// ─── Channel: geocam.ru Japan listing ───────────────────────────────────────
// Hrefs use /en/online/<slug>/. ~19 JP cams, ~10/page.
async function fromGeocam() {
  const base = NEW_AGGREGATOR_INDEX.geocam;
  const pageUrls = [base, `${base}?page=2`, `${base}?page=3`];
  const pageHtmls = await Promise.all(pageUrls.map((u) =>
    fetchText(u, { timeoutMs: 8000, headers: { 'User-Agent': BROWSER_UA } }),
  ));
  const html = pageHtmls.filter(Boolean).join('\n');
  if (!html) return [];

  const features = [];
  const seenHref = new Set();
  const re = /<a[^>]+href="(\/en\/online\/[a-z0-9-]+\/?)"[^>]*>([\s\S]*?)<\/a>/gi;
  let m;
  while ((m = re.exec(html)) !== null && features.length < 80) {
    const href = m[1];
    if (seenHref.has(href)) continue;
    seenHref.add(href);
    const label = m[2].replace(/<[^>]+>/g, '').replace(/\s+/g, ' ').trim();
    const centroid = guessCentroidFromText(label) || TOKYO_CENTROID;
    const { lat, lon } = jitterAround(centroid, features.length);
    features.push(
      makeFeature({
        lat, lon,
        name: label || 'Geocam feed',
        camera_type: 'aggregator_geocam',
        discovery_channel: 'geocam',
        url: absUrl(href, base),
      }),
    );
  }
  return features;
}

// ─── Channel: worldcams.tv Japan listing ────────────────────────────────────
// The /japan/ index lists city directories (e.g., /japan/tokyo/). Individual
// cameras live one level deeper at /japan/<city>/<slug> and that's what we
// want to drop onto the map. Fetch each city directory in parallel and
// collect the real cam anchors.
async function fromWorldcams() {
  const base = NEW_AGGREGATOR_INDEX.worldcams;
  // Index is paginated (4 pages). Each page carries the actual camera anchors
  // `/japan/<city>/<slug>` directly — no need to descend into city directories.
  const pageUrls = [base, `${base}?page=2`, `${base}?page=3`, `${base}?page=4`];
  const pageHtmls = await Promise.all(pageUrls.map((u) =>
    fetchText(u, { timeoutMs: 8000, headers: { 'User-Agent': BROWSER_UA } }),
  ));
  const combined = pageHtmls.filter(Boolean).join('\n');
  if (!combined) return [];

  const camRe = /<a[^>]+href="(\/japan\/([a-z0-9-]+)\/([a-z0-9-]+))"[^>]*>([\s\S]*?)<\/a>/gi;
  const seenHref = new Set();
  const cams = [];
  let m;
  while ((m = camRe.exec(combined)) !== null) {
    const [, href, city, , rawLabel] = m;
    if (seenHref.has(href)) continue;
    seenHref.add(href);
    const label = rawLabel.replace(/<[^>]+>/g, '').replace(/\s+/g, ' ').trim();
    cams.push({ url: absUrl(href, base), name: label || city, city });
  }

  const features = [];
  for (const cam of cams) {
    if (features.length >= 300) break;
    const centroid = PREFECTURE_CENTROIDS[cam.city]
      || guessCentroidFromText(cam.name)
      || TOKYO_CENTROID;
    const { lat, lon } = jitterAround(centroid, features.length);
    features.push(
      makeFeature({
        lat, lon,
        name: cam.name,
        camera_type: 'aggregator_worldcams',
        discovery_channel: 'worldcams',
        url: cam.url,
        city: cam.city,
      }),
    );
  }
  await upgradeYouTubeStreamUrls(features, 4);
  return features;
}

// ─── Channel: webcamera24.com Japan listing ─────────────────────────────────
// Paginated over ~6 pages, hrefs use /fr/camera/japan/<slug>/.
async function fromWebcamera24() {
  const base = NEW_AGGREGATOR_INDEX.webcamera24;
  const pageUrls = [base, ...[2, 3, 4, 5, 6].map((p) => `${base}?page=${p}`)];
  const pageHtmls = await Promise.all(pageUrls.map((u) =>
    fetchText(u, { timeoutMs: 8000, headers: { 'User-Agent': BROWSER_UA } }),
  ));
  const html = pageHtmls.filter(Boolean).join('\n');
  if (!html) return [];

  const features = [];
  const seenHref = new Set();
  const re = /<a[^>]+href="(\/fr\/camera\/japan\/[a-z0-9-]+\/?)"[^>]*>([\s\S]*?)<\/a>/gi;
  let m;
  while ((m = re.exec(html)) !== null && features.length < 200) {
    const href = m[1];
    if (seenHref.has(href)) continue;
    seenHref.add(href);
    const label = m[2].replace(/<[^>]+>/g, '').replace(/\s+/g, ' ').trim();
    const centroid = guessCentroidFromText(label) || TOKYO_CENTROID;
    const { lat, lon } = jitterAround(centroid, features.length);
    features.push(
      makeFeature({
        lat, lon,
        name: label || 'Webcamera24 feed',
        camera_type: 'aggregator_webcamera24',
        discovery_channel: 'webcamera24',
        url: absUrl(href, base),
      }),
    );
  }
  // Detail pages iframe a youtube(-nocookie).com embed — resolve the real
  // video ID so the map popup can play the stream inline.
  await upgradeYouTubeStreamUrls(features, 4);
  await geocodeFeatures(features);
  return features;
}

// ─── Channel: camstreamer.com live search (Japan) ───────────────────────────
async function fromCamstreamer() {
  const base = NEW_AGGREGATOR_INDEX.camstreamer;
  let html = await fetchText(base, {
    timeoutMs: 10000,
    headers: { 'User-Agent': BROWSER_UA },
  });
  // Server-rendered HTML is expected; Chromium fallback only if the plain
  // response comes back empty or clearly shell-only.
  const hasEntries = html && /\/live\/stream\//i.test(html);
  if (!hasEntries) {
    // Decline cookies — accepting the banner was loading a mixed/global feed
    // (e.g. Santa Claus Village, Finland) rather than the country=Japan filter.
    html = await renderHtml(base, {
      timeoutMs: 20000,
      settleMs: 3500,
      acceptCookies: false,
      userAgent: BROWSER_UA,
    });
  }
  if (!html) return [];

  const features = [];

  // Try to parse embedded JSON payloads first (Next.js / nuxt).
  const nextDataMatch = html.match(/<script id="__NEXT_DATA__"[^>]*>([\s\S]*?)<\/script>/);
  if (nextDataMatch) {
    try {
      const data = JSON.parse(nextDataMatch[1]);
      const cams = findCamsDeep(data);
      for (const c of cams.slice(0, 120)) {
        if (!features.length || !features.some((f) => f.properties.url === c.url)) {
          const centroid = (c.lat != null && c.lon != null)
            ? { lat: c.lat, lon: c.lon }
            : (guessCentroidFromText(c.name) || TOKYO_CENTROID);
          const { lat, lon } = (c.lat != null && c.lon != null)
            ? centroid
            : jitterAround(centroid, features.length);
          features.push(
            makeFeature({
              lat, lon,
              name: c.name || 'Camstreamer feed',
              camera_type: 'aggregator_camstreamer',
              discovery_channel: 'camstreamer',
              url: c.url,
            }),
          );
        }
      }
    } catch { /* fall through to HTML scrape */ }
  }

  // Fallback: anchor scrape — hrefs look like /live/stream/<id>-<slug>.
  if (features.length === 0) {
    const re = /<a[^>]+href="(\/live\/stream\/\d+-[a-z0-9-]+)"[^>]*>([\s\S]*?)<\/a>/gi;
    const seenHref = new Set();
    let m;
    while ((m = re.exec(html)) !== null && features.length < 200) {
      const href = m[1];
      if (seenHref.has(href)) continue;
      seenHref.add(href);
      const label = m[2].replace(/<[^>]+>/g, '').replace(/\s+/g, ' ').trim();
      if (!label || /search|signin|login|register/i.test(href)) continue;
      const centroid = guessCentroidFromText(label) || TOKYO_CENTROID;
      const { lat, lon } = jitterAround(centroid, features.length);
      features.push(
        makeFeature({
          lat, lon,
          name: label,
          camera_type: 'aggregator_camstreamer',
          discovery_channel: 'camstreamer',
          url: absUrl(href, base),
        }),
      );
    }
  }
  return features;
}

// Walk an arbitrary JSON tree and pull out objects that look like a camera.
function findCamsDeep(node, out = []) {
  if (!node || typeof node !== 'object') return out;
  if (Array.isArray(node)) {
    for (const v of node) findCamsDeep(v, out);
    return out;
  }
  const name = node.name || node.title || node.cameraName;
  const url = node.url || node.streamUrl || node.liveUrl || node.link;
  const lat = node.lat ?? node.latitude;
  const lon = node.lon ?? node.lng ?? node.longitude;
  if (name && url && typeof url === 'string' && /^(https?:)?\/\//.test(url)) {
    out.push({
      name: String(name),
      url: url.startsWith('//') ? 'https:' + url : url,
      lat: typeof lat === 'number' ? lat : null,
      lon: typeof lon === 'number' ? lon : null,
    });
  }
  for (const v of Object.values(node)) findCamsDeep(v, out);
  return out;
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

/**
 * Run one camera-discovery sweep.
 *
 * @param {object} [opts]
 * @param {(feature, channel) => void} [opts.onCamera]  called once per feature
 *        emitted by any channel, as soon as that channel finishes (or mid-run
 *        for channels that choose to stream — see emit helper below).
 * @param {(channel, summary) => void} [opts.onChannelDone]  channel-level ping
 *        fired when a channel settles (fulfilled or rejected).
 * @param {(channel, error) => void} [opts.onChannelError]
 */
export default async function collectCameraDiscovery(opts = {}) {
  const { onCamera, onChannelDone, onChannelError } = opts;

  const channelDefs = [
    ['osm_overpass',       fromOverpass],
    ['jma_volcano',        fromJMAVolcano],
    ['mlit_river',         fromMLITRiver],
    ['expressway_cctv',    fromExpressway],
    ['broadcast_livecam',  fromBroadcast],
    ['tourism_webcam',     fromTourism],
    ['insecam_scrape',     fromInsecam],
    ['shodan_api',         fromShodanAPI],
    ['skylinewebcams',     fromSkyline],
    ['earthcam',           fromEarthCam],
    ['webcamtaxi',         fromWebcamTaxi],
    ['geocam',             fromGeocam],
    ['worldcams',          fromWorldcams],
    ['webcamera24',        fromWebcamera24],
    ['camstreamer',        fromCamstreamer],
  ];

  // Wrap each channel so (a) no per-channel deadline (it runs as long as it
  // needs, bounded only by the per-request HTTP timeouts inside fetchText /
  // fetchJson / fetchOverpass) and (b) the moment it resolves, every feature
  // it produced is emitted via onCamera. Scraper channels don't currently
  // yield mid-run, so "streaming" is really per-channel-completion streaming —
  // still progressive enough to fill the thread and DB incrementally.
  async function runChannel(name, fn) {
    try {
      const result = await fn();
      const arr = Array.isArray(result) ? result : [];
      if (onCamera) {
        for (const f of arr) {
          try { onCamera(f, name); } catch (e) { /* swallow listener errors */ }
        }
      }
      if (onChannelDone) {
        try { onChannelDone(name, { count: arr.length, ok: true }); } catch {}
      }
      return { name, status: 'fulfilled', value: arr };
    } catch (err) {
      console.error(`[cameraDiscovery] ${name} rejected:`, err?.stack || err);
      if (onChannelError) {
        try { onChannelError(name, err); } catch {}
      }
      if (onChannelDone) {
        try { onChannelDone(name, { count: 0, ok: false, error: err }); } catch {}
      }
      return { name, status: 'rejected', reason: err };
    }
  }

  const channels = await Promise.all(channelDefs.map(([n, fn]) => runChannel(n, fn)));

  const perChannelCounts = {};
  const perChannelErrors = {};
  const all = [];
  for (const r of channels) {
    if (r.status === 'fulfilled') {
      perChannelCounts[r.name] = r.value.length;
      all.push(...r.value);
    } else {
      perChannelCounts[r.name] = 0;
      const e = r.reason;
      perChannelErrors[r.name] = e instanceof Error ? `${e.name}: ${e.message}` : String(e);
    }
  }

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
      channel_errors: perChannelErrors,
      description:
        'Unified Japan camera discovery: OSM + JMA volcano + MLIT river + expressway + broadcast + tourism + Insecam + Shodan + aggregators',
    },
    metadata: {},
  };
}
