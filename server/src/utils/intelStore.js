/**
 * Storage layer for collector output. Polymorphic master table — every
 * collector writes here regardless of shape (camera, station, article, post,
 * AED, sensor reading, …). Geometry is optional; rows without coords are
 * persisted with lat=null and picked up by the LLM geocoder later.
 *
 * Two-layer search: at upsert time we kuromoji-segment title/body/summary
 * into a parallel mirror in `intel_items_fts`; later, an LLM enricher fills
 * in the `keywords` column with semantic tokens (named entities + JA/EN
 * variants). FTS5 indexes both sides, so a single MATCH hits either layer.
 *
 * uid is the primary key — collectors must build a stable id like
 * `<source_id>|<remote_record_key>` so re-runs upsert in place.
 */

import db from './database.js';
import { segmentForFts } from './jpTokenizer.js';
import { defineFtsMirror } from './ftsMirror.js';
import { registerMirror } from './ftsRegistry.js';

export const intelMirror = registerMirror(defineFtsMirror({
  name:             'intel_items_fts',
  baseTable:        'intel_items',
  baseUidColumn:    'uid',
  ftsTable:         'intel_items_fts',
  textColumns:      ['title', 'body', 'summary'],
  keywordsColumn:   'keywords',
  // intel_items.keywords is a JSON array; flatten to space-joined string
  // so the helper can segment it like any other text column.
  keywordsTransform: (raw) => joinKeywordsForFts(raw),
  tokenizer:        'unicode61 remove_diacritics 1',
  segment:          true,
  segmenterVersion: 1,
}));

// Upsert preserves geom_source='llm' across re-runs: if a collector has no
// native coords but the enricher already filled them, don't blow away the
// LLM result on the next refresh. Native coords always win — collectors can
// re-emit a more accurate fix and we honour it.
const upsertBaseStmt = db.prepare(
  `INSERT INTO intel_items
     (uid, source_id, title, body, summary, link, author, language,
      published_at, fetched_at, tags, properties,
      lat, lon, geom_source, geom_at, record_type, sub_source_id, geometry)
   VALUES (@uid, @source_id, @title, @body, @summary, @link, @author, @language,
           @published_at, @fetched_at, @tags, @properties,
           @lat, @lon, @geom_source, @geom_at, @record_type, @sub_source_id, @geometry)
   ON CONFLICT(uid) DO UPDATE SET
     title         = excluded.title,
     body          = excluded.body,
     summary       = excluded.summary,
     link          = excluded.link,
     author        = excluded.author,
     language      = excluded.language,
     published_at  = excluded.published_at,
     fetched_at    = excluded.fetched_at,
     tags          = excluded.tags,
     properties    = excluded.properties,
     lat           = CASE WHEN excluded.lat IS NOT NULL THEN excluded.lat
                          WHEN intel_items.geom_source = 'llm' THEN intel_items.lat
                          ELSE excluded.lat END,
     lon           = CASE WHEN excluded.lon IS NOT NULL THEN excluded.lon
                          WHEN intel_items.geom_source = 'llm' THEN intel_items.lon
                          ELSE excluded.lon END,
     geom_source   = CASE WHEN excluded.geom_source IS NOT NULL THEN excluded.geom_source
                          WHEN intel_items.geom_source = 'llm' THEN intel_items.geom_source
                          ELSE excluded.geom_source END,
     geom_at       = CASE WHEN excluded.geom_source IS NOT NULL THEN excluded.geom_at
                          WHEN intel_items.geom_source = 'llm' THEN intel_items.geom_at
                          ELSE excluded.geom_at END,
     geometry      = COALESCE(excluded.geometry, intel_items.geometry),
     record_type   = COALESCE(excluded.record_type, intel_items.record_type),
     sub_source_id = COALESCE(excluded.sub_source_id, intel_items.sub_source_id)`,
);

// True when lat/lon are real numbers (not null/undefined/NaN/Infinity).
function hasGeo(lat, lon) {
  return Number.isFinite(lat) && Number.isFinite(lon);
}

function toJson(value, fallback) {
  if (value == null) return fallback;
  if (typeof value === 'string') return value;
  try { return JSON.stringify(value); } catch { return fallback; }
}

