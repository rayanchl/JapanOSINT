/**
 * JWT verification for Supabase-issued access tokens.
 *
 * Supabase signs HS256 with a project-level secret (`SUPABASE_JWT_SECRET`).
 * The verified payload carries `sub` (Supabase user id), `email`, and
 * `aud=authenticated`. We attach `req.supabaseUser` and hand off to the
 * tenant-resolution middleware to materialise the local `users` row + active
 * `memberships` row.
 *
 * Behind the `MULTI_TENANT_ENABLED` flag so single-tenant boots are not
 * affected until routes are wired to the new model.
 */

import { jwtVerify } from 'jose';

const JWT_SECRET = process.env.SUPABASE_JWT_SECRET || '';
const SECRET_BYTES = JWT_SECRET ? new TextEncoder().encode(JWT_SECRET) : null;

export const MULTI_TENANT_ENABLED = process.env.MULTI_TENANT_ENABLED === '1';

/**
 * Express middleware: parses `Authorization: Bearer <jwt>` and verifies via
 * HS256. On success: `req.supabaseUser = { id, email }`. On failure: 401.
 *
 * Bypasses verification entirely when MULTI_TENANT_ENABLED is off so the
 * legacy app keeps working during the cutover.
 */
export async function requireSupabaseAuth(req, res, next) {
  if (!MULTI_TENANT_ENABLED) return next();

  if (!SECRET_BYTES) {
    console.error('[auth] MULTI_TENANT_ENABLED=1 but SUPABASE_JWT_SECRET is unset');
    return res.status(503).json({ error: 'Auth not configured' });
  }

  const header = req.headers.authorization || '';
  const match = header.match(/^Bearer\s+(.+)$/i);
  if (!match) return res.status(401).json({ error: 'Missing bearer token' });
  const token = match[1];

  try {
    const { payload } = await jwtVerify(token, SECRET_BYTES, {
      algorithms: ['HS256'],
      // Supabase audience claim. Service-role tokens use 'service_role';
      // we accept either so backend-to-backend admin calls work.
      audience: ['authenticated', 'service_role'],
    });
    if (!payload.sub) return res.status(401).json({ error: 'Token missing sub' });
    req.supabaseUser = {
      id: String(payload.sub),
      email: typeof payload.email === 'string' ? payload.email : null,
      role: typeof payload.role === 'string' ? payload.role : 'authenticated',
    };
    return next();
  } catch (err) {
    return res.status(401).json({ error: 'Invalid token', detail: err.code || err.message });
  }
}
