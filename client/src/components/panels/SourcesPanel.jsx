import React, { useEffect, useMemo, useState } from 'react';
import { errorMessage } from '../../api/client.js';
import { probeSource, setProbeConsent } from '../../api/probe.js';
import { useApi } from '../../hooks/useApi.js';
import { usePagedList } from '../../hooks/usePagedList.js';
import { searchPath } from '../../utils/sourceFormat.js';
import { relativeTime } from '../../utils/time.js';
import StatusBadge from '../ui/StatusBadge';
import LoadingSpinner from '../ui/LoadingSpinner';
import CameraDiscoveryThread from './CameraDiscoveryThread';
import FollowPanel from './FollowPanel';
import DatabaseExplorerTab from './DatabaseExplorerTab';
import DatabaseSchedulerTab from './DatabaseSchedulerTab';

/* Each filter is a SERVER filter on GET /api/sources/search (2026-11-07):
 * this panel used to download the whole /api/status catalogue every 30 s and
 * filter it in the browser. `health=gated` is the search's state for a source
 * that needs a credential it does not have; the old separate "Missing key"
 * filter (requiresKey && !configured) has no search counterpart and is folded
 * into it — a `requires_key` filter on search would bring it back. */
const FILTERS = [
  { key: 'all', label: 'All', q: {} },
  { key: 'working', label: 'Working', q: { health: 'ok' } },
  { key: 'online', label: 'Online', q: { status: 'online' } },
  { key: 'degraded', label: 'Degraded', q: { status: 'degraded' } },
  { key: 'offline', label: 'Offline', q: { status: 'offline' } },
  { key: 'pending', label: 'Pending', q: { status: 'pending' } },
  { key: 'gated', label: 'Gated (needs key)', q: { health: 'gated' } },
];

/* A search row in the field names this panel's renderers were written for
 * (statusapi's camelCase). Key/probe fields are not on a search row; they
 * arrive from GET /api/status/:id when the card is opened. */
function fromSearchRow(r) {
  return {
    id: r.id, name: r.name || r.id, nameJa: r.name_ja, category: r.category, type: r.type,
    description: r.description, url: r.url, status: r.status || 'pending',
    gated: r.health?.state === 'gated', health: r.health,
    lastCheck: r.last_check, lastSuccess: r.last_success,
    responseTimeMs: r.response_time_ms, recordsCount: r.records_last_run, errorMessage: r.error_message,
    intelTotal: r.item_count, intelGeocoded: r.geocoded,
  };
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
      <span className="px-1.5 py-0.5 rounded text-[9px] bg-osint-border/40 text-osint-muted border border-osint-border/30">
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
      <div className="mt-2 px-2 py-1.5 rounded border border-osint-border/40 bg-osint-bg/40 text-[10px] text-osint-muted italic">
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
          <div className="font-mono text-[10px] text-osint-text break-all">
            <span className="text-neon-green">
              {api.probeRequestMethod || 'GET'}
            </span>{' '}
            {api.probeRequestUrl}
          </div>
          {api.probeRequestHeaders && (
            <pre className="mt-1 font-mono text-[9.5px] text-osint-muted whitespace-pre-wrap break-all">
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
            <pre className="font-mono text-[9.5px] text-osint-muted whitespace-pre-wrap break-all max-h-32 overflow-y-auto">
              {prettyJson(api.probeResponseHeaders)}
            </pre>
          )}
          {api.probeResponseBody && (
            <pre className="mt-1 font-mono text-[9.5px] text-osint-text whitespace-pre-wrap break-all max-h-64 overflow-y-auto border-t border-osint-border/30 pt-1">
              {api.probeResponseBody}
            </pre>
          )}
        </div>
      )}
    </div>
  );
}

