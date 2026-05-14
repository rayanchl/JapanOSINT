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
 *   • NHK + municipal + YouTube live-stream cameras (seeded + Data API)
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
  MANUAL_IP_CAMS,
  WEBCAMENDIRECT_SEED,
} from './_cameraSources.js';
import { renderHtml, extractYouTubeEmbed } from '../utils/screenshot.js';
import { geocodeFeatures } from '../utils/cameraGeocode.js';

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
    else if (t.highway === 'speed_camera' || t.traffic_sign === 'JP:225') {
      // オービス: police automatic speed-enforcement installations.
      // Folded in from the retired surveillanceCameras collector.
      camType = 'speed_camera';
    }
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

// ─── Channel: YouTube Data API live broadcasts ──────────────────────────────
// Requires YOUTUBE_API_KEY. Only emits broadcasts whose owner set
// recordingDetails.location — without coordinates we'd just be jittering blind
// search results around Tokyo, which is noise. Quota: search.list = 100 units,
// videos.list ≈ 3 units; tune the scheduler interval to your daily quota.
async function fromYouTubeLive() {
  const apiKey = process.env.YOUTUBE_API_KEY;
  if (!apiKey) return [];

  const searchUrl =
    'https://www.googleapis.com/youtube/v3/search'
    + '?part=snippet&type=video&eventType=live&regionCode=JP&maxResults=50'
    + `&key=${encodeURIComponent(apiKey)}`;
  const search = await fetchJson(searchUrl);
  const ids = (search?.items || []).map((it) => it?.id?.videoId).filter(Boolean);
  if (ids.length === 0) return [];

  const videosUrl =
    'https://www.googleapis.com/youtube/v3/videos'
    + '?part=snippet,liveStreamingDetails,recordingDetails'
    + `&id=${ids.join(',')}`
    + `&key=${encodeURIComponent(apiKey)}`;
  const videos = await fetchJson(videosUrl);
  const items = videos?.items || [];

  const features = [];
  for (const v of items) {
    const loc = v?.recordingDetails?.location;
    const lat = Number.isFinite(loc?.latitude) ? loc.latitude : null;
    const lon = Number.isFinite(loc?.longitude) ? loc.longitude : null;
    if (lat == null || lon == null) continue;
    const sn = v.snippet || {};
    const ls = v.liveStreamingDetails || {};
    features.push(
      makeFeature({
        lat,
        lon,
        name: sn.title || `YouTube live ${v.id}`,
        camera_type: 'youtube_live',
        discovery_channel: 'youtube_live',
        url: `https://www.youtube.com/watch?v=${v.id}`,
        thumbnail_url: sn.thumbnails?.high?.url || sn.thumbnails?.default?.url || null,
        operator: sn.channelTitle || null,
        youtube_id: v.id,
        youtube_channel: sn.channelId || null,
        concurrent_viewers: ls.concurrentViewers != null ? Number(ls.concurrentViewers) : null,
        actual_start_time: ls.actualStartTime || null,
        location_description: v.recordingDetails.locationDescription || null,
      }),
    );
  }
  return features;
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
  await geocodeFeatures(features);
  return features;
}

