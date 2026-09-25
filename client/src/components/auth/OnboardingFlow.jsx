import React, { useState } from 'react';
import { useAuth } from '../../auth/AuthContext.jsx';
import { sessionStore, supabaseClient } from '../../auth/session.js';
import { OAUTH_PROVIDERS, DEFAULT_SUPABASE_URL, DEFAULT_SUPABASE_ANON_KEY } from '../../auth/supabase.js';
import { Button, Input, Field, ErrorNotice, Spinner, cx } from '../ui/kit.jsx';

/**
 * Connect & sign-in gate — the web counterpart of the iOS `OnboardingFlow`
 * (connect → auth → workspace → finish). The showcase panels are dropped:
 * the browser already has the map one click away.
 */
const STEPS = ['auth', 'workspace', 'finish'];

export default function OnboardingFlow() {
  const auth = useAuth();
  const [step, setStep] = useState(auth.me ? 'workspace' : 'auth');

  return (
    <div className="h-screen w-screen overflow-auto bg-osint-bg text-osint-text">
      <div className="min-h-full flex flex-col items-center justify-center p-6">
        <div className="w-full max-w-md">
          <div className="text-center mb-8">
            <div className="font-mono text-3xl font-bold tracking-tight">
              <span>Japan</span><span className="text-accent">OSINT</span>
            </div>
            <div className="text-xs text-osint-muted mt-1">
              Real-time open-source intelligence, fused onto one tactical surface.
            </div>
          </div>

          <StepDots current={step} />

          {step === 'auth' && <AuthStep onDone={() => setStep('workspace')} />}
          {step === 'workspace' && <WorkspaceStep onDone={() => setStep('finish')} />}
          {step === 'finish' && <FinishStep />}
        </div>
      </div>
    </div>
  );
}

function StepDots({ current }) {
  return (
    <div className="flex items-center justify-center gap-1.5 mb-6">
      {STEPS.map((s) => (
        <span key={s} className={cx('h-1.5 rounded-full transition-all', s === current ? 'w-6 bg-accent' : 'w-1.5 bg-osint-border-bright')} />
      ))}
    </div>
  );
}

function ProviderLogo({ id }) {
  // Simple inline marks so no external image is needed.
  switch (id) {
    case 'google': return <span className="font-bold" style={{ color: '#4285F4' }}>G</span>;
    case 'github': return <span className="font-bold">GH</span>;
    case 'twitter': return <span className="font-bold">𝕏</span>;
    case 'facebook': return <span className="font-bold">f</span>;
    case 'apple': return <span className="font-bold"></span>;
    default: return null;
  }
}

