/**
 * Thin Supabase GoTrue (auth) REST client — a port of the iOS app's
 * `SupabaseAuth.swift`. No SDK: the server only needs a valid access token in
 * `Authorization: Bearer`, and password / refresh / PKCE grants are plain
 * POSTs straight to the Supabase project (never through the backend, so the
 * 401→refresh path in the fetch interceptor cannot recurse).
 */

/** Managed-service defaults, same values the iOS bundle ships
 *  (`BuildConfig.swift`). The anon key is a publishable client key. */
export const DEFAULT_SUPABASE_URL = 'https://cbdrdmmlgzqthvhcxnld.supabase.co';
export const DEFAULT_SUPABASE_ANON_KEY = 'sb_publishable_nzjWkrp7sMNxrU0jyuUbMQ_Q9lAVFL_';

export class AuthError extends Error {
  constructor(status, message) {
    super(message || 'request failed');
    this.name = 'AuthError';
    this.status = status;
  }
}

/** GoTrue provider ids and their brand styling (per each platform's sign-in
 *  guidelines), mirroring `OAuthProvider` on iOS. `twitter` is X. */
export const OAUTH_PROVIDERS = [
  { id: 'apple',    label: 'Apple',    bg: '#000000', fg: '#ffffff', border: false },
  { id: 'google',   label: 'Google',   bg: '#ffffff', fg: '#000000', border: true },
  { id: 'github',   label: 'GitHub',   bg: '#ffffff', fg: '#000000', border: true },
  { id: 'twitter',  label: 'X',        bg: '#000000', fg: '#ffffff', border: false },
  { id: 'facebook', label: 'Facebook', bg: '#1877F2', fg: '#ffffff', border: false },
];

function base64Url(bytes) {
  let s = '';
  for (const b of bytes) s += String.fromCharCode(b);
  return btoa(s).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

/** RFC 7636 PKCE pair: 32 random bytes → verifier; base64url(SHA256) → challenge. */
export async function makePkcePair() {
  const bytes = new Uint8Array(32);
  crypto.getRandomValues(bytes);
  const verifier = base64Url(bytes);
  const digest = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(verifier));
  return { verifier, challenge: base64Url(new Uint8Array(digest)) };
}

