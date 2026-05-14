import Database from 'better-sqlite3';
import { mkdirSync } from 'fs';
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const dataDir = resolve(__dirname, '../../data');

mkdirSync(dataDir, { recursive: true });

const dbPath = resolve(dataDir, 'japanmap.db');
const db = new Database(dbPath);

// Enable WAL mode for better concurrent read performance
db.pragma('journal_mode = WAL');
db.pragma('foreign_keys = ON');
// better-sqlite3 defaults busy_timeout to 0 — any concurrent writer (manual
// sqlite3 CLI, a `--watch` restart racing the previous instance, the boot
// migrations themselves) trips an instant SQLITE_BUSY and aborts boot.
// 10 seconds is generous enough to outlast normal contention and short
// enough that a genuinely-stuck holder still surfaces as a real error.
db.pragma('busy_timeout = 10000');

// --------------- Schema ---------------

db.exec(`
  CREATE TABLE IF NOT EXISTS sources (
    id            TEXT PRIMARY KEY,
    name          TEXT NOT NULL,
    type          TEXT NOT NULL CHECK(type IN ('api','dataset','scraped','web_request')),
    category      TEXT NOT NULL,
    url           TEXT,
    status        TEXT NOT NULL DEFAULT 'pending' CHECK(status IN ('online','offline','degraded','pending')),
    last_check    TEXT,
    last_success  TEXT,
    response_time_ms INTEGER,
    records_count INTEGER DEFAULT 0,
    error_message TEXT,
    probe_request_url     TEXT,
    probe_request_method  TEXT,
    probe_request_headers TEXT,
    probe_response_status INTEGER,
    probe_response_headers TEXT,
    probe_response_body   TEXT,
    probe_kind            TEXT,
    probe_consent         INTEGER NOT NULL DEFAULT 0
  );

  DROP TABLE IF EXISTS data_cache;

  CREATE TABLE IF NOT EXISTS fetch_log (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id       TEXT NOT NULL REFERENCES sources(id),
    timestamp       TEXT NOT NULL DEFAULT (datetime('now')),
    status          TEXT NOT NULL,
    records_fetched INTEGER DEFAULT 0,
    duration_ms     INTEGER,
    error           TEXT
  );

  CREATE INDEX IF NOT EXISTS idx_log_source ON fetch_log(source_id);
  CREATE INDEX IF NOT EXISTS idx_log_ts     ON fetch_log(timestamp);

  -- The cameras, transport_stations, transport_lines, social_posts tables
  -- and their FTS mirrors used to live here. They were retired in the Phase
  -- B intel_items master cutover -- every reader and writer now goes through
  -- utils/intelStore (the polymorphic master). The drop migration below
  -- cleans them up on existing DBs; new DBs never see them.

  -- Canonical cross-mode station clusters. One row per physical place
  -- (Shinjuku = one row, even though it spans JR / Tokyo Metro / Toei /
  -- Keio / Odakyu). member_uids references transport_stations.station_uid
  -- so we can always drill back down to the per-mode records.
  CREATE TABLE IF NOT EXISTS station_clusters (
    cluster_uid    TEXT PRIMARY KEY,
    name           TEXT NOT NULL,
    name_ja        TEXT,
    lat            REAL NOT NULL,
    lon            REAL NOT NULL,
    member_uids    TEXT NOT NULL,
    line_colors    TEXT,
    line_names     TEXT,
    line_refs      TEXT,
    line_modes     TEXT,
    mode_set       TEXT NOT NULL,
    operator_set   TEXT,
    first_seen_at  TEXT NOT NULL DEFAULT (datetime('now')),
    last_seen_at   TEXT NOT NULL DEFAULT (datetime('now'))
  );
  CREATE INDEX IF NOT EXISTS idx_station_clusters_ll ON station_clusters(lat, lon);

  -- OSM closed-way polygon footprints for station buildings (Shinjuku
  -- building, Tokyo Station building, etc.). Linked to a cluster when the
  -- cluster centroid falls inside the footprint bbox.
  CREATE TABLE IF NOT EXISTS station_footprints (
    footprint_id   TEXT PRIMARY KEY,
    cluster_uid    TEXT,
    name           TEXT,
    name_ja        TEXT,
    geometry       TEXT NOT NULL,
    bbox_min_lat   REAL NOT NULL,
    bbox_min_lon   REAL NOT NULL,
    bbox_max_lat   REAL NOT NULL,
    bbox_max_lon   REAL NOT NULL,
    source         TEXT,
    first_seen_at  TEXT NOT NULL DEFAULT (datetime('now')),
    last_seen_at   TEXT NOT NULL DEFAULT (datetime('now'))
  );
  CREATE INDEX IF NOT EXISTS idx_station_footprints_cluster
    ON station_footprints(cluster_uid);
  CREATE INDEX IF NOT EXISTS idx_station_footprints_bbox
    ON station_footprints(bbox_min_lat, bbox_min_lon, bbox_max_lat, bbox_max_lon);

  -- One row per (cluster, way) pair, storing the snapped point on that
  -- track's geometry closest to the cluster centroid. A two-track line
  -- (up + down directions) produces two rows per cluster — one dot on
  -- each rail at the station's projection. Renders as one colored dot
  -- per track directly on the line geometry — Apple-Maps aesthetic.
  CREATE TABLE IF NOT EXISTS station_line_dots (
    cluster_uid  TEXT NOT NULL,
    way_uid      TEXT NOT NULL,
    line_color   TEXT NOT NULL,
    line_mode    TEXT NOT NULL,
    lon          REAL NOT NULL,
    lat          REAL NOT NULL,
    PRIMARY KEY (cluster_uid, way_uid)
  );
  CREATE INDEX IF NOT EXISTS idx_station_line_dots_cluster
    ON station_line_dots(cluster_uid);
  CREATE INDEX IF NOT EXISTS idx_station_line_dots_mode
    ON station_line_dots(line_mode);

  CREATE TABLE IF NOT EXISTS gtfs_operators (
    org_id          TEXT PRIMARY KEY,
    org_name        TEXT,
    hydrated_at     TEXT,
    feed_ids        TEXT NOT NULL DEFAULT '[]',
    stop_count      INTEGER NOT NULL DEFAULT 0,
    trip_count      INTEGER NOT NULL DEFAULT 0
  );
  CREATE TABLE IF NOT EXISTS gtfs_routes (
    org_id          TEXT NOT NULL,
    feed_id         TEXT NOT NULL,
    route_id        TEXT NOT NULL,
    short_name      TEXT,
    long_name       TEXT,
    route_type      INTEGER,
    color           TEXT,
    text_color      TEXT,
    PRIMARY KEY (org_id, feed_id, route_id)
  );
  CREATE TABLE IF NOT EXISTS gtfs_trips (
    org_id          TEXT NOT NULL,
    feed_id         TEXT NOT NULL,
    trip_id         TEXT NOT NULL,
    route_id        TEXT,
    service_id      TEXT,
    shape_id        TEXT,
    headsign        TEXT,
    direction_id    INTEGER,
    PRIMARY KEY (org_id, feed_id, trip_id)
  );
  CREATE TABLE IF NOT EXISTS gtfs_stop_times (
    org_id          TEXT NOT NULL,
    feed_id         TEXT NOT NULL,
    trip_id         TEXT NOT NULL,
    stop_sequence   INTEGER NOT NULL,
    stop_id         TEXT,
    arrival_sec     INTEGER,
    departure_sec   INTEGER,
    shape_dist_traveled REAL,
    PRIMARY KEY (org_id, feed_id, trip_id, stop_sequence)
  );
  CREATE INDEX IF NOT EXISTS idx_gtfs_stop_times_stop
    ON gtfs_stop_times(stop_id);
  CREATE INDEX IF NOT EXISTS idx_gtfs_stop_times_trip_time
    ON gtfs_stop_times(org_id, feed_id, trip_id, departure_sec);
  CREATE TABLE IF NOT EXISTS gtfs_shapes (
    org_id          TEXT NOT NULL,
    feed_id         TEXT NOT NULL,
    shape_id        TEXT NOT NULL,
    seq             INTEGER NOT NULL,
    lat             REAL,
    lon             REAL,
    dist_m          REAL,
    PRIMARY KEY (org_id, feed_id, shape_id, seq)
  );
  CREATE TABLE IF NOT EXISTS gtfs_calendar (
    org_id          TEXT NOT NULL,
    feed_id         TEXT NOT NULL,
    service_id      TEXT NOT NULL,
    mon INTEGER, tue INTEGER, wed INTEGER, thu INTEGER,
    fri INTEGER, sat INTEGER, sun INTEGER,
    start_date      TEXT,
    end_date        TEXT,
    PRIMARY KEY (org_id, feed_id, service_id)
  );
  CREATE TABLE IF NOT EXISTS gtfs_feeds (
    feed_id              TEXT PRIMARY KEY,
    ag_id                TEXT,
    ag_name              TEXT,
    pref_code            TEXT,
    pref_name            TEXT,
    feed_name            TEXT,
    fixed_current_url    TEXT,
    license_name         TEXT,
    license_url          TEXT,
    api_key_required     INTEGER NOT NULL DEFAULT 0,
    feed_end_date        TEXT,
    rt_catalog_url       TEXT,
    rt_api_key_required  INTEGER NOT NULL DEFAULT 0,
    rt_status            TEXT,
    last_refreshed_at    TEXT
  );
  CREATE INDEX IF NOT EXISTS idx_gtfs_feeds_agency ON gtfs_feeds(ag_id);

  CREATE TABLE IF NOT EXISTS gtfs_rt_feeds (
    feed_id           TEXT PRIMARY KEY,
    ag_id             TEXT NOT NULL,
    ag_name           TEXT,
    rt_url            TEXT NOT NULL,
    poll_interval_s   INTEGER NOT NULL DEFAULT 30,
    last_polled_at    TEXT,
    last_ok_at        TEXT,
    last_status       TEXT,
    consecutive_fails INTEGER NOT NULL DEFAULT 0
  );

  CREATE TABLE IF NOT EXISTS gtfs_rt_positions (
    org_id       TEXT NOT NULL,
    trip_id      TEXT NOT NULL,
    route_id     TEXT,
    lat          REAL NOT NULL,
    lon          REAL NOT NULL,
    bearing      REAL,
    speed_mps    REAL,
    reported_at  INTEGER NOT NULL,
    received_at  TEXT NOT NULL,
    PRIMARY KEY (org_id, trip_id)
  );
  CREATE INDEX IF NOT EXISTS idx_gtfs_rt_positions_reported
    ON gtfs_rt_positions(reported_at);

  CREATE TABLE IF NOT EXISTS gtfs_rt_trip_updates (
    org_id             TEXT NOT NULL,
    trip_id            TEXT NOT NULL,
    route_id           TEXT,
    stop_id            TEXT,
    stop_sequence      INTEGER NOT NULL,
    arrival_delay_s    INTEGER,
    departure_delay_s  INTEGER,
    reported_at        INTEGER NOT NULL,
    received_at        TEXT NOT NULL,
    PRIMARY KEY (org_id, trip_id, stop_sequence)
  );
  CREATE INDEX IF NOT EXISTS idx_gtfs_rt_trip_updates_stop
    ON gtfs_rt_trip_updates(org_id, stop_id);
  CREATE INDEX IF NOT EXISTS idx_gtfs_rt_trip_updates_reported
    ON gtfs_rt_trip_updates(reported_at);

  CREATE TABLE IF NOT EXISTS gtfs_rt_alerts (
    org_id           TEXT NOT NULL,
    alert_id         TEXT NOT NULL,
    route_ids        TEXT NOT NULL DEFAULT '[]',
    trip_ids         TEXT NOT NULL DEFAULT '[]',
    stop_ids         TEXT NOT NULL DEFAULT '[]',
    header_text      TEXT,
    description_text TEXT,
    cause            TEXT,
    effect           TEXT,
    reported_at      INTEGER NOT NULL,
    received_at      TEXT NOT NULL,
    PRIMARY KEY (org_id, alert_id)
  );
  CREATE INDEX IF NOT EXISTS idx_gtfs_rt_alerts_reported
    ON gtfs_rt_alerts(reported_at);

  -- Per-station ODPT timetable, ingested lazily on first station click.
  -- One row per timetable entry (scheduled train at that station).
  -- Ingest-once-per-station: rows are never overwritten, the fetched-marker
  -- table below gates re-fetch.
  CREATE TABLE IF NOT EXISTS odpt_station_timetable (
    station_id      TEXT NOT NULL,
    line_id         TEXT,
    calendar        TEXT,
    direction       TEXT,
    seq             INTEGER NOT NULL,
    departure_time  TEXT,
    destination_ja  TEXT,
    destination_en  TEXT,
    train_type      TEXT,
    train_name      TEXT,
    is_last         INTEGER NOT NULL DEFAULT 0,
    is_origin       INTEGER NOT NULL DEFAULT 0,
    org_id          TEXT,
    PRIMARY KEY (station_id, line_id, calendar, direction, seq)
  );
  CREATE INDEX IF NOT EXISTS idx_odpt_station_timetable_station
    ON odpt_station_timetable(station_id);

  CREATE TABLE IF NOT EXISTS odpt_station_timetable_fetched (
    station_id   TEXT PRIMARY KEY,
    fetched_at   TEXT NOT NULL,
    entry_count  INTEGER NOT NULL DEFAULT 0
  );

  -- Per-collector TTL table. One row per collector key; seeded at server
  -- boot from sourceRegistry.updateInterval (seconds → ms, floor 60s,
  -- ceiling 24h). Editable at runtime via sqlite or setTtlMs(). Existing
  -- rows are preserved across restarts so manual tuning survives.
  CREATE TABLE IF NOT EXISTS collector_ttls (
    key        TEXT PRIMARY KEY,
    ttl_ms     INTEGER NOT NULL,
    source     TEXT NOT NULL,        -- 'registry' | 'default' | 'user'
    updated_at INTEGER NOT NULL
  );

  -- Cached FeatureCollection per collector key. fetched_at is ms-since-epoch;
  -- ttl_ms snapshotted at write so a live TTL edit doesn't retro-expire
  -- already-cached entries. Pruned every 10 min by scheduler.
  CREATE TABLE IF NOT EXISTS collector_cache (
    key        TEXT PRIMARY KEY,
    fc_json    TEXT NOT NULL,
    fetched_at INTEGER NOT NULL,
    ttl_ms     INTEGER NOT NULL
  );
  CREATE INDEX IF NOT EXISTS idx_collector_cache_fetched
    ON collector_cache(fetched_at);

  -- Non-spatial collector output. Every source registered with kind:'intel'
  -- (RSS feeds, advisories, document indexes, reference tables, TLE etc.)
  -- upserts here on each run. uid is "<source_id>|<stable-record-key>" so
  -- repeat fetches are idempotent. Long bodies (filings, court rulings) live
  -- inline; if a source pushes the table past comfort we'll add chunking.
  CREATE TABLE IF NOT EXISTS intel_items (
    uid          TEXT PRIMARY KEY,
    source_id    TEXT NOT NULL,
    title        TEXT,
    body         TEXT,
    summary      TEXT,
    link         TEXT,
    author       TEXT,
    language     TEXT,
    published_at TEXT,
    fetched_at   TEXT NOT NULL,
    tags         TEXT,
    properties   TEXT NOT NULL DEFAULT '{}'
  );
  CREATE INDEX IF NOT EXISTS idx_intel_items_source
    ON intel_items(source_id, published_at DESC);
  CREATE INDEX IF NOT EXISTS idx_intel_items_fetched
    ON intel_items(fetched_at DESC);

  -- FTS5 mirror tables (intel_items_fts, social_posts_fts, etc.) are owned
  -- by utils/ftsMirror.js — each store module calls defineFtsMirror({...})
  -- which handles CREATE / DROP+RECREATE based on a fingerprint stored in
  -- _fts_meta below. Don't create them here.
  --
  -- _fts_meta is the source of truth for "what shape is each FTS table on
  -- disk in." ftsMirror.ensureSchema() compares the live config against the
  -- stored fingerprint and rebuilds when they diverge (column list change,
  -- tokenizer change, segmenter version bump). Fingerprint is written only
  -- after a successful rebuild so an interrupted rebuild self-heals.
  CREATE TABLE IF NOT EXISTS _fts_meta (
    name         TEXT PRIMARY KEY,
    fingerprint  TEXT NOT NULL,
    columns      TEXT NOT NULL,
    tokenizer    TEXT NOT NULL,
    version      INTEGER NOT NULL,
    rebuilt_at   TEXT NOT NULL
  );

  -- (social_posts retired — see comment above where cameras/transport_*
  --  CREATEs used to live. All social posts now go through intel_items.)

  -- video_items was scaffolding for a YouTube/Niconico video-detail collector
  -- that never landed. Niconico ranking now persists into social_posts (see
  -- collectors/niconicoRanking.js), so video_items has no writers. The table,
  -- its FTS mirror, and store/enricher paths were removed; the cleanup IIFE
  -- below DROPs leftover artefacts on existing DBs.

  -- Pair-level "the LLM said these two transport_stations are/are-not the
  -- same". stationClusterer reads this in Pass 3 and unions any pair with
  -- same=1 AND confidence >= 0.7. Pair ordering: uid_a < uid_b lexically.
  CREATE TABLE IF NOT EXISTS llm_station_merges (
    uid_a       TEXT NOT NULL,
    uid_b       TEXT NOT NULL,
    same        INTEGER NOT NULL CHECK(same IN (0,1)),
    confidence  REAL NOT NULL,
    reason      TEXT,
    decided_at  TEXT NOT NULL DEFAULT (datetime('now')),
    PRIMARY KEY (uid_a, uid_b)
  );
`);

