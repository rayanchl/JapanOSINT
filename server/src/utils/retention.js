/**
 * Data retention. Three tables grow without bound otherwise:
 *   - intel_items  : `pruneOlderThan` existed but had no caller.
 *   - alert_events : one row per match (delivered AND dedup/storm-suppressed),
 *                    forever — a noisy rule writes thousands/day.
 *   - audit_events : every mutation + 10% read sample, forever.
 *
 * Windows are env-overridable (days). 0 disables that table's prune.
 * Run daily from a cron in index.js.
 */

import db from './database.js';
import { pruneOlderThan } from './intelStore.js';

const days = (envName, fallback) => {
  const n = Number(process.env[envName]);
  return Number.isFinite(n) && n >= 0 ? n : fallback;
};

const INTEL_DAYS = days('RETENTION_INTEL_DAYS', 30);
const ALERT_EVENT_DAYS = days('RETENTION_ALERT_EVENT_DAYS', 90);
const AUDIT_DAYS = days('RETENTION_AUDIT_DAYS', 180);

function pruneByAge(table, tsColumn, daysOld) {
  if (daysOld <= 0) return 0;
  // tsColumn values are SQLite datetime strings; compare in-DB.
  const info = db
    .prepare(`DELETE FROM ${table} WHERE ${tsColumn} < datetime('now', ?)`)
    .run(`-${Math.floor(daysOld)} days`);
  return info.changes || 0;
}

export function runRetention() {
  const out = {};
  try {
    if (INTEL_DAYS > 0) {
      out.intel_items = pruneOlderThan(INTEL_DAYS * 86_400_000);
    }
  } catch (err) {
    console.error('[retention] intel_items prune failed:', err.message);
  }
  try {
    out.alert_events = pruneByAge('alert_events', 'matched_at', ALERT_EVENT_DAYS);
  } catch (err) {
    console.error('[retention] alert_events prune failed:', err.message);
  }
  try {
    out.audit_events = pruneByAge('audit_events', 'ts', AUDIT_DAYS);
  } catch (err) {
    console.error('[retention] audit_events prune failed:', err.message);
  }
  // Entity graph GC: intel_items is pruned independently (no FK from
  // entity_mentions), so drop mentions/state pointing at vanished rows, then
  // orphan-GC entities with no remaining mentions (+ their edges). FK CASCADE
  // isn't relied on (PRAGMA foreign_keys may be off) — delete explicitly.
  try {
    const m = db.prepare(
      `DELETE FROM entity_mentions WHERE item_uid NOT IN (SELECT uid FROM intel_items)`,
    ).run();
    const s = db.prepare(
      `DELETE FROM entity_extraction_state WHERE item_uid NOT IN (SELECT uid FROM intel_items)`,
    ).run();
    db.prepare(`
      DELETE FROM entity_relationships WHERE src_entity_id IN (
        SELECT entity_id FROM entities WHERE entity_id NOT IN (SELECT DISTINCT entity_id FROM entity_mentions)
      ) OR dst_entity_id IN (
        SELECT entity_id FROM entities WHERE entity_id NOT IN (SELECT DISTINCT entity_id FROM entity_mentions)
      )`).run();
    const e = db.prepare(
      `DELETE FROM entities WHERE entity_id NOT IN (SELECT DISTINCT entity_id FROM entity_mentions)`,
    ).run();
    out.entity_mentions = m.changes || 0;
    out.entity_extraction_state = s.changes || 0;
    out.entities_orphaned = e.changes || 0;
  } catch (err) {
    console.error('[retention] entity graph GC failed:', err.message);
  }
  console.log('[retention] pruned', JSON.stringify(out));
  return out;
}
