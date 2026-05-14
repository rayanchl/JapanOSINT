/**
 * Read-only database explorer routes for the in-UI DB panel.
 *
 *   GET /api/db/tables                     — list every table + row count + columns
 *   GET /api/db/tables/:name?q=&limit=…    — paginated rows with optional LIKE filter
 *   GET /api/db/scheduler                  — cron jobs + per-source rhythmicity
 *
 * No writes. Table names, sort columns and sort direction are strictly
 * allowlisted against PRAGMA output to stop any injection via identifiers.
 */

import { Router } from 'express';
import db from '../utils/database.js';
import { getLastRunAt as getCameraLastRunAt } from '../utils/cameraRunner.js';
import { getLastRunAt as getTransportLastRunAt } from '../utils/transportRunner.js';
import nextCronRun from '../utils/nextCronRun.js';

const router = Router();

// Tables we surface. Users can only read from this set.
// intel_items is the polymorphic master where every collector mirrors its
// output (geocoded or not) — see utils/collectorMirror.js. Listed first so it
// shows up at the top of the table selector.
const ALLOWED_TABLES = [
  'intel_items',
  'sources',
  'fetch_log',
  'cameras',
  'transport_stations',
  'transport_lines',
];

const MAX_LIMIT = 200;

function tableColumns(name) {
  // PRAGMA table_info returns [{cid, name, type, notnull, dflt_value, pk}].
  // Use quoted identifier to be safe (name is always allowlisted before calling this).
  return db.prepare(`PRAGMA table_info("${name}")`).all();
}

function textColumnsOf(cols) {
  return cols
    .filter((c) => /TEXT|CHAR|CLOB/i.test(c.type || ''))
    .map((c) => c.name);
}

router.get('/tables', (_req, res) => {
  try {
    // Only surface tables that actually exist (future-proofing).
    const placeholders = ALLOWED_TABLES.map(() => '?').join(', ');
    const existing = db
      .prepare(`SELECT name FROM sqlite_master WHERE type='table' AND name IN (${placeholders})`)
      .all(...ALLOWED_TABLES)
      .map((r) => r.name);

    const rows = existing.map((name) => {
      const cols = tableColumns(name);
      const { c } = db.prepare(`SELECT COUNT(*) AS c FROM "${name}"`).get();
      return {
        name,
        row_count: c,
        columns: cols.map((col) => ({ name: col.name, type: col.type || 'TEXT' })),
      };
    });
    res.json(rows);
  } catch (err) {
    console.error('[db] /tables failed:', err?.message);
    res.status(500).json({ error: 'Failed to list tables' });
  }
});

router.get('/tables/:name', (req, res) => {
  const name = String(req.params.name || '');
  if (!ALLOWED_TABLES.includes(name)) {
    return res.status(400).json({ error: 'unknown table' });
  }

  const limit = Math.max(1, Math.min(MAX_LIMIT, parseInt(req.query.limit, 10) || 50));
  const offset = Math.max(0, parseInt(req.query.offset, 10) || 0);
  const q = typeof req.query.q === 'string' ? req.query.q.trim() : '';

  const cols = tableColumns(name);
  const colNames = cols.map((c) => c.name);
  const textCols = textColumnsOf(cols);

  const orderByRaw = typeof req.query.orderBy === 'string' ? req.query.orderBy : '';
  const orderBy = colNames.includes(orderByRaw) ? orderByRaw : null;
  // orderDir: accept ASC or DESC (case-insensitive), default ASC, ignore invalid values.
  let orderDir = 'ASC';
  if (req.query.orderDir) {
    const d = String(req.query.orderDir).toUpperCase();
    if (d === 'DESC') orderDir = 'DESC';
    else if (d === 'ASC') orderDir = 'ASC';
    // else: invalid → keep default ASC
  }

  try {
    const params = [];
    let where = '';
    if (q && textCols.length) {
      const ors = textCols.map((c) => `"${c}" LIKE ?`).join(' OR ');
      where = `WHERE ${ors}`;
      for (let i = 0; i < textCols.length; i++) params.push(`%${q}%`);
    }
    const totalRow = db
      .prepare(`SELECT COUNT(*) AS c FROM "${name}" ${where}`)
      .get(...params);
    const total = totalRow?.c ?? 0;

    const orderSql = orderBy ? `ORDER BY "${orderBy}" ${orderDir}` : '';
    const rows = db
      .prepare(`SELECT * FROM "${name}" ${where} ${orderSql} LIMIT ? OFFSET ?`)
      .all(...params, limit, offset);

    res.json({
      name,
      columns: cols.map((c) => ({ name: c.name, type: c.type || 'TEXT' })),
      rows,
      total,
      limit,
      offset,
      orderBy,
      orderDir,
      q,
    });
  } catch (err) {
    console.error(`[db] /tables/${name} failed:`, err?.message);
    res.status(500).json({ error: 'Failed to read table' });
  }
});

// ── Scheduler summary ──────────────────────────────────────────────────
const SOURCE_PROBE_CRON = '0 */2 * * *';
const CAMERA_CRON = '15 * * * *';
const TRANSPORT_CRON = '30 * * * *';

router.get('/scheduler', (_req, res) => {
  try {
    const sourceProbeLastRow = db
      .prepare('SELECT MAX(timestamp) AS t FROM fetch_log')
      .get();

    const now = new Date();
    const toISO = (v) => {
      if (!v) return null;
      if (v instanceof Date) return v.toISOString();
      return String(v);
    };
    const nextISO = (cron) => {
      const d = nextCronRun(cron, now);
      return d ? d.toISOString() : null;
    };
    const jobs = [
      {
        id: 'source-probe',
        cron: SOURCE_PROBE_CRON,
        description: "Probe every source's health endpoint every 2 hours",
        last_run: toISO(sourceProbeLastRow?.t),
        next_run: nextISO(SOURCE_PROBE_CRON),
      },
      {
        id: 'camera-discovery',
        cron: CAMERA_CRON,
        description: 'Re-run camera discovery hourly at :15',
        last_run: toISO(getCameraLastRunAt()),
        next_run: nextISO(CAMERA_CRON),
      },
      {
        id: 'transport-discovery',
        cron: TRANSPORT_CRON,
        description: 'Re-run transport fan-out hourly at :30',
        last_run: toISO(getTransportLastRunAt()),
        next_run: nextISO(TRANSPORT_CRON),
      },
    ];

    const sourcesOut = db
      .prepare(
        `SELECT id, name, category, last_check, last_success, status, records_count, response_time_ms
         FROM sources ORDER BY id`,
      )
      .all();

    res.json({ jobs, sources: sourcesOut });
  } catch (err) {
    console.error('[db] /scheduler failed:', err?.message);
    res.status(500).json({ error: 'Failed to build scheduler summary' });
  }
});

export default router;
