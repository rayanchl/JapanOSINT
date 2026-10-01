import React, { Suspense } from 'react';
import { Routes, Route, Navigate, useLocation } from 'react-router-dom';
import { AuthProvider, useAuth, GATE } from './auth/AuthContext.jsx';
import { sessionStore } from './auth/session.js';
import AppShell from './components/shell/AppShell.jsx';
import ConsoleHubPage from './components/shell/ConsoleHubPage.jsx';
import OnboardingFlow from './components/auth/OnboardingFlow.jsx';
import AuthCallback from './components/auth/AuthCallback.jsx';
import MapPage from './components/map/MapPage';
import SourceDashboard from './components/dashboard/SourceDashboard';
import SearchPage from './components/search/SearchPage';
import EntitiesPage from './components/entities/EntitiesPage';
import EntityProfile from './components/entities/EntityProfile';
import CameraDiscoveryThread from './components/panels/CameraDiscoveryThread.jsx';
import DatabasePanel from './components/panels/DatabasePanel.jsx';
import FollowPanel from './components/panels/FollowPanel.jsx';
import useDataSources from './hooks/useDataSources';
import { Button, LoadingState, Page, EmptyState } from './components/ui/kit.jsx';
import { ServerSettings } from './components/auth/OnboardingFlow.jsx';

const lazy = (loader) => React.lazy(loader);
const IntelPage = lazy(() => import('./components/intel/IntelPage.jsx'));
const IntelSourceItemsPage = lazy(() => import('./components/intel/IntelSourceItemsPage.jsx'));
const IntelItemPage = lazy(() => import('./components/intel/IntelItemPage.jsx'));
const CasesPage = lazy(() => import('./components/cases/CasesPage.jsx'));
const CaseDetailPage = lazy(() => import('./components/cases/CaseDetailPage.jsx'));
const SavedPage = lazy(() => import('./components/saved/SavedPage.jsx'));
const TimelinePage = lazy(() => import('./components/timeline/TimelinePage.jsx'));
const AlertInboxPage = lazy(() => import('./components/alerts/AlertInboxPage.jsx'));
const AlertsPage = lazy(() => import('./components/alerts/AlertsPage.jsx'));
const AOIPage = lazy(() => import('./components/alerts/AOIPage.jsx'));
const WatchlistsPage = lazy(() => import('./components/alerts/WatchlistsPage.jsx'));
const BreachMonitorsPage = lazy(() => import('./components/alerts/BreachMonitorsPage.jsx'));
const SavedSearchesPage = lazy(() => import('./components/alerts/SavedSearchesPage.jsx'));
const ApiKeysPage = lazy(() => import('./components/console/ApiKeysPage.jsx'));
const WorkspacePage = lazy(() => import('./components/console/WorkspacePage.jsx'));
const SettingsPage = lazy(() => import('./components/console/SettingsPage.jsx'));
const AdminPage = lazy(() => import('./components/console/AdminPage.jsx'));

const THEME_STORAGE_KEY = 'osint:theme';

function readInitialTheme() {
  if (typeof window === 'undefined') return 'dark';
  try {
    const stored = window.localStorage.getItem(THEME_STORAGE_KEY);
    if (stored === 'light' || stored === 'dark') return stored;
  } catch { /* ignore */ }
  if (window.matchMedia?.('(prefers-color-scheme: light)').matches) return 'light';
  return 'dark';
}

export function useTheme() {
  const [theme, setTheme] = React.useState(readInitialTheme);
  React.useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
    try { window.localStorage.setItem(THEME_STORAGE_KEY, theme); } catch { /* ignore */ }
  }, [theme]);
  // Settings › Appearance changes the theme too; keep the sidebar toggle in sync.
  React.useEffect(() => {
    const onChange = (e) => { const t = e?.detail; if (t === 'light' || t === 'dark') setTheme(t); };
    window.addEventListener('osint:theme-change', onChange);
    return () => window.removeEventListener('osint:theme-change', onChange);
  }, []);
  const toggle = React.useCallback(() => setTheme((t) => (t === 'dark' ? 'light' : 'dark')), []);
  return { theme, toggle, setTheme };
}

/** Launch spinner with the backend-unreachable escape (iOS `LoadingGate`). */
function LoadingGate() {
  const auth = useAuth();
  const [slow, setSlow] = React.useState(false);
  const [showSettings, setShowSettings] = React.useState(false);
  React.useEffect(() => { const t = setTimeout(() => setSlow(true), 3000); return () => clearTimeout(t); }, []);
  return (
    <div className="h-screen w-screen flex items-center justify-center bg-osint-bg text-osint-text p-6">
      <div className="w-full max-w-sm text-center space-y-3">
        <LoadingState label="Connecting…" />
        {auth.connectionStalled && (
          <div>
            <div className="text-sm font-semibold">Can't reach the backend</div>
            <div className="text-xs text-osint-muted">The API on this origin is not answering. Check that the server is running.</div>
          </div>
        )}
        {(auth.connectionStalled || slow) && (
          <div className="flex justify-center gap-2">
            <Button onClick={auth.retryBootstrap}>Retry connection</Button>
            <Button variant="ghost" onClick={() => setShowSettings((v) => !v)}>Server settings</Button>
            <Button variant="ghost" onClick={auth.signOut}>Sign in again</Button>
          </div>
        )}
        {showSettings && <div className="text-left"><ServerSettings /></div>}
      </div>
    </div>
  );
}

