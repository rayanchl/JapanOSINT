/**
 * Resend email channel. Free tier: 3000 emails / month, 100/day, no
 * credit card. API: https://resend.com/docs/api-reference/emails/send-email
 *
 * Configuration:
 *   RESEND_API_KEY             - server-side platform key
 *   ALERT_EMAIL_FROM           - "JapanOSINT Alerts <alerts@yourdomain.com>"
 *
 * Channel config per rule:
 *   { type: 'email', target: 'recipient@example.com' }
 */

const RESEND_ENDPOINT = 'https://api.resend.com/emails';

export async function sendEmail({ target, subject, html, text }) {
  const apiKey = process.env.RESEND_API_KEY;
  const from = process.env.ALERT_EMAIL_FROM || 'JapanOSINT <onboarding@resend.dev>';
  if (!apiKey) {
    throw new Error('RESEND_API_KEY not set');
  }
  if (!target || !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(target)) {
    throw new Error(`invalid email target: ${target}`);
  }

  const res = await fetch(RESEND_ENDPOINT, {
    method: 'POST',
    headers: {
      Authorization: `Bearer ${apiKey}`,
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({
      from,
      to: [target],
      subject,
      html,
      text,
    }),
  });
  if (!res.ok) {
    const body = await res.text();
    throw new Error(`resend ${res.status}: ${body.slice(0, 200)}`);
  }
  return res.json();
}
