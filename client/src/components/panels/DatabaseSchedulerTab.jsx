import React, { useEffect, useMemo, useState } from 'react';
import apiUrl from '../../utils/apiUrl.js';

function relativeTime(iso) {
  if (!iso) return 'never';
  const ts = new Date(iso).getTime();
  if (Number.isNaN(ts)) return 'never';
  const now = Date.now();
  const diff = Math.abs(now - ts);
  const future = ts > now;
  const s = Math.floor(diff / 1000);
  const suffix = (v) => future ? `in ${v}` : `${v} ago`;
  if (s < 60) return suffix(`${s}s`);
  const m = Math.floor(s / 60);
  if (m < 60) return suffix(`${m}m`);
  const h = Math.floor(m / 60);
  if (h < 24) return suffix(`${h}h`);
  return suffix(`${Math.floor(h / 24)}d`);
}

function fmtAbs(iso) {
  if (!iso) return '—';
  try {
    return new Date(iso).toLocaleString('en-GB', {
      timeZone: 'Asia/Tokyo',
      dateStyle: 'short',
      timeStyle: 'medium',
    });
  } catch { return iso; }
}

const STATUS_FILTERS = [
  { id: 'all',      label: 'All' },
  { id: 'online',   label: 'Online' },
  { id: 'degraded', label: 'Degraded' },
  { id: 'offline',  label: 'Offline' },
  { id: 'pending',  label: 'Pending' },
];

function StatusPill({ status }) {
  const color = status === 'online'
    ? 'text-status-online border-status-online/40 bg-status-online/10'
    : status === 'degraded'
      ? 'text-yellow-400 border-yellow-600/40 bg-yellow-700/10'
      : status === 'offline'
        ? 'text-status-offline border-status-offline/40 bg-status-offline/10'
        : 'text-gray-400 border-gray-600/40 bg-gray-700/10';
  return (
    <span className={`inline-block px-1.5 py-0.5 rounded text-[9px] border font-mono uppercase ${color}`}>
      {status}
    </span>
  );
}

