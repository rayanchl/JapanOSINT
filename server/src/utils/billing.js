/**
 * Stripe billing — the signup → pay → access spine.
 *
 * Plan drives the rate-limit slice (middleware/rateLimit.js reads
 * `tenant.plan`). This module is the single place that:
 *   1. Lazily constructs the Stripe client from STRIPE_SECRET_KEY (the whole
 *      billing surface no-ops cleanly when the key is unset, so dev/self-host
 *      without Stripe still boots).
 *   2. Maps our four plans ⇄ Stripe price ids (env-configured).
 *   3. Reconciles a Stripe subscription onto a tenant row (the webhook and
 *      checkout-completed paths both funnel through `applySubscription`).
 *
 * DB writes use the raw `db` handle, not `tenantDb`: this is tenancy infra
 * (same documented exception class as middleware/tenant.js) — the webhook has
 * no request/tenant context and we look tenants up by Stripe customer id.
 */

import Stripe from 'stripe';
import db from './database.js';

let _stripe = null;
let _checked = false;

/** Lazily build the Stripe client. Returns null when unconfigured. */
export function getStripe() {
  if (_checked) return _stripe;
  _checked = true;
  const key = process.env.STRIPE_SECRET_KEY;
  _stripe = key ? new Stripe(key) : null;
  return _stripe;
}

export function isBillingConfigured() {
  return Boolean(process.env.STRIPE_SECRET_KEY);
}

/** Plan → configured Stripe price id. 'free' has no price (no charge). */
export function priceForPlan(plan) {
  return {
    pro: process.env.STRIPE_PRICE_PRO,
    team: process.env.STRIPE_PRICE_TEAM,
    enterprise: process.env.STRIPE_PRICE_ENTERPRISE,
  }[plan] || null;
}

/** Stripe price id → our plan name, or null if it isn't one we sell. */
export function planForPrice(priceId) {
  if (!priceId) return null;
  if (priceId === process.env.STRIPE_PRICE_PRO) return 'pro';
  if (priceId === process.env.STRIPE_PRICE_TEAM) return 'team';
  if (priceId === process.env.STRIPE_PRICE_ENTERPRISE) return 'enterprise';
  return null;
}

/** Plans offerable right now: free + every paid tier with a configured price. */
export function availablePlans() {
  const out = [{ id: 'free', price_id: null }];
  for (const p of ['pro', 'team', 'enterprise']) {
    const price_id = priceForPlan(p);
    if (price_id) out.push({ id: p, price_id });
  }
  return out;
}

/** Look up a tenant id by its Stripe customer id (webhook fallback path). */
export function findTenantByStripeCustomerId(customerId) {
  if (!customerId) return null;
  const row = db
    .prepare(`SELECT id FROM tenants WHERE stripe_customer_id = ?`)
    .get(customerId);
  return row?.id || null;
}

/**
 * Ensure the tenant has a Stripe customer; create one on first checkout and
 * persist the id. `tenant` is req.tenant (carries id, name, stripe_customer_id).
 */
export async function ensureStripeCustomer(tenant, email) {
  if (tenant.stripe_customer_id) return tenant.stripe_customer_id;
  const stripe = getStripe();
  if (!stripe) throw new Error('billing_not_configured');
  const customer = await stripe.customers.create({
    email: email || undefined,
    name: tenant.name,
    metadata: { tenant_id: tenant.id },
  });
  db.prepare(`UPDATE tenants SET stripe_customer_id = ? WHERE id = ?`).run(
    customer.id,
    tenant.id,
  );
  tenant.stripe_customer_id = customer.id;
  return customer.id;
}

// Stripe subscription.status values that mean "this tenant is paid up".
// past_due is included as a short grace window — Stripe retries the invoice
// and either recovers (→ active) or cancels (→ we downgrade), and we'd rather
// not hard-lock a paying customer over one failed charge attempt.
const ENTITLED = new Set(['active', 'trialing', 'past_due']);

/**
 * Reconcile a Stripe Subscription object onto the tenant row. Idempotent —
 * safe to call from both checkout.session.completed and every
 * customer.subscription.* webhook (Stripe delivers these out of order and
 * more than once).
 */
export function applySubscription(tenantId, sub) {
  if (!tenantId || !sub) return;
  const priceId = sub.items?.data?.[0]?.price?.id || null;
  const paidPlan = planForPrice(priceId) || 'free';
  const status = sub.status || 'none';
  // Entitled → the plan they bought; anything else (canceled, unpaid,
  // incomplete, incomplete_expired, paused) → drop to free. rateLimit picks
  // the new slice up on the very next request.
  const effectivePlan = ENTITLED.has(status) ? paidPlan : 'free';
  const periodEnd = Number.isFinite(sub.current_period_end)
    ? new Date(sub.current_period_end * 1000).toISOString()
    : null;

  db.prepare(
    `UPDATE tenants
        SET plan = ?,
            subscription_status = ?,
            stripe_subscription_id = ?,
            plan_period_end = ?
      WHERE id = ?`,
  ).run(effectivePlan, status, sub.id || null, periodEnd, tenantId);

  console.log(
    `[billing] tenant ${tenantId} → plan=${effectivePlan} status=${status}`,
  );
}

/**
 * Access gate. Free tier is always allowed (freemium). A tenant on a paid
 * plan whose subscription has lapsed gets a 402 — but in normal operation the
 * webhook downgrades them to 'free' on cancellation, so this is a safety net
 * for the window before that webhook lands. /api/billing and /api/me stay
 * reachable so a lapsed tenant can re-subscribe or inspect its own state.
 */
export function requireActiveSubscription(req, res, next) {
  if (!req.tenant) return next();
  const url = req.originalUrl || '';
  if (url.startsWith('/api/billing') || url.startsWith('/api/me')) return next();

  const plan = req.tenant.plan;
  if (plan === 'free') return next();

  const status = req.tenant.subscription_status || 'none';
  if (ENTITLED.has(status)) return next();

  return res.status(402).json({
    error: 'subscription_inactive',
    detail: `Workspace plan "${plan}" has no active subscription (status: ${status}).`,
    plan,
    subscription_status: status,
  });
}
