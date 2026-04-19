import React, { useMemo, useState, useEffect, useRef } from 'react';
import useCollectorFollowStream from '../../hooks/useCollectorFollowStream';

/* ── Helpers ───────────────────────────────────────────────────────────── */

function fmtBytes(n) {
  if (n == null) return '—';
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  if (n < 1024 * 1024 * 1024) return `${(n / 1024 / 1024).toFixed(1)} MB`;
  return `${(n / 1024 / 1024 / 1024).toFixed(2)} GB`;
}

function fmtLatency(ms) {
  if (ms == null) return '—';
  if (ms < 1000) return `${ms}ms`;
  return `${(ms / 1000).toFixed(2)}s`;
}

function fmtTime(iso) {
  if (!iso) return '';
  try {
    return new Date(iso).toLocaleTimeString('en-GB', { hour12: false });
  } catch { return ''; }
}

function fmtElapsed(ms) {
  if (!Number.isFinite(ms)) return '—';
  const s = Math.floor(ms / 1000);
  const mm = Math.floor(s / 60).toString().padStart(2, '0');
  const ss = (s % 60).toString().padStart(2, '0');
  return `${mm}:${ss}`;
}

function statusClass(hit) {
  if (hit.error) return 'err';
  if (hit.status == null) return 'in-flight';
  if (hit.status >= 500) return '5xx';
  if (hit.status >= 400) return '4xx';
  if (hit.status >= 300) return '3xx';
  if (hit.status >= 200) return '2xx';
  return 'other';
}

const STATUS_TEXT = {
  '2xx': 'text-status-online',
  '3xx': 'text-neon-cyan',
  '4xx': 'text-neon-orange',
  '5xx': 'text-neon-red',
  err: 'text-neon-red',
  'in-flight': 'text-neon-cyan',
  other: 'text-gray-300',
};

const FILTER_PILLS = [
  { key: 'all', label: 'All' },
  { key: '2xx', label: '2xx' },
  { key: '3xx', label: '3xx' },
  { key: '4xx', label: '4xx' },
  { key: '5xx', label: '5xx' },
  { key: 'err', label: 'Err' },
  { key: 'in-flight', label: 'Live' },
];

const DATA_TYPES = ['json', 'geojson', 'xml', 'html', 'csv', 'text', 'binary'];

/* ── Sub-components ────────────────────────────────────────────────────── */

function MethodChip({ method }) {
  const colors = {
    GET: 'bg-status-online/15 text-status-online',
    POST: 'bg-neon-cyan/15 text-neon-cyan',
    PUT: 'bg-neon-orange/15 text-neon-orange',
    DELETE: 'bg-neon-red/15 text-neon-red',
  };
  const c = colors[method] || 'bg-gray-700/40 text-gray-300';
  return (
    <span className={`px-1 py-px rounded text-[9px] font-mono ${c}`}>
      {method}
    </span>
  );
}

function StatusCell({ hit }) {
  const klass = statusClass(hit);
  if (klass === 'in-flight') {
    return (
      <span className={`flex items-center gap-1 ${STATUS_TEXT[klass]}`}>
        <span className="w-1.5 h-1.5 rounded-full bg-neon-cyan pulse-live" />
        <span className="font-mono text-[10px]">…</span>
      </span>
    );
  }
  if (klass === 'err') {
    return <span className={`font-mono text-[10px] ${STATUS_TEXT.err}`}>×</span>;
  }
  return (
    <span className={`font-mono text-[10px] ${STATUS_TEXT[klass]}`}>
      {hit.status}
    </span>
  );
}

