import React, { useEffect, useMemo, useState } from 'react';
import StatusBadge from '../ui/StatusBadge';
import LoadingSpinner from '../ui/LoadingSpinner';
import CameraDiscoveryThread from './CameraDiscoveryThread';
import FollowPanel from './FollowPanel';
import DatabaseExplorerTab from './DatabaseExplorerTab';
import DatabaseSchedulerTab from './DatabaseSchedulerTab';

const FILTERS = [
  { key: 'all', label: 'All' },
  { key: 'working', label: 'Working' },
  { key: 'online', label: 'Online' },
  { key: 'degraded', label: 'Degraded' },
  { key: 'offline', label: 'Offline' },
  { key: 'pending', label: 'Pending' },
  { key: 'missingKey', label: 'Missing key' },
];

function relativeTime(iso) {
  if (!iso) return 'never';
  const ts = new Date(iso).getTime();
  if (Number.isNaN(ts)) return 'never';
  const diff = Math.max(0, Date.now() - ts);
  const s = Math.floor(diff / 1000);
  if (s < 60) return `${s}s ago`;
  const m = Math.floor(s / 60);
  if (m < 60) return `${m}m ago`;
  const h = Math.floor(m / 60);
  if (h < 24) return `${h}h ago`;
  return `${Math.floor(h / 24)}d ago`;
}

function prettyJson(str) {
  if (!str) return '';
  try {
    return JSON.stringify(JSON.parse(str), null, 2);
  } catch {
    return str;
  }
}

function KeyPills({ api }) {
  if (!api.requiresKey) {
    return (
      <span className="px-1.5 py-0.5 rounded text-[9px] bg-gray-700/40 text-gray-400 border border-gray-600/30">
        No key
      </span>
    );
  }
  if (api.configured) {
    return (
      <span
        className="px-1.5 py-0.5 rounded text-[9px] bg-status-online/15 text-status-online border border-status-online/30"
        title={api.envVars.filter((v) => v.set).map((v) => v.name).join(', ')}
      >
        ✓ Configured
      </span>
    );
  }
  return (
    <span
      className="px-1.5 py-0.5 rounded text-[9px] bg-status-offline/15 text-status-offline border border-status-offline/30"
      title={`Missing: ${api.missingVars.join(', ')}`}
    >
      ✗ Missing key
    </span>
  );
}

function ProbeDetail({ api }) {
  if (api.status === 'pending') {
    return (
      <div className="mt-2 px-2 py-1.5 rounded border border-osint-border/40 bg-osint-bg/40 text-[10px] text-gray-400 italic">
        Awaiting first probe…
      </div>
    );
  }

  const hasRequest = api.probeRequestUrl || api.probeRequestMethod;
  const hasResponse =
    api.probeResponseStatus != null ||
    api.probeResponseBody ||
    api.probeResponseHeaders;

  if (!hasRequest && !hasResponse) return null;

  return (
    <div className="mt-2 space-y-2">
      {hasRequest && (
        <div className="rounded border border-osint-border/40 bg-black/40 p-2">
          <div className="text-[9px] uppercase tracking-wider text-neon-cyan mb-1">
            Request
          </div>
          <div className="font-mono text-[10px] text-gray-300 break-all">
            <span className="text-neon-green">
              {api.probeRequestMethod || 'GET'}
            </span>{' '}
            {api.probeRequestUrl}
          </div>
          {api.probeRequestHeaders && (
            <pre className="mt-1 font-mono text-[9.5px] text-gray-400 whitespace-pre-wrap break-all">
              {prettyJson(api.probeRequestHeaders)}
            </pre>
          )}
        </div>
      )}
      {hasResponse && (
        <div className="rounded border border-osint-border/40 bg-black/40 p-2">
          <div className="text-[9px] uppercase tracking-wider text-neon-cyan mb-1 flex items-center gap-2">
            <span>Response</span>
            {api.probeResponseStatus != null && (
              <span
                className={`font-mono ${
                  api.probeResponseStatus >= 200 && api.probeResponseStatus < 300
                    ? 'text-status-online'
                    : api.probeResponseStatus >= 400
                    ? 'text-status-offline'
                    : 'text-status-degraded'
                }`}
              >
                HTTP {api.probeResponseStatus}
              </span>
            )}
          </div>
          {api.probeResponseHeaders && (
            <pre className="font-mono text-[9.5px] text-gray-400 whitespace-pre-wrap break-all max-h-32 overflow-y-auto">
              {prettyJson(api.probeResponseHeaders)}
            </pre>
          )}
          {api.probeResponseBody && (
            <pre className="mt-1 font-mono text-[9.5px] text-gray-200 whitespace-pre-wrap break-all max-h-64 overflow-y-auto border-t border-osint-border/30 pt-1">
              {api.probeResponseBody}
            </pre>
          )}
        </div>
      )}
    </div>
  );
}

