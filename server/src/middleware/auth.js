/**
 * JWT verification for Supabase-issued access tokens.
 *
 * Supabase projects sign access tokens one of two ways:
 *   - Asymmetric (current default): ES256/RS256, verified against the
 *     project's public JWKS at `{SUPABASE_URL}/auth/v1/.well-known/jwks.json`.
 *   - Legacy symmetric: HS256 with the project-level `SUPABASE_JWT_SECRET`.
 *
 * We try JWKS first (when `SUPABASE_URL` is set) and fall back to the shared
 * secret, so projects on either signing mode — or mid-migration — work.
 *
 * The verified payload carries `sub` (Supabase user id), `email`, and
 * `aud=authenticated`. We attach `req.supabaseUser` and hand off to the
 * tenant-resolution middleware to materialise the local `users` row + active
 * `memberships` row.
 *
 * Always enforced — every `/api` request must carry a valid Supabase JWT.
 */

import { jwtVerify, createRemoteJWKSet } from 'jose';

const JWT_SECRET = process.env.SUPABASE_JWT_SECRET || '';
const SECRET_BYTES = JWT_SECRET ? new TextEncoder().encode(JWT_SECRET) : null;

const SUPABASE_URL = (process.env.SUPABASE_URL || '').replace(/\/+$/, '');
const JWKS = SUPABASE_URL
  ? createRemoteJWKSet(new URL(`${SUPABASE_URL}/auth/v1/.well-known/jwks.json`))
  : null;

// Supabase audience claim. Only end-user tokens ('authenticated') are valid
// on this path. 'service_role' is deliberately NOT accepted here: a leaked
// or forged service-role token must not be able to authenticate as a normal
// user and auto-provision/own a tenant. Break-glass / platform-operator
// access has its own dedicated path (routes/breakGlass.js +
// requirePlatformOperator), not this middleware.
const AUDIENCE = ['authenticated'];

/**
 * Express middleware: parses `Authorization: Bearer <jwt>` and verifies the
 * token. On success: `req.supabaseUser = { id, email, role }`. On failure: 401.
 */
export async function requireSupabaseAuth(req, res, next) {
  if (!JWKS && !SECRET_BYTES) {
    console.error(
      '[auth] Neither SUPABASE_URL nor SUPABASE_JWT_SECRET is set — auth cannot be enforced'
    );
    return res.status(503).json({ error: 'Auth not configured' });
  }

  const header = req.headers.authorization || '';
  const match = header.match(/^Bearer\s+(.+)$/i);
  if (!match) return res.status(401).json({ error: 'Missing bearer token' });
  const token = match[1];

  let payload;
  let lastErr;

  // 1) Asymmetric (ES256/RS256) via the project JWKS.
  if (JWKS) {
    try {
      ({ payload } = await jwtVerify(token, JWKS, {
        algorithms: ['ES256', 'RS256'],
        audience: AUDIENCE,
      }));
    } catch (err) {
      lastErr = err;
    }
  }

  // 2) Legacy symmetric (HS256) with the shared project secret.
  if (!payload && SECRET_BYTES) {
    try {
      ({ payload } = await jwtVerify(token, SECRET_BYTES, {
        algorithms: ['HS256'],
        audience: AUDIENCE,
      }));
    } catch (err) {
      lastErr = err;
    }
  }

  if (!payload) {
    return res
      .status(401)
      .json({ error: 'Invalid token', detail: lastErr?.code || lastErr?.message });
  }
  if (!payload.sub) return res.status(401).json({ error: 'Token missing sub' });

  req.supabaseUser = {
    id: String(payload.sub),
    email: typeof payload.email === 'string' ? payload.email : null,
    role: typeof payload.role === 'string' ? payload.role : 'authenticated',
  };
  return next();
}
