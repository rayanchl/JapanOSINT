/**
 * Per-dataset field maps for MLIT KSJ GeoJSON collectors.
 *
 * MLIT publishes KSJ datasets (N02 stations, P02 airports, N05 rail history,
 * N07 bus routes, P11 bus stops, C02 ports) in a "raw" form where attributes
 * are coded `<DATASET>_NNN` (e.g. N02_003 = railwayLineName). Most converters
 * also emit friendlier aliases (railwayLineName, line_name, etc.). Each
 * collector previously hand-rolled its own normalizeFeature with a chain of
 * `props.X || props.Y || props.RAW_KEY`. This module centralises those maps.
 *
 * Plus, helpers for the secondary patterns shared across these collectors:
 *   - geometryToPoint(geom):  Point / LineString midpoint / Polygon centroid
 *   - tryKsjUrl(url, code):   fetch + decode + normalize one mirror URL
 *   - createMlitKsjCollector: factory wrapping the try-env-url → mirrors →
 *                             optional OSM fallback envelope assembly.
 */

import { fetchJson } from '../collectors/_liveHelpers.js';

/**
 * Per-dataset config: id prefix, source tag, geometry mode, alias fallback chain.
 *
 * `geometry` controls how a feature's geometry is collapsed to a Point when the
 * source delivers Lines / Polygons:
 *   - 'point':       require Point input, otherwise the feature is dropped.
 *   - 'midpoint':    LineString → middle vertex; Point → unchanged.
 *   - 'centroid':    Polygon ring centroid; Point → unchanged.
 *   - 'preserve':    keep the original geometry (used by N07 routes that need
 *                    the full LineString to render).
 */
export const KSJ_CONFIG = {
  N02: {
    idPrefix: 'MLIT_N02',
    sourceTag: 'mlit_n02',
    geometry: 'midpoint',
    fields: {
      name:             ['stationName', 'station_name', 'name', 'N02_005'],
      line:             ['railwayLineName', 'line_name', 'line', 'N02_003'],
      operator:         ['operatorCompany', 'operator', 'company', 'N02_004'],
      classification:   ['railwayType', 'N02_001'],
      institution_type: ['institutionType', 'N02_002'],
    },
    extras: (norm) => ({ name_ja: norm.name, station_id_field: 'station_id' }),
  },
  P02: {
    idPrefix: 'MLIT_P02',
    sourceTag: 'mlit_p02',
    geometry: 'centroid',
    fields: {
      name:           ['airportName', 'name', 'P02_004', 'P02_001'],
      icao:           ['icao', 'P02_002'],
      iata:           ['iata', 'P02_003'],
      classification: ['airportType', 'P02_005'],
    },
    extras: () => ({ airport_id_field: 'airport_id' }),
  },
  N05: {
    idPrefix: 'MLIT_N05',
    sourceTag: 'mlit_n05',
    geometry: 'midpoint',
    fields: {
      line:           ['lineName', 'N05_002'],
      operator:       ['operatorCompany', 'operator', 'N05_003'],
      classification: ['railwayType', 'N05_001'],
      opened_year:    ['openedYear', 'N05_004'],
      closed_year:    ['closedYear', 'N05_005'],
    },
    // Status is derived (closed_year present → abolished).
    extras: (norm) => ({
      status: norm.closed_year ? 'abolished' : 'active',
      segment_id_field: 'segment_id',
    }),
  },
  N07: {
    idPrefix: 'MLIT_N07',
    sourceTag: 'mlit_n07',
    geometry: 'preserve',
    fields: {
      name:       ['routeName', 'route_name', 'name', 'N07_002'],
      operator:   ['operatorName', 'operator', 'company', 'N07_003'],
      route_type: ['routeType', 'N07_001'],
    },
    extras: (norm) => ({
      route_type: norm.route_type || 'bus',
      route_id_field: 'route_id',
    }),
  },
  P11: {
    idPrefix: 'MLIT_P11',
    sourceTag: 'mlit_p11',
    geometry: 'preserve',
    fields: {
      name:     ['busStopName', 'stop_name', 'name', 'P11_001'],
      operator: ['operator', 'company', 'busOperator', 'P11_002'],
      route:    ['routeName', 'P11_003'],
    },
    extras: (norm) => ({ name_ja: norm.name, stop_id_field: 'stop_id' }),
  },
  C02: {
    idPrefix: 'MLIT_C02',
    sourceTag: 'mlit_c02',
    geometry: 'centroid',
    fields: {
      name:           ['portName', 'name', 'C02_003', 'C02_001'],
      classification: ['portClass', 'classification', 'C02_002'],
      administrator:  ['administrator', 'C02_004'],
    },
    extras: (norm) => ({ name_ja: norm.name, port_id_field: 'port_id' }),
  },
};

// ── Geometry helpers ──────────────────────────────────────────────────────

export function geometryToPoint(geom, mode) {
  if (!geom) return null;
  if (geom.type === 'Point') return geom.coordinates;

  if (mode === 'midpoint') {
    if (geom.type === 'LineString') {
      const c = geom.coordinates;
      return c[Math.floor(c.length / 2)] || null;
    }
    if (geom.type === 'MultiLineString') {
      const first = geom.coordinates?.[0];
      if (!first || !first.length) return null;
      return first[Math.floor(first.length / 2)];
    }
  }

  if (mode === 'centroid') {
    if (geom.type === 'Polygon') {
      const ring = geom.coordinates?.[0];
      if (!ring || !ring.length) return null;
      const [sx, sy] = ring.reduce(([ax, ay], [x, y]) => [ax + x, ay + y], [0, 0]);
      return [sx / ring.length, sy / ring.length];
    }
    if (geom.type === 'MultiPolygon') {
      const ring = geom.coordinates?.[0]?.[0];
      if (!ring || !ring.length) return null;
      const [sx, sy] = ring.reduce(([ax, ay], [x, y]) => [ax + x, ay + y], [0, 0]);
      return [sx / ring.length, sy / ring.length];
    }
  }

  return null;
}

