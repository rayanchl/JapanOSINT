import React, { useEffect, useMemo, useState } from 'react';

const PAGE_SIZES = [25, 50, 100, 200];

function fmtCell(v) {
  if (v === null || v === undefined) return <span className="text-gray-600">—</span>;
  if (typeof v === 'number') return String(v);
  if (typeof v === 'boolean') return v ? 'true' : 'false';
  const s = String(v);
  if (s.length <= 60) return s;
  return s.slice(0, 57) + '…';
}

export default function DatabaseExplorerTab() {
  const [tables, setTables] = useState([]);
  const [selected, setSelected] = useState(null);
  const [q, setQ] = useState('');
  const [limit, setLimit] = useState(50);
  const [offset, setOffset] = useState(0);
  const [orderBy, setOrderBy] = useState(null);
  const [orderDir, setOrderDir] = useState('DESC');
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(false);
  const [expandedRow, setExpandedRow] = useState(null);

  // Load the table list once.
  useEffect(() => {
    fetch('/api/db/tables')
      .then((r) => r.ok ? r.json() : [])
      .then((j) => {
        // Server returns a bare array; older code returned { tables: [...] }.
        const list = Array.isArray(j) ? j : (j.tables || []);
        setTables(list);
        if (list.length && !selected) setSelected(list[0].name);
      })
      .catch(() => setTables([]));
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Reload rows whenever the query changes.
  useEffect(() => {
    if (!selected) return;
    const params = new URLSearchParams();
    params.set('limit', String(limit));
    params.set('offset', String(offset));
    if (q) params.set('q', q);
    if (orderBy) {
      params.set('orderBy', orderBy);
      params.set('orderDir', orderDir);
    }
    setLoading(true);
    fetch(`/api/db/tables/${encodeURIComponent(selected)}?${params}`)
      .then((r) => r.ok ? r.json() : null)
      .then((j) => setData(j))
      .catch(() => setData(null))
      .finally(() => setLoading(false));
  }, [selected, q, limit, offset, orderBy, orderDir]);

  // Reset pagination/filter when switching tables.
  useEffect(() => {
    setQ('');
    setOffset(0);
    setOrderBy(null);
    setExpandedRow(null);
  }, [selected]);

  const pageCount = data && data.total > 0 ? Math.ceil(data.total / limit) : 1;
  const pageIndex = Math.floor(offset / limit) + 1;

  const handleSort = (col) => {
    if (orderBy === col) {
      setOrderDir(orderDir === 'DESC' ? 'ASC' : 'DESC');
    } else {
      setOrderBy(col);
      setOrderDir('DESC');
    }
    setOffset(0);
  };

  const selectedMeta = useMemo(
    () => tables.find((t) => t.name === selected),
    [tables, selected],
  );

  return (
    <div className="flex h-full">
      {/* Left: table list */}
      <div className="w-40 flex-shrink-0 border-r border-osint-border overflow-y-auto">
        {tables.map((t) => (
          <button
            key={t.name}
            type="button"
            onClick={() => setSelected(t.name)}
            className={`w-full text-left px-3 py-1.5 text-[11px] font-mono border-b border-osint-border/30 hover:bg-white/5 transition-colors ${
              selected === t.name ? 'bg-neon-cyan/10 text-neon-cyan' : 'text-gray-300'
            }`}
          >
            <div>{t.name}</div>
            <div className="text-[9px] text-gray-500">{t.row_count.toLocaleString()} rows</div>
          </button>
        ))}
      </div>

      {/* Right: rows */}
      <div className="flex-1 flex flex-col overflow-hidden">
        {/* Filter + page size */}
        <div className="flex items-center gap-2 px-3 py-2 border-b border-osint-border/50">
          <input
            type="text"
            value={q}
            onChange={(e) => { setQ(e.target.value); setOffset(0); }}
            placeholder="Filter text columns..."
            className="flex-1 px-2 py-1 bg-osint-bg/60 border border-osint-border rounded text-[11px] text-gray-200 placeholder-gray-600 focus:outline-none focus:border-neon-cyan/40 font-mono"
          />
          <select
            value={limit}
            onChange={(e) => { setLimit(parseInt(e.target.value, 10)); setOffset(0); }}
            className="px-2 py-1 bg-osint-bg/60 border border-osint-border rounded text-[10px] text-gray-300"
          >
            {PAGE_SIZES.map((n) => <option key={n} value={n}>{n}/page</option>)}
          </select>
        </div>

        {/* Data table */}
        <div className="flex-1 overflow-auto">
          {loading && (
            <div className="px-3 py-2 text-[10px] text-gray-500">Loading…</div>
          )}
          {!loading && data && data.rows.length === 0 && (
            <div className="px-3 py-2 text-[10px] text-gray-500">No rows.</div>
          )}
          {!loading && data && data.rows.length > 0 && selectedMeta && (
            <table className="w-full text-[10px] font-mono border-collapse">
              <thead className="sticky top-0 bg-osint-surface z-10">
                <tr>
                  {selectedMeta.columns.map((c) => (
                    <th
                      key={c.name}
                      onClick={() => handleSort(c.name)}
                      className="text-left px-2 py-1 border-b border-osint-border/50 text-gray-400 uppercase tracking-wider font-normal cursor-pointer hover:text-neon-cyan"
                      title={`${c.name} · ${c.type}`}
                    >
                      {c.name}
                      {orderBy === c.name && (
                        <span className="ml-1 text-neon-cyan">
                          {orderDir === 'DESC' ? '▾' : '▴'}
                        </span>
                      )}
                    </th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {data.rows.map((row, idx) => {
                  const isExpanded = expandedRow === idx;
                  return (
                    <React.Fragment key={idx}>
                      <tr
                        onClick={() => setExpandedRow(isExpanded ? null : idx)}
                        className={`border-b border-osint-border/20 hover:bg-white/5 cursor-pointer ${
                          isExpanded ? 'bg-neon-cyan/5' : ''
                        }`}
                      >
                        {selectedMeta.columns.map((c) => (
                          <td
                            key={c.name}
                            className="px-2 py-1 text-gray-200 align-top"
                            title={row[c.name] != null ? String(row[c.name]) : ''}
                          >
                            {fmtCell(row[c.name])}
                          </td>
                        ))}
                      </tr>
                      {isExpanded && (
                        <tr>
                          <td
                            colSpan={selectedMeta.columns.length}
                            className="px-2 py-2 bg-black/40 border-b border-osint-border/40"
                          >
                            <pre className="text-[10px] text-gray-300 whitespace-pre-wrap break-all max-h-64 overflow-auto">
                              {JSON.stringify(row, null, 2)}
                            </pre>
                          </td>
                        </tr>
                      )}
                    </React.Fragment>
                  );
                })}
              </tbody>
            </table>
          )}
        </div>

        {/* Pagination */}
        {data && data.total > 0 && (
          <div className="flex items-center justify-between px-3 py-1.5 border-t border-osint-border/50 text-[10px] text-gray-400">
            <div>
              {offset + 1}–{Math.min(offset + data.rows.length, data.total)} of{' '}
              <span className="text-gray-200">{data.total.toLocaleString()}</span>
            </div>
            <div className="flex items-center gap-2">
              <button
                type="button"
                disabled={offset <= 0}
                onClick={() => setOffset(Math.max(0, offset - limit))}
                className="px-2 py-0.5 rounded border border-osint-border hover:text-neon-cyan disabled:opacity-30 disabled:cursor-not-allowed"
              >
                ‹ Prev
              </button>
              <span className="font-mono">
                Page {pageIndex} / {pageCount}
              </span>
              <button
                type="button"
                disabled={offset + limit >= data.total}
                onClick={() => setOffset(offset + limit)}
                className="px-2 py-0.5 rounded border border-osint-border hover:text-neon-cyan disabled:opacity-30 disabled:cursor-not-allowed"
              >
                Next ›
              </button>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
