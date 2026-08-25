// Pure helpers for the server-driven layer catalogue (GET /api/layers, v2).
//
// The server is the source of truth for WHAT a layer is: its id, name,
// category, data_type, modality, kind, member sources and geocoded count.
// The client keeps a presentation table (colour, icon, per-layer paint
// overrides, "hidden because fused into a unified layer") that is merged in
// as decoration, never as the record of truth. `null` from the server means
// "undeclared" and is carried through as null — nothing here guesses a
// modality, a data_type or a category on the server's behalf.

export const MODALITIES = ['point', 'heatmap', 'line', 'polygon', 'raster'];

// Order the panel lists categories in. Anything the server sends that is not
// in this list is appended after it, in first-seen order — a category is
// never dropped for being unknown.
export const CATEGORY_ORDER = [
  'crime', 'safety', 'seismic', 'hazard', 'environment', 'weather', 'ocean',
  'transport', 'health', 'economy', 'statistics', 'government', 'defense',
  'industry', 'infrastructure', 'telecom', 'satellite', 'cameras', 'cyber',
  'tourism', 'culture', 'food', 'agriculture', 'wildlife', 'social',
  'marketplace', 'classifieds', 'commercial', 'geospatial', 'mapping',
  'intelligence',
];

// Presentation colour for a server-only layer (one the client has no
// hand-written style for). Keyed on category so a group reads as a group.
const CATEGORY_COLORS = {
  crime: '#ef5350', safety: '#ff7043', seismic: '#ff4444', hazard: '#ff8a65',
  environment: '#66bb6a', weather: '#4fc3f7', ocean: '#29b6f6',
  transport: '#66bb6a', health: '#ec407a', economy: '#aed581',
  statistics: '#9575cd', government: '#78909c', defense: '#8d6e63',
  industry: '#ffa726', infrastructure: '#90a4ae', telecom: '#26c6da',
  satellite: '#7e57c2', cameras: '#ce93d8', cyber: '#00bcd4',
  tourism: '#26a69a', culture: '#d4e157', food: '#ffca28',
  agriculture: '#9ccc65', wildlife: '#8bc34a', social: '#42a5f5',
  marketplace: '#ff9800', classifieds: '#ff9800', commercial: '#ffb300',
  geospatial: '#5c6bc0', mapping: '#5c6bc0', intelligence: '#ab47bc',
};
const DEFAULT_COLOR = '#9e9e9e';

export function endpointSlug(endpoint) {
  if (!endpoint) return null;
  const seg = String(endpoint).split('?')[0].split('/').filter(Boolean).pop();
  return seg || null;
}

export function normaliseCategory(cat) {
  if (cat == null || cat === '') return 'uncategorised';
  return String(cat).trim().toLowerCase();
}

export function categoryLabel(cat) {
  const s = normaliseCategory(cat);
  return s.charAt(0).toUpperCase() + s.slice(1);
}

function pickModality(v) {
  return MODALITIES.includes(v) ? v : null;
}

/**
 * Merge the server's layer list with the client's presentation table.
 *
 * @param {Array|null} serverLayers  bare array from GET /api/layers (null =
 *                                   not fetched / failed; catalogue is then
 *                                   client-only and says so via `serverId`)
 * @param {Object} clientDefs        the LAYER_DEFINITIONS presentation table
 * @returns {Object} id -> entry
 *
 * Entry shape:
 *   { id, serverId, name, category, modality, data_type, kind, sources,
 *     records_geocoded, temporal, color, icon, endpoint, hidden, sensitive,
 *     subtitle, clientOnly, temporalKey }
 *
 * Client ids (camelCase) are kept as the map-layer id when a client entry
 * matches a server layer on its endpoint slug, so the existing per-layer
 * paint overrides, icon table and popup renderers keep working. Server-only
 * layers use the server id verbatim.
 */