function ApiRow({ api, expanded, onToggle }) {
  return (
    <div className="border border-osint-border/40 rounded bg-osint-bg/40">
      <button
        type="button"
        onClick={onToggle}
        className="w-full grid grid-cols-12 gap-2 items-center px-2 py-1.5 text-left hover:bg-white/5 transition-colors"
      >
        <div className="col-span-5 truncate">
          <div className="text-[11px] text-gray-200 font-medium truncate">
            {api.name}
          </div>
          {api.nameJa && (
            <div className="text-[9px] text-gray-500 truncate">{api.nameJa}</div>
          )}
        </div>
        <div className="col-span-3">
          <StatusBadge type="status" value={api.status} />
        </div>
        <div className="col-span-3 flex justify-end">
          <KeyPills api={api} />
        </div>
        <div className="col-span-1 text-gray-500 text-xs text-right">
          {expanded ? '▾' : '▸'}
        </div>
      </button>

      {expanded && (
        <div className="px-3 pb-2 pt-1 text-[10px] text-gray-400 border-t border-osint-border/40 space-y-1">
          {api.description && (
            <div className="text-gray-300">{api.description}</div>
          )}
          <div className="grid grid-cols-2 gap-x-4 gap-y-0.5 font-mono">
            <div>
              Category: <span className="text-gray-200">{api.category}</span>
            </div>
            <div>
              Type: <span className="text-gray-200">{api.type}</span>
            </div>
            <div>
              Last check:{' '}
              <span className="text-gray-200">{relativeTime(api.lastCheck)}</span>
            </div>
            <div>
              Last success:{' '}
              <span className="text-gray-200">
                {relativeTime(api.lastSuccess)}
              </span>
            </div>
            {api.responseTimeMs != null && (
              <div>
                Response:{' '}
                <span className="text-gray-200">{api.responseTimeMs} ms</span>
              </div>
            )}
            {api.recordsCount != null && (
              <div>
                Records:{' '}
                <span className="text-gray-200">{api.recordsCount}</span>
              </div>
            )}
          </div>
          {api.errorMessage && (
            <div className="text-status-offline break-words">
              Error: {api.errorMessage}
            </div>
          )}
          {api.envVars && api.envVars.length > 0 && (
            <div className="pt-1">
              <div className="text-gray-500 uppercase tracking-wider text-[9px] mb-0.5">
                Environment variables
              </div>
              <div className="flex flex-wrap gap-1">
                {api.envVars.map((v) => (
                  <span
                    key={v.name}
                    className={`px-1.5 py-0.5 rounded border font-mono text-[9px] ${
                      v.set
                        ? 'bg-status-online/10 text-status-online border-status-online/30'
                        : v.role === 'optional'
                        ? 'bg-gray-700/40 text-gray-400 border-gray-600/30'
                        : 'bg-status-offline/10 text-status-offline border-status-offline/30'
                    }`}
                    title={`${v.role}${v.set ? ' · set' : ' · not set'}`}
                  >
                    {v.set ? '✓ ' : '✗ '}
                    {v.name}
                  </span>
                ))}
              </div>
            </div>
          )}
          {api.url && (
            <div className="truncate">
              <a
                href={api.url}
                target="_blank"
                rel="noreferrer"
                className="text-neon-cyan hover:underline"
              >
                {api.url}
              </a>
            </div>
          )}
          <ProbeDetail api={api} />
        </div>
      )}
    </div>
  );
}