/**
 * Upsert a batch of items for a source. Pre-segments JA text via kuromoji
 * before writing the FTS mirror; keywords stay empty until the background
 * enricher fills them in.
 *
 * Each item may carry optional spatial fields:
 *   lat, lon          — native coords (numbers); null/missing = ungeocoded
 *   geomSource        — 'native' | 'llm' | 'fallback' | null (defaults to
 *                       'native' when lat/lon present)
 *   geomAt            — ISO timestamp; defaults to fetchedAt when geo present
 *   recordType        — 'camera' | 'station' | 'article' | 'post' | …
 *   subSourceId       — channel-within-collector id (e.g. 'osm-overpass')
 *
 * Returns { count, geocoded, ungeocoded } so the orchestrator can surface
 * per-source counts in the UI.
 */
export async function upsertItems(items, sourceId, fetchedAtIso) {
  if (!Array.isArray(items) || items.length === 0) {
    return { count: 0, geocoded: 0, ungeocoded: 0 };
  }
  const fetchedAt = fetchedAtIso || new Date().toISOString();

  // 1. Kuromoji segmentation outside any txn. First call awaits dictionary
  //    init (~150 ms cold), subsequent calls hit the cached tokenizer.
  const segs = await intelMirror.segmentRows(items.map((it) => ({
    uid:     it?.uid,
    title:   it?.title ?? null,
    body:    it?.body ?? null,
    summary: it?.summary ?? null,
  })));

  // 2. Single sync transaction: base row + FTS mirror.
  const tx = db.transaction((rows, segments) => {
    let n = 0;
    let geo = 0;
    let nogeo = 0;
    for (let i = 0; i < rows.length; i++) {
      const it = rows[i];
      const seg = segments[i];
      if (!it?.uid || !seg) continue;

      const lat = it.lat != null ? Number(it.lat) : null;
      const lon = it.lon != null ? Number(it.lon) : null;
      const geocoded = hasGeo(lat, lon);
      const geomSource = it.geomSource ?? it.geom_source
        ?? (geocoded ? 'native' : null);
      const geomAt = it.geomAt ?? it.geom_at
        ?? (geocoded ? fetchedAt : null);

      upsertBaseStmt.run({
        uid:           String(it.uid),
        source_id:     sourceId,
        title:         it.title ?? null,
        body:          it.body ?? null,
        summary:       it.summary ?? null,
        link:          it.link ?? null,
        author:        it.author ?? null,
        language:      it.language ?? null,
        published_at:  it.published_at ?? null,
        fetched_at:    fetchedAt,
        tags:          toJson(it.tags, '[]'),
        properties:    toJson(it.properties, '{}'),
        lat:           geocoded ? lat : null,
        lon:           geocoded ? lon : null,
        geom_source:   geomSource,
        geom_at:       geomAt,
        record_type:   it.recordType ?? it.record_type ?? null,
        sub_source_id: it.subSourceId ?? it.sub_source_id ?? null,
        geometry:      it.geometry != null ? toJson(it.geometry, null) : null,
      });
      // keywords stay empty here; the enricher fills them via intelMirror.updateKeywords.
      intelMirror.writeOne({ ...seg, keywords: '' });
      n += 1;
      if (geocoded) geo += 1; else nogeo += 1;
    }
    return { count: n, geocoded: geo, ungeocoded: nogeo };
  });
  return tx(items, segs);
}

/**
 * Aggregates per source, used by /api/intel/sources to render Level-1 list.
 * Now also breaks out geocoded / ungeocoded counts so the UI can show
 * "1,234 rows · 1,100 geocoded · 134 awaiting geo" per source.
 */
export function listSources() {
  return db.prepare(
    `SELECT source_id,
            COUNT(*)                                          AS item_count,
            SUM(CASE WHEN lat IS NOT NULL THEN 1 ELSE 0 END)  AS geocoded,
            SUM(CASE WHEN lat IS NULL THEN 1 ELSE 0 END)      AS ungeocoded,
            SUM(CASE WHEN lat IS NULL AND geom_source IS NULL THEN 1 ELSE 0 END)
                                                              AS awaiting_geo,
            MAX(fetched_at)                                   AS last_fetched,
            MAX(COALESCE(published_at, fetched_at))           AS last_published
       FROM intel_items
      GROUP BY source_id`,
  ).all();
}

/**
 * Paginated listing with optional filters. When `q` is set we go through the
 * FTS virtual table (joined back on uid); otherwise a plain index lookup
 * keyed by source_id + published_at. Cursor is opaque base64 of the last
 * row's published_at|uid.
 */