function HitRow({ hit, expanded, onToggle }) {
  const klass = statusClass(hit);
  const inFlight = klass === 'in-flight';
  return (
    <div
      className={`border-b border-osint-border/30 ${
        inFlight ? 'border-l-2 border-dashed border-l-neon-cyan/50' : ''
      }`}
    >
      <button
        type="button"
        onClick={onToggle}
        className="w-full grid items-center gap-2 px-2 py-1 text-left hover:bg-white/5 transition-colors"
        style={{ gridTemplateColumns: '44px 44px 132px 1fr 1.4fr 56px 60px 48px 56px' }}
      >
        <MethodChip method={hit.method} />
        <StatusCell hit={hit} />
        <span
          className="text-[10px] text-gray-300 truncate font-mono"
          title={hit.collector_key}
        >
          {hit.collector_key}
        </span>
        <span
          className="text-[10px] text-gray-400 truncate font-mono"
          title={hit.host}
        >
          {hit.host}
        </span>
        <span
          className="text-[10px] text-gray-500 truncate font-mono"
          title={hit.path}
        >
          {hit.path || '/'}
        </span>
        <span className="text-[10px] text-gray-300 font-mono text-right">
          {fmtLatency(hit.latency_ms)}
        </span>
        <span className="text-[10px] text-gray-300 font-mono text-right">
          {fmtBytes(hit.response_bytes)}
        </span>
        <span className="text-[10px] text-neon-green font-mono text-right">
          {hit.record_count ?? '—'}
        </span>
        <span className="text-[10px] text-gray-500 font-mono text-right">
          {fmtTime(hit.start_ts)}
        </span>
      </button>

      {expanded && (
        <div className="px-3 py-2 bg-black/30 border-t border-osint-border/30 space-y-1.5 text-[10px]">
          <div className="font-mono text-gray-300 break-all">
            <span className={STATUS_TEXT[klass]}>{hit.method}</span>{' '}
            <span className="text-gray-200">{hit.url}</span>
          </div>
          <div className="grid grid-cols-2 gap-x-4 gap-y-0.5 font-mono text-gray-400">
            <div>
              Collector: <span className="text-gray-200">{hit.collector_key}</span>
            </div>
            <div>
              Source: <span className="text-gray-200">{hit.source_name}</span>
            </div>
            <div>
              Status:{' '}
              <span className={STATUS_TEXT[klass]}>
                {hit.status ?? (hit.error ? 'NETWORK ERROR' : 'in-flight')}
              </span>
            </div>
            <div>
              Latency: <span className="text-gray-200">{fmtLatency(hit.latency_ms)}</span>
            </div>
            <div>
              Bytes: <span className="text-gray-200">{fmtBytes(hit.response_bytes)}</span>
            </div>
            <div>
              Type: <span className="text-gray-200">{hit.data_type || '—'}</span>
            </div>
            <div>
              Records: <span className="text-neon-green">{hit.record_count ?? '—'}</span>
            </div>
            <div>
              Run: <span className="text-gray-200">{hit.run_id?.slice(0, 8)}</span>
            </div>
          </div>
          {hit.error && (
            <div className="text-neon-red font-mono break-words">
              Error: {hit.error}
            </div>
          )}
          <div className="flex gap-2 pt-1">
            <button
              type="button"
              onClick={(e) => {
                e.stopPropagation();
                navigator.clipboard?.writeText(hit.url).catch(() => {});
              }}
              className="px-1.5 py-0.5 rounded text-[9px] border border-osint-border/60 text-gray-400 hover:text-neon-cyan hover:border-neon-cyan/40 font-mono"
            >
              copy URL
            </button>
            <a
              href={hit.url}
              target="_blank"
              rel="noopener noreferrer"
              onClick={(e) => e.stopPropagation()}
              className="px-1.5 py-0.5 rounded text-[9px] border border-osint-border/60 text-gray-400 hover:text-neon-cyan hover:border-neon-cyan/40 font-mono"
            >
              open ↗
            </a>
          </div>
        </div>
      )}
    </div>
  );
}

function RunBanner({ activeRuns, runs }) {
  // Tick once a second so elapsed timers update on screen.
  const [, setTick] = useState(0);
  useEffect(() => {
    if (activeRuns.length === 0) return undefined;
    const t = setInterval(() => setTick((x) => x + 1), 1000);
    return () => clearInterval(t);
  }, [activeRuns.length]);

  if (activeRuns.length > 0) {
    return (
      <div className="px-3 py-1.5 border-b border-osint-border/40 bg-neon-cyan/5 space-y-0.5">
        <div className="flex items-center gap-2 text-[10px]">
          <span className="w-2 h-2 rounded-full bg-status-online pulse-live" />
          <span className="text-neon-cyan font-mono uppercase tracking-wider">
            {activeRuns.length} run{activeRuns.length === 1 ? '' : 's'} active
          </span>
        </div>
        <div className="space-y-0.5 max-h-24 overflow-y-auto">
          {activeRuns.slice(0, 6).map((r) => (
            <div
              key={r.run_id}
              className="flex items-center justify-between text-[10px] font-mono text-gray-400"
            >
              <span className="truncate text-gray-300" title={r.collector_key}>
                {r.collector_key}
              </span>
              <span className="text-gray-500 ml-2 shrink-0">
                {r.trigger} · {r.hit_count || 0} hit · {fmtElapsed(Date.now() - r.started_ms)}
              </span>
            </div>
          ))}
          {activeRuns.length > 6 && (
            <div className="text-[9px] text-gray-500 italic">
              +{activeRuns.length - 6} more…
            </div>
          )}
        </div>
      </div>
    );
  }
  const lastRun = runs[0];
  if (lastRun?.ended_ms) {
    return (
      <div className="px-3 py-1.5 border-b border-osint-border/40 text-[10px] font-mono text-gray-400">
        <span className="text-gray-500">Last run</span>{' '}
        <span className="text-gray-200">{lastRun.collector_key}</span>{' '}
        <span className={lastRun.status === 'ok' ? 'text-status-online' : 'text-neon-red'}>
          {lastRun.status}
        </span>{' '}
        · {lastRun.hit_count} hit · {fmtElapsed(lastRun.duration_ms)}
      </div>
    );
  }
  return (
    <div className="px-3 py-1.5 border-b border-osint-border/40 text-[10px] text-gray-500 italic">
      Waiting for first collector run…
    </div>
  );
}

