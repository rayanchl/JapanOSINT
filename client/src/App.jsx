import React from 'react';
import { Routes, Route, NavLink } from 'react-router-dom';
import MapPage from './components/map/MapPage';
import SourceDashboard from './components/dashboard/SourceDashboard';
import ApiStatusPanel from './components/panels/ApiStatusPanel';
import useDataSources from './hooks/useDataSources';

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

  return <span className="font-mono text-xs text-neon-cyan">{time}</span>;
}

function RadarIcon() {
  return (
    <svg
      width="24"
      height="24"
      viewBox="0 0 24 24"
      fill="none"
      className="inline-block mr-2"
    >
      <circle cx="12" cy="12" r="10" stroke="#00f0ff" strokeWidth="1.5" opacity="0.3" />
      <circle cx="12" cy="12" r="6" stroke="#00f0ff" strokeWidth="1.5" opacity="0.5" />
      <circle cx="12" cy="12" r="2" fill="#00f0ff" />
      <line
        x1="12"
        y1="12"
        x2="19"
        y2="5"
        stroke="#00f0ff"
        strokeWidth="1.5"
        strokeLinecap="round"
      >
        <animateTransform
          attributeName="transform"
          type="rotate"
          from="0 12 12"
          to="360 12 12"
          dur="3s"
          repeatCount="indefinite"
        />
      </line>
    </svg>
  );
}

export default function App() {
  const { sources, stats, isConnected, lastUpdate } = useDataSources();
  const [showApiStatus, setShowApiStatus] = React.useState(false);

  const activeSources = stats?.online ?? 0;

  return (
    <div className="h-screen w-screen flex flex-col bg-osint-bg text-gray-100 overflow-hidden">
      {/* Top Navigation Bar */}
      <nav className="flex items-center justify-between px-4 py-2 bg-osint-surface border-b border-osint-border flex-shrink-0 z-50">
        {/* Left: Logo + Nav Links */}
        <div className="flex items-center gap-6">
          <NavLink to="/" className="flex items-center text-lg font-bold tracking-wide">
            <RadarIcon />
            <span className="neon-text">Japan</span>
            <span className="text-gray-100">OSINT</span>
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
            onClick={() => setShowApiStatus((v) => !v)}
            className={`px-2.5 py-1 rounded text-xs font-medium border transition-colors ${
              showApiStatus
                ? 'bg-neon-cyan/15 text-neon-cyan border-neon-cyan/40'
                : 'bg-transparent text-gray-400 border-osint-border hover:text-neon-cyan hover:border-neon-cyan/40'
            }`}
            title="Show which APIs are working / configured"
          >
            APIs
          </button>
        </div>
      </nav>

      {/* Main Content */}
      <main className="flex-1 relative overflow-hidden">
        <Routes>
          <Route path="/" element={<MapPage />} />
          <Route path="/sources" element={<SourceDashboard sources={sources} stats={stats} />} />
        </Routes>

        {showApiStatus && (
          <div className="absolute top-3 right-3 z-40">
            <ApiStatusPanel onClose={() => setShowApiStatus(false)} />
          </div>
        )}
      </main>
    </div>
  );
}
