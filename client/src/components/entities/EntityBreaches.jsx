import React, { useState } from 'react';
import { Link } from 'react-router-dom';
import { api } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { Section, Pill, Button, ErrorNotice, LoadingState, BoundNote, KV } from '../ui/kit.jsx';

const LIMIT = 50;

/**
 * Breach exposure for one entity — iOS `ExposureSection` (roadmap 23).
 *   GET /api/entities/:type/:id/breaches?limit&offset
 *   → { data: [{breach_id,item_uid,name,title,domain,breach_date,pwn_count,data_classes,verified,sensitive}],
 *       exposure: { breach_count, first_breach_date?, last_breach_date?, data_classes } }
 * Catalog metadata only; no secrets. The route answers 403 for a
 * non-operator BEFORE looking, so a refusal is rendered as a refusal and
 * never as "this identifier appears in no breach".
 */
export default function EntityBreaches({ type, entityId }) {
  const base = `/api/entities/${encodeURIComponent(type)}/${encodeURIComponent(entityId)}/breaches`;
  const { data, error, loading, reload, setData } = useApi(`${base}?limit=${LIMIT}&offset=0`, { deps: [type, entityId] });
  const rows = Array.isArray(data?.data) ? data.data : [];
  const exp = data?.exposure;
  const [more, setMore] = useState(false);
  const [moreError, setMoreError] = useState(null);
  const count = exp?.breach_count ?? null;
  const hasMore = count != null ? rows.length < count : rows.length > 0 && rows.length % LIMIT === 0;

  const loadMore = async () => {
    setMore(true); setMoreError(null);
    try {
      const j = await api.get(`${base}?limit=${LIMIT}&offset=${rows.length}`);
      setData({ ...j, data: [...rows, ...(Array.isArray(j?.data) ? j.data : [])] });
    } catch (e) { setMoreError(e); }
    finally { setMore(false); }
  };

  return (
    <Section label="Breach exposure" right={exp && <span className="font-mono text-[10px] text-osint-muted">{exp.breach_count} breach{exp.breach_count === 1 ? '' : 'es'}</span>}>
      {loading && <LoadingState label="Checking the breach corpus…" />}
      {error && (
        <ErrorNotice error={error} title={error.status === 403 ? 'Breach exposure is operator-gated' : 'Could not read breach exposure'} onRetry={reload}>
          {error.status === 403 && 'The server refused before looking, so this says nothing about whether the identifier appears in a breach.'}
        </ErrorNotice>
      )}
      {!loading && !error && data && (
        <>
          {exp && (
            <KV className="mb-2" pairs={[
              ['first seen', exp.first_breach_date], ['last seen', exp.last_breach_date],
              ['data classes', Array.isArray(exp.data_classes) && exp.data_classes.length ? exp.data_classes.join(', ') : null],
            ]} />
          )}
          {rows.length === 0 && <div className="text-xs text-osint-muted">This identifier appears in no breach of the loaded corpus.</div>}
          <ul className="space-y-1.5">
            {rows.map((b) => (
              <li key={b.breach_id} className="rounded-md border border-osint-border bg-osint-bg/60 px-2 py-1.5">
                <div className="flex flex-wrap items-center gap-1.5">
                  <span className="text-sm text-osint-text">{b.title || b.name || b.breach_id}</span>
                  {b.domain && <span className="font-mono text-[11px] text-osint-muted">{b.domain}</span>}
                  {b.breach_date && <span className="font-mono text-[11px] text-osint-muted">{b.breach_date}</span>}
                  {b.pwn_count != null && <span className="font-mono text-[11px] text-accent">{Number(b.pwn_count).toLocaleString()} accounts</span>}
                  {b.verified === true && <Pill tone="success">verified</Pill>}
                  {b.verified === false && <Pill>unverified</Pill>}
                  {b.sensitive && <Pill tone="danger">sensitive</Pill>}
                  {b.item_uid && <Link className="text-[11px] text-accent hover:underline" to={`/intel/items/${encodeURIComponent(b.item_uid)}`}>record ↗</Link>}
                </div>
                {Array.isArray(b.data_classes) && b.data_classes.length > 0 && (
                  <div className="flex flex-wrap gap-1 mt-1">{b.data_classes.map((c) => <Pill key={c}>{c}</Pill>)}</div>
                )}
              </li>
            ))}
          </ul>
          {moreError && <ErrorNotice error={moreError} title="Could not load more" className="mt-2" />}
          {(hasMore || count != null) && (
            <div className="flex items-center justify-between mt-2">
              <BoundNote shown={rows.length} total={count ?? (hasMore ? rows.length + 1 : rows.length)} noun="breaches" />
              {hasMore && <Button size="sm" busy={more} onClick={loadMore}>Load more</Button>}
            </div>
          )}
          <div className="text-[10px] text-osint-muted mt-2">Derived from breach corpus metadata. No passwords or other secrets are returned by this endpoint.</div>
        </>
      )}
    </Section>
  );
}
