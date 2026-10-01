import {
  LuMap, LuNewspaper, LuSearch, LuFolder, LuNetwork, LuStar, LuChartBar,
  LuChartPie, LuVideo, LuInbox, LuBellRing, LuMapPin, LuEye, LuShieldHalf,
  LuBookmark, LuKey, LuUsers, LuSettings, LuLockKeyhole, LuDatabase, LuClock, LuScrollText,
  LuSlidersHorizontal,
} from 'react-icons/lu';

/**
 * Navigation model — mirrors the iOS `RootView` sidebar (iPad) and
 * `ConsoleHub` list (phone) so both clients expose the same destinations
 * under the same names. `requires` gates a row on the signed-in identity:
 * 'manage' = workspace owner/admin, 'operator' = platform operator.
 */
export const WORKSPACE_NAV = [
  { to: '/', end: true, label: 'Map', icon: LuMap },
  { to: '/intel', label: 'Intel', icon: LuNewspaper },
  { to: '/search', label: 'Search', icon: LuSearch },
  { to: '/cases', label: 'Cases', icon: LuFolder },
  { to: '/entities', label: 'Entities', icon: LuNetwork },
  { to: '/saved', label: 'Saved', icon: LuStar },
  { to: '/timeline', label: 'Timeline', icon: LuChartBar },
];

export const CONSOLE_NAV = [
  { group: 'Operations', to: '/console/sources', label: 'Sources', subtitle: 'Status, charts, collectors', icon: LuChartPie },

  { group: 'Discovery', to: '/console/cameras', label: 'Camera discovery', subtitle: 'Probe public webcams', icon: LuVideo },
  { group: 'Discovery', to: '/console/inbox', label: 'Inbox', subtitle: 'Everything your rules matched', icon: LuInbox, badge: 'unread' },
  { group: 'Discovery', to: '/console/alerts', label: 'Alerts', subtitle: 'Rules, channels, history', icon: LuBellRing },
  { group: 'Discovery', to: '/console/aoi', label: 'Areas of interest', subtitle: 'Saved geofences for alert rules', icon: LuMapPin },
  { group: 'Discovery', to: '/console/watchlists', label: 'Watchlists', subtitle: 'Entities you are following', icon: LuEye },
  { group: 'Discovery', to: '/console/breach-monitors', label: 'Breach monitors', subtitle: 'Watch an address or domain', icon: LuShieldHalf },
  { group: 'Discovery', to: '/console/saved-searches', label: 'Saved searches', subtitle: 'Re-run and turn into alerts', icon: LuBookmark },

  { group: 'Configuration', to: '/console/api-keys', label: 'API keys', subtitle: 'Credentials & overlays', icon: LuKey },
  { group: 'Configuration', to: '/console/workspace', label: 'Workspace', subtitle: 'Members, permissions, queries', icon: LuUsers, requires: 'manage' },
  { group: 'Configuration', to: '/console/settings', label: 'Settings', subtitle: 'Appearance, limits, schedules', icon: LuSettings },

  { group: 'Admin', to: '/console/admin', label: 'Admin panel', subtitle: 'Maintenance, breach corpus, server', icon: LuLockKeyhole, requires: 'operator' },
  { group: 'Admin', to: '/console/database', label: 'Database', subtitle: 'Raw table browser', icon: LuDatabase, requires: 'operator' },
  { group: 'Admin', to: '/console/scheduler', label: 'Scheduler', subtitle: 'Run history and intervals', icon: LuClock, requires: 'operator' },
  { group: 'Admin', to: '/console/follow', label: 'Follow log', subtitle: 'Live collector output', icon: LuScrollText, requires: 'operator' },
];

/** Phone bottom tab bar — the iOS 5-tab layout. */
export const TAB_BAR = [
  WORKSPACE_NAV[0], WORKSPACE_NAV[1], WORKSPACE_NAV[2], WORKSPACE_NAV[3],
  { to: '/console', label: 'Console', icon: LuSlidersHorizontal, badge: 'unread' },
];

export function visibleConsoleNav({ canManageWorkspace, isPlatformAdmin }) {
  return CONSOLE_NAV.filter((r) => {
    if (r.requires === 'manage') return canManageWorkspace;
    if (r.requires === 'operator') return isPlatformAdmin === true;
    return true;
  });
}

export function groupBy(rows) {
  const out = [];
  for (const r of rows) {
    let g = out.find((x) => x.group === r.group);
    if (!g) { g = { group: r.group, rows: [] }; out.push(g); }
    g.rows.push(r);
  }
  return out;
}
