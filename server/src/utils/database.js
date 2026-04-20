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
    probe_kind            TEXT
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

  CREATE TABLE IF NOT EXISTS cameras (
    camera_uid         TEXT PRIMARY KEY,
    name               TEXT NOT NULL,
    camera_type        TEXT,
    lat                REAL NOT NULL,
    lon                REAL NOT NULL,
    url                TEXT,
    thumbnail_url      TEXT,
    operator           TEXT,
    country            TEXT DEFAULT 'JP',
    discovery_channels TEXT NOT NULL,
    properties         TEXT NOT NULL,
    first_seen_at      TEXT NOT NULL DEFAULT (datetime('now')),
    last_seen_at       TEXT NOT NULL DEFAULT (datetime('now')),
    seen_count         INTEGER NOT NULL DEFAULT 1
  );

  CREATE INDEX IF NOT EXISTS idx_cameras_last_seen ON cameras(last_seen_at);
  CREATE INDEX IF NOT EXISTS idx_cameras_type      ON cameras(camera_type);
  CREATE INDEX IF NOT EXISTS idx_cameras_geo       ON cameras(lat, lon);

  CREATE TABLE IF NOT EXISTS transport_stations (
    station_uid    TEXT PRIMARY KEY,
    mode           TEXT NOT NULL,
    name           TEXT,
    operator       TEXT,
    line           TEXT,
    lat            REAL NOT NULL,
    lon            REAL NOT NULL,
    sources        TEXT NOT NULL,
    properties     TEXT NOT NULL,
    first_seen_at  TEXT NOT NULL DEFAULT (datetime('now')),
    last_seen_at   TEXT NOT NULL DEFAULT (datetime('now')),
    seen_count     INTEGER NOT NULL DEFAULT 1
  );

  CREATE INDEX IF NOT EXISTS idx_transport_stations_mode ON transport_stations(mode);
  CREATE INDEX IF NOT EXISTS idx_transport_stations_geo  ON transport_stations(lat, lon);
  CREATE INDEX IF NOT EXISTS idx_transport_stations_seen ON transport_stations(last_seen_at);

  CREATE TABLE IF NOT EXISTS transport_lines (
    line_uid       TEXT PRIMARY KEY,
    mode           TEXT NOT NULL,
    name           TEXT,
    operator       TEXT,
    coordinates    TEXT NOT NULL,
    sources        TEXT NOT NULL,
    properties     TEXT NOT NULL,
    first_seen_at  TEXT NOT NULL DEFAULT (datetime('now')),
    last_seen_at   TEXT NOT NULL DEFAULT (datetime('now')),
    seen_count     INTEGER NOT NULL DEFAULT 1
  );

  CREATE INDEX IF NOT EXISTS idx_transport_lines_mode ON transport_lines(mode);
  CREATE INDEX IF NOT EXISTS idx_transport_lines_seen ON transport_lines(last_seen_at);

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
`);

// --------------- Schema migration ---------------
// Older DBs were created without probe_* columns and with a CHECK constraint
// that disallows 'pending'. SQLite can't ALTER a CHECK, so if we detect the
// legacy shape we drop and recreate `sources`. Runtime health is re-derived
// on the next scheduler sweep, so no real data is lost.
(function ensureSchema() {
  const cols = db.prepare("PRAGMA table_info(sources)").all();
  const hasProbe = cols.some((c) => c.name === 'probe_request_url');
  if (!hasProbe) {
    console.log('[database] Migrating sources table for probe capture...');
    db.pragma('foreign_keys = OFF');
    db.exec(`
      DELETE FROM fetch_log;
      DROP TABLE IF EXISTS sources;
      CREATE TABLE sources (
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
        probe_kind            TEXT
      );
    `);
    db.pragma('foreign_keys = ON');
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