// --------------- Schema migration ---------------
// Older DBs were created without probe_* columns. SQLite can ADD COLUMN
// in-place, so just append whatever's missing — no data loss, no fetch_log
// wipe. The legacy CHECK constraint on `status` did not include 'pending';
// if we still see that shape we recreate the table while preserving every
// row (and their fetch_log foreign-key relationships).
(function ensureSchema() {
  const cols = db.prepare("PRAGMA table_info(sources)").all();
  if (cols.length === 0) return; // fresh DB — main CREATE handled it

  const colNames = new Set(cols.map((c) => c.name));
  const probeCols = [
    ['probe_request_url',     'TEXT'],
    ['probe_request_method',  'TEXT'],
    ['probe_request_headers', 'TEXT'],
    ['probe_response_status', 'INTEGER'],
    ['probe_response_headers','TEXT'],
    ['probe_response_body',   'TEXT'],
    ['probe_kind',            'TEXT'],
    ['probe_consent',         'INTEGER NOT NULL DEFAULT 0'],
  ];

  // Detect the legacy CHECK constraint that omits 'pending'. PRAGMA doesn't
  // expose CHECK clauses, so peek at sqlite_master.sql.
  const tableSql = db
    .prepare("SELECT sql FROM sqlite_master WHERE type='table' AND name='sources'")
    .get()?.sql || '';
  const hasPendingInCheck = /CHECK\s*\(\s*status\s+IN[^)]*'pending'/i.test(tableSql);
  const needsRebuild = !hasPendingInCheck;

  if (needsRebuild) {
    console.log('[database] Rebuilding sources table to widen status CHECK (preserving rows)...');
    db.pragma('foreign_keys = OFF');
    const tx = db.transaction(() => {
      db.exec(`
        CREATE TABLE sources_new (
          id            TEXT PRIMARY KEY,
          name          TEXT NOT NULL,
          type          TEXT NOT NULL CHECK(type IN ('api','dataset','scraped','web_request')),
          category      TEXT NOT NULL,
          url           TEXT,
          status        TEXT NOT NULL DEFAULT 'pending' CHECK(status IN ('online','offline','degraded','pending')),
          last_check    TEXT,
          last_success  TEXT,
          response_time_ms INTEGER,
          records_count INTEGER DEFAULT 0,
          error_message TEXT,
          probe_request_url     TEXT,
          probe_request_method  TEXT,
          probe_request_headers TEXT,
          probe_response_status INTEGER,
          probe_response_headers TEXT,
          probe_response_body   TEXT,
          probe_kind            TEXT,
          probe_consent         INTEGER NOT NULL DEFAULT 0
        );
      `);
      // Copy whatever columns exist; missing columns become NULL.
      const oldCols = [...colNames];
      const sharedNewCols = [
        'id','name','type','category','url','status',
        'last_check','last_success','response_time_ms','records_count','error_message',
        ...probeCols.map(([c]) => c),
      ];
      const selectList = sharedNewCols
        .map((c) => oldCols.includes(c) ? `"${c}"` : `NULL AS "${c}"`)
        .join(', ');
      db.exec(`INSERT INTO sources_new (${sharedNewCols.map((c) => `"${c}"`).join(', ')}) SELECT ${selectList} FROM sources;`);
      db.exec(`DROP TABLE sources;`);
      db.exec(`ALTER TABLE sources_new RENAME TO sources;`);
    });
    tx();
    db.pragma('foreign_keys = ON');
  } else {
    // Modern shape — just patch any missing probe columns in place.
    for (const [name, type] of probeCols) {
      if (!colNames.has(name)) {
        db.exec(`ALTER TABLE sources ADD COLUMN ${name} ${type};`);
      }
    }
  }
})();

