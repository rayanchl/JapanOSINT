import React, { useState } from 'react';
import { Link } from 'react-router-dom';
import { useAuth } from '../../auth/AuthContext.jsx';
import { visibleConsoleNav, groupBy } from './navConfig.js';
import { useUnreadCount } from './AppShell.jsx';
import { Page, Section, Row, Button, ConfirmDialog, cx } from '../ui/kit.jsx';

/**
 * The Console hub — the phone's fourth tab on iOS (`ConsoleHub`). On wide
 * screens the sidebar already lists every destination; this page still
 * serves as the landing for `/console` and carries the account section.
 */
export default function ConsoleHubPage() {
  const auth = useAuth();
  const unread = useUnreadCount();
  const [confirmOut, setConfirmOut] = useState(false);
  const groups = groupBy(visibleConsoleNav(auth));

  return (
    <Page title="Console" subtitle="Operations, discovery and configuration surfaces for this workspace.">
      {groups.map((g) => (
        <Section key={g.group} label={g.group} padded={false}>
          <ul className="divide-y divide-osint-border">
            {g.rows.map((r) => (
              <li key={r.to}>
                <Link to={r.to} className="flex items-center gap-3 px-3 py-2.5 hover:bg-white/5">
                  <span className="flex items-center justify-center w-7 h-7 rounded-md bg-accent/10 text-accent flex-shrink-0">
                    <r.icon size={15} />
                  </span>
                  <span className="min-w-0 flex-1">
                    <span className="block text-sm font-medium text-osint-text">{r.label}</span>
                    <span className="block text-[11px] text-osint-muted">{r.subtitle}</span>
                  </span>
                  {r.badge === 'unread' && unread > 0 && (
                    <span className="font-mono text-[10px] rounded-full bg-accent text-black px-1.5">{unread}</span>
                  )}
                  <span className="text-osint-muted">›</span>
                </Link>
              </li>
            ))}
          </ul>
        </Section>
      ))}

      <Section label="Account">
        {auth.accountEmail && <Row label="Signed in as">{auth.accountEmail}</Row>}
        {auth.tenantName && <Row label="Workspace">{auth.tenantName}{auth.role ? ` · ${auth.role}` : ''}</Row>}
        <Row label={<span className="text-neon-red">Disconnect</span>} hint="Switch backend, workspace or account.">
          <Button variant="danger" size="sm" onClick={() => setConfirmOut(true)}>Disconnect</Button>
        </Row>
      </Section>

      <ConfirmDialog
        open={confirmOut}
        onClose={() => setConfirmOut(false)}
        onConfirm={() => { setConfirmOut(false); auth.signOut(); }}
        title="Disconnect?"
        confirmLabel="Disconnect"
        message="Signs you out and returns to the sign-in screen. Saved items and cached layers in this browser are kept."
      />
      <div className={cx('h-4')} />
    </Page>
  );
}
