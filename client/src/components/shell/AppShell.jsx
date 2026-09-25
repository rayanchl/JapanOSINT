import React, { useEffect, useState } from 'react';
import { NavLink, useLocation, useNavigate } from 'react-router-dom';
import { LuPanelLeftClose, LuPanelLeftOpen, LuSun, LuMoon, LuLogOut } from 'react-icons/lu';
import { useAuth } from '../../auth/AuthContext.jsx';
import { WORKSPACE_NAV, TAB_BAR, visibleConsoleNav, groupBy } from './navConfig.js';
import { cx, ConfirmDialog, Toaster } from '../ui/kit.jsx';
import { useApi } from '../../hooks/useApi.js';

const COLLAPSE_KEY = 'osint:sidebar-collapsed';

function readCollapsed() {
  try { return window.localStorage.getItem(COLLAPSE_KEY) === '1'; } catch { return false; }
}

/** Unread alert-events count for the Console / Inbox badges. */
export function useUnreadCount() {
  const { data } = useApi('/api/alert-events/unread-count', { poll: 60_000 });
  if (!data) return 0;
  const n = data.unread ?? data.count ?? data.unread_count ?? 0;
  return Number.isFinite(Number(n)) ? Number(n) : 0;
}

function JSTClock({ className }) {
  const [time, setTime] = useState('');
  useEffect(() => {
    const fmtT = new Intl.DateTimeFormat('en-GB', { timeZone: 'Asia/Tokyo', hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false });
    const fmtD = new Intl.DateTimeFormat('en-GB', { timeZone: 'Asia/Tokyo', year: 'numeric', month: '2-digit', day: '2-digit' });
    const update = () => { const now = new Date(); setTime(`${fmtD.format(now)} ${fmtT.format(now)} JST`); };
    update();
    const t = setInterval(update, 1000);
    return () => clearInterval(t);
  }, []);
  return <span className={cx('font-mono text-[11px] text-osint-muted tabular-nums', className)}>{time}</span>;
}

function NavItem({ to, end, label, icon: Icon, badge, collapsed, subtitle }) {
  return (
    <NavLink
      to={to}
      end={end}
      title={collapsed ? label : subtitle}
      className={({ isActive }) => cx(
        'flex items-center gap-2.5 rounded-md px-2 py-1.5 text-sm transition-colors relative',
        collapsed && 'justify-center px-0',
        isActive ? 'bg-accent/10 text-accent' : 'text-osint-muted hover:text-osint-text hover:bg-white/5',
      )}
    >
      {({ isActive }) => (
        <>
          <span className={cx('flex items-center justify-center w-7 h-7 rounded-md flex-shrink-0', isActive ? 'bg-accent/15' : 'bg-transparent')}>
            <Icon size={16} />
          </span>
          {!collapsed && <span className="truncate flex-1">{label}</span>}
          {badge > 0 && (
            <span className={cx('font-mono text-[10px] rounded-full bg-accent text-black px-1.5 min-w-[18px] text-center',
              collapsed && 'absolute -top-0.5 -right-0.5')}>
              {badge > 99 ? '99+' : badge}
            </span>
          )}
        </>
      )}
    </NavLink>
  );
}

function SectionHeader({ children, collapsed }) {
  if (collapsed) return <div className="h-px bg-osint-border my-2 mx-2" />;
  return <div className="px-2 pt-3 pb-1 font-mono text-[10px] font-semibold uppercase tracking-[0.12em] text-osint-muted">{children}</div>;
}