// ── Field-resolver ────────────────────────────────────────────────────────

function pickField(props, aliases) {
  for (const a of aliases) {
    const v = props?.[a];
    if (v != null && v !== '') return v;
  }
  return null;
}

/**
 * Normalize one MLIT KSJ feature to the canonical layer shape.
 * Returns `null` if the feature lacks usable geometry / required fields.
 */
export function normalizeKsjFeature(code, f, i) {
  const cfg = KSJ_CONFIG[code];
  if (!cfg) throw new Error(`mlitNormalizer: unknown dataset ${code}`);
  if (!f || !f.geometry) return null;
  const props = f.properties || {};

  // Resolve all mapped fields via the alias chain.
  const norm = {};
  for (const [key, aliases] of Object.entries(cfg.fields)) {
    norm[key] = pickField(props, aliases);
  }

  // Geometry handling
  let geometry;
  if (cfg.geometry === 'preserve') {
    geometry = f.geometry;
  } else {
    const coords = geometryToPoint(f.geometry, cfg.geometry);
    if (!coords || coords.length < 2) return null;
    geometry = { type: 'Point', coordinates: [coords[0], coords[1]] };
  }

  const idField = (cfg.extras?.(norm)?.segment_id_field
    || cfg.extras?.(norm)?.station_id_field
    || cfg.extras?.(norm)?.airport_id_field
    || cfg.extras?.(norm)?.route_id_field
    || cfg.extras?.(norm)?.stop_id_field
    || cfg.extras?.(norm)?.port_id_field
    || 'feature_id');
  const idValue = `${cfg.idPrefix}_${i}`;

  // Pull "extras" (status, name_ja, etc.) but strip the *_field metadata keys.
  const rawExtras = cfg.extras ? cfg.extras(norm) : {};
  const extras = {};
  for (const [k, v] of Object.entries(rawExtras)) {
    if (k.endsWith('_field')) continue;
    extras[k] = v;
  }

  return {
    type: 'Feature',
    geometry,
    properties: {
      [idField]: idValue,
      ...norm,
      ...extras,
      source: cfg.sourceTag,
    },
  };
}

// ── URL try-helper ────────────────────────────────────────────────────────

export async function tryKsjUrl(url, code, { timeoutMs = 30000, filter = null } = {}) {
  const data = await fetchJson(url, { timeoutMs });
  if (!data) return null;
  const raw = Array.isArray(data) ? data
    : Array.isArray(data.features) ? data.features
    : null;
  if (!raw) return null;
  let features = raw.map((f, i) => normalizeKsjFeature(code, f, i)).filter(Boolean);
  if (filter) features = features.filter(filter);
  return features.length > 0 ? features : null;
}

// ── Collector factory ─────────────────────────────────────────────────────

/**
 * Build a complete MLIT KSJ collector with try-env-url → mirrors → optional
 * OSM fallback → empty envelope.
 *
 * @param {object} opts
 * @param {string} opts.code           - KSJ code: 'N02','P02','N05','N07','P11','C02'
 * @param {string} opts.envKey         - e.g. 'MLIT_N02_GEOJSON_URL'
 * @param {string[]} opts.mirrors      - fallback URLs to try in order
 * @param {() => Promise<any[]|null>} [opts.osmFallback] - optional Overpass fallback
 * @param {string} opts.description
 * @param {string} [opts.envHint]      - shown in `_meta.env_hint` when no live source
 * @param {(f: any) => boolean} [opts.filter]  - optional predicate after normalize
 * @param {string} [opts.osmSourceTag] - source tag emitted on _meta when OSM fallback wins
 * @param {number} [opts.timeoutMs=30000]
 */
export function createMlitKsjCollector({
  code,
  envKey,
  mirrors,
  osmFallback = null,
  osmSourceTag = null,
  description,
  envHint = null,
  filter = null,
  timeoutMs = 30000,
}) {
  const cfg = KSJ_CONFIG[code];
  if (!cfg) throw new Error(`createMlitKsjCollector: unknown dataset ${code}`);

  return async function collect() {
    const envUrl = envKey ? (process.env[envKey] || null) : null;
    const urls = [envUrl, ...mirrors].filter(Boolean);

    let features = null;
    let usedUrl = null;
    let liveSrc = null;

    for (const url of urls) {
      const result = await tryKsjUrl(url, code, { timeoutMs, filter });
      if (result) {
        features = result;
        usedUrl = url;
        liveSrc = `${cfg.sourceTag}_live`;
        break;
      }
    }

    if (!features && osmFallback) {
      features = await osmFallback();
      if (features && features.length) liveSrc = osmSourceTag || 'osm_fallback';
    }

    const live = !!(features && features.length);
    if (!live) features = [];

    return {
      type: 'FeatureCollection',
      features,
      _meta: {
        source: liveSrc || `${cfg.sourceTag}_empty`,
        fetchedAt: new Date().toISOString(),
        recordCount: features.length,
        live,
        source_url: usedUrl,
        env_hint: live ? null : envHint,
        description,
      },
    };
  };
}