function ApiRow({ api: listRow, expanded, onToggle }) {
  // The full status row (keys, probe request/response) for this one source,
  // read when the card opens — GET /api/status/:id, not the whole catalogue.
  const one = useApi(expanded ? `/api/status/${encodeURIComponent(listRow.id)}` : null, { enabled: expanded, deps: [expanded, listRow.id] });
  const [patch, setPatch] = useState(null);
  const api = { ...listRow, ...(one.data || {}), ...(patch || {}), name: (one.data?.name || listRow.name) };
  const keysKnown = Boolean(one.data);
  const onApiUpdate = (updated) => { const { ok: _ok, ...rest } = updated || {}; setPatch((p) => ({ ...(p || {}), ...rest })); };
  const [pinging, setPinging] = useState(false);
  const [consentBusy, setConsentBusy] = useState(false);
  const [actionError, setActionError] = useState(null);

  const pingNow = async (e) => {
    e.stopPropagation();
    setActionError(null);
    setPinging(true);
    try {
      onApiUpdate(await probeSource(api.id));
    } catch (err) {
      setActionError(`Ping refused — ${errorMessage(err)}`);
    } finally {
      setPinging(false);
    }
  };

  const toggleConsent = async (e) => {
    e.stopPropagation();
    setActionError(null);
    setConsentBusy(true);
    try {
      onApiUpdate(await setProbeConsent(api.id, !api.probeConsent));
    } catch (err) {
      setActionError(`Consent update refused — ${errorMessage(err)}`);
    } finally {
      setConsentBusy(false);
    }
  };

  return (
    <div className="border border-osint-border/40 rounded bg-osint-bg/40">
      <button
        type="button"
        onClick={onToggle}
        className="w-full grid grid-cols-12 gap-2 items-center px-2 py-1.5 text-left hover:bg-white/5 transition-colors"
      >
        <div className="col-span-5 truncate">
          <div className="text-[11px] text-osint-text font-medium truncate">
            {api.name}
          </div>
          {api.nameJa && (
            <div className="text-[9px] text-osint-muted truncate">{api.nameJa}</div>
          )}
          {api.gated && (
            <div className="mt-0.5">
              <span
                className="px-1.5 py-0.5 rounded text-[9px] bg-status-degraded/15 text-status-degraded border border-status-degraded/30"
                title="Open the card and use Ping / Allow scheduled probing"
              >
                Probing needs API credentials — manual ping in card settings
              </span>
            </div>
          )}
        </div>
        <div className="col-span-3">
          <StatusBadge type="status" value={api.gated ? 'gated' : api.status} />
        </div>
        <div className="col-span-3 flex justify-end">
          {keysKnown ? <KeyPills api={api} /> : (api.gated && api.health?.credentials_required?.length
            ? <span className="px-1.5 py-0.5 rounded text-[9px] bg-status-offline/15 text-status-offline border border-status-offline/30 font-mono">needs {api.health.credentials_required.join(', ')}</span>
            : null)}
        </div>
        <div className="col-span-1 text-osint-muted text-xs text-right">
          {expanded ? '▾' : '▸'}
        </div>
      </button>

      {expanded && (
        <div className="px-3 pb-2 pt-1 text-[10px] text-osint-muted border-t border-osint-border/40 space-y-1">
          {one.loading && !one.data && <div>Loading this source's key and probe detail…</div>}
          {one.error && <div className="text-status-offline">Key and probe detail unavailable — {errorMessage(one.error)}</div>}
          {api.description && (
            <div className="text-osint-text">{api.description}</div>
          )}
          <div className="grid grid-cols-2 gap-x-4 gap-y-0.5 font-mono">
            <div>
              Category: <span className="text-osint-text">{api.category}</span>
            </div>
            <div>
              Type: <span className="text-osint-text">{api.type}</span>
            </div>
            <div>
              Last check:{' '}
              <span className="text-osint-text">{relativeTime(api.lastCheck)}</span>
            </div>
            <div>
              Last success:{' '}
              <span className="text-osint-text">
                {relativeTime(api.lastSuccess)}
              </span>
            </div>
            {api.responseTimeMs != null && (
              <div>
                Response:{' '}
                <span className="text-osint-text">{api.responseTimeMs} ms</span>
              </div>
            )}
            {api.recordsCount != null && (
              <div>
                Records:{' '}
                <span className="text-osint-text">{api.recordsCount}</span>
              </div>
            )}
            {(api.intelTotal ?? 0) > 0 && (
              <div className="col-span-2 pt-0.5 mt-0.5 border-t border-osint-border/30">
                <span className="text-osint-muted">Intel master:</span>{' '}
                <span className="text-osint-text">{api.intelTotal.toLocaleString()}</span>
                {' · '}
                <span className="text-status-online">{api.intelGeocoded.toLocaleString()} geocoded</span>
                {api.intelUngeocoded > 0 && (
                  <>
                    {' · '}
                    <span className="text-osint-muted">
                      {api.intelUngeocoded.toLocaleString()} ungeocoded
                    </span>
                  </>
                )}
                {api.intelAwaitingGeo > 0 && (
                  <>
                    {' · '}
                    <span className="text-status-pending" title="Awaiting LLM geocoder">
                      {api.intelAwaitingGeo.toLocaleString()} awaiting geo
                    </span>
                  </>
                )}
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
              <div className="text-osint-muted uppercase tracking-wider text-[9px] mb-0.5">
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
                        ? 'bg-osint-border/40 text-osint-muted border-osint-border/30'
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
          {keysKnown && api.requiresKey && (
            <div className="pt-1 flex flex-wrap items-center gap-2">
              <button
                type="button"
                onClick={pingNow}
                disabled={pinging || !api.configured}
                title={!api.configured ? `Set ${api.missingVars.join(', ')} first` : 'Probe once with your credentials'}
                className="px-2 py-0.5 rounded text-[10px] border bg-neon-cyan/10 text-neon-cyan border-neon-cyan/40 hover:bg-neon-cyan/20 disabled:opacity-40 disabled:cursor-not-allowed transition-colors"
              >
                {pinging ? 'Pinging…' : 'Ping now'}
              </button>
              <button
                type="button"
                onClick={toggleConsent}
                disabled={consentBusy || (!api.probeConsent && !api.configured)}
                title={
                  !api.configured && !api.probeConsent
                    ? `Set ${api.missingVars.join(', ')} first`
                    : api.probeConsent
                    ? 'Stop the scheduler from probing this source'
                    : 'Allow the scheduler to probe this source every 2h'
                }
                className={`px-2 py-0.5 rounded text-[10px] border transition-colors disabled:opacity-40 disabled:cursor-not-allowed ${
                  api.probeConsent
                    ? 'bg-status-online/10 text-status-online border-status-online/40 hover:bg-status-online/20'
                    : 'bg-osint-border/40 text-osint-text border-osint-border/40 hover:bg-gray-600/40'
                }`}
              >
                {consentBusy
                  ? '…'
                  : api.probeConsent
                  ? '✓ Scheduled probing on'
                  : 'Allow scheduled probing'}
              </button>
              {actionError && (
                <span className="text-[9px] text-status-offline">{actionError}</span>
              )}
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
          {keysKnown && <ProbeDetail api={api} />}
        </div>
      )}
    </div>
  );
}

export default function SourcesPanel({ onClose }) {
  const [filter, setFilter] = useState('all');
  const [draft, setDraft] = useState('');
  const [search, setSearch] = useState('');
  const [expandedId, setExpandedId] = useState(null);
  // Tabs: sources | discovery | follow | tables | scheduler
  const [tab, setTab] = useState('sources');
  useEffect(() => { const t = setTimeout(() => setSearch(draft.trim()), 300); return () => clearTimeout(t); }, [draft]);

  // The counters: /api/status?summary=1, which counts every source server
  // side and ships no rows. Polled every 30 s as the panel always was.
  const sum = useApi(tab === 'sources' ? '/api/status?summary=1' : null, { enabled: tab === 'sources', poll: 30_000, deps: [tab] });
  const f = FILTERS.find((x) => x.key === filter) || FILTERS[0];
  const path = useMemo(() => searchPath({ q: search, ...f.q, sort: 'name' }), [search, f]);
  const list = usePagedList(tab === 'sources' ? path : null, { pageSize: 50, enabled: tab === 'sources' });
  const filtered = useMemo(() => list.rows.map(fromSearchRow), [list.rows]);
  const data = sum.data;
  const loading = list.loading;
  const error = list.error ? errorMessage(list.error) : null;

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
                tab === t.id ? 'text-neon-cyan' : 'text-osint-muted hover:text-osint-text'
              }`}
            >
              {t.label}
            </button>
          ))}
          {tab === 'sources' && summary && (
            <span className="text-[10px] text-osint-muted font-mono">
              {summary.working}/{summary.total} working
            </span>
          )}
        </div>
        <button
          type="button"
          onClick={onClose}
          className="text-osint-muted hover:text-neon-cyan text-sm leading-none"
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
        <div className="grid grid-cols-6 gap-2 px-4 py-2 text-[10px] border-b border-osint-border/40 flex-shrink-0">
          <div>
            <div className="text-osint-muted">Online</div>
            <div className="font-mono text-status-online">{summary.online}</div>
          </div>
          <div>
            <div className="text-osint-muted">Degraded</div>
            <div className="font-mono text-status-degraded">
              {summary.degraded}
            </div>
          </div>
          <div>
            <div className="text-osint-muted">Offline</div>
            <div className="font-mono text-status-offline">
              {summary.offline}
            </div>
          </div>
          <div>
            <div className="text-osint-muted">Pending</div>
            <div className="font-mono text-osint-muted">
              {summary.pending ?? 0}
            </div>
          </div>
          <div>
            <div className="text-osint-muted">Gated</div>
            <div className="font-mono text-osint-text">
              {summary.gated ?? 0}
            </div>
          </div>
          <div>
            <div className="text-osint-muted">Keys set</div>
            <div className="font-mono text-neon-cyan">
              {summary.configured}/{summary.requiresKey}
            </div>
          </div>
        </div>
      )}

      <div className="px-3 py-2 border-b border-osint-border/40 space-y-2 flex-shrink-0">
        <input
          type="text"
          placeholder="Search sources (server-side: id, name, 日本語, collector, host)…"
          value={draft}
          onChange={(e) => setDraft(e.target.value)}
          className="w-full bg-osint-bg/60 border border-osint-border rounded px-2 py-1 text-xs text-osint-text placeholder:text-osint-muted/70 focus:outline-none focus:border-neon-cyan/50"
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
                  : 'bg-osint-bg/40 text-osint-muted border-osint-border hover:text-osint-text'
              }`}
            >
              {f.label}
            </button>
          ))}
        </div>
      </div>

      <div className="flex-1 overflow-y-auto px-3 py-2 space-y-1">
        {loading && list.rows.length === 0 && (
          <div className="flex items-center justify-center py-6">
            <LoadingSpinner />
          </div>
        )}
        {/* Gating this on `!data` hid every failed 30s refresh: the rows below
         *  stayed on screen with their status badges asserting a health we had
         *  stopped being able to confirm. State it in-band instead. */}
        {error && (
          <div className="text-status-offline text-xs px-2 py-3">
            Failed to load sources: {error}
            {list.rows.length > 0 && ' — the rows below are the last successful fetch, not current.'}
          </div>
        )}
        {sum.error && (
          <div className="text-status-offline text-[10px] px-2">Fleet counters unavailable — {errorMessage(sum.error)}</div>
        )}
        {list.loaded && !error && filtered.length === 0 && (
          <div className="text-osint-muted text-xs px-2 py-4 text-center">
            No sources match the current filter.
          </div>
        )}
        {filtered.map((api) => (
            <ApiRow
              key={api.id}
              api={api}
              expanded={expandedId === api.id}
              onToggle={() =>
                setExpandedId((id) => (id === api.id ? null : api.id))
              }
            />
          ))}
        {filtered.length > 0 && (
          <div className="flex items-center gap-2 px-1 py-1 text-[10px] font-mono text-osint-muted">
            <span>{filtered.length.toLocaleString()} of {list.total != null ? list.total.toLocaleString() : 'an unreported number of'} sources</span>
            {list.hasMore && (
              <button type="button" onClick={list.loadMore} disabled={list.loadingMore} className="px-2 py-0.5 rounded border border-osint-border hover:text-neon-cyan disabled:opacity-40">
                {list.loadingMore ? 'Loading…' : 'Load more'}
              </button>
            )}
            {list.moreError && <span className="text-status-offline">next page failed — {errorMessage(list.moreError)}</span>}
          </div>
        )}
      </div>

      {data && (
        <div className="px-3 py-1.5 border-t border-osint-border/40 text-[10px] text-osint-muted flex items-center justify-between flex-shrink-0">
          <span>Counters refresh every 30 s; the list is a server search (reload with the filter buttons)</span>
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
