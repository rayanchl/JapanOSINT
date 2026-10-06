import { useMemo } from 'react';
import { useApi } from './useApi.js';

/**
 * Who wrote this? Everything in a workspace is visible to its members
 * (decided 2026-10-05), so a shared row — a saved search, a history entry, an
 * OSINT run, a note — names its author by internal `user_id` (plus `mine` for
 * the caller). This turns that id into the teammate's email through the
 * roster any member may read:
 *   GET /api/members → { members: [{ user_id, email, role }], invites: [...] }
 */
export function memberNameMap(data) {
  const out = {};
  const ms = Array.isArray(data?.members) ? data.members : [];
  for (const m of ms) if (m && m.user_id) out[m.user_id] = m.email || m.user_id;
  return out;
}

/**
 * Label for a row's author. `mine` wins ("you"); a known member shows their
 * email; an id no longer on the roster (a former member) says so rather than
 * printing a raw uuid; a row with no author at all says that.
 */
export function authorLabel(row, names, { userId } = {}) {
  const id = row?.user_id ?? row?.author_id ?? row?.created_by ?? null;
  const mine = row?.mine ?? (userId && id ? id === userId : undefined);
  if (mine) return 'you';
  if (!id) return 'unknown author';
  if (!names || !Object.keys(names).length) return 'a teammate'; // roster not loaded (yet)
  return names[id] || 'a former member';
}

/** `{ [user_id]: email }` for the active workspace. Empty until loaded. */
export function useMemberNames() {
  const { data } = useApi('/api/members');
  return useMemo(() => memberNameMap(data), [data]);
}