function AuthStep({ onDone }) {
  const auth = useAuth();
  const [mode, setMode] = useState('signin');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState(null);
  const [notice, setNotice] = useState(null);
  const [showServer, setShowServer] = useState(false);
  // Ask the project which providers are actually enabled, so a button is
  // never offered for a provider that would only answer with an error.
  // null = not known (request failed) → every provider is shown, with a note.
  const [settings, setSettings] = useState(undefined);
  React.useEffect(() => {
    let alive = true;
    supabaseClient().settings().then((s) => { if (alive) setSettings(s); }, () => { if (alive) setSettings(null); });
    return () => { alive = false; };
  }, []);
  const providers = settings ? OAUTH_PROVIDERS.filter((p) => settings.providers.includes(p.id)) : OAUTH_PROVIDERS;

  const submit = async (e) => {
    e.preventDefault();
    setBusy(true); setError(null); setNotice(null);
    try {
      if (mode === 'signin') await auth.signIn(email.trim(), password);
      else await auth.signUp(email.trim(), password);
      onDone();
    } catch (err) {
      if (err?.status === 200) setNotice(err.message);
      else setError(err);
    } finally { setBusy(false); }
  };

  return (
    <div className="space-y-4">
      <div>
        <h2 className="font-mono text-lg font-semibold">Sign in</h2>
        <p className="text-xs text-osint-muted">
          Authenticate with your Supabase account. The server provisions a personal workspace on first sign-in.
        </p>
      </div>

      <form onSubmit={submit} className="space-y-3">
        <Field label="Email">
          <Input type="email" autoComplete="email" value={email} onChange={(e) => setEmail(e.target.value)} required placeholder="you@example.com" />
        </Field>
        <Field label="Password">
          <Input type="password" autoComplete={mode === 'signin' ? 'current-password' : 'new-password'} value={password} onChange={(e) => setPassword(e.target.value)} required minLength={6} placeholder="••••••••" />
        </Field>
        {error && <ErrorNotice error={error} title="Sign-in failed" />}
        {notice && <div className="text-xs text-accent border border-accent/40 bg-accent/10 rounded px-3 py-2">{notice}</div>}
        <Button type="submit" variant="primary" size="lg" busy={busy} className="w-full">
          {mode === 'signin' ? 'Sign in' : 'Create account'}
        </Button>
        <button type="button" className="w-full text-xs text-osint-muted hover:text-osint-text" onClick={() => setMode(mode === 'signin' ? 'signup' : 'signin')}>
          {mode === 'signin' ? 'No account yet? Create one' : 'Already have an account? Sign in'}
        </button>
      </form>

      {settings === undefined && <div className="text-[11px] text-osint-muted text-center">Checking which sign-in providers this project enables…</div>}
      {settings === null && <div className="text-[11px] text-osint-muted text-center">Could not read the project's provider list; every provider is shown, but only the ones enabled in Supabase will work.</div>}
      {settings?.signupDisabled && mode === 'signup' && <div className="text-[11px] text-accent text-center">Sign-up is disabled on this project; ask an admin for an invite.</div>}

      {providers.length > 0 && (
        <div className="flex items-center gap-3 text-[10px] uppercase tracking-wider text-osint-muted">
          <span className="h-px flex-1 bg-osint-border" />or continue with<span className="h-px flex-1 bg-osint-border" />
        </div>
      )}

      <div className="grid grid-cols-1 gap-2">
        {providers.map((p) => (
          <button
            key={p.id}
            type="button"
            onClick={() => auth.startOAuth(p.id).catch((err) => setError(err))}
            className="flex items-center justify-center gap-2 rounded-md py-2 text-sm font-medium transition-opacity hover:opacity-90"
            style={{ background: p.bg, color: p.fg, border: p.border ? '1px solid #d0d5dd' : '1px solid transparent' }}
          >
            <ProviderLogo id={p.id} />
            Continue with {p.label}
          </button>
        ))}
      </div>

      <button type="button" onClick={() => setShowServer((v) => !v)} className="text-xs text-osint-muted hover:text-osint-text w-full text-center">
        Server settings {showServer ? '▴' : '▾'}
      </button>
      {showServer && <ServerSettings />}
    </div>
  );
}

/** Supabase project override (self-host users). The backend itself is the
 *  same origin as this page, so there is no backend URL to edit here. */
export function ServerSettings() {
  const [url, setUrl] = useState(sessionStore.supabaseURL);
  const [anon, setAnon] = useState(sessionStore.supabaseAnonKey);
  const [saved, setSaved] = useState(false);
  const save = () => {
    sessionStore.setSupabaseConfig(
      url.trim() === DEFAULT_SUPABASE_URL ? null : url.trim(),
      anon.trim() === DEFAULT_SUPABASE_ANON_KEY ? null : anon.trim(),
    );
    setSaved(true); setTimeout(() => setSaved(false), 1500);
  };
  return (
    <div className="rounded-md border border-osint-border bg-osint-surface p-3 space-y-2">
      <Field label="Supabase project URL" hint="Only change this for a self-hosted project. The API is served from this page's own origin.">
        <Input mono value={url} onChange={(e) => setUrl(e.target.value)} placeholder={DEFAULT_SUPABASE_URL} />
      </Field>
      <Field label="Supabase anon key (publishable, not a secret)">
        <Input mono value={anon} onChange={(e) => setAnon(e.target.value)} placeholder={DEFAULT_SUPABASE_ANON_KEY} />
      </Field>
      <div className="flex justify-end">
        <Button size="sm" onClick={save}>{saved ? 'Saved' : 'Save'}</Button>
      </div>
    </div>
  );
}

function WorkspaceStep({ onDone }) {
  const auth = useAuth();
  const [busy, setBusy] = useState(null);
  const ms = auth.memberships;
  return (
    <div className="space-y-4">
      <div>
        <h2 className="font-mono text-lg font-semibold">Workspace</h2>
        <p className="text-xs text-osint-muted">
          Everyone you invite shares one workspace. Analysts, admins and developers all read the same collected data — the role only scopes what each can change.
        </p>
      </div>
      <div className="rounded-md border border-osint-border bg-osint-surface divide-y divide-osint-border">
        {ms.length === 0 && (
          <div className="p-3 text-xs text-osint-muted">
            {auth.tenantName ? `Active workspace: ${auth.tenantName}` : 'No workspace membership was returned by the server.'}
          </div>
        )}
        {ms.map((m) => {
          const active = m.id === auth.tenantId;
          return (
            <button
              key={m.id}
              type="button"
              disabled={busy != null}
              onClick={async () => { setBusy(m.id); await auth.switchTenant(m.id); setBusy(null); }}
              className={cx('w-full flex items-center justify-between px-3 py-2 text-left', active ? 'text-accent' : 'text-osint-text hover:bg-white/5')}
            >
              <div>
                <div className="text-sm font-medium">{m.name}</div>
                <div className="text-[11px] text-osint-muted font-mono">{m.slug} · {m.role || 'member'} · {m.plan || 'free'}</div>
              </div>
              {busy === m.id ? <Spinner size={12} /> : active ? <span className="text-xs font-mono">active</span> : null}
            </button>
          );
        })}
      </div>
      <Button variant="primary" size="lg" className="w-full" onClick={onDone}>Continue</Button>
    </div>
  );
}

function FinishStep() {
  const auth = useAuth();
  return (
    <div className="space-y-4 text-center">
      <h2 className="font-mono text-lg font-semibold">You're set</h2>
      <p className="text-xs text-osint-muted">
        New data lands on the Map and in the Intel feed as collectors run. Set up alert rules under Console → Alerts to get notified on matches.
      </p>
      <div className="text-xs text-osint-muted font-mono">
        {auth.accountEmail}{auth.tenantName ? ` · ${auth.tenantName}` : ''}
      </div>
      <Button variant="primary" size="lg" className="w-full" onClick={auth.completeOnboarding}>Enter JapanOSINT</Button>
    </div>
  );
}
