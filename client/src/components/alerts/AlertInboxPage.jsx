import React, { useCallback, useEffect, useRef, useState } from 'react';
import { Link } from 'react-router-dom';
import { LuRefreshCw, LuMailOpen, LuCheckCheck, LuBellOff, LuBell, LuExternalLink } from 'react-icons/lu';
import { api, errorMessage } from '../../api/client.js';
import { relativeTime, fmtAbs } from '../../utils/time.js';
import { isSafeUrl } from '../../utils/safeUrl.js';
import {
  Page, Card, Pill, Button, Segmented, Select, ErrorNotice, EmptyState, LoadingState, BoundNote, CopyButton, toast, cx,
} from '../ui/kit.jsx';
import { MUTE_OPTIONS } from './alertShared.jsx';
import { useAuth } from '../../auth/AuthContext.jsx';
import { canWriteWorkspace } from '../../auth/roles.js';

/**
 * Alert inbox — port of iOS `AlertInboxView`/`AlertInboxModel`.
 *   GET  /api/alert-events?unread=1&limit=&cursor=  → {data, page:{next_cursor,limit,total}, meta}
 *   POST /api/alert-events/:id/read · POST /api/alert-events/read-all
 *   POST /api/alerts/:rule_id/mute {duration_sec} · /unmute
 * `page.total` is MEASURED by the server and null when its COUNT failed —
 * never rendered as a plausible 0.
 *
 * The inbox is the workspace's, not the reader's: every member sees every
 * event, and read state is shared (alertsapi.c alerteventsapi), so marking an
 * event read marks it read for everyone. Reading and marking read are open to
 * every member; muting is a change to the rule and needs analyst or above.
 */
const PAGE = 100;

/**
 * GET /api/alert-events/:id/deliveries — every delivery row for one event
 * (alertsapi.c alertdeliveriesapi): channel, attempt, status, HTTP code,
 * error, next attempt, plus a summary. The ledger was written on every
 * attempt and had no UI until 2026-11-07, so "why did this alert never
 * arrive" could only be answered from `delivered_channels`.
 */