export async function listItems({
  source = null,
  sources = null,
  tag = null,
  since = null,
  until = null,
  lang = null,
  q = null,
  recordType = null,
  subSourceId = null,
  hasGeom = null,        // 'yes' | 'no' | null (no filter)
  limit = 50,
  cursor = null,
} = {}) {
  const safeLimit = Math.min(200, Math.max(1, parseInt(limit, 10) || 50));

  let cursorPub = null, cursorUid = null;
  if (cursor) {
    try {
      const decoded = JSON.parse(Buffer.from(cursor, 'base64url').toString('utf8'));
      cursorPub = decoded.p ?? null;
      cursorUid = decoded.u ?? null;
    } catch { /* ignore bad cursor */ }
  }

  const where = [];
  const params = {};
  if (source) { where.push('intel_items.source_id = @source'); params.source = source; }
  if (Array.isArray(sources) && sources.length > 0) {
    const placeholders = sources.map((_, i) => `@source_${i}`).join(',');
    where.push(`intel_items.source_id IN (${placeholders})`);
    sources.forEach((s, i) => { params[`source_${i}`] = s; });
  }
  if (since) { where.push('COALESCE(intel_items.published_at, intel_items.fetched_at) >= @since'); params.since = since; }
  if (until) { where.push('COALESCE(intel_items.published_at, intel_items.fetched_at) <= @until'); params.until = until; }
  if (lang)  { where.push('intel_items.language = @lang'); params.lang = lang; }
  if (tag)   { where.push("EXISTS (SELECT 1 FROM json_each(intel_items.tags) WHERE json_each.value = @tag)"); params.tag = tag; }
  if (recordType)  { where.push('intel_items.record_type = @recordType'); params.recordType = recordType; }
  if (subSourceId) { where.push('intel_items.sub_source_id = @subSourceId'); params.subSourceId = subSourceId; }
  if (hasGeom === 'yes') where.push('intel_items.lat IS NOT NULL');
  if (hasGeom === 'no')  where.push('intel_items.lat IS NULL');

  let rows;
  let total = null;

  if (q && q.trim()) {
    if (cursorPub != null) {
      where.push('(COALESCE(intel_items.published_at, intel_items.fetched_at) < @cursorPub OR (COALESCE(intel_items.published_at, intel_items.fetched_at) = @cursorPub AND intel_items.uid > @cursorUid))');
      params.cursorPub = cursorPub;
      params.cursorUid = cursorUid;
    }
    const result = await intelMirror.search({
      q,
      where,
      params,
      orderBy: 'COALESCE(intel_items.published_at, intel_items.fetched_at) DESC, intel_items.uid ASC',
      limit: safeLimit,
    });
    rows = result.rows;
  } else {
    if (cursorPub != null) {
      where.push('(COALESCE(intel_items.published_at, intel_items.fetched_at) < @cursorPub OR (COALESCE(intel_items.published_at, intel_items.fetched_at) = @cursorPub AND intel_items.uid > @cursorUid))');
      params.cursorPub = cursorPub;
      params.cursorUid = cursorUid;
    }
    const whereSql = where.length > 0 ? `WHERE ${where.join(' AND ')}` : '';
    rows = db.prepare(
      `SELECT * FROM intel_items
        ${whereSql}
        ORDER BY COALESCE(published_at, fetched_at) DESC, uid ASC
        LIMIT @limit`,
    ).all({ ...params, limit: safeLimit });
  }

  const items = rows.map(rowToItem);

  let nextCursor = null;
  if (items.length === safeLimit) {
    const last = items[items.length - 1];
    const cursorObj = { p: last.published_at ?? last.fetched_at, u: last.uid };
    nextCursor = Buffer.from(JSON.stringify(cursorObj), 'utf8').toString('base64url');
  }

  return { items, nextCursor, total };
}

export function getItem(uid) {
  const row = db.prepare('SELECT * FROM intel_items WHERE uid = ?').get(uid);
  if (!row) return null;
  return rowToItem(row, { full: true });
}

