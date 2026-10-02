import React, { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState } from 'react';
import { api, ApiError } from '../api/client.js';
import { sessionStore, supabaseClient, coalescedRefresh, installFetchInterceptor, onSessionLost, REFRESH } from './session.js';
import { makePkcePair, AuthError } from './supabase.js';
import { claimLocalData, releaseLocalData } from './localData.js';

installFetchInterceptor();

/**
 * Owns the authenticated session and drives the top-level gate — a port of
 * the iOS `AuthSession`:
 *   loading → onboarding | ready
 * Sign in / sign up / social / sign out against Supabase; `/api/me` resolves
 * the active workspace; the fetch interceptor keeps every request carrying
 * the bearer and one 401 self-heals through a coalesced refresh.
 */
const AuthContext = createContext(null);

export const GATE = { loading: 'loading', onboarding: 'onboarding', ready: 'ready' };

function isUnreachable(e) {
  return e instanceof ApiError && e.status < 0;
}

async function meWithTimeout(ms) {
  const ctl = new AbortController();
  const t = setTimeout(() => ctl.abort(), ms);
  try {
    return await api.get('/api/me', { signal: ctl.signal });
  } catch (e) {
    if (e?.name === 'AbortError') throw new ApiError(-1, 'timed out reaching the backend');
    throw e;
  } finally { clearTimeout(t); }
}

