import React from 'react';
import { useApi } from '../../hooks/useApi.js';
import { Section, Pill, ErrorNotice, LoadingState, BoundNote } from '../ui/kit.jsx';
import IntelItemRow from './IntelItemRow.jsx';

/** One line saying what a vector-neighbour list is built from, from its meta.
 *  Exported for tests. */
export function similarBasis(meta) {
  if (!meta) return null;
  if (meta.mode === 'case_related' && meta.case) {
    const c = meta.case;
    const capped = c.vectors_capped > 0 ? ` (${c.vectors_capped} past the ${c.vector_cap}-vector cap)` : '';
    return `nearest to the mean of ${c.with_vector} of ${c.intel_items} pinned intel item(s) with a vector${capped}; pinned items excluded`;
  }
  if (meta.mode === 'similar') return 'nearest to this item’s stored vector; the item itself excluded';
  return null;
}

/**
 * Rows nearest to a stored vector — GET /api/intel/items/:uid/similar or
 * /api/cases/:id/related (core/semsearchapi.c). No embedding call is made on
 * the server. A 409 `not_embedded` is not an empty result: the record (or
 * every pinned item) has no vector yet, and the coverage block says how far
 * the embedding pod has got.
 */
export default function SimilarItems({ path, label = 'Similar items', noun = 'similar items' }) {
  const { data, error, loading, reload } = useApi(path, { deps: [path] });
  const rows = Array.isArray(data?.data) ? data.data : [];
  const meta = data?.meta;
  const cov = error?.body?.meta?.coverage || meta?.coverage;
  const basis = similarBasis(meta || error?.body?.meta);
  const covLine = cov && cov.embedded_count != null
    ? `index: ${Number(cov.embedded_count).toLocaleString()} embedded${cov.eligible_count != null ? ` of ${Number(cov.eligible_count).toLocaleString()} eligible` : ''}${cov.model ? ` · ${cov.model}` : ''}`
    : null;
  return (
    <Section label={label} right={meta && <span className="font-mono text-[10px] text-osint-muted">k {meta.k}</span>}>
      {loading && <LoadingState label="Ranking by vector similarity…" />}
      {error && error.status === 409 && (
        <div className="text-xs text-osint-muted space-y-1">
          <div>{error.body?.detail || 'No vector yet — nothing to rank against.'}</div>
          {basis && <div className="font-mono text-[10px]">{basis}</div>}
          {covLine && <div className="font-mono text-[10px]">{covLine}</div>}
        </div>
      )}
      {error && error.status !== 409 && (
        <ErrorNotice error={error} title={error.status === 503 ? 'Vector search is unavailable on this server' : 'Could not rank similar items'} onRetry={reload}>
          {error.status === 503 && 'The embedding pod has not built an index. This is not an empty result.'}
          {covLine && <div className="font-mono mt-1 text-[10px]">{covLine}</div>}
        </ErrorNotice>
      )}
      {meta && (
        <div className="space-y-0.5 mb-1">
          <div className="flex flex-wrap items-center gap-2">
            <BoundNote shown={meta.shown} total={meta.total} noun={noun} />
            {meta.tenant_withheld > 0 && <Pill tone="warning">{meta.tenant_withheld} withheld (other tenants)</Pill>}
          </div>
          {basis && <div className="font-mono text-[10px] text-osint-muted">{basis}</div>}
          {covLine && <div className="font-mono text-[10px] text-osint-muted">{covLine}</div>}
        </div>
      )}
      {!loading && !error && data && rows.length === 0 && <div className="text-xs text-osint-muted">The index returned no other row.</div>}
      <div className="space-y-1">
        {rows.map((it) => <IntelItemRow key={it.uid} item={it} />)}
      </div>
    </Section>
  );
}
