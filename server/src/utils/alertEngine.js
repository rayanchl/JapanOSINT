/**
 * Alert engine. Called fire-and-forget by intelStore after every NEW
 * intel_items row commits. Evaluates every enabled rule for the tuple's
 * tenant, dedupes / storm-guards, then dispatches to email and generic
 * webhook channels.
 *
 * Predicate shape (predicate_json):
 *   {
 *     q?: string,           // FTS match against title + body + summary
 *     source_ids?: string[],
 *     tags_any?: string[],  // matches if item has any of these
 *     tags_all?: string[],  // matches if item has all of these
 *     bbox?: [west, south, east, north],
 *     record_types?: string[],
 *   }
 *
 * All fields are optional and combine with AND. A rule with `{}` matches
 * every row (use with care — exists for "give me everything for this
 * tenant" workflows).
 *
 * Channel shape (entries inside channels_json):
 *   {
 *     type: 'email' | 'webhook',
 *     target: string,          // email address | http endpoint
 *     secret?: string,         // required for type='webhook'
 *   }
 */

import { randomUUID } from 'crypto';
import db from './database.js';
import { sendEmail } from './channels/email.js';
import { sendWebhook } from './channels/webhook.js';

const stmtRulesForTenant = db.prepare(`
  SELECT id, tenant_id, name, predicate_json, channels_json,
         dedup_window_sec, storm_cap_per_hour, muted_until
    FROM alert_rules
   WHERE tenant_id = ?
     AND enabled = 1
     AND (muted_until IS NULL OR muted_until < datetime('now'))
`);

const stmtRecentDuplicate = db.prepare(`
  SELECT 1 FROM alert_events
   WHERE rule_id = ?
     AND item_uid = ?
     AND matched_at > datetime('now', ?)
   LIMIT 1
`);

const stmtFiresInLastHour = db.prepare(`
  SELECT COUNT(*) AS n FROM alert_events
   WHERE rule_id = ?
     AND suppressed = 0
     AND matched_at > datetime('now', '-1 hour')
`);

const stmtInsertEvent = db.prepare(`
  INSERT INTO alert_events
    (id, tenant_id, rule_id, item_uid, delivered_channels_json, suppressed, reason)
  VALUES
    (?, ?, ?, ?, ?, ?, ?)
`);

const stmtMuteRule = db.prepare(`
  UPDATE alert_rules SET muted_until = datetime('now', '+1 hour')
   WHERE id = ?
`);

/**
 * Called by intelStore after a successful 'new' upsert.
 *
 * Fire-and-forget: errors are caught and logged but never propagated to
 * the caller. The intel pipeline must not be slowed or destabilised by
 * the alert path.
 */
export function evaluateForNewItem(item) {
  if (!item || !item.uid) return;
  // Don't block intelStore — kick off async without await.
  Promise.resolve()
    .then(() => evaluate(item))
    .catch((err) => console.error('[alerts] eval failed:', err.message));
}

async function evaluate(item) {
  const tenantId = item.tenant_id || 'legacy';
  const rules = stmtRulesForTenant.all(tenantId);
  if (rules.length === 0) return;

  for (const rule of rules) {
    let predicate;
    try {
      predicate = JSON.parse(rule.predicate_json || '{}');
    } catch {
      continue;
    }
    if (!matches(item, predicate)) continue;

    // Dedup window: skip if this (rule, item) fired recently.
    const dedupWin = `-${Math.max(1, rule.dedup_window_sec || 3600)} seconds`;
    if (stmtRecentDuplicate.get(rule.id, item.uid, dedupWin)) {
      stmtInsertEvent.run(
        randomUUID(), tenantId, rule.id, item.uid,
        '[]', 1, 'duplicate',
      );
      continue;
    }

    // Storm guard: rules firing > cap/hour auto-mute for 1 h.
    const cap = rule.storm_cap_per_hour || 100;
    const recent = stmtFiresInLastHour.get(rule.id).n;
    if (recent >= cap) {
      stmtInsertEvent.run(
        randomUUID(), tenantId, rule.id, item.uid,
        '[]', 1, `storm_cap(${cap}/h)`,
      );
      stmtMuteRule.run(rule.id);
      console.warn(`[alerts] rule ${rule.id} muted for 1h (storm cap ${cap}/h hit)`);
      continue;
    }

    // Dispatch to every channel; collect successes + failures.
    let channels;
    try {
      channels = JSON.parse(rule.channels_json || '[]');
    } catch {
      continue;
    }
    const delivered = [];
    const failures = [];
    await Promise.all(channels.map(async (ch) => {
      try {
        await dispatch(ch, item, rule);
        delivered.push(ch.type);
      } catch (err) {
        failures.push(`${ch.type}: ${err.message}`);
      }
    }));

    stmtInsertEvent.run(
      randomUUID(), tenantId, rule.id, item.uid,
      JSON.stringify(delivered),
      0,
      failures.length ? failures.join(' | ').slice(0, 500) : null,
    );
  }
}