// station_line_dots was originally keyed (cluster_uid, line_color, line_mode)
// with one dot per colour. Current schema keys by (cluster_uid, way_uid) so a
// two-track line emits one dot per physical track. Drop + recreate if we
// detect the old schema — the clusterer rewrites every row on its next run.
(function ensureStationLineDotsSchema() {
  try {
    const cols = db.prepare("PRAGMA table_info(station_line_dots)").all();
    if (cols.length === 0) return; // table not created yet — CREATE TABLE handled it
    const hasWayUid = cols.some((c) => c.name === 'way_uid');
    if (!hasWayUid) {
      console.log('[database] Migrating station_line_dots schema to include way_uid...');
      db.exec(`
        DROP TABLE IF EXISTS station_line_dots;
        CREATE TABLE station_line_dots (
          cluster_uid  TEXT NOT NULL,
          way_uid      TEXT NOT NULL,
          line_color   TEXT NOT NULL,
          line_mode    TEXT NOT NULL,
          lon          REAL NOT NULL,
          lat          REAL NOT NULL,
          PRIMARY KEY (cluster_uid, way_uid)
        );
        CREATE INDEX idx_station_line_dots_cluster
          ON station_line_dots(cluster_uid);
        CREATE INDEX idx_station_line_dots_mode
          ON station_line_dots(line_mode);
      `);
    }
  } catch (err) {
    console.warn('[database] station_line_dots migration failed:', err?.message);
  }
})();

