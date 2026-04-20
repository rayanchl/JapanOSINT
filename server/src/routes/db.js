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
import sources from '../utils/sourceRegistry.js';
import { getLastRunAt as getCameraLastRunAt } from '../utils/cameraRunner.js';
import { getLastRunAt as getTransportLastRunAt } from '../utils/transportRunner.js';
import nextCronRun from '../utils/nextCronRun.js';

const router = Router();

// Tables we surface. Users can only read from this set.
const ALLOWED_TABLES = [
  'sources',
  'fetch_log',
  'cameras',
  'transport_stations',
  'transport_lines',
];

const MAX_LIMIT = 200;

function tableColumns(name) {
  // PRAGMA table_info returns [{cid, name, type, notnull, dflt_value, pk}].
  return db.prepare(`PRAGMA table_info(${name})`).all();
}

function textColumnsOf(cols) {
  return cols
    .filter((c) => /TEXT|CHAR|CLOB/i.test(c.type || ''))
    .map((c) => c.name);
}

router.get('/tables', (_req, res) => {
  try {
    const rows = ALLOWED_TABLES.map((name) => {
      const cols = tableColumns(name);
      const { c } = db.prepare(`SELECT COUNT(*) AS c FROM ${name}`).get();
      return {
        name,
        row_count: c,
        columns: cols.map((col) => ({ name: col.name, type: col.type || 'TEXT' })),
      };
    });
    res.json({ tables: rows });
  } catch (err) {
    console.error('[db] /tables failed:', err?.message);
    res.status(500).json({ error: 'Failed to list tables' });
  }
});

router.get('/tables/:name', (req, res) => {
  const name = String(req.params.name || '');
  if (!ALLOWED_TABLES.includes(name)) {
    return res.status(400).json({ error: `Unknown table: ${name}` });
  }

  const limit = Math.max(1, Math.min(MAX_LIMIT, parseInt(req.query.limit, 10) || 50));
  const offset = Math.max(0, parseInt(req.query.offset, 10) || 0);
  const q = typeof req.query.q === 'string' ? req.query.q.trim() : '';

  const cols = tableColumns(name);
  const colNames = cols.map((c) => c.name);
  const textCols = textColumnsOf(cols);

  const orderByRaw = typeof req.query.orderBy === 'string' ? req.query.orderBy : '';
  const orderBy = colNames.includes(orderByRaw) ? orderByRaw : null;
  const orderDir = req.query.orderDir === 'ASC' ? 'ASC' : 'DESC';

  try {
    const params = [];
    let where = '';
    if (q && textCols.length) {
      const ors = textCols.map((c) => `${c} LIKE ?`).join(' OR ');
      where = `WHERE ${ors}`;
      for (let i = 0; i < textCols.length; i++) params.push(`%${q}%`);
    }
    const totalRow = db
      .prepare(`SELECT COUNT(*) AS c FROM ${name} ${where}`)
      .get(...params);
    const total = totalRow?.c ?? 0;

    const orderSql = orderBy ? `ORDER BY ${orderBy} ${orderDir}` : '';
    const rows = db
      .prepare(`SELECT * FROM ${name} ${where} ${orderSql} LIMIT ? OFFSET ?`)
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
    const jobs = [
      {
        id: 'source-probe',
        cron: SOURCE_PROBE_CRON,
        description: 'Probe every free-API source for status + response time',
        last_run: sourceProbeLastRow?.t || null,
        next_run: nextCronRun(SOURCE_PROBE_CRON, now),
      },
      {
        id: 'camera-discovery',
        cron: CAMERA_CRON,
        description: 'Fan out every camera-discovery channel, dedupe into cameras table',
        last_run: getCameraLastRunAt(),
        next_run: nextCronRun(CAMERA_CRON, now),
      },
      {
        id: 'transport-discovery',
        cron: TRANSPORT_CRON,
        description: 'Fuse unified train/subway/bus/ship/port collectors into transport tables',
        last_run: getTransportLastRunAt(),
        next_run: nextCronRun(TRANSPORT_CRON, now),
      },
    ];

    // Merge DB status onto the registry entries so the grid has the full set
    // (registry entries without a DB row still appear as pending).
    const byId = new Map();
    for (const s of sources) byId.set(s.id, s);

    const dbRows = db
      .prepare(
        `SELECT id, name, category, status, last_check, last_success,
                response_time_ms, records_count
         FROM sources`,
      )
      .all();

    const dbById = new Map();
    for (const r of dbRows) dbById.set(r.id, r);

    const sourcesOut = [];
    for (const s of sources) {
      const row = dbById.get(s.id);
      sourcesOut.push({
        id: s.id,
        name: s.name,
        category: s.category || null,
        status: row?.status || s.status || 'pending',
        last_check: row?.last_check || null,
        last_success: row?.last_success || null,
        records_count: row?.records_count ?? null,
        response_time_ms: row?.response_time_ms ?? null,
        probe_cron: SOURCE_PROBE_CRON,
      });
    }

    res.json({ jobs, sources: sourcesOut });
  } catch (err) {
    console.error('[db] /scheduler failed:', err?.message);
    res.status(500).json({ error: 'Failed to build scheduler summary' });
  }
});

export default router;
