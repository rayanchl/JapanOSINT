/**
 * Audit log writer. Records every state-changing request to audit_events
 * (POST / PUT / PATCH / DELETE), plus a 10% sample of reads so the trail
 * isn't blind to suspicious volume / scraping patterns.
 *
 * Writes happen AFTER the response is sent — we hook `res.on('finish')` so
 * the log entry includes the final status code without slowing the response
 * path. Write failures are swallowed: the audit log must never crash a
 * request.
 *
 * Behind MULTI_TENANT_ENABLED.
 */

import { randomUUID } from 'crypto';
import db from '../utils/database.js';
import { MULTI_TENANT_ENABLED } from './auth.js';

const READ_SAMPLE_RATE = 0.10;

const insertStmt = db.prepare(`
  INSERT INTO audit_events
    (id, tenant_id, user_id, action, target, payload_json, ts, ip, ua)
  VALUES
    (?, ?, ?, ?, ?, ?, datetime('now'), ?, ?)
`);

export function auditWriter(req, res, next) {
  if (!MULTI_TENANT_ENABLED) return next();
  // No tenant context yet (e.g. /api/health, auth failures) — skip; the
  // middleware before us already 401'd if a tenant was required.
  if (!req.tenant) return next();

  const method = req.method.toUpperCase();
  const isMutation = method !== 'GET' && method !== 'HEAD' && method !== 'OPTIONS';

  if (!isMutation && Math.random() >= READ_SAMPLE_RATE) {
    return next();
  }

  // Snapshot what we need before the response finishes — req fields are
  // safe to read after, but the body may be consumed by handlers.
  const action = `${method} ${routeTemplate(req)}`;
  const target = pickTarget(req);
  const tenantId = req.tenant.id;
  const userId = req.user?.id ?? null;
  const ip = req.ip;
  const ua = req.get('user-agent') || null;
  const body = pickPayload(req);

  res.on('finish', () => {
    try {
      insertStmt.run(
        randomUUID(),
        tenantId,
        userId,
        action,
        target,
        JSON.stringify({ ...body, status: res.statusCode }),
        ip,
        ua,
      );
    } catch (err) {
      console.error('[audit] insert failed:', err.message);
    }
  });

  next();
}

/**
 * Extract the route template (e.g. "/api/intel/items") rather than the
 * concrete URL (which leaks IDs). Express populates `req.route` once a
 * router matches; we fall back to the pathname for unrouted requests.
 */
function routeTemplate(req) {
  if (req.route?.path) return `${req.baseUrl || ''}${req.route.path}`;
  return req.path;
}

/**
 * For mutations, capture the "target" id from URL params or known body
 * fields. Keeps the audit trail useful without serialising the whole
 * payload.
 */
function pickTarget(req) {
  const p = req.params || {};
  return p.id ?? p.tenantId ?? p.sourceId ?? p.ruleId ?? p.uid ?? null;
}

/**
 * Capture a small, safe slice of the request body for audit context. Drops
 * fields that look like secrets (the BYOK endpoint posts plaintext keys
 * which absolutely must not land in audit_events).
 */
const SECRET_LIKE = /^(password|secret|token|api[_-]?key|authorization)$/i;
function pickPayload(req) {
  const out = {};
  const body = req.body;
  if (!body || typeof body !== 'object') return out;
  for (const [k, v] of Object.entries(body)) {
    if (SECRET_LIKE.test(k)) {
      out[k] = '<redacted>';
      continue;
    }
    if (typeof v === 'string' && v.length > 200) {
      out[k] = `${v.slice(0, 200)}…`;
    } else if (v && typeof v === 'object') {
      // Don't recurse into nested objects — one level is enough for
      // useful audit context, and limits blow-up risk.
      out[k] = '<object>';
    } else {
      out[k] = v;
    }
  }
  return out;
}