function FilterStrip({
  statusFilter, setStatusFilter,
  typeFilter, setTypeFilter,
  text, setText,
  collectorChoices, collectorFilter, setCollectorFilter,
}) {
  return (
    <div className="px-3 py-2 border-b border-osint-border/40 space-y-1.5 flex-shrink-0">
      <div className="flex flex-wrap gap-1">
        {FILTER_PILLS.map((p) => (
          <button
            key={p.key}
            type="button"
            onClick={() => setStatusFilter(p.key)}
            className={`px-2 py-0.5 rounded text-[10px] border transition-colors font-mono ${
              statusFilter === p.key
                ? 'bg-neon-cyan/15 text-neon-cyan border-neon-cyan/40'
                : 'bg-osint-bg/40 text-gray-400 border-osint-border hover:text-gray-200'
            }`}
          >
            {p.label}
          </button>
        ))}
      </div>
      <div className="flex flex-wrap gap-1">
        {DATA_TYPES.map((t) => (
          <button
            key={t}
            type="button"
            onClick={() =>
              setTypeFilter((cur) => {
                const next = new Set(cur);
                if (next.has(t)) next.delete(t); else next.add(t);
                return next;
              })
            }
            className={`px-1.5 py-0.5 rounded text-[9px] border font-mono transition-colors ${
              typeFilter.has(t)
                ? 'bg-neon-green/15 text-neon-green border-neon-green/40'
                : 'bg-osint-bg/40 text-gray-500 border-osint-border hover:text-gray-300'
            }`}
          >
            {t}
          </button>
        ))}
      </div>
      <div className="grid grid-cols-2 gap-2">
        <input
          type="text"
          placeholder="filter host or path…"
          value={text}
          onChange={(e) => setText(e.target.value)}
          className="bg-osint-bg/60 border border-osint-border rounded px-2 py-1 text-[10px] text-gray-200 placeholder-gray-500 focus:outline-none focus:border-neon-cyan/50 font-mono"
        />
        <select
          value={collectorFilter}
          onChange={(e) => setCollectorFilter(e.target.value)}
          className="bg-osint-bg/60 border border-osint-border rounded px-2 py-1 text-[10px] text-gray-200 focus:outline-none focus:border-neon-cyan/50 font-mono"
        >
          <option value="">All collectors</option>
          {collectorChoices.map((c) => (
            <option key={c} value={c}>{c}</option>
          ))}
        </select>
      </div>
    </div>
  );
}

/* ── Main ──────────────────────────────────────────────────────────────── */

