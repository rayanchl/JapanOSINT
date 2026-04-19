/**
 * Persistent deduplicated transport store.
 *
 * Mirrors `cameraStore.js`: every station + every track segment ever surfaced
 * by any upstream feed is upserted into one of two tables, indexed by mode
 * (train | subway | bus | ship | port). Repeat sightings union their `sources`
 * array, bump seen_count, and refresh last_seen_at while leaving first_seen_at
 * intact.
 *
 * Returns GeoJSON FeatureCollections (Points + LineStrings interleaved) so the
 * map layer reads them directly with no further translation.
 */

import crypto from 'node:crypto';
import db from './database.js';

// ── Stations ───────────────────────────────────────────────────────────────

const stmtStationGet = db.prepare(
  'SELECT * FROM transport_stations WHERE station_uid = ?',
);

const stmtStationInsert = db.prepare(`
  INSERT INTO transport_stations (
    station_uid, mode, name, operator, line, lat, lon, sources, properties
  ) VALUES (
    @station_uid, @mode, @name, @operator, @line, @lat, @lon, @sources, @properties
  )
`);

const stmtStationUpdate = db.prepare(`
  UPDATE transport_stations SET
    name         = @name,
    operator     = @operator,
    line         = @line,
    sources      = @sources,
    properties   = @properties,
    last_seen_at = datetime('now'),
    seen_count   = seen_count + 1
  WHERE station_uid = @station_uid
`);

const stmtStationsByMode = db.prepare(
  'SELECT * FROM transport_stations WHERE mode = ?',
);

const stmtStationCountByMode = db.prepare(
  "SELECT COUNT(*) c FROM transport_stations WHERE mode = ?",
);

const stmtStationNew24hByMode = db.prepare(
  "SELECT COUNT(*) c FROM transport_stations WHERE mode = ? AND first_seen_at >= datetime('now', '-1 day')",
);

// ── Lines ──────────────────────────────────────────────────────────────────

const stmtLineGet = db.prepare(
  'SELECT * FROM transport_lines WHERE line_uid = ?',
);

const stmtLineInsert = db.prepare(`
  INSERT INTO transport_lines (
    line_uid, mode, name, operator, coordinates, sources, properties
  ) VALUES (
    @line_uid, @mode, @name, @operator, @coordinates, @sources, @properties
  )
`);

const stmtLineUpdate = db.prepare(`
  UPDATE transport_lines SET
    name         = @name,
    operator     = @operator,
    coordinates  = @coordinates,
    sources      = @sources,
    properties   = @properties,
    last_seen_at = datetime('now'),
    seen_count   = seen_count + 1
  WHERE line_uid = @line_uid
`);

const stmtLinesByMode = db.prepare(
  'SELECT * FROM transport_lines WHERE mode = ?',
);

const stmtLineCountByMode = db.prepare(
  "SELECT COUNT(*) c FROM transport_lines WHERE mode = ?",
);

// ── UID derivation ─────────────────────────────────────────────────────────

function lat4(v) { return (Math.round((v ?? 0) * 10000) / 10000).toFixed(4); }

function stationUid({ mode, lat, lon, name }) {
  const tail = (name || '').toString().toLowerCase().slice(0, 40);
  return `${mode}:${lat4(lat)}:${lat4(lon)}:${tail}`;
}

function lineUid({ mode, name, operator, coordinates }) {
  const head = (operator || '').toString().toLowerCase().slice(0, 24);
  const nm = (name || '').toString().toLowerCase().slice(0, 40);
  const hash = crypto
    .createHash('md5')
    .update(JSON.stringify(coordinates || []))
    .digest('hex')
    .slice(0, 12);
  return `${mode}:${head}:${nm}:${hash}`;
}

// ── Row → Feature ──────────────────────────────────────────────────────────

function safeJson(text, fallback) {
  try { return JSON.parse(text); } catch { return fallback; }
}

function rowToStationFeature(row) {
  const properties = safeJson(row.properties, {});
  const sources = safeJson(row.sources, []);
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [row.lon, row.lat] },
    properties: {
      ...properties,
      station_uid: row.station_uid,
      mode: row.mode,
      name: row.name,
      operator: row.operator,
      line: row.line,
      sources,
      first_seen_at: row.first_seen_at,
      last_seen_at: row.last_seen_at,
      seen_count: row.seen_count,
    },
  };
}

function rowToLineFeature(row) {
  const properties = safeJson(row.properties, {});
  const sources = safeJson(row.sources, []);
  const coords = safeJson(row.coordinates, []);
  return {
    type: 'Feature',
    geometry: { type: 'LineString', coordinates: coords },
    properties: {
      ...properties,
      line_uid: row.line_uid,
      mode: row.mode,
      name: row.name,
      operator: row.operator,
      sources,
      first_seen_at: row.first_seen_at,
      last_seen_at: row.last_seen_at,
      seen_count: row.seen_count,
      kind: 'track',
    },
  };
}

// ── Upserts ────────────────────────────────────────────────────────────────

/**
 * Upsert one Point station feature.
 * @param {object} feature  GeoJSON Point feature
 * @param {string} mode     'train' | 'subway' | 'bus' | 'ship' | 'port'
 * @param {string} source   upstream contributor name
 */