function rowToItem(row, { full = false } = {}) {
  let tags = [];
  try { tags = row.tags ? JSON.parse(row.tags) : []; } catch { /* ignore */ }
  let properties = {};
  try { properties = row.properties ? JSON.parse(row.properties) : {}; } catch { /* ignore */ }
  let keywords = null;
  try { if (row.keywords) keywords = JSON.parse(row.keywords); } catch { /* ignore */ }
  const out = {
    uid: row.uid,
    source_id: row.source_id,
    title: row.title,
    summary: row.summary,
    link: row.link,
    author: row.author,
    language: row.language,
    published_at: row.published_at,
    fetched_at: row.fetched_at,
    tags,
    lat: row.lat ?? null,
    lon: row.lon ?? null,
    geom_source: row.geom_source ?? null,
    geom_at: row.geom_at ?? null,
    record_type: row.record_type ?? null,
    sub_source_id: row.sub_source_id ?? null,
  };
  if (row._excerpt != null) out._excerpt = row._excerpt;
  if (full) {
    out.body = row.body;
    out.properties = properties;
    if (keywords) out.keywords = keywords;
  } else {
    if (Object.keys(properties).length > 0) out.properties = properties;
    if (keywords) out.keywords = keywords;
  }
  return out;
}

/**
 * Synchronous single-row upsert. Uses kuromoji's sync segmenter so the FTS
 * mirror stays in lockstep with the base table — every transport carriage,
 * station, and camera write fans out to intel_items_fts in the same txn.
 * Returns { kind: 'new'|'updated', uid }.
 *
 * Differs from upsertItems(): single-row, no awaitable kuromoji warm-up.
 * Safe because index.js gates startScheduler() on ensureTokenizer(), so
 * every collector that can reach this function runs with a warm tokenizer.
 *
 * Keywords are reset to '' on every write (matches upsertItems' contract);
 * the LLM enricher re-fills them via intelMirror.updateKeywords later.
 */
const stmtMasterExists = db.prepare('SELECT 1 FROM intel_items WHERE uid = ?');
export function upsertItemSync(item, sourceId, fetchedAtIso) {
  if (!item?.uid) return null;
  const fetchedAt = fetchedAtIso || new Date().toISOString();
  const lat = item.lat != null ? Number(item.lat) : null;
  const lon = item.lon != null ? Number(item.lon) : null;
  const geocoded = Number.isFinite(lat) && Number.isFinite(lon);
  const geomSource = item.geomSource ?? item.geom_source ?? (geocoded ? 'native' : null);
  const geomAt = item.geomAt ?? item.geom_at ?? (geocoded ? fetchedAt : null);

  const existed = !!stmtMasterExists.get(String(item.uid));
  const seg = intelMirror.segmentRowSync({
    uid:     item.uid,
    title:   item.title ?? null,
    body:    item.body ?? null,
    summary: item.summary ?? null,
  });
  const tx = db.transaction(() => {
    upsertBaseStmt.run({
      uid:           String(item.uid),
      source_id:     sourceId,
      title:         item.title ?? null,
      body:          item.body ?? null,
      summary:       item.summary ?? null,
      link:          item.link ?? null,
      author:        item.author ?? null,
      language:      item.language ?? null,
      published_at:  item.published_at ?? null,
      fetched_at:    fetchedAt,
      tags:          toJson(item.tags, '[]'),
      properties:    toJson(item.properties, '{}'),
      lat:           geocoded ? lat : null,
      lon:           geocoded ? lon : null,
      geom_source:   geomSource,
      geom_at:       geomAt,
      record_type:   item.recordType ?? item.record_type ?? null,
      sub_source_id: item.subSourceId ?? item.sub_source_id ?? null,
      geometry:      item.geometry != null ? toJson(item.geometry, null) : null,
    });
    if (seg) intelMirror.writeOne({ ...seg, keywords: '' });
  });
  tx();
  return { kind: existed ? 'updated' : 'new', uid: String(item.uid) };
}

/**
 * LLM-geocode write-back into intel_items. Used by cameraStore /
 * socialPostsStore applyGeocodeOk paths so the resolved lat/lon flows into
 * the polymorphic master alongside the typed-table update. Marks geom_source
 * = 'llm' and clears the failure counter.
 *
 * `uid` is the master uid ('<source_id>|<inner_uid>'). When the camera /
 * social store calls this, it builds the uid from its own primary key.
 *
 * Returns 1 if a row was updated, 0 if no matching uid (the row may not
 * have been mirrored yet — silently no-op).
 */
const stmtApplyMasterGeocodeOk = db.prepare(
  `UPDATE intel_items
      SET lat = ?, lon = ?,
          geom_source = 'llm',
          geom_at = datetime('now'),
          geom_failed = 0,
          geometry = '{"type":"Point","coordinates":[' || ? || ',' || ? || ']}'
    WHERE uid = ?`,
);