export default function AppShell({ children, theme, onToggleTheme, status }) {
  const auth = useAuth();
  const [collapsed, setCollapsed] = useState(readCollapsed);
  const [confirmOut, setConfirmOut] = useState(false);
  const unread = useUnreadCount();
  const location = useLocation();
  const navigate = useNavigate();

  useEffect(() => { try { window.localStorage.setItem(COLLAPSE_KEY, collapsed ? '1' : '0'); } catch { /* ignore */ } }, [collapsed]);

  const consoleRows = visibleConsoleNav(auth);
  const groups = groupBy(consoleRows);
  const isMap = location.pathname === '/';

  return (
    <div className="h-screen w-screen flex bg-osint-bg text-osint-text overflow-hidden">
      {/* Sidebar (md+) */}
      <aside className={cx('hidden md:flex flex-col border-r border-osint-border bg-osint-surface flex-shrink-0 transition-[width] duration-200', collapsed ? 'w-14' : 'w-60')}>
        <div className={cx('flex items-center border-b border-osint-border h-12 px-3', collapsed ? 'justify-center' : 'justify-between')}>
          {!collapsed && (
            <NavLink to="/" className="font-mono text-base font-bold tracking-tight">
              <span>Japan</span><span className="text-accent">OSINT</span>
            </NavLink>
          )}
          <button type="button" onClick={() => setCollapsed((v) => !v)} className="text-osint-muted hover:text-osint-text" title={collapsed ? 'Expand sidebar' : 'Collapse sidebar'}>
            {collapsed ? <LuPanelLeftOpen size={16} /> : <LuPanelLeftClose size={16} />}
          </button>
        </div>

        <nav className="flex-1 overflow-y-auto px-2 py-1">
          <SectionHeader collapsed={collapsed}>Workspace</SectionHeader>
          {WORKSPACE_NAV.map((r) => <NavItem key={r.to} {...r} collapsed={collapsed} />)}
          {groups.map((g) => (
            <React.Fragment key={g.group}>
              <SectionHeader collapsed={collapsed}>{g.group === 'Operations' ? 'Console' : g.group}</SectionHeader>
              {g.rows.map((r) => <NavItem key={r.to} {...r} collapsed={collapsed} badge={r.badge === 'unread' ? unread : 0} />)}
            </React.Fragment>
          ))}
        </nav>

        <div className="border-t border-osint-border p-2 space-y-1.5">
          {!collapsed && status && (
            <div className="px-1 space-y-0.5">
              <div className="flex items-center gap-2 text-[11px]">
                <span className={cx('w-2 h-2 rounded-full', status.isConnected ? 'status-online pulse-live' : 'status-offline')} />
                <span className={status.isConnected ? 'text-neon-green' : 'text-neon-red'}>{status.isConnected ? 'LIVE' : 'DISCONNECTED'}</span>
                <span className="text-osint-muted ml-auto font-mono" title={status.sourcesError || ''}>
                  {status.activeSources == null ? (status.sourcesError ? 'sources unavailable' : '—') : `${status.activeSources} online`}
                </span>
              </div>
              <JSTClock />
            </div>
          )}
          <div className={cx('flex items-center gap-1', collapsed ? 'flex-col' : 'justify-between')}>
            {!collapsed && (
              <div className="min-w-0 px-1">
                <div className="text-[11px] text-osint-text truncate font-mono" title={auth.accountEmail || ''}>{auth.accountEmail || 'signed in'}</div>
                <div className="text-[10px] text-osint-muted truncate" title={auth.tenantName || ''}>{auth.tenantName || ''}{auth.role ? ` · ${auth.role}` : ''}</div>
              </div>
            )}
            <div className="flex items-center gap-0.5">
              <button type="button" onClick={onToggleTheme} className="p-1.5 rounded text-osint-muted hover:text-accent" title={theme === 'dark' ? 'Switch to light theme' : 'Switch to dark theme'} aria-label="Toggle color theme">
                {theme === 'dark' ? <LuSun size={14} /> : <LuMoon size={14} />}
              </button>
              <button type="button" onClick={() => setConfirmOut(true)} className="p-1.5 rounded text-osint-muted hover:text-neon-red" title="Disconnect" aria-label="Disconnect">
                <LuLogOut size={14} />
              </button>
            </div>
          </div>
        </div>
      </aside>

      {/* Content */}
      <div className="flex-1 min-w-0 flex flex-col">
        {/* Phone top bar — the map draws its own chrome */}
        {!isMap && (
          <div className="md:hidden flex items-center justify-between h-11 px-3 border-b border-osint-border bg-osint-surface flex-shrink-0">
            <NavLink to="/" className="font-mono text-sm font-bold"><span>Japan</span><span className="text-accent">OSINT</span></NavLink>
            <div className="flex items-center gap-2">
              <span className={cx('w-2 h-2 rounded-full', status?.isConnected ? 'status-online' : 'status-offline')} />
              <button type="button" onClick={onToggleTheme} className="text-osint-muted" aria-label="Toggle color theme">
                {theme === 'dark' ? <LuSun size={14} /> : <LuMoon size={14} />}
              </button>
            </div>
          </div>
        )}
        <main className="flex-1 relative overflow-hidden pb-14 md:pb-0">
          {children}
        </main>
      </div>

      {/* Phone tab bar */}
      <nav className="md:hidden fixed bottom-0 inset-x-0 h-14 border-t border-osint-border bg-osint-surface/95 backdrop-blur flex items-stretch z-[90]">
        {TAB_BAR.map((t) => (
          <NavLink
            key={t.to}
            to={t.to}
            end={t.end}
            className={({ isActive }) => cx('flex-1 flex flex-col items-center justify-center gap-0.5 text-[10px] relative',
              isActive || (t.to === '/console' && location.pathname.startsWith('/console')) ? 'text-accent' : 'text-osint-muted')}
          >
            <t.icon size={18} />
            {t.label}
            {t.badge === 'unread' && unread > 0 && (
              <span className="absolute top-1.5 right-1/4 font-mono text-[9px] rounded-full bg-accent text-black px-1 min-w-[16px] text-center">{unread > 99 ? '99+' : unread}</span>
            )}
          </NavLink>
        ))}
      </nav>

      <ConfirmDialog
        open={confirmOut}
        onClose={() => setConfirmOut(false)}
        onConfirm={() => { setConfirmOut(false); auth.signOut(); navigate('/'); }}
        title="Disconnect?"
        confirmLabel="Disconnect"
        message="Signs you out and returns to the sign-in screen. Saved items and cached layers in this browser are kept."
      />
      <Toaster />
    </div>
  );
}
