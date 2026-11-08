import { useCallback, useEffect, useRef, useState } from 'react';
import { api, ApiError } from '../api/client.js';
import { canWriteWorkspace, isWorkspaceAdmin } from '../auth/roles.js';

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

/*
 * Who may change a case — exactly casesapi.c's three gates, which combine the
 * caller's WORKSPACE role with their role on this case's roster
 * (`my_case_role`, null when they are not on it; GET /api/cases/:id carries
 * it, and so does every row of the GET /api/cases list). Nothing here depends on who created the case: the
 * creator is put on the roster as lead when the case is made, and that lead
 * role — which the roster can later change — is what counts.
 */

/** can_write(): edit fields, change status, pin/unpin, comment. Analyst or
 *  above in the workspace, or the case's lead or a contributor — a case
 *  viewer reads only (casesapi.c, decided 2026-10-09). */
export function caseCanWrite(workspaceRole, myCaseRole) {
  return canWriteWorkspace(workspaceRole) || myCaseRole === 'lead' || myCaseRole === 'contributor';
}

/** can_delete(): workspace owner/admin, or the case lead. */
export function caseCanDelete(workspaceRole, myCaseRole) {
  return isWorkspaceAdmin(workspaceRole) || myCaseRole === 'lead';
}

/** can_manage_roster(): analyst or above in the workspace, or the case lead. */
export function caseCanManageRoster(workspaceRole, myCaseRole) {
  return canWriteWorkspace(workspaceRole) || myCaseRole === 'lead';
}

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
 * Where a keyset page says the next page starts. Every route answers
 * `page.next_cursor`; the camera-stills route answered `page.cursor`
 * (2026-11-07) and a client keyed on one name read the other as "no more".
 * Both are accepted until the server spelling is one.
 */
export function nextCursorOf(page) {
  return (page && (page.next_cursor || page.cursor)) || null;
}

/** The measured total a keyset page carries, or null when it sent none. */
export function cursorTotalOf(page) {
  return page && Number.isFinite(page.total) ? page.total : null;
}

/**
 * THE keyset-paged loader for any `{data:[…],page:{next_cursor,total?}}`
 * endpoint. `path` already carries its own filters; the hook appends
 * `limit` and `cursor`.
 *
 * What it guarantees (2026-11-07 — every one of these was missing from at
 * least one of the hand-rolled loaders it replaces):
 *   - a response that arrives after a newer reload/path change is dropped
 *     (request sequence), so a slow first page cannot overwrite a newer list;
 *   - `total` is the server's own count when it sends one (`page.total`),
 *     null otherwise — never rows.length dressed up as a total;
 *   - a failed "load more" lands in `moreError` and leaves `error` (the
 *     list's own state) and the rows already shown alone;
 *   - `page.cursor` is read as well as `page.next_cursor`;
 *   - a repeated row on a later page (an insert shifting the keyset) is not
 *     appended twice when `key` names the identity field.
 */
export function usePaged(path, { limit = 50, enabled = true, deps = [], key = 'id', query } = {}) {
  const [rows, setRows] = useState([]);
  const [cursor, setCursor] = useState(null);
  const [page, setPage] = useState(null);
  const [meta, setMeta] = useState(null);
  const [error, setError] = useState(null);
  const [moreError, setMoreError] = useState(null);
  const [loading, setLoading] = useState(Boolean(path) && enabled);
  const [loadingMore, setLoadingMore] = useState(false);
  const [loaded, setLoaded] = useState(false);
  const seq = useRef(0);
  const qkey = JSON.stringify(query || {});

  const fetchPage = useCallback(async (cur) => {
    const sep = path.includes('?') ? '&' : '?';
    const url = `${path}${sep}limit=${limit}${cur ? `&cursor=${encodeURIComponent(cur)}` : ''}`;
    return api.get(url, query ? { query } : undefined);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [path, limit, qkey]);

  const reload = useCallback(async ({ silent = false } = {}) => {
    if (!path || !enabled) { setLoading(false); return; }
    const my = ++seq.current;
    if (!silent) setLoading(true);
    try {
      const j = await fetchPage(null);
      if (my !== seq.current) return;
      setRows(Array.isArray(j?.data) ? j.data : []);
      setPage(j?.page || null);
      setCursor(nextCursorOf(j?.page));
      setMeta(j?.meta || null);
      setError(null);
      setMoreError(null);
      setLoaded(true);
    } catch (e) {
      if (my !== seq.current || e?.name === 'AbortError') return;
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
      if (my !== seq.current) return;              // a reload replaced the list
      const next = Array.isArray(j?.data) ? j.data : [];
      setRows((xs) => {
        if (!key) return [...xs, ...next];
        const seen = new Set(xs.map((r) => r?.[key]));
        return [...xs, ...next.filter((r) => r?.[key] == null || !seen.has(r[key]))];
      });
      setPage(j?.page || null);
      setCursor(nextCursorOf(j?.page));
      setMoreError(null);
    } catch (e) {
      if (my === seq.current && e?.name !== 'AbortError') setMoreError(e);
    } finally { if (my === seq.current) setLoadingMore(false); }
  }, [cursor, loadingMore, fetchPage, key]);

  useEffect(() => { reload(); }, [reload]);

  return {
    rows, setRows, page, meta, hasMore: Boolean(cursor), total: cursorTotalOf(page),
    error, moreError, loading, loaded, loadingMore, reload, loadMore,
  };
}

/**
 * GET /api/export/:kind as a file download (exportapi.c: intel, entities,
 * breach, case, alert-events; format json | csv | geojson). The query is the
 * kind's own filters — exportapi reads only those and refuses (414) a query
 * string it would have to truncate. A refusal before streaming starts is an
 * HTTP error whose body names it (`source_required`, `forbidden`,
 * `sort_not_honoured`, …); that body is thrown as an ApiError so the caller
 * shows the server's reason, never a bare status. The plan's row cap and any
 * truncation are stated inside the file by the server.
 */
export async function downloadExport(kind, query = {}, format = 'csv') {
  const res = await api.raw(`/api/export/${encodeURIComponent(kind)}`, { query: { ...query, format } });
  if (!res.ok) {
    let body = null;
    try { body = await res.json(); } catch { /* not json */ }
    const msg = (body && (body.detail || body.error)) || `HTTP ${res.status}`;
    throw new ApiError(res.status, msg, body);
  }
  const blob = await res.blob();
  downloadBlob(blob, filenameFromDisposition(res.headers.get('content-disposition'), `${kind}.${format}`));
  return blob.size;
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
