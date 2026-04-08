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
    status        TEXT NOT NULL DEFAULT 'offline' CHECK(status IN ('online','offline','degraded')),
    last_check    TEXT,
    last_success  TEXT,
    response_time_ms INTEGER,
    records_count INTEGER DEFAULT 0,
    error_message TEXT
  );

  CREATE TABLE IF NOT EXISTS data_cache (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id   TEXT NOT NULL REFERENCES sources(id),
    layer_type  TEXT NOT NULL,
    geojson     TEXT NOT NULL,
    bbox        TEXT,
    fetched_at  TEXT NOT NULL DEFAULT (datetime('now')),
    expires_at  TEXT NOT NULL
  );

  CREATE INDEX IF NOT EXISTS idx_cache_source   ON data_cache(source_id);
  CREATE INDEX IF NOT EXISTS idx_cache_layer    ON data_cache(layer_type);
  CREATE INDEX IF NOT EXISTS idx_cache_expires  ON data_cache(expires_at);

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
`);

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
    error_message   = @error_message
  WHERE id = @id
`);

const stmtInsertLog = db.prepare(`
  INSERT INTO fetch_log (source_id, status, records_fetched, duration_ms, error)
  VALUES (@source_id, @status, @records_fetched, @duration_ms, @error)
`);

const stmtGetCache = db.prepare(`
  SELECT * FROM data_cache
  WHERE source_id = @source_id AND layer_type = @layer_type
    AND expires_at > datetime('now')
  ORDER BY fetched_at DESC
  LIMIT 1
`);

const stmtSetCache = db.prepare(`
  INSERT INTO data_cache (source_id, layer_type, geojson, bbox, expires_at)
  VALUES (@source_id, @layer_type, @geojson, @bbox, @expires_at)
`);

// --------------- Helper functions ---------------

export function upsertSource({ id, name, type, category, url, status = 'offline' }) {
  return stmtUpsertSource.run({ id, name, type, category, url, status });
}

export function updateSourceStatus({ id, status, response_time_ms = null, records_count = null, error_message = null }) {
  return stmtUpdateStatus.run({ id, status, response_time_ms, records_count, error_message });
}

export function logFetch({ source_id, status, records_fetched = 0, duration_ms = null, error = null }) {
  return stmtInsertLog.run({ source_id, status, records_fetched, duration_ms, error });
}

export function getCachedData(source_id, layer_type) {
  return stmtGetCache.get({ source_id, layer_type });
}

export function setCachedData({ source_id, layer_type, geojson, bbox = null, expires_at }) {
  return stmtSetCache.run({ source_id, layer_type, geojson, bbox, expires_at });
}

export function getAllSources() {
  return db.prepare('SELECT * FROM sources ORDER BY category, name').all();
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
