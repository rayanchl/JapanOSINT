import React from 'react';
import { useParams, useSearchParams, useNavigate } from 'react-router-dom';
import apiUrl from '../../utils/apiUrl.js';
import { entityVisual } from '../../utils/entityVisuals.js';
import { useEntity } from '../../hooks/useSearch.js';
import EntityGraph from './EntityGraph.jsx';

/** Resolve a /:type/lookup?q=value chip link to a concrete entity_id. */
function useResolvedId(type, id) {
  const [params] = useSearchParams();
  const [resolved, setResolved] = React.useState(id === 'lookup' ? null : id);
  const [missing, setMissing] = React.useState(false);
  React.useEffect(() => {
    if (id !== 'lookup') { setResolved(id); return; }
    const q = params.get('q');
    if (!q) { setMissing(true); return; }
    (async () => {
      try {
        const r = await fetch(apiUrl(`/api/entities/search?q=${encodeURIComponent(q)}&type=${type}&limit=1`));
        const j = r.ok ? await r.json() : { results: [] };
        if (j.results?.[0]) setResolved(j.results[0].entity_id);
        else setMissing(true);
      } catch { setMissing(true); }
    })();
  }, [type, id, params]);
  return { resolved, missing };
}

export default function EntityProfile() {
  const { type, id } = useParams();
  const navigate = useNavigate();
  const { resolved, missing } = useResolvedId(type, id);
  const [tab, setTab] = React.useState('graph');

  if (missing) {
    return <div className="p-8 text-sm text-gray-500">Entity not found. It may not have been extracted yet.</div>;
  }
  if (!resolved) {
    return <div className="p-8 text-sm text-gray-500">Resolving…</div>;
  }
  return <Loaded type={type} entityId={resolved} tab={tab} setTab={setTab} navigate={navigate} />;
}

function Loaded({ type, entityId, tab, setTab, navigate }) {
  const { profile, graph, mentions, depth, setDepth, loading } = useEntity(type, entityId);
  const v = entityVisual(type);

  return (
    <div className="h-full overflow-auto p-4">
      <div className="max-w-4xl mx-auto space-y-4">
        <button type="button" onClick={() => navigate(-1)} className="text-xs text-gray-500 hover:text-gray-300">
          ← back
        </button>

        <div className="flex items-baseline gap-3">
          <span className={`px-2 py-0.5 rounded border text-xs ${v.color}`}>{v.label}</span>
          <h1 className="text-lg font-semibold text-gray-100">
            {loading ? 'Loading…' : (profile?.value || entityId)}
          </h1>
          {profile && (
            <span className="text-xs text-gray-500">
              {profile.mention_count} mention{profile.mention_count === 1 ? '' : 's'}
            </span>
          )}
        </div>

        {profile?.aliases?.length > 0 && (
          <div className="text-xs text-gray-500">aliases: {profile.aliases.join(' · ')}</div>
        )}

        <div className="flex gap-1 border-b border-osint-border">
          {['graph', 'timeline', 'raw'].map((t) => (
            <button
              key={t}
              type="button"
              onClick={() => setTab(t)}
              className={`px-3 py-1.5 text-sm ${tab === t ? 'text-neon-cyan border-b-2 border-neon-cyan' : 'text-gray-500 hover:text-gray-300'}`}
            >
              {t === 'graph' ? 'Relationships' : t === 'timeline' ? 'Timeline' : 'Raw'}
            </button>
          ))}
        </div>

        {tab === 'graph' && (
          <div className="space-y-2">
            <div className="flex items-center gap-2 text-xs text-gray-500">
              depth:
              {[1, 2, 3].map((d) => (
                <button
                  key={d}
                  type="button"
                  onClick={() => setDepth(d)}
                  className={`px-2 py-0.5 rounded border ${depth === d ? 'text-neon-cyan border-neon-cyan/40' : 'border-osint-border hover:text-gray-300'}`}
                >
                  {d}
                </button>
              ))}
            </div>
            <EntityGraph graph={graph} rootId={entityId} />
          </div>
        )}

        {tab === 'timeline' && (
          <ul className="space-y-2">
            {mentions.length === 0 && <li className="text-sm text-gray-600">No mentions.</li>}
            {mentions.map((m, i) => (
              <li key={i} className="rounded border border-osint-border bg-osint-surface p-2">
                <div className="text-xs text-gray-500">
                  {m.source_id} · {m.field} · {m.published_at || m.created_at}
                </div>
                <div className="text-sm text-gray-200">{m.title || m.surface || '(untitled)'}</div>
                {m.summary && <div className="text-xs text-gray-400 mt-0.5">{m.summary}</div>}
                {m.link && (
                  <a href={m.link} target="_blank" rel="noreferrer" className="text-xs text-neon-cyan hover:underline">
                    source ↗
                  </a>
                )}
              </li>
            ))}
          </ul>
        )}

        {tab === 'raw' && (
          <pre className="text-xs text-gray-400 bg-black/20 rounded p-3 overflow-auto">
            {JSON.stringify(profile, null, 2)}
          </pre>
        )}
      </div>
    </div>
  );
}