/** Operator-only surfaces render the server's own refusal, not a blank. */
function OperatorOnly({ children }) {
  const auth = useAuth();
  if (auth.isPlatformAdmin === false) {
    return (
      <Page title="Platform operators only">
        <EmptyState title="This surface affects every workspace on the host.">
          The server answered 403 for this account. Operators are listed in PLATFORM_OPERATOR_EMAILS / PLATFORM_OPERATOR_IDS.
        </EmptyState>
      </Page>
    );
  }
  return children;
}

function ManageOnly({ children }) {
  const auth = useAuth();
  if (!auth.canManageWorkspace) {
    return (
      <Page title="Workspace">
        <EmptyState title="Owner or admin role required.">Your role in this workspace is {auth.role || 'unknown'}.</EmptyState>
      </Page>
    );
  }
  return children;
}

function Fallback() { return <LoadingState />; }

function Shell() {
  const { sources, stats, isConnected, lastUpdate, error: sourcesError } = useDataSources();
  const { theme, toggle } = useTheme();
  const status = { isConnected, activeSources: stats?.online, sourcesError, lastUpdate };

  return (
    <AppShell theme={theme} onToggleTheme={toggle} status={status}>
      <Suspense fallback={<Fallback />}>
        <Routes>
          <Route path="/" element={<MapPage />} />
          <Route path="/intel" element={<IntelPage />} />
          <Route path="/intel/sources/:id" element={<IntelSourceItemsPage />} />
          <Route path="/intel/items/:uid" element={<IntelItemPage />} />
          <Route path="/search" element={<SearchPage />} />
          <Route path="/cases" element={<CasesPage />} />
          <Route path="/cases/:id" element={<CaseDetailPage />} />
          <Route path="/entities" element={<EntitiesPage />} />
          <Route path="/entities/:type/:id" element={<EntityProfile />} />
          <Route path="/saved" element={<SavedPage />} />
          <Route path="/timeline" element={<TimelinePage />} />

          <Route path="/console" element={<ConsoleHubPage />} />
          <Route path="/console/sources" element={<SourceDashboard sources={sources} stats={stats} pollError={sourcesError} lastUpdate={lastUpdate} />} />
          <Route path="/console/cameras" element={<div className="h-full overflow-hidden"><CameraDiscoveryThread /></div>} />
          <Route path="/console/inbox" element={<AlertInboxPage />} />
          <Route path="/console/alerts" element={<AlertsPage />} />
          <Route path="/console/aoi" element={<AOIPage />} />
          <Route path="/console/watchlists" element={<WatchlistsPage />} />
          <Route path="/console/breach-monitors" element={<BreachMonitorsPage />} />
          <Route path="/console/saved-searches" element={<SavedSearchesPage />} />
          <Route path="/console/api-keys" element={<ApiKeysPage />} />
          <Route path="/console/workspace" element={<ManageOnly><WorkspacePage /></ManageOnly>} />
          <Route path="/console/settings" element={<SettingsPage />} />
          <Route path="/console/admin" element={<OperatorOnly><AdminPage /></OperatorOnly>} />
          <Route path="/console/database" element={<OperatorOnly><div className="h-full overflow-auto p-4 flex justify-center"><DatabasePanel /></div></OperatorOnly>} />
          <Route path="/console/scheduler" element={<OperatorOnly><div className="h-full overflow-auto p-4 flex justify-center"><DatabasePanel initialTab="scheduler" /></div></OperatorOnly>} />
          <Route path="/console/follow" element={<OperatorOnly><div className="h-full overflow-hidden"><FollowPanel embedded /></div></OperatorOnly>} />

          {/* Old top-level paths */}
          <Route path="/sources" element={<Navigate to="/console/sources" replace />} />
          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </Suspense>
    </AppShell>
  );
}

/** A social sign-in landing. Supabase only returns to `redirect_to` when that
 *  URL is on the project's allowlist; otherwise it falls back to the Site URL,
 *  so the `?code=` (or `?error=`) can arrive on ANY path. Dropping it there
 *  left the user on the sign-in screen as if the button did nothing — so a
 *  pending PKCE verifier plus a provider result is a callback wherever it is. */
function isOAuthLanding(location) {
  if (location.pathname === '/auth/callback') return true;
  if (!sessionStore.pkceVerifier) return false;
  const q = new URLSearchParams(location.search);
  const h = new URLSearchParams(location.hash.replace(/^#/, ''));
  return q.has('code') || q.has('error') || q.has('error_description') || h.has('access_token') || h.has('error');
}

function Gate() {
  const auth = useAuth();
  const location = useLocation();
  if (isOAuthLanding(location)) return <AuthCallback />;
  switch (auth.gate) {
    case GATE.loading: return <LoadingGate />;
    case GATE.onboarding: return <OnboardingFlow />;
    default: return <Shell />;
  }
}

export default function App() {
  return (
    <AuthProvider>
      <Gate />
    </AuthProvider>
  );
}