export function AuthProvider({ children }) {
  const [gate, setGate] = useState(GATE.loading);
  const [me, setMe] = useState(null);
  const [connectionStalled, setConnectionStalled] = useState(false);
  const [lastError, setLastError] = useState(null);
  // null = not yet probed; the server's 403 is the only signal we trust.
  const [isPlatformAdmin, setIsPlatformAdmin] = useState(null);
  const booted = useRef(false);

  const adopt = useCallback((m) => {
    // Bookmarks / recent queries left by a DIFFERENT account are wiped before
    // this one can see them (see auth/localData.js).
    if (m?.user?.id) claimLocalData(m.user.id);
    setMe(m);
    if (m?.tenant?.id) sessionStore.setTenantId(m.tenant.id);
  }, []);

  // Asks an operator-gated route the cheapest question there is. This used to
  // call /api/db/tables on every boot — six COUNT(*) scans on the server's
  // event loop for every operator page load, just to learn a boolean.
  // /api/media/capabilities sits behind the same opgate_check() and, once its
  // one-time tool probe has run, answers from memory with no DB access.
  const probeOperator = useCallback(async () => {
    try {
      await api.get('/api/media/capabilities');
      setIsPlatformAdmin(true);
    } catch (e) {
      if (e instanceof ApiError && (e.status === 403 || e.status === 401)) setIsPlatformAdmin(false);
      else setIsPlatformAdmin(null);
    }
  }, []);

  const bootstrap = useCallback(async () => {
    if (!sessionStore.onboardingCompleted || !sessionStore.accessToken) {
      setGate(GATE.onboarding);
      return;
    }
    setConnectionStalled(false);
    try {
      const m = await meWithTimeout(6000);
      adopt(m);
      setGate(GATE.ready);
      probeOperator();
    } catch (e) {
      if (e instanceof ApiError && e.status === 401) {
        const outcome = await coalescedRefresh();
        if (outcome === REFRESH.ok) {
          try {
            const m = await api.get('/api/me');
            adopt(m);
            setGate(GATE.ready);
            probeOperator();
            return;
          } catch (e2) {
            // Reachability is not a verdict on the session.
            if (isUnreachable(e2)) { setConnectionStalled(true); return; }
          }
        } else if (outcome === REFRESH.transient) {
          // Supabase could not be reached: keep the tokens and offer a retry
          // instead of signing the user out over a network blip.
          setConnectionStalled(true);
          return;
        }
        sessionStore.clearTokens();
        setGate(GATE.onboarding);
      } else if (isUnreachable(e)) {
        setConnectionStalled(true);
      } else {
        // The backend answered with something other than 401 (503 auth not
        // configured, 5xx…): do not lock a returning user out.
        setLastError(e.message);
        setGate(GATE.ready);
      }
    }
  }, [adopt, probeOperator]);

  useEffect(() => {
    if (booted.current) return;
    booted.current = true;
    bootstrap();
  }, [bootstrap]);

  useEffect(() => onSessionLost(() => {
    setMe(null);
    setGate(GATE.onboarding);
  }), []);

  const establish = useCallback(async (session) => {
    sessionStore.setTokens(session.access_token, session.refresh_token);
    const m = await api.get('/api/me');
    adopt(m);
    probeOperator();
    return m;
  }, [adopt, probeOperator]);

  const signIn = useCallback(async (email, password) => {
    const s = await supabaseClient().signIn(email, password);
    return establish(s);
  }, [establish]);

  const signUp = useCallback(async (email, password) => {
    const r = await supabaseClient().signUp(email, password);
    if (r.kind === 'session') return establish(r.session);
    throw new AuthError(200, 'Check your inbox to confirm your email, then sign in.');
  }, [establish]);

  /** Social sign-in. Leaves the page for the provider; `/auth/callback`
   *  finishes the PKCE exchange and calls `completeOAuthCallback`. */
  const startOAuth = useCallback(async (provider) => {
    const pkce = await makePkcePair();
    sessionStore.beginOAuthFlow(pkce.verifier);
    sessionStore.setOauthReturn(window.location.pathname + window.location.search);
    const redirect = `${window.location.origin}/auth/callback`;
    window.location.assign(supabaseClient().oauthAuthorizeURL(provider, pkce.challenge, redirect));
  }, []);

  const completeOAuthCallback = useCallback(async (href) => {
    // Only a flow THIS tab started, within its TTL (session.js). The pending
    // flow is consumed whatever happens next — a code is single-use, and a
    // verifier left behind after a failed exchange is a standing invitation
    // for someone else's callback to be completed with it.
    const flow = sessionStore.pendingOAuthFlow();
    let s;
    try {
      if (!flow) throw new AuthError(0, 'No sign-in was started in this tab (or it expired) — the callback was not used. Start the sign-in again.');
      s = await supabaseClient().completeOAuth(href, flow.verifier);
    } finally {
      sessionStore.clearOAuthFlow();
    }
    return establish(s);
  }, [establish]);

  const signOut = useCallback(() => {
    // Revoke the refresh token server-side, not just locally. Fire-and-forget:
    // signing out must not wait on (or fail because of) the network.
    const access = sessionStore.accessToken;
    if (access) supabaseClient().logout(access).catch(() => { /* best effort; local state is cleared regardless */ });
    sessionStore.clearTokens();
    sessionStore.setTenantId(null);
    sessionStore.setOnboardingCompleted(false);
    sessionStore.clearOAuthFlow();
    releaseLocalData();
    setMe(null);
    setIsPlatformAdmin(null);
    setGate(GATE.onboarding);
  }, []);

  const switchTenant = useCallback(async (tenantId) => {
    sessionStore.setTenantId(tenantId || null);
    try { adopt(await api.get('/api/me')); } catch (e) { setLastError(e.message); }
  }, [adopt]);

  const completeOnboarding = useCallback(() => {
    sessionStore.setOnboardingCompleted(true);
    setGate(GATE.ready);
  }, []);

  const retryBootstrap = useCallback(async () => {
    setGate(GATE.loading);
    setConnectionStalled(false);
    await bootstrap();
  }, [bootstrap]);

  const refreshMe = useCallback(async () => {
    try { adopt(await api.get('/api/me')); } catch { /* keep what we have */ }
  }, [adopt]);

  const value = useMemo(() => {
    const role = me?.tenant?.role;
    return {
      gate, me, lastError, connectionStalled, isPlatformAdmin,
      accountEmail: me?.user?.email ?? null,
      tenantName: me?.tenant?.name ?? null,
      tenantId: me?.tenant?.id ?? sessionStore.tenantId,
      memberships: me?.memberships ?? [],
      role,
      canManageWorkspace: role === 'owner' || role === 'admin',
      hasSession: Boolean(sessionStore.accessToken),
      signIn, signUp, startOAuth, completeOAuthCallback, signOut, switchTenant,
      completeOnboarding, retryBootstrap, refreshMe,
    };
  }, [gate, me, lastError, connectionStalled, isPlatformAdmin, signIn, signUp, startOAuth,
      completeOAuthCallback, signOut, switchTenant, completeOnboarding, retryBootstrap, refreshMe]);

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

export function useAuth() {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error('useAuth must be used inside <AuthProvider>');
  return ctx;
}
