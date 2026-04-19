const SECRET_QUERY_KEYS = new Set([
  'api_key',
  'apikey',
  'key',
  'token',
  'access_token',
  'subscription-key',
  'subscription_key',
  'auth',
  'password',
]);

const SECRET_HEADERS = new Set([
  'authorization',
  'x-api-key',
  'x-auth-token',
  'subscription-key',
  'ocp-apim-subscription-key',
  'cookie',
  'set-cookie',
]);

export function redactUrl(url) {
  if (!url) return url;
  try {
    const u = new URL(url);
    for (const k of [...u.searchParams.keys()]) {
      if (SECRET_QUERY_KEYS.has(k.toLowerCase())) {
        u.searchParams.set(k, '[REDACTED]');
      }
    }
    return u.toString();
  } catch {
    return url;
  }
}

export function redactHeaders(headers) {
  const out = {};
  if (!headers) return out;
  const entries =
    typeof headers.entries === 'function' ? [...headers.entries()] : Object.entries(headers);
  for (const [k, v] of entries) {
    out[k] = SECRET_HEADERS.has(k.toLowerCase()) ? '[REDACTED]' : v;
  }
  return out;
}

export function truncateBody(text, maxLines = 200) {
  if (text == null) return null;
  const str = typeof text === 'string' ? text : String(text);
  const lines = str.split('\n');
  if (lines.length <= maxLines) return str;
  return lines.slice(0, maxLines).join('\n') + `\n…[truncated ${lines.length - maxLines} more lines]`;
}