// One-shot drop of video_items + its FTS mirror. Boot logs from the FTS PR
// confirmed the table was empty in production; nothing of value is lost.
// Idempotent — DROP ... IF EXISTS makes subsequent boots no-ops.
(function dropVideoItemsTable() {
  try {
    db.exec(`
      DROP TABLE IF EXISTS video_items_fts;
      DROP TABLE IF EXISTS video_items;
    `);
    db.prepare(`DELETE FROM _fts_meta WHERE name = 'video_items_fts'`).run();
  } catch (err) {
    console.warn('[database] video_items drop failed:', err?.message);
  }
})();

// Phase B retirement: drop typed spatial tables now that every reader and
// writer goes through intel_items (the polymorphic master). Data was
// preserved by intelBackfill.js; this just removes the now-unused storage.
// Idempotent — DROP IF EXISTS + delete from _fts_meta.
(function dropTypedSpatialTables() {
  try {
    db.exec(`
      DROP TABLE IF EXISTS cameras_fts;
      DROP TABLE IF EXISTS social_posts_fts;
      DROP TABLE IF EXISTS cameras;
      DROP TABLE IF EXISTS transport_stations;
      DROP TABLE IF EXISTS transport_lines;
      DROP TABLE IF EXISTS social_posts;
    `);
    db.prepare(`DELETE FROM _fts_meta WHERE name IN ('cameras_fts','social_posts_fts')`).run();
  } catch (err) {
    console.warn('[database] typed-table drop failed:', err?.message);
  }
})();