export function createSupabaseAuth({ projectURL, anonKey }) {
  const trimmed = String(projectURL || '').trim().replace(/\/+$/, '');

  function endpoint(path) {
    if (!trimmed || !anonKey) throw new AuthError(0, 'Supabase URL or anon key is missing/invalid.');
    return `${trimmed}/auth/v1/${path}`;
  }

  async function send(path, body) {
    const url = endpoint(path);
    let res;
    try {
      res = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', apikey: anonKey },
        body: JSON.stringify(body),
      });
    } catch (e) {
      throw new AuthError(-1, `Could not reach Supabase (${e.message || 'network error'})`);
    }
    const text = await res.text();
    let json = null;
    try { json = text ? JSON.parse(text) : null; } catch { /* non-JSON body */ }
    if (!res.ok) {
      const msg = json?.error_description || json?.msg || json?.message || json?.error || text || '';
      throw new AuthError(res.status, `Supabase ${res.status}: ${msg || 'request failed'}`);
    }
    return json;
  }

  function decodeSession(j) {
    if (!j || typeof j.access_token !== 'string' || !j.access_token) {
      throw new AuthError(0, 'Could not read the Supabase response.');
    }
    return {
      access_token: j.access_token,
      refresh_token: j.refresh_token,
      expires_in: j.expires_in ?? null,
      user: j.user ? { id: j.user.id, email: j.user.email ?? null } : null,
    };
  }

  return {
    /** Public project settings: which providers are enabled, whether sign-up
     *  is open, whether email confirmation is required. Not a secret. */
    settings: async () => {
      const res = await fetch(endpoint('settings'), { headers: { apikey: anonKey } });
      if (!res.ok) throw new AuthError(res.status, `Supabase ${res.status}: could not read auth settings`);
      const j = await res.json();
      const ext = j?.external || {};
      return {
        providers: Object.keys(ext).filter((k) => ext[k] === true && k !== 'email' && k !== 'phone'),
        emailEnabled: ext.email !== false,
        signupDisabled: Boolean(j?.disable_signup),
        emailConfirmRequired: j?.mailer_autoconfirm === false,
      };
    },

    signIn: async (email, password) =>
      decodeSession(await send('token?grant_type=password', { email, password })),

    /** `/signup` yields a Session only when email confirmation is OFF; when it is
     *  ON GoTrue answers 200 with a bare User and no tokens. */
    signUp: async (email, password) => {
      const j = await send('signup', { email, password });
      if (j && typeof j.access_token === 'string' && j.access_token) {
        return { kind: 'session', session: decodeSession(j) };
      }
      return { kind: 'confirmation_required' };
    },

    refresh: async (refreshToken) =>
      decodeSession(await send('token?grant_type=refresh_token', { refresh_token: refreshToken })),

    /** Revoke this session's refresh token server-side (`scope=local`: this
     *  browser only, other devices stay signed in). Clearing localStorage
     *  alone left the refresh token valid for anyone who had copied it. */
    logout: async (accessToken) => {
      if (!accessToken) return;
      let res;
      try {
        res = await fetch(endpoint('logout?scope=local'), {
          method: 'POST',
          headers: { apikey: anonKey, Authorization: `Bearer ${accessToken}` },
        });
      } catch (e) {
        throw new AuthError(-1, `Could not reach Supabase (${e.message || 'network error'})`);
      }
      // 401/403/404: the token is already expired or the session is gone —
      // the outcome sign-out wants.
      if (!res.ok && ![401, 403, 404].includes(res.status)) {
        throw new AuthError(res.status, `Supabase ${res.status}: logout failed`);
      }
    },

    /** GoTrue `/authorize` URL for a social provider (PKCE). */
    oauthAuthorizeURL: (provider, codeChallenge, redirectTo) => {
      const u = new URL(endpoint('authorize'));
      u.searchParams.set('provider', provider);
      u.searchParams.set('redirect_to', redirectTo);
      u.searchParams.set('code_challenge', codeChallenge);
      u.searchParams.set('code_challenge_method', 's256');
      return u.toString();
    },

    /** Completes a social sign-in from the provider callback URL — PKCE
     *  ONLY. The exchange needs this browser's code_verifier, so a `code`
     *  minted for someone else's challenge cannot become a session here.
     *
     *  There is deliberately no implicit-flow fallback: `#access_token=&
     *  refresh_token=` in the URL used to be adopted as-is, with nothing
     *  binding it to a sign-in this browser started, so a crafted link could
     *  sign the victim into the attacker's account (login CSRF / session
     *  swap). This client never requests the implicit flow, so a token in the
     *  fragment is refused, not adopted. */
    completeOAuth: async (callbackURL, verifier) => {
      if (!verifier) throw new AuthError(0, 'No sign-in in progress on this browser (missing PKCE verifier).');
      const u = new URL(callbackURL);
      const h = new URLSearchParams(u.hash.replace(/^#/, ''));
      const err = u.searchParams.get('error_description') || u.searchParams.get('error')
        || h.get('error_description') || h.get('error');
      if (err) throw new AuthError(-1, err.replace(/\+/g, ' '));
      const code = u.searchParams.get('code');
      if (code) {
        return decodeSession(await send('token?grant_type=pkce', { auth_code: code, code_verifier: verifier }));
      }
      if (h.has('access_token')) {
        throw new AuthError(0, 'The sign-in callback carried tokens in the URL fragment (implicit flow). This app only completes PKCE sign-ins it started, so they were not used.');
      }
      throw new AuthError(0, 'The sign-in callback carried no authorization code.');
    },
  };
}