export function upsertStation(feature, mode, source) {
  const p = feature?.properties || {};
  const coords = feature?.geometry?.coordinates;
  if (!Array.isArray(coords) || coords.length < 2) return null;
  const [lon, lat] = coords;
  if (!Number.isFinite(lat) || !Number.isFinite(lon)) return null;

  const name = p.name || p.station_name || null;
  const uid = stationUid({ mode, lat, lon, name });
  const existing = stmtStationGet.get(uid);

  if (!existing) {
    const sources = Array.from(new Set([source, ...(p.sources || [])].filter(Boolean)));
    stmtStationInsert.run({
      station_uid: uid,
      mode,
      name,
      operator: p.operator || null,
      line: p.line || p.line_name || null,
      lat,
      lon,
      sources: JSON.stringify(sources),
      properties: JSON.stringify(p),
    });
    return { kind: 'new', feature: rowToStationFeature(stmtStationGet.get(uid)) };
  }

  const prevSources = safeJson(existing.sources, []);
  const nextSources = Array.from(new Set([...prevSources, source, ...(p.sources || [])].filter(Boolean)));
  const prevProps = safeJson(existing.properties, {});
  const mergedProps = { ...p, ...prevProps };

  stmtStationUpdate.run({
    station_uid: uid,
    name: existing.name || name,
    operator: existing.operator || p.operator || null,
    line: existing.line || p.line || p.line_name || null,
    sources: JSON.stringify(nextSources),
    properties: JSON.stringify(mergedProps),
  });
  return { kind: 'updated', feature: rowToStationFeature(stmtStationGet.get(uid)) };
}

/**
 * Upsert one LineString track feature.
 */
export function upsertLine(feature, mode, source) {
  const p = feature?.properties || {};
  const geom = feature?.geometry;
  if (!geom) return null;
  // Accept LineString and MultiLineString (collapse multi to first segment).
  let coordinates = null;
  if (geom.type === 'LineString' && Array.isArray(geom.coordinates) && geom.coordinates.length >= 2) {
    coordinates = geom.coordinates;
  } else if (geom.type === 'MultiLineString' && Array.isArray(geom.coordinates) && geom.coordinates.length > 0) {
    coordinates = geom.coordinates[0];
  }
  if (!coordinates || coordinates.length < 2) return null;

  const name = p.name || p.line_name || p.route_name || null;
  const operator = p.operator || null;
  const uid = lineUid({ mode, name, operator, coordinates });
  const existing = stmtLineGet.get(uid);

  if (!existing) {
    const sources = Array.from(new Set([source, ...(p.sources || [])].filter(Boolean)));
    stmtLineInsert.run({
      line_uid: uid,
      mode,
      name,
      operator,
      coordinates: JSON.stringify(coordinates),
      sources: JSON.stringify(sources),
      properties: JSON.stringify(p),
    });
    return { kind: 'new', feature: rowToLineFeature(stmtLineGet.get(uid)) };
  }

  const prevSources = safeJson(existing.sources, []);
  const nextSources = Array.from(new Set([...prevSources, source, ...(p.sources || [])].filter(Boolean)));
  const prevProps = safeJson(existing.properties, {});
  const mergedProps = { ...p, ...prevProps };

  stmtLineUpdate.run({
    line_uid: uid,
    name: existing.name || name,
    operator: existing.operator || operator,
    coordinates: JSON.stringify(coordinates),
    sources: JSON.stringify(nextSources),
    properties: JSON.stringify(mergedProps),
  });
  return { kind: 'updated', feature: rowToLineFeature(stmtLineGet.get(uid)) };
}

export const upsertStationsTx = db.transaction((features, mode, source) => {
  const out = [];
  for (const f of features) {
    const r = upsertStation(f, mode, source);
    if (r) out.push(r);
  }
  return out;
});

export const upsertLinesTx = db.transaction((features, mode, source) => {
  const out = [];
  for (const f of features) {
    const r = upsertLine(f, mode, source);
    if (r) out.push(r);
  }
  return out;
});

// ── Read API ───────────────────────────────────────────────────────────────

export function getStationsByMode(mode) {
  return stmtStationsByMode.all(mode).map(rowToStationFeature);
}

export function getLinesByMode(mode) {
  return stmtLinesByMode.all(mode).map(rowToLineFeature);
}

/**
 * One FeatureCollection per layer endpoint. Stations + lines interleaved so
 * the client can split them with a `geometry-type` filter.
 */
export function getTransportFeatureCollection(mode) {
  const stations = getStationsByMode(mode);
  const lines = getLinesByMode(mode);
  return {
    type: 'FeatureCollection',
    features: [...lines, ...stations],
    _meta: {
      source: `transport_store:${mode}`,
      stationCount: stations.length,
      lineCount: lines.length,
      recordCount: stations.length + lines.length,
      fetchedAt: new Date().toISOString(),
    },
  };
}

export function transportStats(mode) {
  return {
    stations: stmtStationCountByMode.get(mode).c,
    lines: stmtLineCountByMode.get(mode).c,
    new24h: stmtStationNew24hByMode.get(mode).c,
  };
}

export default {
  upsertStation,
  upsertLine,
  upsertStationsTx,
  upsertLinesTx,
  getStationsByMode,
  getLinesByMode,
  getTransportFeatureCollection,
  transportStats,
};