export function DeliveriesPanel({ eventId }) {
  const [state, setState] = useState({ loading: true, data: null, error: null });
  useEffect(() => {
    let alive = true;
    setState({ loading: true, data: null, error: null });
    api.get(`/api/alert-events/${encodeURIComponent(eventId)}/deliveries`)
      .then((d) => { if (alive) setState({ loading: false, data: d, error: null }); })
      .catch((e) => { if (alive) setState({ loading: false, data: null, error: e }); });
    return () => { alive = false; };
  }, [eventId]);
  if (state.loading) return <div className="text-[11px] text-osint-muted">Loading delivery attempts…</div>;
  if (state.error) return <div className="text-[11px] text-neon-red font-mono">Delivery ledger unavailable — {errorMessage(state.error)}</div>;
  const rows = Array.isArray(state.data?.data) ? state.data.data : [];
  const sum = state.data?.summary || {};
  return (
    <div className="mt-2 border-t border-osint-border/40 pt-2 text-[11px]">
      <div className="font-mono text-osint-muted">
        {rows.length} row{rows.length === 1 ? '' : 's'} · {sum.total_attempts ?? 0} attempts · ok {sum.ok ?? 0} · pending {sum.pending ?? 0} · failed {sum.failed ?? 0} · dead {sum.dead ?? 0} · skipped {sum.skipped ?? 0}
      </div>
      {sum.note && <div className="text-osint-muted mt-0.5">{sum.note}</div>}
      {rows.length > 0 && (
        <table className="w-full mt-1">
          <thead><tr className="text-osint-muted text-left"><th className="pr-2">channel</th><th className="pr-2">target</th><th className="pr-2">attempt</th><th className="pr-2">status</th><th className="pr-2">HTTP</th><th className="pr-2">when</th><th>error</th></tr></thead>
          <tbody>
            {rows.map((r) => (
              <tr key={r.id} className="border-t border-osint-border/30 align-top">
                <td className="pr-2 font-mono">{r.no_channel_attempted ? 'none attempted' : `${r.channel_type} #${r.channel_idx}`}</td>
                <td className="pr-2 font-mono break-all">{r.target || '—'}</td>
                <td className="pr-2 font-mono">{r.attempt}</td>
                <td className="pr-2"><Pill tone={r.status === 'ok' ? 'success' : r.status === 'pending' ? 'accent' : 'danger'}>{r.status}</Pill></td>
                <td className="pr-2 font-mono">{r.http_code ?? '—'}</td>
                <td className="pr-2 font-mono" title={r.attempted_at || r.next_attempt_at || ''}>{r.attempted_at ? relativeTime(r.attempted_at) : r.next_attempt_at ? `next ${relativeTime(r.next_attempt_at)}` : '—'}</td>
                <td className="text-osint-muted break-all">{r.error || ''}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </div>
  );
}

export default function AlertInboxPage() {
  const auth = useAuth();
  const canMute = canWriteWorkspace(auth.role);
  const [filter, setFilter] = useState('all');
  const [rows, setRows] = useState([]);
  const [page, setPage] = useState(null);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(true);
  const [busy, setBusy] = useState(null);
  const [openDeliveries, setOpenDeliveries] = useState(null);
  // Only the newest request may write. Without this, switching All → Unread
  // while the All page was still in flight let the late All answer land under
  // the Unread filter.
  const seq = useRef(0);

  const load = useCallback(async (cursor) => {
    const my = ++seq.current;
    setLoading(true); setError(null);
    try {
      const j = await api.get('/api/alert-events', { query: { limit: PAGE, unread: filter === 'unread' ? 1 : undefined, cursor } });
      if (my !== seq.current) return;
      const d = Array.isArray(j?.data) ? j.data : [];
      setRows((xs) => (cursor ? [...xs, ...d] : d));
      setPage(j?.page || null);
    } catch (e) { if (my === seq.current) setError(e); }
    finally { if (my === seq.current) setLoading(false); }
  }, [filter]);

  useEffect(() => { setRows([]); setPage(null); load(); }, [load]);

  const markRead = async (ev) => {
    setBusy(ev.id);
    try {
      await api.post(`/api/alert-events/${encodeURIComponent(ev.id)}/read`);
      setRows((xs) => xs.map((x) => (x.id === ev.id ? { ...x, unread: false, read_at: new Date().toISOString() } : x)));
    } catch (e) { toast(errorMessage(e), { tone: 'danger', ttl: 6000 }); }
    finally { setBusy(null); }
  };

  const markAll = async () => {
    setBusy('all');
    try {
      await api.post('/api/alert-events/read-all');
      toast('Inbox marked read', { tone: 'accent' });
      await load();
    } catch (e) { toast(errorMessage(e), { tone: 'danger', ttl: 6000 }); }
    finally { setBusy(null); }
  };

  const muteRule = async (ev, dur) => {
    setBusy(ev.id);
    try {
      if (dur === 'unmute') await api.post(`/api/alerts/${encodeURIComponent(ev.rule_id)}/unmute`);
      else await api.post(`/api/alerts/${encodeURIComponent(ev.rule_id)}/mute`, { duration_sec: dur });
      const label = MUTE_OPTIONS.find((o) => String(o.value) === String(dur))?.label?.replace('Mute ', '') || '';
      toast(dur === 'unmute' ? `Unmuted “${ev.rule_name || ev.rule_id}”` : `Muted “${ev.rule_name || ev.rule_id}” for ${label}`, { tone: 'accent' });
    } catch (e) { toast(errorMessage(e), { tone: 'danger', ttl: 6000 }); }
    finally { setBusy(null); }
  };

  const unreadShown = rows.filter((r) => r.unread).length;
  const total = page?.total;

  return (
    <Page
      title="Inbox"
      subtitle="Everything this workspace's alert rules matched, newest first. Read state is shared: marking an event read marks it read for every member."
      actions={(
        <>
          <Segmented value={filter} onChange={setFilter} options={[{ value: 'all', label: 'All' }, { value: 'unread', label: 'Unread', count: unreadShown || undefined }]} />
          <Button onClick={() => load()} title="Refresh inbox"><LuRefreshCw size={13} /></Button>
          <Button onClick={markAll} busy={busy === 'all'} disabled={rows.length === 0} title="Mark every event read, for the whole workspace"><LuCheckCheck size={13} /> Mark all read</Button>
        </>
      )}
    >
      {error && <ErrorNotice error={error} title="Could not load the inbox" onRetry={() => load()} />}
      {loading && rows.length === 0 && !error && <LoadingState label="Loading inbox…" />}
      {!loading && !error && rows.length === 0 && (
        <EmptyState title={filter === 'unread' ? 'Nothing unread' : 'Inbox is empty'} action={filter === 'unread' ? <Button onClick={() => setFilter('all')}>Show all</Button> : <Link to="/console/alerts"><Button>Manage alert rules</Button></Link>}>
          Matches land here as soon as a rule fires. Delivery to email / webhook channels happens independently.
        </EmptyState>
      )}

      {rows.length > 0 && (
        <div className="space-y-1.5">
          {rows.map((ev) => (
            <Card key={ev.id} padded={false} className={cx('transition-colors', ev.unread ? 'border-accent/40' : '')}>
              <div className="p-3 flex items-start gap-3">
                <span className={cx('mt-1.5 w-2 h-2 rounded-full flex-shrink-0', ev.unread ? 'bg-accent' : 'bg-osint-border-bright')} aria-label={ev.unread ? 'unread' : 'read'} />
                <div className="min-w-0 flex-1">
                  <div className="flex items-center gap-2 flex-wrap">
                    <Link to={`/intel/items/${encodeURIComponent(ev.item_uid)}`} className={cx('text-sm truncate hover:text-accent', ev.unread ? 'text-osint-text font-medium' : 'text-osint-text')}>
                      {ev.item_title || ev.item_uid}
                    </Link>
                    {ev.item_source_id && <span className="text-[11px] text-osint-muted font-mono">· {ev.item_source_id}</span>}
                  </div>
                  <div className="flex items-center gap-2 flex-wrap mt-1 text-[11px] text-osint-muted">
                    <Pill tone="accent" title={ev.rule_id}>{ev.rule_name || ev.rule_id}</Pill>
                    <span className="font-mono" title={fmtAbs(ev.matched_at)}>{relativeTime(ev.matched_at)}</span>
                    {(ev.delivered_channels || []).length > 0 && <span className="font-mono">→ {ev.delivered_channels.join(', ')}</span>}
                    {ev.item_link && isSafeUrl(ev.item_link) && (
                      <a href={ev.item_link} target="_blank" rel="noreferrer" className="inline-flex items-center gap-0.5 hover:text-accent"><LuExternalLink size={11} /> source</a>
                    )}
                  </div>
                </div>
                <div className="flex items-center gap-1 flex-wrap justify-end">
                  {ev.unread && <Button size="sm" busy={busy === ev.id} onClick={() => markRead(ev)} title="Mark read"><LuMailOpen size={12} /> Read</Button>}
                  {canMute && (
                    <Select className="text-[11px] py-1" value="" title="Mute this rule for…" onChange={(e) => { const v = e.target.value; if (v) muteRule(ev, v === 'forever' || v === 'unmute' ? v : Number(v)); }}>
                      <option value="">Rule…</option>
                      {MUTE_OPTIONS.map((o) => <option key={String(o.value)} value={String(o.value)}>{o.label}</option>)}
                      <option value="unmute">Unmute rule</option>
                    </Select>
                  )}
                  <Button size="sm" variant="ghost" onClick={() => setOpenDeliveries((x) => (x === ev.id ? null : ev.id))} title="Per-channel delivery attempts">Deliveries</Button>
                  <CopyButton text={ev.item_uid} label="Copy id" />
                </div>
              </div>
              {openDeliveries === ev.id && <div className="px-3 pb-3"><DeliveriesPanel eventId={ev.id} /></div>}
            </Card>
          ))}
          <div className="flex items-center justify-between pt-1">
            {total == null ? (
              <div className="text-[11px] text-osint-muted font-mono">showing {rows.length} events · total not reported by the server</div>
            ) : (
              <BoundNote shown={rows.length} total={total} noun="events" />
            )}
            {page?.next_cursor && <Button size="sm" busy={loading} onClick={() => load(page.next_cursor)}>Load more</Button>}
          </div>
        </div>
      )}
      {canMute && <div className="text-[11px] text-osint-muted flex items-center gap-1"><LuBell size={11} /> Muting from here silences the whole rule for the workspace, not just this event. <LuBellOff size={11} /></div>}
    </Page>
  );
}