const stmtApplyMasterGeocodeFail = db.prepare(
  `UPDATE intel_items
      SET geom_failed = COALESCE(geom_failed, 0) + 1,
          geom_source = CASE
            WHEN COALESCE(geom_failed, 0) + 1 >= 5 THEN 'failed'
            ELSE geom_source
          END
    WHERE uid = ?`,
);

export function applyGeocodeToMaster({ uid, lat, lon }) {
  if (!uid || !Number.isFinite(lat) || !Number.isFinite(lon)) return 0;
  return stmtApplyMasterGeocodeOk.run(lat, lon, lon, lat, String(uid)).changes;
}

export function applyGeocodeFailToMaster({ uid }) {
  if (!uid) return 0;
  return stmtApplyMasterGeocodeFail.run(String(uid)).changes;
}

/**
 * Look up a single intel_items row by its master uid (e.g.
 * 'camera-discovery|OSM:n123'). Returns the parsed item or null. Used by the
 * camera-popup endpoint and any other "fetch one record by id" caller after
 * the typed-table read paths get retired.
 */
export function getItemByUid(uid) {
  if (!uid) return null;
  const row = db.prepare('SELECT * FROM intel_items WHERE uid = ?').get(String(uid));
  if (!row) return null;
  return rowToItem(row, { full: true });
}

/**
 * Fetch one record by `<sourceId>|<inner_uid>` where the caller knows the
 * inner uid (e.g. cameraStore.getCameraByUid(camera_uid)). Convenience wrapper
 * over getItemByUid that prepends the sourceId namespace.
 */
export function getItemBySourceUid(sourceId, innerUid) {
  if (!sourceId || !innerUid) return null;
  return getItemByUid(`${sourceId}|${innerUid}`);
}

/**
 * Per-source counts for the SourcesPanel / DatabaseExplorer UI. Returns one
 * row per source_id with total / geocoded / ungeocoded / awaiting LLM geo /
 * last fetched. Cheap (uses indexed source_id + the partial geom indexes).
 */
export function getCounts({ sourceId = null } = {}) {
  const where = sourceId ? 'WHERE source_id = @sourceId' : '';
  const rows = db.prepare(
    `SELECT source_id,
            COUNT(*)                                        AS total,
            SUM(CASE WHEN lat IS NOT NULL THEN 1 ELSE 0 END) AS geocoded,
            SUM(CASE WHEN lat IS NULL THEN 1 ELSE 0 END)     AS ungeocoded,
            SUM(CASE WHEN lat IS NULL AND geom_source IS NULL THEN 1 ELSE 0 END)
                                                            AS awaiting_geo,
            MAX(fetched_at)                                 AS last_fetched
       FROM intel_items
       ${where}
       GROUP BY source_id`,
  ).all(sourceId ? { sourceId } : {});
  return rows;
}

/**
 * Geocoded subset for the map. Returns FeatureCollection-shaped GeoJSON for
 * one source (or one (source, sub_source) channel). The map endpoint wraps
 * this for backwards-compat with existing client code that expects the FC
 * shape from /api/data/:layer.
 *
 * Geometry priority: the stored `geometry` column wins when present (so
 * polygons / lines / multi-shapes from the original collector survive the
 * round-trip); otherwise we build a Point from lat/lon. Rows without lat/lon
 * are excluded — the intel tab is where those live.
 *
 * `includeUngeocoded` lets callers fetch the full result set (used by
 * /api/intel/* paths that expose both halves to the UI). The default keeps
 * map endpoints geocoded-only.
 */
// Whitelist of timestamp columns reachable through the time-window filter.
// Keeps user-supplied layer.temporal.field from leaking into a SQL injection
// surface — callers must pick a column from this set or the filter is skipped.
const TIME_FIELDS = new Set([
  'published_at', 'fetched_at', 'geom_at', 'keywords_at',
]);

