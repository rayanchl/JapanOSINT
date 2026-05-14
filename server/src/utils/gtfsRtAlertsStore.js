/**
 * FTS mirror over `gtfs_rt_alerts.header_text` + `description_text`. Lives
 * in its own module because writers are spread across `gtfsRtPoller.js`
 * (native GTFS-RT decode) and `odptToGtfsRt.js` (ODPT TrainInformation
 * adapter). Both call `writeAlertFts(row)` from inside their existing
 * db.transaction() so base + FTS stay atomic.
 *
 * Composite uid: `org_id || ':' || alert_id` — base table is keyed by
 * (org_id, alert_id), and the FTS mirror needs a single string column.
 *
 * TTL sweep is `pruneExpired(seconds)` — atomic DELETE FROM base RETURNING
 * uid, then loop FTS deletes inside one txn.
 */

import { defineFtsMirror } from './ftsMirror.js';
import { registerMirror } from './ftsRegistry.js';

export const gtfsRtAlertsMirror = registerMirror(defineFtsMirror({
  name:             'gtfs_rt_alerts_fts',
  baseTable:        'gtfs_rt_alerts',
  baseUidColumn:    "(org_id || ':' || alert_id)",
  ftsTable:         'gtfs_rt_alerts_fts',
  textColumns:      ['header_text', 'description_text'],
  tokenizer:        'unicode61 remove_diacritics 1',
  segment:          true,
  segmenterVersion: 1,
}));

/** Sync, callable inside the caller's db.transaction. Idempotent. */
export function writeAlertFts({ org_id, alert_id, header_text, description_text }) {
  if (!org_id || !alert_id) return;
  const uid = `${org_id}:${alert_id}`;
  gtfsRtAlertsMirror.writeOne(gtfsRtAlertsMirror.segmentRowSync({
    uid,
    header_text:      header_text ?? null,
    description_text: description_text ?? null,
  }));
}

/** Atomic TTL sweep: DELETE FROM base RETURNING uid, drop matching FTS rows. */
export function pruneExpiredAlerts(maxAgeSeconds) {
  return gtfsRtAlertsMirror.pruneByCondition(
    "reported_at < unixepoch('now') - ?",
    [maxAgeSeconds],
  );
}
