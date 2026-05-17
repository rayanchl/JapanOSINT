/**
 * Alert rules CRUD. Behind /api so the auth + tenant middleware always
 * populates req.tenant before these handlers run.
 *
 * Routes:
 *   GET    /api/alerts                 list rules for active tenant
 *   POST   /api/alerts                 create rule
 *   GET    /api/alerts/:id             one rule
 *   PATCH  /api/alerts/:id             update name/predicate/channels/etc.
 *   DELETE /api/alerts/:id             delete (cascade alert_events)
 *   POST   /api/alerts/:id/mute        body { duration_sec | "forever" }
 *   POST   /api/alerts/:id/test        synthesise + dispatch (debug only)
 *   GET    /api/alerts/:id/events      last N firings for the rule
 */

import express from 'express';
import { randomUUID } from 'crypto';
import { tenantDb } from '../utils/tenancy.js';
import { evaluateForNewItem } from '../utils/alertEngine.js';

const router = express.Router();

// Channels accepted by the alert engine. Keep in sync with alertEngine.dispatch.
const CHANNEL_TYPES = new Set(['email', 'webhook']);

function tenantId(req) {
  return req.tenant.id;
}

router.get('/', (req, res) => {
  const rows = tenantDb(tenantId(req)).prepare(`
    SELECT id, name, enabled, predicate_json, channels_json,
           dedup_window_sec, storm_cap_per_hour, muted_until,
           created_at, updated_at
      FROM alert_rules
     WHERE tenant_id = @tenant_id
     ORDER BY created_at DESC
  `).all();
  res.json({ data: rows.map(decodeRow) });
});

router.post('/', (req, res) => {
  const parsed = validateRule(req.body);
  if (parsed.error) return res.status(400).json({ error: parsed.error });

  const id = randomUUID();
  const tdb = tenantDb(tenantId(req));
  tdb.prepare(`
    INSERT INTO alert_rules
      (id, tenant_id, name, enabled, predicate_json, channels_json,
       dedup_window_sec, storm_cap_per_hour, created_by)
    VALUES (@id, @tenant_id, @name, @enabled, @predicate_json, @channels_json,
            @dedup_window_sec, @storm_cap_per_hour, @created_by)
  `).run({
    id,
    name: parsed.name,
    enabled: parsed.enabled ? 1 : 0,
    predicate_json: JSON.stringify(parsed.predicate),
    channels_json: JSON.stringify(parsed.channels),
    dedup_window_sec: parsed.dedup_window_sec,
    storm_cap_per_hour: parsed.storm_cap_per_hour,
    created_by: req.user?.id || null,
  });

  const row = tdb.prepare(
    `SELECT * FROM alert_rules WHERE id = @id AND tenant_id = @tenant_id`,
  ).get({ id });
  res.status(201).json({ data: decodeRow(row) });
});

router.get('/:id', (req, res) => {
  const row = tenantDb(tenantId(req)).prepare(
    `SELECT * FROM alert_rules WHERE id = @id AND tenant_id = @tenant_id`,
  ).get({ id: req.params.id });
  if (!row) return res.status(404).json({ error: 'Not found' });
  res.json({ data: decodeRow(row) });
});

router.patch('/:id', (req, res) => {
  const tdb = tenantDb(tenantId(req));
  const existing = tdb.prepare(
    `SELECT * FROM alert_rules WHERE id = @id AND tenant_id = @tenant_id`,
  ).get({ id: req.params.id });
  if (!existing) return res.status(404).json({ error: 'Not found' });

  // Merge: caller can send any subset of fields.
  const merged = {
    name: req.body.name ?? existing.name,
    enabled: req.body.enabled ?? !!existing.enabled,
    predicate: req.body.predicate ?? safeJson(existing.predicate_json, {}),
    channels: req.body.channels ?? safeJson(existing.channels_json, []),
    dedup_window_sec: req.body.dedup_window_sec ?? existing.dedup_window_sec,
    storm_cap_per_hour: req.body.storm_cap_per_hour ?? existing.storm_cap_per_hour,
  };
  const parsed = validateRule(merged);
  if (parsed.error) return res.status(400).json({ error: parsed.error });

  tdb.prepare(`
    UPDATE alert_rules
       SET name = @name, enabled = @enabled, predicate_json = @predicate_json,
           channels_json = @channels_json, dedup_window_sec = @dedup_window_sec,
           storm_cap_per_hour = @storm_cap_per_hour,
           updated_at = datetime('now')
     WHERE id = @id AND tenant_id = @tenant_id
  `).run({
    name: parsed.name,
    enabled: parsed.enabled ? 1 : 0,
    predicate_json: JSON.stringify(parsed.predicate),
    channels_json: JSON.stringify(parsed.channels),
    dedup_window_sec: parsed.dedup_window_sec,
    storm_cap_per_hour: parsed.storm_cap_per_hour,
    id: req.params.id,
  });
  const row = tdb.prepare(
    `SELECT * FROM alert_rules WHERE id = @id AND tenant_id = @tenant_id`,
  ).get({ id: req.params.id });
  res.json({ data: decodeRow(row) });
});

