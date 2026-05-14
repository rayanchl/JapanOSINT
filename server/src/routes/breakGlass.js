/**
 * Break-glass admin login. Bypasses Supabase Auth entirely so the platform
 * stays operable during a Supabase outage.
 *
 * Two hard gates:
 *   1. BREAK_GLASS_ENABLED=1 in the environment.
 *   2. A valid TOTP code from ADMIN_TOTP_SECRET.
 *
 * On success: issues an HS256 JWT signed with BREAK_GLASS_JWT_SECRET that
 * names a synthetic admin user inside the `legacy` tenant. The audit log
 * gets a loud entry for every use — this is meant to surface in monitoring
 * the moment it's invoked.
 *
 * Mounting:
 *   app.use('/admin/break-glass', breakGlassRouter);
 *
 * Disable by leaving BREAK_GLASS_ENABLED unset. Rotate the TOTP secret
 * after any use during an incident.
 */

import express from 'express';
import { createHmac, randomUUID } from 'crypto';
import { SignJWT } from 'jose';
import db from '../utils/database.js';

const router = express.Router();

const ENABLED = process.env.BREAK_GLASS_ENABLED === '1';
const TOTP_SECRET = process.env.ADMIN_TOTP_SECRET || '';
const JWT_SECRET = process.env.BREAK_GLASS_JWT_SECRET || '';
const TOTP_STEP_SECONDS = 30;
const TOTP_WINDOW = 1; // accept current step ± 1 for clock skew

router.post('/login', async (req, res) => {
  if (!ENABLED) {
    return res.status(404).json({ error: 'Not found' });
  }
  if (!TOTP_SECRET || !JWT_SECRET) {
    return res.status(503).json({ error: 'Break-glass not configured' });
  }

  const code = String(req.body?.code || '').replace(/\s+/g, '');
  if (!/^\d{6}$/.test(code)) {
    return res.status(400).json({ error: 'Six-digit TOTP code required' });
  }

  if (!verifyTotp(code, TOTP_SECRET)) {
    auditDeny(req);
    return res.status(401).json({ error: 'Invalid TOTP' });
  }

  // Synthesise a one-hour admin JWT scoped to the legacy tenant.
  const jwtSecret = new TextEncoder().encode(JWT_SECRET);
  const token = await new SignJWT({
    sub: 'break-glass-admin',
    email: 'break-glass@local',
    role: 'service_role',
    tenant_id: 'legacy',
    break_glass: true,
  })
    .setProtectedHeader({ alg: 'HS256' })
    .setIssuedAt()
    .setExpirationTime('1h')
    .sign(jwtSecret);

  auditAllow(req);
  return res.json({ token, expires_in: 3600 });
});

// ── TOTP verification (RFC 6238) ─────────────────────────────────────────

function verifyTotp(code, secret) {
  const key = base32Decode(secret);
  if (!key) return false;
  const now = Math.floor(Date.now() / 1000);
  const step = Math.floor(now / TOTP_STEP_SECONDS);
  for (let drift = -TOTP_WINDOW; drift <= TOTP_WINDOW; drift++) {
    if (totpAt(key, step + drift) === code) return true;
  }
  return false;
}

function totpAt(key, step) {
  const buf = Buffer.alloc(8);
  buf.writeBigUInt64BE(BigInt(step));
  const hmac = createHmac('sha1', key).update(buf).digest();
  const offset = hmac[hmac.length - 1] & 0x0f;
  const bin = ((hmac[offset] & 0x7f) << 24)
    | ((hmac[offset + 1] & 0xff) << 16)
    | ((hmac[offset + 2] & 0xff) << 8)
    | (hmac[offset + 3] & 0xff);
  return String(bin % 1_000_000).padStart(6, '0');
}

function base32Decode(s) {
  const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ234567';
  const cleaned = s.replace(/=+$/, '').toUpperCase().replace(/\s+/g, '');
  let bits = 0;
  let value = 0;
  const out = [];
  for (const ch of cleaned) {
    const idx = alphabet.indexOf(ch);
    if (idx < 0) return null;
    value = (value << 5) | idx;
    bits += 5;
    if (bits >= 8) {
      bits -= 8;
      out.push((value >>> bits) & 0xff);
    }
  }
  return Buffer.from(out);
}

// ── Audit ────────────────────────────────────────────────────────────────

function auditAllow(req) {
  insertAudit({
    action: 'break_glass.login.ok',
    payload: { ip: req.ip, ua: req.get('user-agent') || null },
  });
  console.warn('[break-glass] ADMIN LOGIN ISSUED', req.ip);
}

function auditDeny(req) {
  insertAudit({
    action: 'break_glass.login.denied',
    payload: { ip: req.ip, ua: req.get('user-agent') || null },
  });
  console.warn('[break-glass] login denied', req.ip);
}

function insertAudit({ action, payload }) {
  try {
    db.prepare(`
      INSERT INTO audit_events (id, tenant_id, user_id, action, target, payload_json, ts, ip, ua)
      VALUES (?, 'legacy', NULL, ?, NULL, ?, datetime('now'), ?, ?)
    `).run(
      randomUUID(),
      action,
      JSON.stringify(payload?.body ?? {}),
      payload?.ip || null,
      payload?.ua || null,
    );
  } catch (err) {
    // Audit write must never crash a request. Log loudly and continue.
    console.error('[break-glass] audit insert failed:', err.message);
  }
}

export default router;
