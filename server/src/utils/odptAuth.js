/**
 * ODPT API key resolver. Checks the three env vars ODPT collectors have
 * accepted historically, in order of preference. Returns null when no key
 * is configured.
 *
 * Keys are free, self-serve:
 *   - https://developer.odpt.org/ (production)
 *   - https://api-challenge.odpt.org/ (Challenge 2024+)
 */
export function getOdptToken() {
  return process.env.ODPT_TOKEN
    || process.env.ODPT_CONSUMER_KEY
    || process.env.ODPT_CHALLENGE_TOKEN
    || null;
}
