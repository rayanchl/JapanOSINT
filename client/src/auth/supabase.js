/**
 * The parts of Supabase GoTrue the browser still touches itself: the PUBLIC
 * project settings, the social-provider authorize URL (PKCE), and parsing of
 * the provider callback. Everything that carries or yields a token — password,
 * sign-up, PKCE code exchange, refresh, logout — goes through the server's
 * `/api/auth/...` routes (session.js), so the refresh token reaches the browser
 * only as an httpOnly cookie (WEB-13) and is never visible to page script.
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

    /** GoTrue `/authorize` URL for a social provider (PKCE). */
    oauthAuthorizeURL: (provider, codeChallenge, redirectTo) => {
      const u = new URL(endpoint('authorize'));
      u.searchParams.set('provider', provider);
      u.searchParams.set('redirect_to', redirectTo);
      u.searchParams.set('code_challenge', codeChallenge);
      u.searchParams.set('code_challenge_method', 's256');
      return u.toString();
    },

    /** Validates a social sign-in callback URL — PKCE ONLY — and returns the
     *  `{ auth_code, code_verifier }` to exchange at `/api/auth/oauth`. The
     *  exchange needs this browser's code_verifier, so a `code` minted for
     *  someone else's challenge cannot become a session here.
     *
     *  There is deliberately no implicit-flow fallback: `#access_token=&
     *  refresh_token=` in the URL used to be adopted as-is, with nothing
     *  binding it to a sign-in this browser started, so a crafted link could
     *  sign the victim into the attacker's account (login CSRF / session
     *  swap). This client never requests the implicit flow, so a token in the
     *  fragment is refused, not adopted. */
    parseOAuthCallback: (callbackURL, verifier) => {
      if (!verifier) throw new AuthError(0, 'No sign-in in progress on this browser (missing PKCE verifier).');
      const u = new URL(callbackURL);
      const h = new URLSearchParams(u.hash.replace(/^#/, ''));
      const err = u.searchParams.get('error_description') || u.searchParams.get('error')
        || h.get('error_description') || h.get('error');
      if (err) throw new AuthError(-1, err.replace(/\+/g, ' '));
      const code = u.searchParams.get('code');
      if (code) return { auth_code: code, code_verifier: verifier };
      if (h.has('access_token')) {
        throw new AuthError(0, 'The sign-in callback carried tokens in the URL fragment (implicit flow). This app only completes PKCE sign-ins it started, so they were not used.');
      }
      throw new AuthError(0, 'The sign-in callback carried no authorization code.');
    },
  };
}
