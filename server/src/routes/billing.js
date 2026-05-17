/**
 * Billing routes — the customer-facing signup → pay → access flow.
 *
 *   GET  /api/billing/status   plan + subscription state + offerable plans
 *   POST /api/billing/checkout owner/admin → Stripe Checkout session URL
 *   POST /api/billing/portal   owner/admin → Stripe Billing Portal URL
 *
 * The Stripe webhook (`billingWebhookHandler`) is exported separately and
 * mounted in index.js BEFORE express.json() and OUTSIDE the /api auth chain:
 * Stripe can't present a Supabase JWT, so integrity comes from the signed
 * payload, which requires the untouched raw request body.
 */

import { Router } from 'express';
import {
  getStripe,
  isBillingConfigured,
  availablePlans,
  priceForPlan,
  ensureStripeCustomer,
  applySubscription,
  findTenantByStripeCustomerId,
} from '../utils/billing.js';
import { tenantUsage } from '../utils/usage.js';

const router = Router();
const ADMIN_ROLES = new Set(['owner', 'admin']);

function returnBase() {
  return process.env.BILLING_RETURN_URL || process.env.APP_BASE_URL || '';
}

router.get('/status', (req, res) => {
  res.json({
    billing_enabled: isBillingConfigured(),
    plan: req.tenant.plan,
    subscription_status: req.tenant.subscription_status || 'none',
    current_period_end: req.tenant.plan_period_end || null,
    plans: availablePlans(),
  });
});

// Per-tenant collector usage (BYOK vs platform-key runs) over a trailing
// window — backs a usage panel and informs plan/quota decisions.
router.get('/usage', (req, res) => {
  res.json(tenantUsage(req.tenant.id, req.query?.days));
});

router.post('/checkout', async (req, res) => {
  const stripe = getStripe();
  if (!stripe) return res.status(503).json({ error: 'billing_not_configured' });
  if (!ADMIN_ROLES.has(req.role)) {
    return res.status(403).json({ error: 'admin_role_required' });
  }

  const plan = String(req.body?.plan || '').toLowerCase();
  const price = priceForPlan(plan);
  if (!price) {
    return res.status(400).json({
      error: 'invalid_plan',
      detail: `No Stripe price configured for plan "${plan}"`,
    });
  }

  try {
    const customerId = await ensureStripeCustomer(req.tenant, req.user?.email);
    const base = returnBase();
    const session = await stripe.checkout.sessions.create({
      mode: 'subscription',
      customer: customerId,
      line_items: [{ price, quantity: 1 }],
      client_reference_id: req.tenant.id,
      subscription_data: { metadata: { tenant_id: req.tenant.id } },
      allow_promotion_codes: true,
      success_url: `${base}/billing?status=success&session_id={CHECKOUT_SESSION_ID}`,
      cancel_url: `${base}/billing?status=cancel`,
    });
    res.json({ url: session.url });
  } catch (err) {
    console.error('[billing] checkout failed:', err?.message);
    res.status(502).json({ error: 'checkout_failed' });
  }
});

router.post('/portal', async (req, res) => {
  const stripe = getStripe();
  if (!stripe) return res.status(503).json({ error: 'billing_not_configured' });
  if (!ADMIN_ROLES.has(req.role)) {
    return res.status(403).json({ error: 'admin_role_required' });
  }
  if (!req.tenant.stripe_customer_id) {
    return res.status(400).json({
      error: 'no_customer',
      detail: 'No subscription to manage yet — start a checkout first.',
    });
  }
  try {
    const session = await stripe.billingPortal.sessions.create({
      customer: req.tenant.stripe_customer_id,
      return_url: `${returnBase()}/billing`,
    });
    res.json({ url: session.url });
  } catch (err) {
    console.error('[billing] portal failed:', err?.message);
    res.status(502).json({ error: 'portal_failed' });
  }
});

export default router;

/**
 * Stripe webhook. Mounted in index.js as:
 *   app.post('/api/billing/webhook',
 *            express.raw({ type: 'application/json' }),
 *            billingWebhookHandler)
 * registered before express.json() so req.body is the raw Buffer Stripe's
 * signature is computed over.
 */
export async function billingWebhookHandler(req, res) {
  const stripe = getStripe();
  const secret = process.env.STRIPE_WEBHOOK_SECRET;
  if (!stripe || !secret) {
    return res.status(503).json({ error: 'billing_not_configured' });
  }

  let event;
  try {
    event = stripe.webhooks.constructEvent(
      req.body,
      req.headers['stripe-signature'],
      secret,
    );
  } catch (err) {
    console.warn('[billing] webhook signature check failed:', err?.message);
    return res.status(400).json({ error: 'invalid_signature' });
  }

  try {
    switch (event.type) {
      case 'checkout.session.completed': {
        const s = event.data.object;
        if (s.mode === 'subscription' && s.subscription) {
          const sub = await stripe.subscriptions.retrieve(s.subscription);
          const tenantId =
            s.client_reference_id ||
            sub.metadata?.tenant_id ||
            findTenantByStripeCustomerId(s.customer);
          if (tenantId) applySubscription(tenantId, sub);
        }
        break;
      }
      case 'customer.subscription.created':
      case 'customer.subscription.updated':
      case 'customer.subscription.deleted': {
        const sub = event.data.object;
        const tenantId =
          sub.metadata?.tenant_id ||
          findTenantByStripeCustomerId(sub.customer);
        if (tenantId) applySubscription(tenantId, sub);
        break;
      }
      default:
        break; // ignore unrelated events
    }
    res.json({ received: true });
  } catch (err) {
    // 200 so Stripe doesn't retry-storm an app-side bug (signature already
    // verified above). The failure is logged for follow-up.
    console.error('[billing] webhook handler error:', err?.stack || err?.message);
    res.json({ received: true, handler_error: true });
  }
}