export default function SourcesPanel({ onClose }) {
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [filter, setFilter] = useState('all');
  const [search, setSearch] = useState('');
  const [expandedId, setExpandedId] = useState(null);
  // Tabs: sources | discovery | follow | tables | scheduler
  const [tab, setTab] = useState('sources');

  useEffect(() => {
    let cancelled = false;
    const load = async () => {
      try {
        const res = await fetch('/api/status');
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const json = await res.json();
        if (!cancelled) {
          setData(json);
          setError(null);
        }
      } catch (err) {
        if (!cancelled) setError(err.message || 'Failed to load');
      } finally {
        if (!cancelled) setLoading(false);
      }
    };
    load();
    const t = setInterval(load, 30_000);
    return () => {
      cancelled = true;
      clearInterval(t);
    };
  }, []);

  const filtered = useMemo(() => {
    if (!data) return [];
    const q = search.trim().toLowerCase();
    return data.apis
      .filter((a) => {
        if (filter === 'working') {
          return a.status === 'online' && (!a.requiresKey || a.configured);
        }
        if (filter === 'online') return a.status === 'online';
        if (filter === 'degraded') return a.status === 'degraded';
        if (filter === 'offline') return a.status === 'offline';
        if (filter === 'pending') return a.status === 'pending';
        if (filter === 'missingKey') return a.requiresKey && !a.configured;
        return true;
      })
      .filter((a) => {
        if (!q) return true;
        return (
          a.name.toLowerCase().includes(q) ||
          a.id.toLowerCase().includes(q) ||
          (a.category && a.category.toLowerCase().includes(q))
        );
      })
      .sort((a, b) => {
        const rank = (x) => {
          if (x.requiresKey && !x.configured) return 5;
          if (x.status === 'online') return 0;
          if (x.status === 'degraded') return 1;
          if (x.status === 'offline') return 2;
          if (x.status === 'pending') return 3;
          return 4;
        };
        const r = rank(a) - rank(b);
        return r !== 0 ? r : a.name.localeCompare(b.name);
      });
  }, [data, filter, search]);

  const summary = data?.summary;

  return (
    <div className="glass-panel flex flex-col w-[720px] max-w-[95vw] max-h-[80vh] shadow-xl">
      <div className="flex items-center justify-between px-4 py-2 border-b border-osint-border/50 flex-shrink-0">
        <div className="flex items-center gap-3 flex-wrap">
          {[
            { id: 'sources',   label: 'Sources' },
            { id: 'discovery', label: 'Camera Discovery' },
            { id: 'follow',    label: 'Follow' },
            { id: 'tables',    label: 'DB Tables' },
            { id: 'scheduler', label: 'Scheduler' },
          ].map((t) => (
            <button
              key={t.id}
              type="button"
              onClick={() => setTab(t.id)}
              className={`text-sm font-bold transition-colors ${
                tab === t.id ? 'text-neon-cyan' : 'text-gray-500 hover:text-gray-300'
              }`}
            >
              {t.label}
            </button>
          ))}
          {tab === 'sources' && summary && (
            <span className="text-[10px] text-gray-400 font-mono">
              {summary.working}/{summary.total} working
            </span>
          )}
        </div>
        <button
          type="button"
          onClick={onClose}
          className="text-gray-400 hover:text-neon-cyan text-sm leading-none"
          aria-label="Close"
        >
          ✕
        </button>
      </div>

      {tab === 'discovery' && (
        <div className="flex-1 flex flex-col min-h-0">
          <CameraDiscoveryThread />
        </div>
      )}

      {tab === 'follow' && (
        <div className="flex-1 flex flex-col min-h-0">
          <FollowPanel embedded />
        </div>
      )}

      {tab === 'tables' && (
        <div className="flex-1 flex flex-col min-h-0">
          <DatabaseExplorerTab />
        </div>
      )}

      {tab === 'scheduler' && (
        <div className="flex-1 flex flex-col min-h-0">
          <DatabaseSchedulerTab />
        </div>
      )}

      {tab === 'sources' && (
      <>


      {summary && (
        <div className="grid grid-cols-5 gap-2 px-4 py-2 text-[10px] border-b border-osint-border/40 flex-shrink-0">
          <div>
            <div className="text-gray-500">Online</div>
            <div className="font-mono text-status-online">{summary.online}</div>
          </div>
          <div>
            <div className="text-gray-500">Degraded</div>
            <div className="font-mono text-status-degraded">
              {summary.degraded}
            </div>
          </div>
          <div>
            <div className="text-gray-500">Offline</div>
            <div className="font-mono text-status-offline">
              {summary.offline}
            </div>
          </div>
          <div>
            <div className="text-gray-500">Pending</div>
            <div className="font-mono text-gray-400">
              {summary.pending ?? 0}
            </div>
          </div>
          <div>
            <div className="text-gray-500">Keys set</div>
            <div className="font-mono text-neon-cyan">
              {summary.configured}/{summary.requiresKey}
            </div>
          </div>
        </div>
      )}

      <div className="px-3 py-2 border-b border-osint-border/40 space-y-2 flex-shrink-0">
        <input
          type="text"
          placeholder="Search sources…"
          value={search}
          onChange={(e) => setSearch(e.target.value)}
          className="w-full bg-osint-bg/60 border border-osint-border rounded px-2 py-1 text-xs text-gray-200 placeholder-gray-500 focus:outline-none focus:border-neon-cyan/50"
        />
        <div className="flex flex-wrap gap-1">
          {FILTERS.map((f) => (
            <button
              key={f.key}
              type="button"
              onClick={() => setFilter(f.key)}
              className={`px-2 py-0.5 rounded text-[10px] border transition-colors ${
                filter === f.key
                  ? 'bg-neon-cyan/15 text-neon-cyan border-neon-cyan/40'
                  : 'bg-osint-bg/40 text-gray-400 border-osint-border hover:text-gray-200'
              }`}
            >
              {f.label}
            </button>
          ))}
        </div>
      </div>

      <div className="flex-1 overflow-y-auto px-3 py-2 space-y-1">
        {loading && !data && (
          <div className="flex items-center justify-center py-6">
            <LoadingSpinner />
          </div>
        )}
        {error && !data && (
          <div className="text-status-offline text-xs px-2 py-3">
            Failed to load sources: {error}
          </div>
        )}
        {data && filtered.length === 0 && (
          <div className="text-gray-500 text-xs px-2 py-4 text-center">
            No sources match the current filter.
          </div>
        )}
        {data &&
          filtered.map((api) => (
            <ApiRow
              key={api.id}
              api={api}
              expanded={expandedId === api.id}
              onToggle={() =>
                setExpandedId((id) => (id === api.id ? null : api.id))
              }
            />
          ))}
      </div>

      {data && (
        <div className="px-3 py-1.5 border-t border-osint-border/40 text-[10px] text-gray-500 flex items-center justify-between flex-shrink-0">
          <span>Auto-refresh 30s</span>
          <span className="font-mono">
            Updated {relativeTime(data.timestamp)}
          </span>
        </div>
      )}
      </>
      )}
    </div>
  );
}