router.delete('/:id', (req, res) => {
  // Cascade: remove the rule's event history too. Atomic + tenant-scoped.
  tenantDb(tenantId(req)).transaction((t) => {
    t.prepare(`DELETE FROM alert_events WHERE rule_id = @id AND tenant_id = @tenant_id`)
      .run({ id: req.params.id });
    t.prepare(`DELETE FROM alert_rules WHERE id = @id AND tenant_id = @tenant_id`)
      .run({ id: req.params.id });
  });
  res.status(204).end();
});

router.post('/:id/mute', (req, res) => {
  const duration = req.body?.duration_sec;
  let mutedUntil;
  if (duration === 'forever' || duration === null) {
    // 100 years out — treat as forever, but reversible.
    mutedUntil = `datetime('now', '+36500 days')`;
  } else if (Number.isFinite(Number(duration)) && Number(duration) > 0) {
    mutedUntil = `datetime('now', '+${Math.floor(Number(duration))} seconds')`;
  } else {
    return res.status(400).json({ error: 'duration_sec must be positive number or "forever"' });
  }
  // SQL injection safe — duration is integer-validated above and embedded
  // into the literal datetime expression. Better than binding because
  // SQLite's strftime args don't take parameter markers cleanly.
  const result = tenantDb(tenantId(req)).prepare(`
    UPDATE alert_rules SET muted_until = ${mutedUntil}, updated_at = datetime('now')
     WHERE id = @id AND tenant_id = @tenant_id
  `).run({ id: req.params.id });
  if (result.changes === 0) return res.status(404).json({ error: 'Not found' });
  res.json({ ok: true });
});

router.post('/:id/unmute', (req, res) => {
  const result = tenantDb(tenantId(req)).prepare(`
    UPDATE alert_rules SET muted_until = NULL, updated_at = datetime('now')
     WHERE id = @id AND tenant_id = @tenant_id
  `).run({ id: req.params.id });
  if (result.changes === 0) return res.status(404).json({ error: 'Not found' });
  res.json({ ok: true });
});

router.get('/:id/events', (req, res) => {
  const limit = Math.min(500, Math.max(1, Number(req.query.limit) || 100));
  // LEFT JOIN intel_items so the client can render a meaningful row
  // (title + source + link) instead of a bare uid. Additive — existing
  // fields are unchanged; item_* are null when the matched row has since
  // been pruned from intel_items.
  const rows = tenantDb(tenantId(req)).prepare(`
    SELECT e.id, e.item_uid, e.matched_at, e.delivered_channels_json,
           e.suppressed, e.reason,
           i.title     AS item_title,
           i.source_id AS item_source_id,
           i.link      AS item_link
      FROM alert_events e
      LEFT JOIN intel_items i ON i.uid = e.item_uid
     WHERE e.rule_id = @rule_id AND e.tenant_id = @tenant_id
     ORDER BY e.matched_at DESC
     LIMIT @limit
  `).all({ rule_id: req.params.id, limit });
  res.json({
    data: rows.map((r) => ({
      id: r.id,
      item_uid: r.item_uid,
      matched_at: r.matched_at,
      suppressed: r.suppressed,
      reason: r.reason,
      delivered_channels: safeJson(r.delivered_channels_json, []),
      item_title: r.item_title ?? null,
      item_source_id: r.item_source_id ?? null,
      item_link: r.item_link ?? null,
    })),
  });
});

/**
 * Synthetic test-fire. Builds a fake intel item from request body fields,
 * runs it through the engine's matcher + dispatch. Useful for verifying
 * channel configuration before a real event happens. Does NOT write to
 * intel_items.
 */
