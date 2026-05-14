import React, { useState, useEffect, useMemo, useCallback } from 'react';
import apiUrl from '../../utils/apiUrl.js';
import {
  PieChart, Pie, Cell, BarChart, Bar, XAxis, YAxis, Tooltip,
  ResponsiveContainer, Legend,
} from 'recharts';
import StatusBadge from '../ui/StatusBadge';
import LoadingSpinner from '../ui/LoadingSpinner';

const COLORS = {
  online: '#00ff88',
  degraded: '#ffb74d',
  offline: '#ff4444',
  API: '#00f0ff',
  Dataset: '#3b82f6',
  Scraped: '#ff8c00',
  'Web Request': '#a855f7',
};

const CATEGORY_COLORS = [
  '#00f0ff', '#00ff88', '#ff8c00', '#a855f7', '#f06292',
  '#ffd600', '#42a5f5', '#ef5350', '#78909c', '#4dd0e1',
];

function StatCard({ label, value, color, subtitle }) {
  return (
    <div className="glass-panel p-4 flex flex-col">
      <span className="text-[10px] uppercase tracking-widest text-gray-500 mb-1">{label}</span>
      <span className="text-2xl font-mono font-bold" style={{ color: color || '#00f0ff' }}>
        {value ?? '-'}
      </span>
      {subtitle && <span className="text-[10px] text-gray-600 mt-1">{subtitle}</span>}
    </div>
  );
}

function DarkTooltip({ active, payload, label }) {
  if (!active || !payload?.length) return null;
  return (
    <div className="glass-panel px-3 py-2 text-xs">
      <p className="text-gray-300 mb-1">{label || payload[0]?.name}</p>
      {payload.map((p, i) => (
        <p key={i} className="font-mono" style={{ color: p.color || '#00f0ff' }}>
          {p.name}: {p.value}
        </p>
      ))}
    </div>
  );
}