export default function FollowPanel({ onClose }) {
  const {
    hits, runs, activeRuns, connected, paused, setPaused, seeded, clear,
  } = useCollectorFollowStream();

  const [statusFilter, setStatusFilter] = useState('all');
  const [typeFilter, setTypeFilter] = useState(new Set());
  const [text, setText] = useState('');
  const [collectorFilter, setCollectorFilter] = useState('');
  const [expanded, setExpanded] = useState(null);

  // Detect whether the user has scrolled away from the top — if so, don't
  // auto-pin to newest (mirrors devtools network tab behavior).
  const listRef = useRef(null);
  const [atTop, setAtTop] = useState(true);
  const onScroll = () => {
    const el = listRef.current;
    if (!el) return;
    setAtTop(el.scrollTop < 8);
  };
  useEffect(() => {
    if (atTop && listRef.current) listRef.current.scrollTop = 0;
  }, [hits, atTop]);

  const collectorChoices = useMemo(() => {
    const seen = new Set();
    for (const h of hits) seen.add(h.collector_key);
    return [...seen].sort();
  }, [hits]);

  const filtered = useMemo(() => {
    const q = text.trim().toLowerCase();
    return hits.filter((h) => {
      if (statusFilter !== 'all' && statusClass(h) !== statusFilter) return false;
      if (typeFilter.size > 0 && !typeFilter.has(h.data_type)) return false;
      if (collectorFilter && h.collector_key !== collectorFilter) return false;
      if (q) {
        const hay = `${h.host} ${h.path} ${h.url}`.toLowerCase();
        if (!hay.includes(q)) return false;
      }
      return true;
    });
  }, [hits, statusFilter, typeFilter, collectorFilter, text]);

  const inFlightCount = useMemo(
    () => hits.filter((h) => statusClass(h) === 'in-flight').length,
    [hits],
  );

  return (
    <div className="glass-panel flex flex-col w-[680px] max-w-[95vw] max-h-[80vh] shadow-xl">
      {/* Header */}
      <div className="flex items-center justify-between px-3 py-2 border-b border-osint-border/50 flex-shrink-0">
        <div className="flex items-center gap-2">
          <span
            className={`w-2 h-2 rounded-full ${
              connected ? 'bg-status-online pulse-live' : 'bg-status-offline'
            }`}
          />
          <span className="text-sm font-bold text-neon-cyan">Collector Follow</span>
          <span className="text-[10px] text-gray-400 font-mono">
            {filtered.length}/{hits.length} hit{hits.length === 1 ? '' : 's'}
            {inFlightCount > 0 && (
              <span className="ml-2 text-neon-cyan">· {inFlightCount} live</span>
            )}
            {!seeded && <span className="ml-2 text-gray-500">· loading</span>}
          </span>
        </div>
        <div className="flex items-center gap-1">
          <button
            type="button"
            onClick={() => setPaused((p) => !p)}
            className={`px-2 py-0.5 rounded text-[10px] border font-mono transition-colors ${
              paused
                ? 'bg-neon-orange/15 text-neon-orange border-neon-orange/40'
                : 'bg-osint-bg/40 text-gray-400 border-osint-border hover:text-gray-200'
            }`}
            title={paused ? 'Resume stream' : 'Pause stream'}
          >
            {paused ? 'paused' : 'pause'}
          </button>
          <button
            type="button"
            onClick={clear}
            className="px-2 py-0.5 rounded text-[10px] border border-osint-border bg-osint-bg/40 text-gray-400 hover:text-gray-200 font-mono"
          >
            clear
          </button>
          <button
            type="button"
            onClick={onClose}
            className="ml-1 text-gray-400 hover:text-neon-cyan text-sm leading-none px-1"
            aria-label="Close"
          >
            ✕
          </button>
        </div>
      </div>

      <RunBanner activeRuns={activeRuns} runs={runs} />

      <FilterStrip
        statusFilter={statusFilter}
        setStatusFilter={setStatusFilter}
        typeFilter={typeFilter}
        setTypeFilter={setTypeFilter}
        text={text}
        setText={setText}
        collectorChoices={collectorChoices}
        collectorFilter={collectorFilter}
        setCollectorFilter={setCollectorFilter}
      />

      {/* Column header */}
      <div
        className="grid items-center gap-2 px-2 py-1 text-[9px] uppercase tracking-wider text-gray-500 font-mono border-b border-osint-border/40 bg-osint-bg/40 flex-shrink-0"
        style={{ gridTemplateColumns: '44px 44px 132px 1fr 1.4fr 56px 60px 48px 56px' }}
      >
        <span>method</span>
        <span>code</span>
        <span>collector</span>
        <span>host</span>
        <span>path</span>
        <span className="text-right">latency</span>
        <span className="text-right">size</span>
        <span className="text-right">recs</span>
        <span className="text-right">time</span>
      </div>

      {/* List */}
      <div
        ref={listRef}
        onScroll={onScroll}
        className="flex-1 overflow-y-auto min-h-0"
      >
        {filtered.length === 0 ? (
          <div className="text-gray-500 text-xs px-3 py-6 text-center italic">
            {hits.length === 0
              ? 'No collector traffic yet. The first cron tick or any /api/data/* call will populate this stream.'
              : 'No hits match the current filter.'}
          </div>
        ) : (
          filtered.map((h) => (
            <HitRow
              key={h.key}
              hit={h}
              expanded={expanded === h.key}
              onToggle={() => setExpanded((e) => (e === h.key ? null : h.key))}
            />
          ))
        )}
      </div>

      {/* Footer */}
      <div className="px-3 py-1 border-t border-osint-border/40 text-[10px] font-mono text-gray-500 flex items-center justify-between flex-shrink-0">
        <span>
          {connected ? 'streaming' : 'reconnecting…'}
          {!atTop && (
            <button
              type="button"
              onClick={() => { if (listRef.current) listRef.current.scrollTop = 0; }}
              className="ml-2 text-neon-cyan hover:underline"
            >
              scroll to top
            </button>
          )}
        </span>
        <span>buf {hits.length}/1000</span>
      </div>
    </div>
  );
}
