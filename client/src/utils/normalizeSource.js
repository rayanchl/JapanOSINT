/**
 * `/api/sources` is a raw `SELECT * FROM sources` dump, so the wire keys are
 * the sqlite column names (`records_count`, `response_time_ms`, `last_check`,
 * `last_success`) and `type` is the lowercase CHECK-constrained value
 * (`api` | `dataset` | `scraped` | `web_request`).
 *
 * Normalise once at the fetch boundary so render code can read camelCase.
 */

export const TYPE_LABELS = {
  api: 'API',
  dataset: 'Dataset',
  scraped: 'Scraped',
  web_request: 'Web Request',
};

/** Display label for a wire `type` value. Unknown values pass through. */
export function typeLabel(type) {
  if (!type) return '';
  return TYPE_LABELS[String(type).toLowerCase()] || type;
}

const ALIASES = [
  ['records', ['recordCount', 'records_count']],
  ['responseTime', ['response_time_ms']],
  ['lastCheck', ['last_check']],
  ['lastSuccess', ['last_success']],
];

/**
 * Map one raw source row onto the camelCase shape the UI reads. Already
 * camelCase input (e.g. a WebSocket patch) is preserved, and keys the row
 * doesn't carry at all are left absent rather than defaulted — so this is
 * safe to run over a partial patch as well as a full row.
 */
export function normalizeSource(row) {
  if (!row || typeof row !== 'object') return row;
  const out = { ...row };
  if (out.type) out.type = String(out.type).toLowerCase();
  for (const [camel, snakes] of ALIASES) {
    if (out[camel] !== undefined) continue;
    for (const snake of snakes) {
      if (row[snake] !== undefined) { out[camel] = row[snake]; break; }
    }
  }
  return out;
}

export function normalizeSources(rows) {
  return Array.isArray(rows) ? rows.map(normalizeSource) : [];
}

export default normalizeSource;