export function buildCatalog(serverLayers, clientDefs) {
  const server = new Map();
  if (Array.isArray(serverLayers)) {
    for (const l of serverLayers) {
      if (l && typeof l.id === 'string' && l.id) server.set(l.id, l);
    }
  }
  const catalog = {};
  const claimed = new Set();

  for (const [id, def] of Object.entries(clientDefs || {})) {
    const slug = endpointSlug(def?.endpoint);
    const srv = slug ? server.get(slug) : null;
    if (srv) claimed.add(srv.id);
    catalog[id] = {
      id,
      serverId: srv ? srv.id : null,
      name: srv?.name || def.name || id,
      category: normaliseCategory(srv ? srv.category : def.category),
      modality: srv ? pickModality(srv.modality) : null,
      data_type: srv && typeof srv.data_type === 'string' ? srv.data_type : null,
      kind: srv ? (srv.kind || null) : 'client',
      sources: srv && Array.isArray(srv.sources) ? srv.sources : [],
      records_geocoded: srv && Number.isFinite(srv.records_geocoded) ? srv.records_geocoded : null,
      temporal: srv?.temporal || (def.temporal ? { field: def.temporalKey || 'year_month' } : null),
      // The temporal WINDOW selector in the panel filters on year_month-style
      // strings and only for layers the client table declared temporal; the
      // server's temporal object names a timestamp field, not a month bucket.
      temporalKey: def.temporal ? (def.temporalKey || 'year_month') : null,
      clientTemporal: !!def.temporal,
      color: def.color || CATEGORY_COLORS[normaliseCategory(srv ? srv.category : def.category)] || DEFAULT_COLOR,
      icon: def.icon || null,
      endpoint: def.endpoint || null,
      hidden: !!def.hidden,
      sensitive: !!def.sensitive,
      subtitle: def.subtitle || null,
      clientOnly: !srv,
    };
  }

  for (const [sid, srv] of server) {
    if (claimed.has(sid)) continue;
    const category = normaliseCategory(srv.category);
    catalog[sid] = {
      id: sid,
      serverId: sid,
      name: srv.name || sid,
      category,
      modality: pickModality(srv.modality),
      data_type: typeof srv.data_type === 'string' ? srv.data_type : null,
      kind: srv.kind || null,
      sources: Array.isArray(srv.sources) ? srv.sources : [],
      records_geocoded: Number.isFinite(srv.records_geocoded) ? srv.records_geocoded : null,
      temporal: srv.temporal || null,
      temporalKey: null,
      clientTemporal: false,
      color: CATEGORY_COLORS[category] || DEFAULT_COLOR,
      icon: null,
      endpoint: null,
      hidden: false,
      sensitive: false,
      subtitle: null,
      clientOnly: false,
    };
  }
  return catalog;
}

/**
 * Group visible catalogue entries by category, in CATEGORY_ORDER first and
 * then every other category the server sent, in first-seen order.
 * Returns [[category, [ids...]], ...]. Never drops a category.
 */
export function groupByCategory(catalog) {
  const groups = new Map();
  for (const cat of CATEGORY_ORDER) groups.set(cat, []);
  for (const [id, entry] of Object.entries(catalog || {})) {
    if (!entry || entry.hidden) continue;
    const cat = normaliseCategory(entry.category);
    if (!groups.has(cat)) groups.set(cat, []);
    groups.get(cat).push(id);
  }
  const out = [];
  for (const [cat, ids] of groups) if (ids.length > 0) out.push([cat, ids]);
  return out;
}

/**
 * Classify a loaded collection for the panel: which of the four states it
 * is in. These are distinct facts and must not collapse into one another —
 * a 404 is "the server has no such layer", an error is "we do not know",
 * empty is "the server answered and holds zero records".
 */
export function classifyLayerData(fc) {
  if (!fc) return { state: 'unloaded' };
  if (fc._notFound || fc._status === 404) return { state: 'not_found', status: 404, message: fc._error || 'HTTP 404' };
  if (fc._error) return { state: 'error', status: fc._status ?? null, message: fc._error };
  const n = Array.isArray(fc.features) ? fc.features.length : 0;
  const meta = fc._meta || {};
  const available = Number.isFinite(meta.records_available) ? meta.records_available : null;
  const truncated = !!meta.truncated;
  if (n === 0) return { state: 'empty', count: 0, available: available ?? 0 };
  return { state: 'loaded', count: n, available: available ?? n, truncated };
}

/** In-band statement of a bounded view: "showing N of M". */
export function truncationNotice(fc) {
  const c = classifyLayerData(fc);
  if (c.state !== 'loaded' || !c.truncated) return null;
  const total = Number.isFinite(c.available) ? c.available.toLocaleString() : 'unknown';
  return `Showing ${c.count.toLocaleString()} of ${total} records`;
}