export function selectGeoFeatures({
  sourceId,
  subSourceId = null,
  recordType = null,
  includeUngeocoded = false,
  // Time-window filter for the time-slider feature. `at` is the upper bound
  // (slider thumb position, ISO string or Date), `windowSec` is the trailing
  // window size in seconds. `field`/`fallbackField` choose the row's "event
  // time" column with a COALESCE fallback so rows missing the primary field
  // still appear via fetched_at. The query produces an _approx_time flag
  // indicating which rows fell back so the popup can show "approximate".
  at = null,
  windowSec = null,
  field = 'published_at',
  fallbackField = 'fetched_at',
} = {}) {
  const where = ['source_id = @sourceId'];
  if (!includeUngeocoded) where.push('lat IS NOT NULL');
  const params = { sourceId };
  if (subSourceId) { where.push('sub_source_id = @subSourceId'); params.subSourceId = subSourceId; }
  if (recordType)  { where.push('record_type = @recordType'); params.recordType = recordType; }

  let approxSelect = '0 AS _approx_time';
  if (at && Number.isFinite(windowSec) && windowSec > 0
      && TIME_FIELDS.has(field) && TIME_FIELDS.has(fallbackField)) {
    const atIso = (at instanceof Date) ? at.toISOString() : String(at);
    const loIso = new Date(new Date(atIso).getTime() - windowSec * 1000).toISOString();
    where.push(`COALESCE(${field}, ${fallbackField}) BETWEEN @atLo AND @atHi`);
    params.atLo = loIso;
    params.atHi = atIso;
    approxSelect = `(${field} IS NULL) AS _approx_time`;
  }

  const rows = db.prepare(
    `SELECT uid, source_id, sub_source_id, record_type, lat, lon, geometry,
            title, summary, link, language, published_at, fetched_at,
            properties, tags,
            ${approxSelect}
       FROM intel_items
      WHERE ${where.join(' AND ')}`,
  ).all(params);
  const features = rows.map((row) => {
    let properties = {};
    try { properties = row.properties ? JSON.parse(row.properties) : {}; } catch { /* ignore */ }
    let tags = [];
    try { tags = row.tags ? JSON.parse(row.tags) : []; } catch { /* ignore */ }
    let geometry = null;
    if (row.geometry) {
      try { geometry = JSON.parse(row.geometry); } catch { /* fall through */ }
    }
    if (!geometry && row.lat != null && row.lon != null) {
      geometry = { type: 'Point', coordinates: [row.lon, row.lat] };
    }
    const out = {
      type: 'Feature',
      geometry,
      properties: {
        ...properties,
        uid: row.uid,
        source_id: row.source_id,
        sub_source_id: row.sub_source_id,
        record_type: row.record_type,
        title: row.title,
        summary: row.summary,
        link: row.link,
        language: row.language,
        published_at: row.published_at,
        fetched_at: row.fetched_at,
        tags,
      },
    };
    if (row._approx_time) out.properties.approx_time = true;
    return out;
  });
  return { type: 'FeatureCollection', features };
}

/** Decode a JSON keywords array (or pass through) to a flat space-joined string. */
export function joinKeywordsForFts(jsonOrArray) {
  if (!jsonOrArray) return '';
  let arr = jsonOrArray;
  if (typeof jsonOrArray === 'string') {
    try { arr = JSON.parse(jsonOrArray); } catch { return jsonOrArray; }
  }
  if (!Array.isArray(arr)) return '';
  return arr.filter((k) => typeof k === 'string').join(' ');
}

/**
 * Update the LLM-extracted keywords for one row. Atomic across base + FTS.
 * Caller passes the raw keywords array (JSON-stringified for the base column)
 * and we segment + write the FTS keyword string here.
 */
export async function updateItemKeywords(uid, keywordsArray) {
  if (uid == null) return 0;
  const json = JSON.stringify(Array.isArray(keywordsArray) ? keywordsArray : []);
  const flat = joinKeywordsForFts(keywordsArray);
  const segmented = await segmentForFts(flat);
  const updateBaseStmt = db.prepare(
    `UPDATE intel_items
        SET keywords = ?,
            keywords_at = datetime('now'),
            keywords_failed = 0
      WHERE uid = ?`,
  );
  const tx = db.transaction(() => {
    updateBaseStmt.run(json, String(uid));
    intelMirror.updateKeywords(uid, segmented);
  });
  tx();
  return 1;
}

/** Periodic maintenance: drop rows older than max_age_ms. Atomic across base + FTS. */
export function pruneOlderThan(maxAgeMs) {
  const cutoff = new Date(Date.now() - maxAgeMs).toISOString();
  return intelMirror.pruneByCondition('fetched_at < ?', [cutoff]);
}

/**
 * Compatibility alias for boot orchestration. The registry's
 * rebuildAllAtBoot() now drives this — kept exported for any external
 * caller that wants to force a rebuild.
 */
export async function ftsRebuildFromBase(opts = {}) {
  return intelMirror.rebuildFromBase(opts);
}