// (cameras.llm_place_name / cameras.llm_geocoded_at migration retired —
//  the cameras table is gone and its data lives in intel_items, where
//  geom_source = 'llm' + geom_at do the same job.)

// intel_items keywords overlay. ADD COLUMNs are idempotent. The legacy
// content='intel_items' triggers belonged to the pre-mirror FTS design;
// drop them unconditionally. FTS table shape is now governed by
// utils/ftsMirror.js via _fts_meta fingerprinting — see ftsMirror.ensureSchema().
(function ensureIntelKeywordsSchema() {
  try {
    const cols = db.prepare('PRAGMA table_info(intel_items)').all().map((c) => c.name);
    if (cols.length === 0) return;
    if (!cols.includes('keywords')) {
      db.exec('ALTER TABLE intel_items ADD COLUMN keywords TEXT');
    }
    if (!cols.includes('keywords_at')) {
      db.exec('ALTER TABLE intel_items ADD COLUMN keywords_at TEXT');
    }
    if (!cols.includes('keywords_failed')) {
      db.exec('ALTER TABLE intel_items ADD COLUMN keywords_failed INTEGER DEFAULT 0');
    }
    db.exec(`
      DROP TRIGGER IF EXISTS intel_items_ai;
      DROP TRIGGER IF EXISTS intel_items_au;
      DROP TRIGGER IF EXISTS intel_items_ad;
    `);
  } catch (err) {
    console.warn('[database] intel_items keywords migration failed:', err?.message);
  }
})();