export default function SourceDashboard({ sources: propSources, stats: propStats }) {
  const [sources, setSources] = useState(propSources || []);
  const [stats, setStats] = useState(propStats || null);
  const [loading, setLoading] = useState(!propSources?.length);
  const [sortField, setSortField] = useState('name');
  const [sortDir, setSortDir] = useState('asc');
  const [filterType, setFilterType] = useState('');
  const [filterStatus, setFilterStatus] = useState('');
  const [filterCategory, setFilterCategory] = useState('');
  const [expandedRow, setExpandedRow] = useState(null);

  // Fetch sources if not provided via props
  const fetchData = useCallback(async () => {
    try {
      const [srcRes, stRes] = await Promise.all([
        fetch(apiUrl('/api/sources')),
        fetch(apiUrl('/api/sources/stats')),
      ]);
      if (srcRes.ok) {
        const data = await srcRes.json();
        setSources(Array.isArray(data) ? data : data.sources || []);
      }
      if (stRes.ok) {
        setStats(await stRes.json());
      }
    } catch (err) {
      console.warn('[SourceDashboard] fetch error:', err.message);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 30000);
    return () => clearInterval(interval);
  }, [fetchData]);

  useEffect(() => {
    if (propSources?.length) setSources(propSources);
  }, [propSources]);

  useEffect(() => {
    if (propStats) setStats(propStats);
  }, [propStats]);

  // Derived data
  const statusCounts = useMemo(() => {
    const counts = { online: 0, degraded: 0, offline: 0, gated: 0 };
    sources.forEach((s) => {
      if (s.gated) {
        counts.gated++;
        return;
      }
      const st = (s.status || 'offline').toLowerCase();
      if (counts[st] !== undefined) counts[st]++;
    });
    return counts;
  }, [sources]);

  const typeCounts = useMemo(() => {
    const map = {};
    sources.forEach((s) => {
      const t = s.type || 'Unknown';
      map[t] = (map[t] || 0) + 1;
    });
    return Object.entries(map).map(([name, value]) => ({ name, value }));
  }, [sources]);

  const statusChartData = useMemo(() => {
    return Object.entries(statusCounts).map(([name, value]) => ({ name, value }));
  }, [statusCounts]);

  const categoryRecords = useMemo(() => {
    const map = {};
    sources.forEach((s) => {
      const cat = s.category || 'Other';
      map[cat] = (map[cat] || 0) + (s.records || s.recordCount || 0);
    });
    return Object.entries(map)
      .map(([name, records]) => ({ name, records }))
      .sort((a, b) => b.records - a.records);
  }, [sources]);

  // Filtering + sorting
  const filteredSources = useMemo(() => {
    let filtered = [...sources];
    if (filterType) filtered = filtered.filter((s) => s.type === filterType);
    if (filterStatus) filtered = filtered.filter((s) => (s.status || '').toLowerCase() === filterStatus);
    if (filterCategory) filtered = filtered.filter((s) => s.category === filterCategory);

    filtered.sort((a, b) => {
      let av = a[sortField] ?? '';
      let bv = b[sortField] ?? '';
      if (typeof av === 'string') av = av.toLowerCase();
      if (typeof bv === 'string') bv = bv.toLowerCase();
      if (av < bv) return sortDir === 'asc' ? -1 : 1;
      if (av > bv) return sortDir === 'asc' ? 1 : -1;
      return 0;
    });

    return filtered;
  }, [sources, filterType, filterStatus, filterCategory, sortField, sortDir]);

  const handleSort = (field) => {
    if (sortField === field) {
      setSortDir((d) => (d === 'asc' ? 'desc' : 'asc'));
    } else {
      setSortField(field);
      setSortDir('asc');
    }
  };

  const uniqueTypes = useMemo(() => [...new Set(sources.map((s) => s.type).filter(Boolean))], [sources]);
  const uniqueCategories = useMemo(() => [...new Set(sources.map((s) => s.category).filter(Boolean))], [sources]);
  const totalRecords = useMemo(() => sources.reduce((sum, s) => sum + (s.records || s.recordCount || 0), 0), [sources]);

  const sortIcon = (field) => {
    if (sortField !== field) return '';
    return sortDir === 'asc' ? ' \u25B2' : ' \u25BC';
  };

  if (loading && !sources.length) {
    return (
      <div className="flex items-center justify-center h-full">
        <LoadingSpinner size="lg" />
      </div>
    );
  }

  return (
    <div className="h-full overflow-y-auto p-6 bg-osint-bg">
      <div className="max-w-7xl mx-auto space-y-6">
        {/* Header */}
        <div className="flex items-center justify-between">
          <h1 className="text-xl font-bold text-gray-100">
            <span className="text-neon-cyan">Source</span> Monitor
          </h1>
          <div className="flex items-center gap-2 text-xs text-gray-500">
            <span className="pulse-live w-2 h-2 rounded-full bg-neon-green inline-block" />
            Auto-refresh 30s
          </div>
        </div>

        {/* Stats Cards */}
        <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-7 gap-3">
          <StatCard label="Total Sources" value={sources.length} color="#00f0ff" />
          <StatCard label="Online" value={statusCounts.online} color="#00ff88" />
          <StatCard label="Degraded" value={statusCounts.degraded} color="#ffb74d" />
          <StatCard label="Offline" value={statusCounts.offline} color="#ff4444" />
          <StatCard label="Gated" value={statusCounts.gated} color="#9ca3af" />
          <StatCard label="Total Records" value={totalRecords.toLocaleString()} color="#a855f7" />
          <StatCard
            label="Last Update"
            value={new Date().toLocaleTimeString('en-GB', { timeZone: 'Asia/Tokyo' })}
            color="#3b82f6"
            subtitle="JST"
          />
        </div>

        {/* Charts Section */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
          {/* Sources by Type */}
          <div className="glass-panel p-4">
            <h3 className="text-xs uppercase tracking-wider text-gray-500 mb-3">Sources by Type</h3>
            <ResponsiveContainer width="100%" height={200}>
              <PieChart>
                <Pie
                  data={typeCounts}
                  cx="50%"
                  cy="50%"
                  outerRadius={70}
                  innerRadius={35}
                  dataKey="value"
                  stroke="#0a0e17"
                  strokeWidth={2}
                >
                  {typeCounts.map((entry, i) => (
                    <Cell key={i} fill={COLORS[entry.name] || CATEGORY_COLORS[i % CATEGORY_COLORS.length]} />
                  ))}
                </Pie>
                <Tooltip content={<DarkTooltip />} />
                <Legend
                  wrapperStyle={{ fontSize: '10px', color: '#9ca3af' }}
                />
              </PieChart>
            </ResponsiveContainer>
          </div>

          {/* Sources by Status */}
          <div className="glass-panel p-4">
            <h3 className="text-xs uppercase tracking-wider text-gray-500 mb-3">Sources by Status</h3>
            <ResponsiveContainer width="100%" height={200}>
              <PieChart>
                <Pie
                  data={statusChartData}
                  cx="50%"
                  cy="50%"
                  outerRadius={70}
                  innerRadius={35}
                  dataKey="value"
                  stroke="#0a0e17"
                  strokeWidth={2}
                >
                  {statusChartData.map((entry, i) => (
                    <Cell key={i} fill={COLORS[entry.name] || '#666'} />
                  ))}
                </Pie>
                <Tooltip content={<DarkTooltip />} />
                <Legend wrapperStyle={{ fontSize: '10px', color: '#9ca3af' }} />
              </PieChart>
            </ResponsiveContainer>
          </div>

          {/* Records by Category */}
          <div className="glass-panel p-4">
            <h3 className="text-xs uppercase tracking-wider text-gray-500 mb-3">Records by Category</h3>
            <ResponsiveContainer width="100%" height={200}>
              <BarChart data={categoryRecords} layout="vertical">
                <XAxis type="number" tick={{ fontSize: 10, fill: '#6b7280' }} />
                <YAxis type="category" dataKey="name" tick={{ fontSize: 10, fill: '#9ca3af' }} width={80} />
                <Tooltip content={<DarkTooltip />} />
                <Bar dataKey="records" radius={[0, 4, 4, 0]}>
                  {categoryRecords.map((_, i) => (
                    <Cell key={i} fill={CATEGORY_COLORS[i % CATEGORY_COLORS.length]} />
                  ))}
                </Bar>
              </BarChart>
            </ResponsiveContainer>
          </div>
        </div>

        {/* Data Flow Visualization */}
        <div className="glass-panel p-4">
          <h3 className="text-xs uppercase tracking-wider text-gray-500 mb-3">Data Pipeline</h3>
          <div className="flex items-center justify-center gap-3 text-xs flex-wrap">
            <div className="flex flex-col items-center gap-1 px-4 py-3 rounded border border-neon-cyan/20 bg-neon-cyan/5 min-w-[100px]">
              <span className="text-neon-cyan font-mono text-lg">{sources.length}</span>
              <span className="text-gray-400">Sources</span>
            </div>
            <span className="text-gray-600 text-lg">\u2192</span>
            <div className="flex flex-col items-center gap-1 px-4 py-3 rounded border border-neon-orange/20 bg-neon-orange/5 min-w-[100px]">
              <span className="text-neon-orange font-mono text-lg">ETL</span>
              <span className="text-gray-400">Processing</span>
            </div>
            <span className="text-gray-600 text-lg">\u2192</span>
            <div className="flex flex-col items-center gap-1 px-4 py-3 rounded border border-neon-green/20 bg-neon-green/5 min-w-[100px]">
              <span className="text-neon-green font-mono text-lg">{totalRecords.toLocaleString()}</span>
              <span className="text-gray-400">Records</span>
            </div>
            <span className="text-gray-600 text-lg">\u2192</span>
            <div className="flex flex-col items-center gap-1 px-4 py-3 rounded border border-neon-purple/20 bg-neon-purple/5 min-w-[100px]">
              <span className="text-neon-purple font-mono text-lg">12</span>
              <span className="text-gray-400">Map Layers</span>
            </div>
          </div>
        </div>

        {/* Filters */}
        <div className="flex flex-wrap items-center gap-3">
          <select
            value={filterStatus}
            onChange={(e) => setFilterStatus(e.target.value)}
            className="bg-osint-surface border border-osint-border rounded px-3 py-1.5 text-xs text-gray-300 focus:outline-none focus:border-neon-cyan/40"
          >
            <option value="">All Status</option>
            <option value="online">Online</option>
            <option value="degraded">Degraded</option>
            <option value="offline">Offline</option>
          </select>

          <select
            value={filterType}
            onChange={(e) => setFilterType(e.target.value)}
            className="bg-osint-surface border border-osint-border rounded px-3 py-1.5 text-xs text-gray-300 focus:outline-none focus:border-neon-cyan/40"
          >
            <option value="">All Types</option>
            {uniqueTypes.map((t) => (
              <option key={t} value={t}>{t}</option>
            ))}
          </select>

          <select
            value={filterCategory}
            onChange={(e) => setFilterCategory(e.target.value)}
            className="bg-osint-surface border border-osint-border rounded px-3 py-1.5 text-xs text-gray-300 focus:outline-none focus:border-neon-cyan/40"
          >
            <option value="">All Categories</option>
            {uniqueCategories.map((c) => (
              <option key={c} value={c}>{c}</option>
            ))}
          </select>

          <span className="text-xs text-gray-600 ml-auto">
            {filteredSources.length} of {sources.length} sources
          </span>
        </div>

        {/* Source Table */}
        <div className="glass-panel overflow-hidden">
          <div className="overflow-x-auto">
            <table className="w-full text-xs">
              <thead>
                <tr className="border-b border-osint-border text-gray-500 uppercase tracking-wider">
                  {[
                    { key: 'status', label: 'Status' },
                    { key: 'name', label: 'Name' },
                    { key: 'type', label: 'Type' },
                    { key: 'category', label: 'Category' },
                    { key: 'records', label: 'Records' },
                    { key: 'responseTime', label: 'Resp. Time' },
                    { key: 'lastCheck', label: 'Last Check' },
                    { key: 'lastSuccess', label: 'Last Success' },
                  ].map((col) => (
                    <th
                      key={col.key}
                      className="px-3 py-2 text-left cursor-pointer hover:text-neon-cyan transition-colors"
                      onClick={() => handleSort(col.key)}
                    >
                      {col.label}{sortIcon(col.key)}
                    </th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {filteredSources.map((src, i) => (
                  <React.Fragment key={src.id || i}>
                    <tr
                      className="border-b border-osint-border/50 hover:bg-neon-cyan/5 transition-colors cursor-pointer"
                      onClick={() => setExpandedRow(expandedRow === i ? null : i)}
                    >
                      <td className="px-3 py-2.5">
                        <StatusBadge type="status" value={src.status || 'offline'} />
                      </td>
                      <td className="px-3 py-2.5 text-gray-200 font-medium">{src.name}</td>
                      <td className="px-3 py-2.5">
                        <StatusBadge type="type" value={src.type} />
                      </td>
                      <td className="px-3 py-2.5 text-gray-400">{src.category}</td>
                      <td className="px-3 py-2.5 font-mono text-neon-green">
                        {(src.records || src.recordCount || 0).toLocaleString()}
                      </td>
                      <td className="px-3 py-2.5 font-mono text-gray-400">
                        {src.responseTime ? `${src.responseTime}ms` : '-'}
                      </td>
                      <td className="px-3 py-2.5 font-mono text-gray-500">
                        {src.lastCheck
                          ? new Date(src.lastCheck).toLocaleTimeString('en-GB', { timeZone: 'Asia/Tokyo' })
                          : '-'}
                      </td>
                      <td className="px-3 py-2.5 font-mono text-gray-500">
                        {src.lastSuccess
                          ? new Date(src.lastSuccess).toLocaleTimeString('en-GB', { timeZone: 'Asia/Tokyo' })
                          : '-'}
                      </td>
                    </tr>

                    {/* Expanded row - fetch logs */}
                    {expandedRow === i && (
                      <tr>
                        <td colSpan={8} className="px-4 py-3 bg-osint-bg/50">
                          <div className="text-[10px] text-gray-500 space-y-1">
                            <div className="flex items-center gap-2 mb-2">
                              <span className="text-gray-400 font-medium">Recent Fetch Logs</span>
                              {src.endpoint && (
                                <span className="font-mono text-neon-cyan/60">{src.endpoint}</span>
                              )}
                            </div>
                            {(src.recentLogs || []).length > 0 ? (
                              src.recentLogs.map((log, j) => (
                                <div key={j} className="flex items-center gap-3 font-mono">
                                  <span className="text-gray-600">
                                    {new Date(log.timestamp).toLocaleString('en-GB', { timeZone: 'Asia/Tokyo' })}
                                  </span>
                                  <span className={log.success ? 'text-neon-green' : 'text-neon-red'}>
                                    {log.success ? 'OK' : 'FAIL'}
                                  </span>
                                  <span className="text-gray-500">{log.message || `${log.records || 0} records`}</span>
                                </div>
                              ))
                            ) : (
                              <span className="text-gray-600 italic">No recent logs available</span>
                            )}
                            {src.description && (
                              <p className="text-gray-500 mt-2 pt-2 border-t border-osint-border/30">
                                {src.description}
                              </p>
                            )}
                          </div>
                        </td>
                      </tr>
                    )}
                  </React.Fragment>
                ))}

                {filteredSources.length === 0 && (
                  <tr>
                    <td colSpan={8} className="px-4 py-8 text-center text-gray-600">
                      No sources match the current filters
                    </td>
                  </tr>
                )}
              </tbody>
            </table>
          </div>
        </div>
      </div>
    </div>
  );
}
