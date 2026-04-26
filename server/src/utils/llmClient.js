// server/src/utils/llmClient.js
const DEFAULT_BASE = 'http://localhost:1234';
const DEFAULT_MODEL = 'local-model';
const DEFAULT_TIMEOUT_MS = 30_000;

/**
 * OpenAI-compatible chat completion against LM Studio (or any compatible
 * server). Returns the parsed JSON object the model produced, or null on
 * any failure. Never throws past the caller.
 *
 * @param {object} args
 * @param {Array}  args.messages     OpenAI-style messages array. Vision callers
 *                                   may pass an array `content` with `text` and
 *                                   `image_url` parts; the client passes it through.
 * @param {object} args.jsonSchema   JSON Schema enforced via response_format.
 * @param {number} [args.timeoutMs]  Request timeout, default 30s.
 * @param {string} [args.baseUrl]    Override LM Studio URL (env or default otherwise).
 * @param {string} [args.model]      Override model id (env or default otherwise).
 */
export async function chat({ messages, jsonSchema, timeoutMs, baseUrl, model }) {
  const resolvedBase = (baseUrl || process.env.LLM_BASE_URL || DEFAULT_BASE).replace(/\/$/, '');
  const url = `${resolvedBase}/v1/chat/completions`;
  const body = {
    model: model || process.env.LLM_MODEL || DEFAULT_MODEL,
    messages,
    temperature: 0.1,
    response_format: {
      type: 'json_schema',
      json_schema: { name: 'response', strict: true, schema: jsonSchema },
    },
  };

  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs ?? DEFAULT_TIMEOUT_MS);
  try {
    const res = await fetch(url, {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify(body),
      signal: controller.signal,
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    const envelope = await res.json().catch(() => null);
    const content = envelope?.choices?.[0]?.message?.content;
    if (typeof content !== 'string' || content.length === 0) return null;
    try {
      return JSON.parse(content);
    } catch {
      return null;
    }
  } catch {
    clearTimeout(timer);
    return null;
  }
}