// YT-id extraction via plain HTTP — aggregator detail pages usually inline
// the video in server-rendered HTML (youtube/youtube-nocookie iframe src or a
// JSON-LD VideoObject.embedUrl). Much cheaper than spinning up Chromium.
// Negative lookahead `(?!live_stream)` is critical: `live_stream` is exactly
// 11 chars of [\w-], so the bare ID class would happily capture it from a
// channel-live embed URL (`embed/live_stream?channel=UC…`), producing a bogus
// `watch?v=live_stream` rewrite. The channel-live form is handled by the
// dedicated regex below.
const YT_ID_RE = /(?:youtube(?:-nocookie)?\.com\/(?:embed\/(?!live_stream\b)|watch\?[^"'\s]*?v=)|youtu\.be\/)([\w-]{11})/;
const YT_CHANNEL_LIVE_RE = /youtube(?:-nocookie)?\.com\/embed\/live_stream\?[^"'\s]*?channel=(UC[\w-]{22})/i;

// Returns `{ kind: 'video' | 'channel', id }` when the page embeds a YouTube
// stream, or null. Channel-live embeds (e.g. scs.com.ua) point at a YouTube
// channel rather than a static video ID; YouTube resolves the live broadcast
// at iframe-load time. Single fetch covers both regexes.
async function extractYouTubeIdOrChannelFast(url) {
  try {
    const html = await fetchText(url, {
      timeoutMs: 6000,
      headers: { 'User-Agent': BROWSER_UA },
    });
    if (!html) return null;
    const cm = html.match(YT_CHANNEL_LIVE_RE);
    if (cm) return { kind: 'channel', id: cm[1] };
    const vm = html.match(YT_ID_RE);
    if (vm) return { kind: 'video', id: vm[1] };
    return null;
  } catch {
    return null;
  }
}

// Resolve each feature's `url` to a YouTube watch link (or channel-live embed
// URL) when the source page embeds a YouTube stream. Runs plain fetch first
// (instant, no Chromium contention); falls back to Chromium only when the
// cheap path misses (Chromium path only finds video IDs today).
async function upgradeYouTubeStreamUrls(features, concurrency = 6) {
  const queue = features.slice();
  async function worker() {
    while (queue.length) {
      const f = queue.shift();
      const url = f.properties?.url;
      if (!url) continue;
      try {
        let result = await extractYouTubeIdOrChannelFast(url);
        if (!result) {
          const ytId = await extractYouTubeEmbed(url);
          if (ytId) result = { kind: 'video', id: ytId };
        }
        if (!result) continue;
        f.properties.original_page_url = url;
        if (result.kind === 'video') {
          f.properties.url = `https://www.youtube.com/watch?v=${result.id}`;
          f.properties.youtube_id = result.id;
          // Canonicalize camera_uid on the YouTube video id so two aggregators
          // pointing at the same stream converge to the SAME row. The original
          // uid (built from the aggregator URL at makeFeature time) would
          // otherwise let the DB upsert create a fresh row per aggregator.
          f.properties.camera_uid = `yt:${result.id}`;
        } else {
          f.properties.url = `https://www.youtube.com/embed/live_stream?channel=${result.id}`;
          f.properties.youtube_channel = result.id;
          f.properties.camera_uid = `ytc:${result.id}`;
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
        // city_centroid is just a guessed hub + jitter — the LLM enricher
        // can do better with the camera's name, so flag for re-geocoding.
        location_uncertain: real ? 0 : 1,
        auth_required: false,
      }),
    );
  }
  return features;
}

// ─── Channel: Shodan API (camera-scoped) ────────────────────────────────────
async function fromShodanAPI() {
  // Read at call-time so keys set via the iOS Sources panel (which mutates
  // process.env via apiKeysStore.setKey) take effect without a server restart.
  const key = process.env.SHODAN_API_KEY || '';
  if (!key) return [];
  const query = 'country:JP';
  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 10000);
    const res = await fetch(
      `https://api.shodan.io/shodan/host/search?key=${key}&query=${encodeURIComponent(query)}`,
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
  // Site now ships a Cloudflare "Access denied" (error 1005, hard IP block,
  // not a JS challenge) to most datacenter IPs — Playwright cannot bypass.
  // Channel typically returns 0 from cloud hosts; works from residential IPs.
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
  await geocodeFeatures(features);
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
  await geocodeFeatures(features);
  return features;
}

// ─── Channel: worldcam.eu Japan listing ─────────────────────────────────────
// Paginated at /webcams/asia/japan/p/<N>. Each index page carries ~33 cam
// anchors of the form /webcams/asia/japan/<id>-<slug>. Detail pages embed the
// camera's coords in <meta property="og:latitude"> / og:longitude, so we fetch
// each detail with bounded concurrency for precise geolocation — fall back to
// a city-name centroid guess when the detail fetch fails.
//
// The site does NOT proxy YouTube; cams are still-image thumbnails, so the
// post-collection upgradeYouTubeStreamUrls pass won't do anything for this
// channel — we still register the cam URL so OSINT users can click through.
const WORLDCAM_EU_MAX_PAGES = 6;     // ~200 cams — balances coverage vs HTTP cost
const WORLDCAM_EU_DETAIL_CONCURRENCY = 8;

async function fromWorldcamEu() {
  const base = NEW_AGGREGATOR_INDEX.worldcam_eu;
  const pageUrls = [base, ...Array.from({ length: WORLDCAM_EU_MAX_PAGES - 1 }, (_, i) => `${base}/p/${i + 2}`)];
  const pageHtmls = await Promise.all(pageUrls.map((u) =>
    fetchText(u, { timeoutMs: 10000, headers: { 'User-Agent': BROWSER_UA } }).catch(() => null),
  ));
  const combined = pageHtmls.filter(Boolean).join('\n');
  if (!combined) return [];

  // Anchor: /webcams/asia/japan/<id>-<slug> — label is the anchor's inner text.
  const camRe = /<a[^>]+href="(\/webcams\/asia\/japan\/(\d+)-([a-z0-9-]+))"[^>]*>([\s\S]*?)<\/a>/gi;
  const seen = new Map();
  let m;
  while ((m = camRe.exec(combined)) !== null) {
    const [, href, id, slug, rawLabel] = m;
    if (seen.has(id)) continue;
    const label = rawLabel.replace(/<[^>]+>/g, '').replace(/\s+/g, ' ').trim();
    if (!label) continue;
    seen.set(id, { id, slug, href, label });
  }
  if (seen.size === 0) return [];

  const cams = Array.from(seen.values());

  // Fetch detail pages with bounded concurrency to pull og:latitude / og:longitude.
  const ogLat = /<meta\s+property="og:latitude"\s+content="(-?\d+(?:\.\d+)?)"\s*\/?>/i;
  const ogLon = /<meta\s+property="og:longitude"\s+content="(-?\d+(?:\.\d+)?)"\s*\/?>/i;
  const queue = cams.slice();
  async function detailWorker() {
    while (queue.length) {
      const cam = queue.shift();
      try {
        const html = await fetchText(absUrl(cam.href, base), {
          timeoutMs: 8000,
          headers: { 'User-Agent': BROWSER_UA },
        });
        if (!html) continue;
        const la = html.match(ogLat);
        const lo = html.match(ogLon);
        if (la && lo) {
          cam.lat = parseFloat(la[1]);
          cam.lon = parseFloat(lo[1]);
        }
      } catch { /* leave coords undefined, fall back below */ }
    }
  }
  await Promise.all(
    Array.from({ length: WORLDCAM_EU_DETAIL_CONCURRENCY }, detailWorker),
  );

  const features = [];
  for (const cam of cams) {
    let lat, lon;
    if (Number.isFinite(cam.lat) && Number.isFinite(cam.lon)) {
      lat = cam.lat;
      lon = cam.lon;
    } else {
      const centroid = guessCentroidFromText(cam.label) || TOKYO_CENTROID;
      ({ lat, lon } = jitterAround(centroid, features.length));
    }
    features.push(
      makeFeature({
        lat, lon,
        name: cam.label,
        camera_type: 'aggregator_worldcam_eu',
        discovery_channel: 'worldcam_eu',
        url: absUrl(cam.href, base),
      }),
    );
  }
  // upgradeYouTubeStreamUrls is still worth calling in case individual cams
  // turn out to embed YouTube (currently rare on worldcam.eu) — cheap no-op
  // otherwise because extractYouTubeIdFast returns null fast.
  await upgradeYouTubeStreamUrls(features, 4);
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

// ─── Channel: manual IP cams (Shodan-style hand-picks) ─────────────────────
function fromManualIpCams() {
  return MANUAL_IP_CAMS.map((c) => {
    const url = `http://${c.ip}:${c.port}${c.path || '/'}`;
    return makeFeature({
      lat: c.lat,
      lon: c.lon,
      name: c.name,
      camera_type: 'ip_camera',
      discovery_channel: 'manual_ip_seed',
      ip: c.ip,
      port: c.port,
      product: c.product || null,
      operator: c.operator || null,
      url,
    });
  });
}

// ─── Channel: webcamendirect.net explicit URLs (seed) ──────────────────────
function fromWebcamendirectSeed() {
  return WEBCAMENDIRECT_SEED.map((c) =>
    makeFeature({
      lat: c.lat,
      lon: c.lon,
      name: c.name,
      camera_type: 'aggregator_webcamendirect',
      discovery_channel: 'webcamendirect_seed',
      url: c.url,
    }),
  );
}

// ─── Channel: webcamendirect.net /japon listing ────────────────────────────
// Site refactored: /japon is now just a category+country menu. JP cams live
// under /japon/<category> subpages, but those pages mix in non-JP "related"
// cams (e.g. a Costa Rica surf cam on the `/japon/plage` page). Each cam's
// <img alt="<Category> / <Country>"> reveals its country — we filter to alt
// ending in "/ Japon" (FR) or "/ Japan" (EN fallback) so cross-promos don't
// pollute the JP camera fleet. A previous attempt to drop this filter
// (relying only on the URL scope) leaked Costa Rica / other non-JP cams.
async function fromWebcamendirectList() {
  const base = NEW_AGGREGATOR_INDEX.webcamendirect;
  const indexHtml = await fetchText(base, { timeoutMs: 20000, headers: { 'User-Agent': BROWSER_UA } });
  if (!indexHtml) return [];

  // Discover JP category subpages: /japon/<slug>
  const catRe = /href="(\/japon\/[a-z0-9-]+)"/gi;
  const categories = [...new Set([...indexHtml.matchAll(catRe)].map((m) => m[1]))];
  if (categories.length === 0) return [];

  const features = [];
  const seenId = new Set();

  for (const cat of categories) {
    if (features.length >= 200) break;
    const url = absUrl(cat, base);
    const html = await fetchText(url, { timeoutMs: 20000, headers: { 'User-Agent': BROWSER_UA } });
    if (!html) continue;

    // Match cam-tile anchors whose image alt ends in "/ Japon" (or "/ Japan")
    const tileRe = /<a[^>]+href="(https?:\/\/webcamendirect\.net\/webcam\/(\d+)-([a-z0-9-]+)\.html)"[^>]*>\s*<img[^>]+alt="[^"]*\/\s*Jap(?:on|an)"/gi;
    let m;
    while ((m = tileRe.exec(html)) !== null && features.length < 200) {
      const detailUrl = m[1];
      const id = m[2];
      const slug = m[3];
      if (seenId.has(id)) continue;
      seenId.add(id);

      // Try to grab the <h3 class="mv-name"> name near the matching id; fall
      // back to slug-derived title.
      const nameRe = new RegExp(`webcam/${id}-[^"]+\\.html"[^>]*>\\s*<h3[^>]*>([^<]+)</h3>`, 'i');
      const nameMatch = html.match(nameRe);
      const name = (nameMatch && nameMatch[1].trim())
        || slug.split('-').filter((s) => !/^\d+$/.test(s)).join(' ')
              .replace(/\b\w/g, (c) => c.toUpperCase()).trim()
        || `webcamendirect ${id}`;

      const centroid = guessCentroidFromText(slug) || TOKYO_CENTROID;
      const { lat, lon } = jitterAround(centroid, features.length);
      features.push(
        makeFeature({
          lat, lon,
          name,
          camera_type: 'aggregator_webcamendirect',
          discovery_channel: 'webcamendirect_list',
          url: detailUrl,
        }),
      );
    }
  }

  await geocodeFeatures(features);
  return features;
}

// ─── Channel: camscape.com search ?s=japan (pages 1–6) ─────────────────────
// Site renders each result as TWO anchors per slug: an image-card anchor
// (empty text, just a wrapped <img>) and a title-text anchor. The old regex
// required 3+ chars of plain text between <a> and </a>, which dropped every
// image-card anchor — roughly half of all matches. Loosened to match any
// anchor by href and derive the name from inner text if present, otherwise
// from the slug. Dedup by detail URL keeps "two anchors, one cam".
async function fromCamscape() {
  const base = NEW_AGGREGATOR_INDEX.camscape;
  const features = [];
  const seen = new Set();
  for (let page = 1; page <= 6; page++) {
    const url = page === 1 ? base : `https://www.camscape.com/page/${page}/?s=japan`;
    const html = await fetchText(url, { timeoutMs: 20000, headers: { 'User-Agent': BROWSER_UA } });
    if (!html) continue;
    // Capture: 1=detail URL, 2=slug, 3=inner content up to </a> (may be
    // image-only or contain a title). The slug is the deterministic fallback
    // for naming when inner text is empty.
    const re = /<a[^>]+href="(https?:\/\/www\.camscape\.com\/webcam\/([a-z0-9-]+)\/?)"[^>]*>([\s\S]{0,400}?)<\/a>/gi;
    let m;
    while ((m = re.exec(html)) !== null && features.length < 200) {
      const detailUrl = m[1];
      if (seen.has(detailUrl)) continue;
      seen.add(detailUrl);
      const slug = m[2];
      const innerText = m[3]
        .replace(/&#?[a-z0-9]+;/gi, ' ')
        .replace(/<[^>]+>/g, ' ')
        .replace(/\s+/g, ' ')
        .trim();
      const title = innerText.length >= 3
        ? innerText
        : slug.split('-').filter((s) => !/^\d+$/.test(s)).join(' ')
              .replace(/\b\w/g, (c) => c.toUpperCase()).trim()
          || 'camscape Japan feed';
      const centroid = guessCentroidFromText(title) || TOKYO_CENTROID;
      const { lat, lon } = jitterAround(centroid, features.length);
      features.push(
        makeFeature({
          lat, lon,
          name: title,
          camera_type: 'aggregator_camscape',
          discovery_channel: 'camscape',
          url: detailUrl,
        }),
      );
    }
  }
  await geocodeFeatures(features);
  return features;
}

// ─── Channel: tabi.cam /japan/ ─────────────────────────────────────────────
// Site now ships the full JP listing in initial HTML and uses RELATIVE hrefs
// like /japan/<slug>-<id>/. Plain fetch yields ~40 cams without Playwright.
async function fromTabiCam() {
  const base = NEW_AGGREGATOR_INDEX.tabicam;
  const html = await fetchText(base, { timeoutMs: 20000, headers: { 'User-Agent': BROWSER_UA } });
  if (!html) return [];
  const features = [];
  // Accept both relative (/japan/...) and absolute (https://tabi.cam/japan/...)
  const re = /<a[^>]+href="((?:https?:\/\/tabi\.cam)?\/japan\/[a-z0-9-]+\/?)"[^>]*>([\s\S]{0,260}?)<\/a>/gi;
  const seen = new Set();
  let m;
  while ((m = re.exec(html)) !== null && features.length < 200) {
    const href = m[1];
    if (seen.has(href)) continue;
    seen.add(href);
    const slugMatch = href.match(/\/japan\/([a-z0-9-]+)/i);
    const slug = slugMatch ? slugMatch[1] : '';
    // Prefer the anchor's visible text; fall back to slug-derived title.
    const innerText = m[2].replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
    const name = innerText
      || slug.split('-').filter((s) => !/^\d+$/.test(s)).join(' ')
              .replace(/\b\w/g, (c) => c.toUpperCase()).trim()
      || 'tabi.cam feed';
    const centroid = guessCentroidFromText(slug) || TOKYO_CENTROID;
    const { lat, lon } = jitterAround(centroid, features.length);
    features.push(
      makeFeature({
        lat, lon,
        name,
        camera_type: 'aggregator_tabicam',
        discovery_channel: 'tabi_cam',
        url: absUrl(href, base),
      }),
    );
  }
  await geocodeFeatures(features);
  return features;
}

// ─── Channel: webcam.scs.com.ua /en/asia/japan/ (paginated) ────────────────
// Three correctness traps fixed here vs. the previous version:
//   1. The 200-feature cap used to be checked inside the while-loop, which
//      meant page 1 alone could fill it (200 unique anchors) and pages 2-5
//      were fetched-but-discarded. Raised to a per-run budget that allows
//      every page to contribute. The fetched-but-discarded outcome was the
//      "DB has 308 cams but live scrape only returns 200" mismatch.
//   2. `seenLocal` was scoped to a single page, so a cam re-listed on
//      multiple pages (sticky / featured) could double-count. Lifted to
//      `seenHref` outside the page loop for cross-page dedup.
//   3. The regex matched ANY anchor under /en/asia/japan/ — including the
//      sort/filter UI controls (e.g. `/?sort_by=popularity` → "Sort by
//      Popularity"). Exclude hrefs containing `?` or whitelist-known UI
//      segments (`/sort`, `/filter`).
async function fromScsComUa() {
  const baseRoot = NEW_AGGREGATOR_INDEX.scsComUa;
  const features = [];
  const seenHref = new Set();
  const MAX = 1000;  // 5 pages × ~200 anchors; headroom for sticky entries
  for (let page = 1; page <= 5; page++) {
    if (features.length >= MAX) break;
    const url = page === 1 ? baseRoot : `${baseRoot}page-${page}/`;
    const html = await fetchText(url, { timeoutMs: 20000, headers: { 'User-Agent': BROWSER_UA } });
    if (!html) continue;
    const re = /<a[^>]+href="(\/en\/asia\/japan\/[^"]+)"[^>]*>[\s\S]{0,160}?(?:alt|title)="([^"]{3,120})"/gi;
    let m;
    while ((m = re.exec(html)) !== null && features.length < MAX) {
      const href = m[1];
      if (href.includes('/page-')) continue;
      if (href.includes('?')) continue;        // /?sort_by=…
      if (/\/(sort|filter)\b/i.test(href)) continue;
      if (seenHref.has(href)) continue;
      seenHref.add(href);
      const label = m[2].replace(/<[^>]+>/g, '').trim();
      const centroid = guessCentroidFromText(label) || TOKYO_CENTROID;
      const { lat, lon } = jitterAround(centroid, features.length);
      features.push(
        makeFeature({
          lat, lon,
          name: label || 'scs webcam',
          camera_type: 'aggregator_scs_com_ua',
          discovery_channel: 'scs_com_ua',
          url: absUrl(href, baseRoot),
        }),
      );
    }
  }
  // Detail pages embed a YouTube channel-live iframe; resolve the channel ID
  // so the iOS resolver can route to a working iframe URL.
  await upgradeYouTubeStreamUrls(features, 4);
  await geocodeFeatures(features);
  return features;
}

// ─── Channel: Windy.com webcams API ────────────────────────────────────────
// Only source with real lat/lon + embed URLs. Free key from api.windy.com.
async function fromWindy() {
  const key = process.env.WINDY_API_KEY;
  if (!key) {
    console.log('[cameraDiscovery] windy_api: WINDY_API_KEY not set, skipping');
    return [];
  }
  const features = [];
  // Windy v3 caps `limit` at 50 — anything larger returns HTTP 400.
  const pageSize = 50;
  for (let offset = 0; offset < 2000; offset += pageSize) {
    // Windy v3 uses `countries=` (plural) for the country filter. The singular
    // `country=` is silently ignored and returns the worldwide set.
    const url = `https://api.windy.com/webcams/api/v3/webcams?countries=JP&limit=${pageSize}&offset=${offset}&include=location,player,images`;
    let data;
    try {
      data = await fetchJson(url, {
        timeoutMs: 15000,
        headers: { 'x-windy-api-key': key },
      });
    } catch (err) {
      console.warn('[cameraDiscovery] windy_api fetch failed:', err?.message);
      break;
    }
    const items = Array.isArray(data?.webcams) ? data.webcams : [];
    if (items.length === 0) break;
    for (const it of items) {
      const lat = it.location?.latitude;
      const lon = it.location?.longitude;
      if (!Number.isFinite(lat) || !Number.isFinite(lon)) continue;
      const embed = it.player?.live?.embed || it.player?.day?.embed || null;
      const thumb = it.images?.current?.thumbnail || it.images?.current?.preview || null;
      features.push(
        makeFeature({
          lat, lon,
          name: it.title || `Windy webcam ${it.webcamId}`,
          camera_type: 'aggregator_windy',
          discovery_channel: 'windy_api',
          url: embed,
          thumbnail_url: thumb,
          windy_id: it.webcamId,
          city: it.location?.city || null,
        }),
      );
    }
    if (items.length < pageSize) break;
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
/**
 * Explicit sources contract — the 23 channels surfaced as { id, label, run }.
 * Lets the SourcesPanel display per-channel status (count + error) for one
 * collector, and lets callers run a subset of channels (testing, debugging).
 *
 * The default-export function below still consumes the same list internally
 * so collectCameraDiscovery() behaves identically to before.
 */
export const sources = [
  { id: 'osm_overpass',        label: 'OSM Overpass',           run: fromOverpass },
  { id: 'jma_volcano',         label: 'JMA volcano cams',       run: fromJMAVolcano },
  { id: 'mlit_river',          label: 'MLIT river cams',        run: fromMLITRiver },
  { id: 'expressway_cctv',     label: 'Expressway CCTV',        run: fromExpressway },
  { id: 'broadcast_livecam',   label: 'Broadcast livecams',     run: fromBroadcast },
  { id: 'tourism_webcam',      label: 'Tourism webcams',        run: fromTourism },
  { id: 'insecam_scrape',      label: 'Insecam scrape',         run: fromInsecam },
  { id: 'shodan_api',          label: 'Shodan API',             run: fromShodanAPI },
  { id: 'skylinewebcams',      label: 'Skylinewebcams',         run: fromSkyline },
  { id: 'earthcam',            label: 'EarthCam',               run: fromEarthCam },
  { id: 'webcamtaxi',          label: 'WebcamTaxi',             run: fromWebcamTaxi },
  { id: 'geocam',              label: 'Geocam',                 run: fromGeocam },
  { id: 'worldcams',           label: 'Worldcams',              run: fromWorldcams },
  { id: 'webcamera24',         label: 'Webcamera24',            run: fromWebcamera24 },
  { id: 'camstreamer',         label: 'Camstreamer',            run: fromCamstreamer },
  { id: 'worldcam_eu',         label: 'WorldCam.eu',            run: fromWorldcamEu },
  { id: 'manual_ip_seed',      label: 'Manual IP cam seed',     run: fromManualIpCams },
  { id: 'webcamendirect_seed', label: 'Webcam-en-direct seed',  run: fromWebcamendirectSeed },
  { id: 'webcamendirect_list', label: 'Webcam-en-direct list',  run: fromWebcamendirectList },
  { id: 'camscape',            label: 'Camscape',               run: fromCamscape },
  { id: 'tabi_cam',            label: 'Tabi-cam',               run: fromTabiCam },
  { id: 'scs_com_ua',          label: 'scs.com.ua',             run: fromScsComUa },
  { id: 'windy_api',           label: 'Windy API',              run: fromWindy },
  { id: 'youtube_live',        label: 'YouTube Live API',       run: fromYouTubeLive },
];

export default async function collectCameraDiscovery(opts = {}) {
  const { onCamera, onChannelDone, onChannelError } = opts;

  const channelDefs = sources.map((s) => [s.id, s.run]);

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
  };
}