// intel_items polymorphic-master upgrade. Adds geometry + record_type +
// sub_source_id so every collector (spatial or not) can write here. Geometry
// columns are nullable: rows without coords still land in the table, and the
// llmEnricher fills them later. record_type is the cheap discriminator we
// filter on instead of digging into properties JSON. sub_source_id captures
// which channel/source-within-collector emitted the row (e.g. 'osm-overpass'
// inside camera-discovery), so the SourcesPanel can show per-channel status.
(function ensureIntelMasterSchema() {
  try {
    const cols = db.prepare('PRAGMA table_info(intel_items)').all().map((c) => c.name);
    if (cols.length === 0) return;
    const adds = [
      ['lat',           'REAL'],
      ['lon',           'REAL'],
      ['geom_source',   'TEXT'],
      ['geom_at',       'TEXT'],
      ['record_type',   'TEXT'],
      ['sub_source_id', 'TEXT'],
      // Full GeoJSON geometry as JSON string. lat/lon hold the indexed
      // representative point (centroid for polygons, the point itself for
      // points). geometry preserves polygons / lines / multi-shapes for
      // map rendering when the cutover lands.
      ['geometry',      'TEXT'],
      // LLM-geocode failure counter. Matches the keywords_failed pattern.
      // After 5 attempts we stop trying so we don't loop on rows the LLM
      // can't resolve. Clear this column to re-queue a row.
      ['geom_failed',   'INTEGER DEFAULT 0'],
    ];
    for (const [name, type] of adds) {
      if (!cols.includes(name)) {
        db.exec(`ALTER TABLE intel_items ADD COLUMN ${name} ${type}`);
      }
    }
    db.exec(`
      CREATE INDEX IF NOT EXISTS idx_intel_items_geom
        ON intel_items(lat, lon) WHERE lat IS NOT NULL;
      CREATE INDEX IF NOT EXISTS idx_intel_items_type
        ON intel_items(record_type, source_id);
      CREATE INDEX IF NOT EXISTS idx_intel_items_subsrc
        ON intel_items(source_id, sub_source_id);
      CREATE INDEX IF NOT EXISTS idx_intel_items_geom_pending
        ON intel_items(source_id) WHERE lat IS NULL AND geom_source IS NULL;
    `);
  } catch (err) {
    console.warn('[database] intel_items master migration failed:', err?.message);
  }
})();

