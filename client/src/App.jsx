import React from 'react';
import { Routes, Route, NavLink } from 'react-router-dom';
import MapPage from './components/map/MapPage';
import SourceDashboard from './components/dashboard/SourceDashboard';
import SourcesPanel from './components/panels/SourcesPanel';
import FollowPanel from './components/panels/FollowPanel';
import useDataSources from './hooks/useDataSources';

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

function useTheme() {
  const [theme, setTheme] = React.useState(readInitialTheme);
  React.useEffect(() => {
    const root = document.documentElement;
    root.setAttribute('data-theme', theme);
    try { window.localStorage.setItem(THEME_STORAGE_KEY, theme); } catch { /* ignore */ }
  }, [theme]);
  const toggle = React.useCallback(
    () => setTheme((t) => (t === 'dark' ? 'light' : 'dark')),
    [],
  );
  return { theme, toggle };
}

function JSTClock() {
  const [time, setTime] = React.useState('');

  React.useEffect(() => {
    const update = () => {
      const now = new Date();
      const jst = new Intl.DateTimeFormat('en-GB', {
        timeZone: 'Asia/Tokyo',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
        hour12: false,
      }).format(now);
      const date = new Intl.DateTimeFormat('en-GB', {
        timeZone: 'Asia/Tokyo',
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
      }).format(now);
      setTime(`${date} ${jst} JST`);
    };
    update();
    const interval = setInterval(update, 1000);
    return () => clearInterval(interval);
  }, []);

  return <span className="font-mono text-xs text-gray-300">{time}</span>;
}

export default function App() {
  const { sources, stats, isConnected, lastUpdate } = useDataSources();
  const { theme, toggle: toggleTheme } = useTheme();
  const [showSources, setShowSources] = React.useState(false);
  const [showFollow, setShowFollow] = React.useState(false);

  const openSources = () => { setShowFollow(false); setShowSources((v) => !v); };
  const openFollow = () => { setShowSources(false); setShowFollow((v) => !v); };

  const activeSources = stats?.online ?? 0;

  return (
    <div className="h-screen w-screen flex flex-col bg-osint-bg text-gray-100 overflow-hidden">
      {/* Top Navigation Bar */}
      <nav className="flex items-center justify-between px-4 py-2 bg-osint-surface border-b border-osint-border flex-shrink-0 z-50">
        {/* Left: Logo + Nav Links */}
        <div className="flex items-center gap-6">
          <NavLink to="/" className="flex items-center text-lg font-bold tracking-wide">
            <span className="text-gray-100">Japan</span>
            <span className="text-gray-400">OSINT</span>
          </NavLink>

          <div className="flex items-center gap-1 ml-4">
            <NavLink
              to="/"
              end
              className={({ isActive }) =>
                `px-3 py-1.5 rounded text-sm font-medium transition-colors ${
                  isActive
                    ? 'bg-neon-cyan/10 text-neon-cyan border border-neon-cyan/30'
                    : 'text-gray-400 hover:text-gray-200 hover:bg-white/5'
                }`
              }
            >
              Map
            </NavLink>
            <NavLink
              to="/sources"
              className={({ isActive }) =>
                `px-3 py-1.5 rounded text-sm font-medium transition-colors ${
                  isActive
                    ? 'bg-neon-cyan/10 text-neon-cyan border border-neon-cyan/30'
                    : 'text-gray-400 hover:text-gray-200 hover:bg-white/5'
                }`
              }
            >
              Sources
            </NavLink>
          </div>
        </div>

        {/* Right: Status Info */}
        <div className="flex items-center gap-5 text-xs">
          <JSTClock />

          <div className="flex items-center gap-2">
            <div
              className={`w-2 h-2 rounded-full ${
                isConnected ? 'status-online pulse-live' : 'status-offline'
              }`}
            />
            <span className={isConnected ? 'text-status-online' : 'text-status-offline'}>
              {isConnected ? 'LIVE' : 'DISCONNECTED'}
            </span>
          </div>

          <div className="flex items-center gap-1.5 text-gray-400">
            <span className="font-mono text-neon-green">{activeSources}</span>
            <span>active sources</span>
          </div>

          {lastUpdate && (
            <div className="text-gray-500 font-mono">
              Last: {new Date(lastUpdate).toLocaleTimeString('en-GB', { timeZone: 'Asia/Tokyo' })}
            </div>
          )}

          <button
            type="button"
            onClick={openSources}
            className={`px-2.5 py-1 rounded text-xs font-medium border transition-colors ${
              showSources
                ? 'bg-neon-cyan/15 text-neon-cyan border-neon-cyan/40'
                : 'bg-transparent text-gray-400 border-osint-border hover:text-neon-cyan hover:border-neon-cyan/40'
            }`}
            title="Show all sources with live probe detail"
          >
            Sources
          </button>

          <button
            type="button"
            onClick={openFollow}
            className={`px-2.5 py-1 rounded text-xs font-medium border transition-colors ${
              showFollow
                ? 'bg-neon-cyan/15 text-neon-cyan border-neon-cyan/40'
                : 'bg-transparent text-gray-400 border-osint-border hover:text-neon-cyan hover:border-neon-cyan/40'
            }`}
            title="Follow live collector HTTP requests"
          >
            Follow
          </button>

          <button
            type="button"
            onClick={toggleTheme}
            className="px-2.5 py-1 rounded text-xs font-medium border bg-transparent text-gray-400 border-osint-border hover:text-neon-cyan hover:border-neon-cyan/40 transition-colors"
            title={theme === 'dark' ? 'Switch to light theme' : 'Switch to dark theme'}
            aria-label="Toggle color theme"
          >
            {theme === 'dark' ? '\u2600' : '\u263D'}
          </button>
        </div>
      </nav>

      {/* Main Content */}
      <main className="flex-1 relative overflow-hidden">
        <Routes>
          <Route path="/" element={<MapPage />} />
          <Route path="/sources" element={<SourceDashboard sources={sources} stats={stats} />} />
        </Routes>

        {showSources && (
          <div className="absolute top-3 right-3 z-40">
            <SourcesPanel onClose={() => setShowSources(false)} />
          </div>
        )}

        {showFollow && (
          <div className="absolute top-3 right-3 z-40">
            <FollowPanel onClose={() => setShowFollow(false)} />
          </div>
        )}
      </main>
    </div>
  );
}
