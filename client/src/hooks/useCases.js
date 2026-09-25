import { useCallback, useEffect, useRef, useState } from 'react';
import { api } from '../api/client.js';

/* ------------------------------------------------------------------------
 * Cases — vocabularies (casesapi.h) and a keyset-paged list loader shared by
 * CasesPage, CaseDetailPage and the timeline's case scope.
 * ---------------------------------------------------------------------- */

export const CASE_STATUSES = ['open', 'closed', 'archived'];

export const CaseStatus = {
  label: (s) => ({ open: 'Open', closed: 'Closed', archived: 'Archived' }[s] || s || '—'),
  tone: (s) => ({ open: 'success', closed: 'neutral', archived: 'cyan' }[s] || 'neutral'),
};

/** priority: integer 0…5; the server 400s anything else. */
export const CASE_PRIORITIES = [0, 1, 2, 3, 4, 5];
export const CasePriority = {
  label: (p) => ['None', 'Low', 'Moderate', 'High', 'Severe', 'Critical'][Number(p)] ?? String(p),
  tone: (p) => (p >= 4 ? 'danger' : p === 3 ? 'warning' : p === 2 ? 'cyan' : 'neutral'),
};

/** The server's shared ref_type vocabulary (annotationsapi.c REF_TYPES). */
export const CASE_REF_TYPES = ['intel_item', 'entity', 'breach_item', 'feature', 'camera', 'search_run', 'attachment'];
export const CaseRefType = {
  label: (t) => ({
    intel_item: 'Intel items', entity: 'Entities', breach_item: 'Breach records', feature: 'Map features',
    camera: 'Cameras', search_run: 'Search runs', attachment: 'Attachments',
  }[t] || t),
  sortIndex: (t) => { const i = CASE_REF_TYPES.indexOf(t); return i < 0 ? 99 : i; },
};

export const CaseActivityKind = {
  label: (k) => ({
    created: 'Case created', updated: 'Fields edited', status_changed: 'Status changed',
    item_pinned: 'Finding pinned', item_unpinned: 'Finding unpinned', members_changed: 'Roster changed',
    comment: 'Comment',
  }[k] || k || 'Event'),
  tone: (k) => ({ comment: 'accent', item_pinned: 'success', item_unpinned: 'warning', created: 'cyan' }[k] || 'neutral'),
  isSystem: (k) => k !== 'comment',
};

export const CASE_ROLES = ['lead', 'contributor', 'viewer'];

/** Route for a pinned reference, or null when the web has no page for it. */
export function refLink(refType, refId, snapshot) {
  if (!refId) return null;
  switch (refType) {
    case 'intel_item': return `/intel/items/${encodeURIComponent(refId)}`;
    case 'entity': {
      const d = snapshot?.data;
      const type = d?.type ? String(d.type).toLowerCase() : null;
      const id = d?.entity_id || refId;
      // "type:id" is how entity refs are written when no snapshot resolved.
      if (!type && String(refId).includes(':')) {
        const [t, ...rest] = String(refId).split(':');
        return `/entities/${encodeURIComponent(t.toLowerCase())}/${encodeURIComponent(rest.join(':'))}`;
      }
      return type ? `/entities/${encodeURIComponent(type)}/${encodeURIComponent(id)}` : null;
    }
    case 'camera': return '/console/cameras';
    case 'search_run': return '/search';
    default: return null;
  }
}

/**
 * Keyset-paged loader for any `{data:[…],page:{next_cursor}}` endpoint.
 * `path` already carries its own filters; the hook appends `cursor`.
 */
export function usePaged(path, { limit = 50, enabled = true, deps = [] } = {}) {
  const [rows, setRows] = useState([]);
  const [cursor, setCursor] = useState(null);
  const [meta, setMeta] = useState(null);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(Boolean(path) && enabled);
  const [loadingMore, setLoadingMore] = useState(false);
  const seq = useRef(0);

  const fetchPage = useCallback(async (cur) => {
    const sep = path.includes('?') ? '&' : '?';
    const url = `${path}${sep}limit=${limit}${cur ? `&cursor=${encodeURIComponent(cur)}` : ''}`;
    return api.get(url);
  }, [path, limit]);

  const reload = useCallback(async () => {
    if (!path || !enabled) { setLoading(false); return; }
    const my = ++seq.current;
    setLoading(true);
    try {
      const j = await fetchPage(null);
      if (my !== seq.current) return;
      setRows(Array.isArray(j?.data) ? j.data : []);
      setCursor(j?.page?.next_cursor || null);
      setMeta(j?.meta || null);
      setError(null);
    } catch (e) {
      if (my !== seq.current) return;
      setError(e);
    } finally { if (my === seq.current) setLoading(false); }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [fetchPage, path, enabled, ...deps]);

  const loadMore = useCallback(async () => {
    if (!cursor || loadingMore) return;
    const my = seq.current;
    setLoadingMore(true);
    try {
      const j = await fetchPage(cursor);
      if (my !== seq.current) return;
      setRows((xs) => [...xs, ...(Array.isArray(j?.data) ? j.data : [])]);
      setCursor(j?.page?.next_cursor || null);
      setError(null);
    } catch (e) {
      if (my === seq.current) setError(e);
    } finally { setLoadingMore(false); }
  }, [cursor, loadingMore, fetchPage]);

  useEffect(() => { reload(); }, [reload]);

  return { rows, setRows, hasMore: Boolean(cursor), meta, error, loading, loadingMore, reload, loadMore };
}

/** Trigger a browser download for a Blob. */
export function downloadBlob(blob, filename) {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url; a.download = filename || 'download';
  document.body.appendChild(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 2000);
}

/** Parse `filename="…"` out of a Content-Disposition header. */
export function filenameFromDisposition(header, fallback) {
  const m = /filename="?([^";]+)"?/i.exec(header || '');
  return m ? m[1] : fallback;
}