// --------------- Prepared statements ---------------

const stmtUpsertSource = db.prepare(`
  INSERT INTO sources (id, name, type, category, url, status)
  VALUES (@id, @name, @type, @category, @url, @status)
  ON CONFLICT(id) DO UPDATE SET
    name     = excluded.name,
    type     = excluded.type,
    category = excluded.category,
    url      = excluded.url
`);

const stmtUpdateStatus = db.prepare(`
  UPDATE sources SET
    status          = @status,
    last_check      = datetime('now'),
    last_success    = CASE WHEN @status = 'online' THEN datetime('now') ELSE last_success END,
    response_time_ms = @response_time_ms,
    records_count   = COALESCE(@records_count, records_count),
    error_message   = @error_message,
    probe_request_url     = @probe_request_url,
    probe_request_method  = @probe_request_method,
    probe_request_headers = @probe_request_headers,
    probe_response_status = @probe_response_status,
    probe_response_headers = @probe_response_headers,
    probe_response_body   = @probe_response_body,
    probe_kind            = @probe_kind
  WHERE id = @id
`);

const stmtInsertLog = db.prepare(`
  INSERT INTO fetch_log (source_id, status, records_fetched, duration_ms, error)
  VALUES (@source_id, @status, @records_fetched, @duration_ms, @error)
`);