router.post('/:id/test', (req, res) => {
  const rule = tenantDb(tenantId(req)).prepare(
    `SELECT * FROM alert_rules WHERE id = @id AND tenant_id = @tenant_id`,
  ).get({ id: req.params.id });
  if (!rule) return res.status(404).json({ error: 'Not found' });

  const fakeItem = {
    uid: `test-${randomUUID()}`,
    source_id: req.body?.source_id || 'alert.test',
    tenant_id: tenantId(req),
    title: req.body?.title || `Test fire: ${rule.name}`,
    summary: req.body?.summary || 'This is a test alert delivered from the rule test endpoint.',
    body: null,
    link: req.body?.link || null,
    lat: null, lon: null,
    tags: '[]',
    record_type: 'test',
    fetched_at: new Date().toISOString(),
    published_at: new Date().toISOString(),
  };
  evaluateForNewItem(fakeItem);
  res.json({ ok: true, fired: true });
});

// ── helpers ────────────────────────────────────────────────────────────────

function decodeRow(row) {
  return {
    id: row.id,
    name: row.name,
    enabled: !!row.enabled,
    predicate: safeJson(row.predicate_json, {}),
    channels: safeJson(row.channels_json, []).map((c) => {
      // Never echo back a webhook secret in API responses.
      if (c?.type === 'webhook' && c.secret) {
        return { ...c, secret: '••••' };
      }
      return c;
    }),
    dedup_window_sec: row.dedup_window_sec,
    storm_cap_per_hour: row.storm_cap_per_hour,
    muted_until: row.muted_until,
    created_at: row.created_at,
    updated_at: row.updated_at,
  };
}

function safeJson(s, fallback) {
  if (!s) return fallback;
  try { return JSON.parse(s); } catch { return fallback; }
}

function validateRule(body) {
  if (!body) return { error: 'body required' };
  const name = String(body.name || '').trim();
  if (!name) return { error: 'name required' };
  if (name.length > 200) return { error: 'name too long' };

  const predicate = body.predicate ?? {};
  if (typeof predicate !== 'object' || Array.isArray(predicate)) {
    return { error: 'predicate must be an object' };
  }
  // Optional fields validated below; missing = match everything.
  if (predicate.q != null && typeof predicate.q !== 'string') return { error: 'predicate.q must be string' };
  if (predicate.source_ids && !Array.isArray(predicate.source_ids)) return { error: 'predicate.source_ids must be array' };
  if (predicate.tags_any && !Array.isArray(predicate.tags_any)) return { error: 'predicate.tags_any must be array' };
  if (predicate.tags_all && !Array.isArray(predicate.tags_all)) return { error: 'predicate.tags_all must be array' };
  if (predicate.bbox && (!Array.isArray(predicate.bbox) || predicate.bbox.length !== 4)) {
    return { error: 'predicate.bbox must be [w,s,e,n]' };
  }

  const channels = body.channels ?? [];
  if (!Array.isArray(channels) || channels.length === 0) return { error: 'at least one channel required' };
  for (const ch of channels) {
    if (!ch || !CHANNEL_TYPES.has(ch.type)) {
      return { error: `invalid channel type: ${ch?.type}` };
    }
    if (!ch.target || typeof ch.target !== 'string') {
      return { error: `channel ${ch.type} missing target` };
    }
    if (ch.type === 'email') {
      if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(ch.target)) {
        return { error: 'email target is not a valid address' };
      }
    }
    if (ch.type === 'webhook') {
      if (!/^https?:\/\//.test(ch.target)) return { error: 'webhook target must be http(s) URL' };
      if (!ch.secret || ch.secret.length < 16) return { error: 'webhook secret required (≥16 chars)' };
    }
  }

  const dedup = Number(body.dedup_window_sec ?? 3600);
  const storm = Number(body.storm_cap_per_hour ?? 100);
  if (!Number.isFinite(dedup) || dedup < 0) return { error: 'dedup_window_sec must be ≥0' };
  if (!Number.isFinite(storm) || storm < 1) return { error: 'storm_cap_per_hour must be ≥1' };

  return {
    name,
    enabled: body.enabled !== false,
    predicate,
    channels,
    dedup_window_sec: Math.floor(dedup),
    storm_cap_per_hour: Math.floor(storm),
  };
}

export default router;
