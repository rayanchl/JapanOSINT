/**
 * note.com — Japanese long-form publishing platform.
 *
 * No official public API. The site renders trending content via internal
 * endpoints like `/api/v3/categories/` and `/api/v2/notes`. These are
 * undocumented, subject to change, and may 404 without notice.
 *
 * We try a short allowlist of known-working endpoints and degrade quietly
 * on any failure. Non-geospatial; returns text features.
 */

const BASE = 'https://note.com';
const TIMEOUT_MS = 12000;

const CANDIDATE_ENDPOINTS = [
  '/api/v3/hashtags/trending',
  '/api/v2/categories/trending',
];

async function tryEndpoint(path) {
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(`${BASE}${path}`, {
      signal: controller.signal,
      headers: { 'user-agent': 'JapanOSINT/1.0', 'accept': 'application/json' },
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    return await res.json();
  } catch {
    return null;
  }
}

export default async function collectNoteComTrending() {
  let data = null;
  let usedPath = null;
  for (const p of CANDIDATE_ENDPOINTS) {
    const r = await tryEndpoint(p);
    if (r) { data = r; usedPath = p; break; }
  }

  // Shape of `data` is inconsistent across endpoints; a best-effort
  // normalisation pulls whatever title/url pairs we can spot.
  const items = [];
  const walk = (node) => {
    if (!node || typeof node !== 'object') return;
    if (Array.isArray(node)) return node.forEach(walk);
    if (node.name && node.url) items.push({ title: node.name, url: String(node.url), kind: 'hashtag' });
    else if (node.title && node.note_url) items.push({ title: node.title, url: node.note_url, kind: 'note' });
    else if (node.title && node.url) items.push({ title: node.title, url: node.url, kind: 'note' });
    for (const v of Object.values(node)) walk(v);
  };
  if (data) walk(data);

  const features = items.slice(0, 100).map((it, i) => ({
    type: 'Feature',
    geometry: null,
    properties: {
      id: `NOTE_${i + 1}`,
      title: it.title,
      url: it.url,
      kind: it.kind,
      source: 'note_com',
    },
  }));

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: features.length ? 'note_com_live' : 'note_com_unavailable',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      endpoint_used: usedPath,
      caveat: 'Unofficial/undocumented endpoints; likely to break on schema change',
      description: 'note.com trending hashtags/notes (Japanese long-form)',
    },
    metadata: {},
  };
}