const stmtSetProbeConsent = db.prepare(`
  UPDATE sources SET probe_consent = @value WHERE id = @id
`);

// --------------- Helper functions ---------------

export function upsertSource({ id, name, type, category, url, status = 'pending' }) {
  const allowedStatus = new Set(['online', 'offline', 'degraded', 'pending']);
  const safeStatus = allowedStatus.has(status) ? status : 'pending';
  return stmtUpsertSource.run({ id, name, type, category, url, status: safeStatus });
}

export function updateSourceStatus({
  id,
  status,
  response_time_ms = null,
  records_count = null,
  error_message = null,
  probe_request_url = null,
  probe_request_method = null,
  probe_request_headers = null,
  probe_response_status = null,
  probe_response_headers = null,
  probe_response_body = null,
  probe_kind = null,
}) {
  return stmtUpdateStatus.run({
    id,
    status,
    response_time_ms,
    records_count,
    error_message,
    probe_request_url,
    probe_request_method,
    probe_request_headers,
    probe_response_status,
    probe_response_headers,
    probe_response_body,
    probe_kind,
  });
}

export function logFetch({ source_id, status, records_fetched = 0, duration_ms = null, error = null }) {
  return stmtInsertLog.run({ source_id, status, records_fetched, duration_ms, error });
}

export function setProbeConsent(id, value) {
  return stmtSetProbeConsent.run({ id, value: value ? 1 : 0 });
}

export function getAllSources() {
  return db.prepare('SELECT * FROM sources ORDER BY category, name').all();
}

/**
 * Delete any `sources` rows whose id isn't in the provided set. Also purges
 * their dependent fetch_log rows so FK constraints stay happy.
 * Used on startup to drop collectors that were removed from sourceRegistry.
 */
export function pruneSourcesNotIn(activeIds) {
  const active = new Set(activeIds);
  const existing = db.prepare('SELECT id FROM sources').all().map((r) => r.id);
  const stale = existing.filter((id) => !active.has(id));
  if (stale.length === 0) return [];
  const delLog = db.prepare('DELETE FROM fetch_log WHERE source_id = ?');
  const delSrc = db.prepare('DELETE FROM sources WHERE id = ?');
  const tx = db.transaction((ids) => {
    for (const id of ids) {
      delLog.run(id);
      delSrc.run(id);
    }
  });
  tx(stale);
  return stale;
}

export function getSourceById(id) {
  return db.prepare('SELECT * FROM sources WHERE id = ?').get(id);
}

export function getLogsBySourceId(id, limit = 50) {
  return db.prepare('SELECT * FROM fetch_log WHERE source_id = ? ORDER BY timestamp DESC LIMIT ?').all(id, limit);
}

export function getStats() {
  const total = db.prepare('SELECT COUNT(*) as count FROM sources').get().count;
  const online = db.prepare("SELECT COUNT(*) as count FROM sources WHERE status = 'online'").get().count;
  const totalRecords = db.prepare('SELECT COALESCE(SUM(records_count),0) as total FROM sources').get().total;
  const byCategory = db.prepare('SELECT category, COUNT(*) as count FROM sources GROUP BY category ORDER BY category').all();
  const byType = db.prepare('SELECT type, COUNT(*) as count FROM sources GROUP BY type ORDER BY type').all();
  return { total, online, totalRecords, byCategory, byType };
}

export default db;