export default function DatabaseSchedulerTab() {
  const [state, setState] = useState(null);
  const [loading, setLoading] = useState(true);
  const [filter, setFilter] = useState('all');
  const [q, setQ] = useState('');
  const [sortBy, setSortBy] = useState('last_check');
  const [sortDir, setSortDir] = useState('DESC');

  useEffect(() => {
    let alive = true;
    setLoading(true);
    fetch(apiUrl('/api/db/scheduler'))
      .then((r) => r.ok ? r.json() : null)
      .then((j) => { if (alive) setState(j); })
      .catch(() => {})
      .finally(() => { if (alive) setLoading(false); });
    return () => { alive = false; };
  }, []);

  const filteredSources = useMemo(() => {
    if (!state) return [];
    let rows = state.sources || [];
    if (filter !== 'all') rows = rows.filter((s) => s.status === filter);
    const qq = q.trim().toLowerCase();
    if (qq) {
      rows = rows.filter((s) =>
        s.id.toLowerCase().includes(qq) ||
        (s.name || '').toLowerCase().includes(qq) ||
        (s.category || '').toLowerCase().includes(qq),
      );
    }
    rows = [...rows].sort((a, b) => {
      const av = a[sortBy];
      const bv = b[sortBy];
      if (av == null && bv == null) return 0;
      if (av == null) return 1;
      if (bv == null) return -1;
      const cmp = av < bv ? -1 : av > bv ? 1 : 0;
      return sortDir === 'DESC' ? -cmp : cmp;
    });
    return rows;
  }, [state, filter, q, sortBy, sortDir]);

  const toggleSort = (col) => {
    if (sortBy === col) setSortDir(sortDir === 'DESC' ? 'ASC' : 'DESC');
    else { setSortBy(col); setSortDir('DESC'); }
  };

  return (
    <div className="flex flex-col h-full overflow-hidden">
      {/* Jobs */}
      <div className="p-3 border-b border-osint-border/50">
        <div className="text-[9px] uppercase tracking-wider text-gray-500 mb-2">
          Scheduled jobs
        </div>
        {loading && !state && (
          <div className="text-[10px] text-gray-500">Loading…</div>
        )}
        {state && (
          <div className="grid grid-cols-1 md:grid-cols-3 gap-2">
            {state.jobs.map((j) => (
              <div
                key={j.id}
                className="rounded border border-osint-border/40 bg-osint-bg/40 p-2"
              >
                <div className="flex items-center justify-between mb-1">
                  <span className="font-mono text-[11px] text-neon-cyan">{j.id}</span>
                  <code className="text-[9px] text-gray-500">{j.cron}</code>
                </div>
                <div className="text-[9.5px] text-gray-400 leading-snug mb-1.5">
                  {j.description}
                </div>
                <div className="text-[10px] font-mono space-y-0.5">
                  <div>
                    <span className="text-gray-500">Last:</span>{' '}
                    <span className="text-gray-200">{relativeTime(j.last_run)}</span>
                    <span className="text-gray-500"> · {fmtAbs(j.last_run)}</span>
                  </div>
                  <div>
                    <span className="text-gray-500">Next:</span>{' '}
                    <span className="text-gray-200">{relativeTime(j.next_run)}</span>
                    <span className="text-gray-500"> · {fmtAbs(j.next_run)}</span>
                  </div>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Source grid */}
      <div className="flex items-center gap-2 px-3 py-2 border-b border-osint-border/50">
        <div className="flex items-center gap-1">
          {STATUS_FILTERS.map((f) => (
            <button
              key={f.id}
              type="button"
              onClick={() => setFilter(f.id)}
              className={`px-2 py-0.5 rounded text-[10px] border transition-colors ${
                filter === f.id
                  ? 'bg-neon-cyan/15 text-neon-cyan border-neon-cyan/40'
                  : 'bg-transparent text-gray-400 border-osint-border hover:text-neon-cyan'
              }`}
            >
              {f.label}
            </button>
          ))}
        </div>
        <input
          type="text"
          value={q}
          onChange={(e) => setQ(e.target.value)}
          placeholder="Filter by id, name, category..."
          className="flex-1 px-2 py-1 bg-osint-bg/60 border border-osint-border rounded text-[11px] text-gray-200 placeholder-gray-600 focus:outline-none focus:border-neon-cyan/40 font-mono"
        />
        <span className="text-[10px] text-gray-500">
          {filteredSources.length}/{state?.sources?.length ?? 0}
        </span>
      </div>

      <div className="flex-1 overflow-auto">
        <table className="w-full text-[10px] font-mono border-collapse">
          <thead className="sticky top-0 bg-osint-surface z-10">
            <tr>
              {[
                ['id', 'Source'],
                ['category', 'Category'],
                ['status', 'Status'],
                ['last_check', 'Last check'],
                ['last_success', 'Last success'],
                ['records_count', 'Records'],
                ['response_time_ms', 'Resp ms'],
              ].map(([key, label]) => (
                <th
                  key={key}
                  onClick={() => toggleSort(key)}
                  className="text-left px-2 py-1 border-b border-osint-border/50 text-gray-400 uppercase tracking-wider font-normal cursor-pointer hover:text-neon-cyan"
                >
                  {label}
                  {sortBy === key && (
                    <span className="ml-1 text-neon-cyan">
                      {sortDir === 'DESC' ? '▾' : '▴'}
                    </span>
                  )}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {filteredSources.map((s) => (
              <tr
                key={s.id}
                className="border-b border-osint-border/20 hover:bg-white/5"
              >
                <td className="px-2 py-1 text-gray-200">
                  <div className="text-gray-200">{s.id}</div>
                  <div className="text-[9px] text-gray-500 truncate max-w-[180px]">{s.name}</div>
                </td>
                <td className="px-2 py-1 text-gray-400">{s.category || '—'}</td>
                <td className="px-2 py-1"><StatusPill status={s.status} /></td>
                <td className="px-2 py-1 text-gray-300">{relativeTime(s.last_check)}</td>
                <td className="px-2 py-1 text-gray-300">{relativeTime(s.last_success)}</td>
                <td className="px-2 py-1 text-right text-gray-300">
                  {s.records_count != null ? s.records_count.toLocaleString() : '—'}
                </td>
                <td className="px-2 py-1 text-right text-gray-300">
                  {s.response_time_ms != null ? s.response_time_ms : '—'}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}
