// Adapter: convert ODPT's TrainInformation JSON (operator-wide status and
// delay text) into gtfs_rt_alerts rows. ODPT also publishes per-train delay
// at odpt:Train for some operators, but for now we only map the simpler
// TrainInformation feed. Runs once on boot (cheap) and every 5 min via cron.
//
// This writes into gtfs_rt_alerts, not a separate odpt_* table, so the
// station popup and service-alert surface can be mode-agnostic.

import db from './database.js';
import { getOdptToken } from './odptAuth.js';

const ENDPOINT_CHALLENGE = 'https://api-challenge.odpt.org/api/v4/odpt:TrainInformation';
const ENDPOINT_PROD = 'https://api.odpt.org/api/v4/odpt:TrainInformation';

const stmtUpsert = db.prepare(`
  INSERT INTO gtfs_rt_alerts (
    org_id, alert_id, route_ids, trip_ids, stop_ids,
    header_text, description_text, cause, effect,
    reported_at, received_at
  ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))
  ON CONFLICT(org_id, alert_id) DO UPDATE SET
    route_ids        = excluded.route_ids,
    header_text      = excluded.header_text,
    description_text = excluded.description_text,
    reported_at      = excluded.reported_at,
    received_at      = excluded.received_at
`);

/**
 * Fetch odpt:TrainInformation and write each record into gtfs_rt_alerts.
 * Safe to call with no token — returns {seeded:0, reason:'no ODPT token'}.
 */
export async function refreshOdptTrainInformationAlerts() {
  const token = getOdptToken();
  if (!token) return { seeded: 0, reason: 'no ODPT token' };

  // Try the challenge endpoint first (most devs have the challenge token);
  // fall back to prod if the challenge one is 4xx.
  let res = await fetch(`${ENDPOINT_CHALLENGE}?acl:consumerKey=${encodeURIComponent(token)}`);
  if (!res.ok) {
    res = await fetch(`${ENDPOINT_PROD}?acl:consumerKey=${encodeURIComponent(token)}`);
  }
  if (!res.ok) throw new Error(`ODPT TrainInformation HTTP ${res.status}`);
  const body = await res.json();

  let seeded = 0;
  const now = Math.floor(Date.now() / 1000);
  const tx = db.transaction((items) => {
    for (const it of items) {
      const op = (it['odpt:operator'] || '').replace(/^odpt\.Operator:/, '') || 'unknown';
      const line = (it['odpt:railway'] || '').replace(/^odpt\.Railway:/, '') || null;
      const id = it['owl:sameAs'] || it['@id'];
      if (!id) continue;
      const statusText = it['odpt:trainInformationText']?.ja
                      || it['odpt:trainInformationText']?.en
                      || null;
      const statusLabel = it['odpt:trainInformationStatus']?.ja || null;
      stmtUpsert.run(
        op,
        id,
        JSON.stringify(line ? [line] : []),
        JSON.stringify([]),
        JSON.stringify([]),
        statusLabel,
        statusText,
        null,
        null,
        now,
      );
      seeded++;
    }
  });
  tx(Array.isArray(body) ? body : []);
  return { seeded };
}
