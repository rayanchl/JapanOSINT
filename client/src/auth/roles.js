/**
 * Workspace roles, ranked the way the server ranks them.
 *
 * Every member of a workspace reads every case, area of interest, watchlist,
 * alert rule and breach monitor in it. What a member may CHANGE is decided by
 * their workspace role, not by who created the row:
 *
 *   aoiapi.c, alertsapi.c   role_rank(): owner 4 > admin 3 > analyst 2 >
 *                           viewer 1; create/edit/delete need analyst or above
 *   breach_monitor.c        can_write(): owner, admin or analyst
 *   casesapi.c              tenant_at_least_analyst() / tenant_is_admin(),
 *                           plus the per-case roster (see useCases.js)
 *
 * A missing or unrecognised role ranks 0 and may do nothing, because that is
 * what the server does with it (it fails closed). The client mirrors these
 * rules so it offers exactly the buttons the server will honour: no button
 * that answers 403, and no author-only rule the server does not have.
 */
export function roleRank(role) {
  switch (role) {
    case 'owner': return 4;
    case 'admin': return 3;
    case 'analyst': return 2;
    case 'viewer': return 1;
    default: return 0;
  }
}

/** Analyst, admin or owner: create, edit and delete workspace rows. */
export function canWriteWorkspace(role) { return roleRank(role) >= 2; }

/** Owner or admin. */
export function isWorkspaceAdmin(role) { return roleRank(role) >= 3; }

/**
 * One line for a member whose role cannot change these rows, or null when it
 * can. Says why the buttons are missing instead of leaving them to guess.
 */
export function readOnlyNote(role, noun) {
  if (canWriteWorkspace(role)) return null;
  return `Your role in this workspace is ${role || 'not known'}: you can read every ${noun} here, `
    + 'but creating, editing and deleting them needs the analyst, admin or owner role.';
}