/**
 * Predicate matcher. Pure function: all checks run in JS, no DB
 * round-trips. FTS comes pre-matched at the row level by query: we
 * compute the substring match locally since rules with a `q` field check
 * the new row's text directly.
 */
function matches(item, p) {
  if (p.source_ids?.length && !p.source_ids.includes(item.source_id)) return false;
  if (p.record_types?.length && !p.record_types.includes(item.record_type)) return false;

  if (p.q) {
    const hay = [
      item.title || '', item.summary || '', item.body || '',
    ].join(' ').toLowerCase();
    const needle = String(p.q).toLowerCase();
    if (!hay.includes(needle)) return false;
  }

  if (p.tags_any?.length || p.tags_all?.length) {
    let tags = [];
    try {
      tags = JSON.parse(item.tags || '[]');
    } catch { tags = []; }
    const tagSet = new Set(tags.map((t) => String(t).toLowerCase()));
    if (p.tags_any?.length) {
      const hit = p.tags_any.some((t) => tagSet.has(String(t).toLowerCase()));
      if (!hit) return false;
    }
    if (p.tags_all?.length) {
      const hit = p.tags_all.every((t) => tagSet.has(String(t).toLowerCase()));
      if (!hit) return false;
    }
  }

  if (p.bbox?.length === 4) {
    const [w, s, e, n] = p.bbox.map(Number);
    if (item.lat == null || item.lon == null) return false;
    if (item.lat < s || item.lat > n) return false;
    if (item.lon < w || item.lon > e) return false;
  }

  return true;
}

async function dispatch(ch, item, rule) {
  const link = item.link || null;
  const payload = {
    rule: { id: rule.id, name: rule.name },
    item: {
      uid: item.uid,
      source_id: item.source_id,
      title: item.title,
      summary: item.summary,
      link,
      lat: item.lat,
      lon: item.lon,
      tags: safeParseTags(item.tags),
      fetched_at: item.fetched_at,
      published_at: item.published_at,
    },
    matched_at: new Date().toISOString(),
  };

  switch (ch.type) {
    case 'email': {
      await sendEmail({
        target: ch.target,
        subject: `[${rule.name}] ${item.title || item.uid}`.slice(0, 200),
        text: textBody(rule, item),
        html: htmlBody(rule, item),
      });
      return;
    }
    case 'webhook': {
      await sendWebhook({
        target: ch.target,
        secret: ch.secret,
        event: payload,
      });
      return;
    }
    default:
      throw new Error(`unknown channel type: ${ch.type}`);
  }
}

function safeParseTags(t) {
  if (!t) return [];
  if (Array.isArray(t)) return t;
  try { return JSON.parse(t); } catch { return []; }
}

function textBody(rule, item) {
  return [
    `Alert: ${rule.name}`,
    item.title ? `Title: ${item.title}` : null,
    `Source: ${item.source_id}`,
    item.summary ? `\n${item.summary}` : null,
    item.link ? `\nLink: ${item.link}` : null,
  ].filter(Boolean).join('\n');
}

function htmlBody(rule, item) {
  const esc = (s) => String(s ?? '').replace(/[&<>]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' }[c]));
  return `
    <h2 style="margin:0 0 8px;font:600 16px/1.3 -apple-system,sans-serif">${esc(rule.name)}</h2>
    <p style="margin:0 0 8px;font:14px/1.4 -apple-system,sans-serif"><strong>${esc(item.title || item.uid)}</strong></p>
    <p style="margin:0 0 8px;color:#555;font:13px/1.4 -apple-system,sans-serif">Source: <code>${esc(item.source_id)}</code></p>
    ${item.summary ? `<p style="margin:0 0 12px;font:13px/1.5 -apple-system,sans-serif">${esc(item.summary)}</p>` : ''}
    ${item.link ? `<p><a href="${esc(item.link)}" style="color:#FFB347;font:600 13px -apple-system,sans-serif">Open source →</a></p>` : ''}
  `.trim();
}
