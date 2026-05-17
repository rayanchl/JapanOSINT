// server/src/utils/llmClient.js
//
// Single LLM runtime for the whole codebase: a local llama.cpp `llama-server`
// hosting the tuned model (gpt-oss-20b). Two entrypoints:
//
//   chat({ messages, jsonSchema })  - OpenAI-compatible path (/v1/chat/completions).
//                                     Backwards-compatible with every existing
//                                     caller. llama.cpp enforces `jsonSchema`
//                                     by compiling it to a GBNF grammar
//                                     internally, so "GBNF everywhere" holds
//                                     at the engine level with zero call-site
//                                     churn. An explicit `grammar` overrides.
//
//   complete({ prompt, grammar })   - llama.cpp native /completion path. Sends a
//                                     verbatim inline GBNF grammar. Used by the
//                                     ported OSINTsaas pipeline so its tuned
//                                     prompts/grammars run unchanged.
//
// Both return null on any failure and never throw past the caller.

const DEFAULT_BASE = 'http://localhost:8080'; // llama.cpp llama-server
const DEFAULT_MODEL = 'local-model';
const DEFAULT_TIMEOUT_MS = 30_000;

function resolveBase(baseUrl) {
  return (baseUrl || process.env.LLM_BASE_URL || DEFAULT_BASE).replace(/\/$/, '');
}

/** Master switch shared with the enrichment workers. */
export function llmEnabled() {
  return process.env.LLM_ENABLED !== 'false';
}

/**
 * OpenAI-compatible chat completion against llama.cpp's /v1/chat/completions.
 * Returns the parsed JSON object the model produced, or null on any failure.
 *
 * @param {object} args
 * @param {Array}  args.messages       OpenAI-style messages array. Vision callers
 *                                     may pass an array `content`; passed through.
 * @param {object} [args.jsonSchema]   JSON Schema; enforced by llama.cpp via an
 *                                     internally-compiled GBNF grammar.
 * @param {string} [args.grammar]      Inline GBNF; overrides `jsonSchema` when set.
 * @param {number} [args.temperature]  Sampling temperature, default 0.1.
 * @param {number} [args.timeoutMs]    Request timeout, default 30s.
 * @param {string} [args.baseUrl]      Override llama-server URL.
 * @param {string} [args.model]        Override model id.
 */
export async function chat({ messages, jsonSchema, grammar, temperature, timeoutMs, baseUrl, model }) {
  const url = `${resolveBase(baseUrl)}/v1/chat/completions`;
  const body = {
    model: model || process.env.LLM_MODEL || DEFAULT_MODEL,
    messages,
    temperature: temperature ?? 0.1,
  };
  if (grammar) {
    body.grammar = grammar;
  } else if (jsonSchema) {
    body.response_format = {
      type: 'json_schema',
      json_schema: { name: 'response', strict: true, schema: jsonSchema },
    };
  }

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

/**
 * llama.cpp native /completion with an inline GBNF grammar. Returns the raw
 * generated string (caller parses), or null on failure. This is the path the
 * ported OSINTsaas pipeline uses so its verbatim tuned prompts + .gbnf files
 * run exactly as they did in the C backend.
 *
 * @param {object} args
 * @param {string} args.prompt        The fully-built prompt string.
 * @param {string} [args.grammar]     Inline GBNF grammar content.
 * @param {number} [args.maxTokens]   n_predict, default 2048.
 * @param {number} [args.temperature] Sampling temperature, default 0.2.
 * @param {string[]} [args.stop]      Stop strings.
 * @param {number} [args.timeoutMs]   Request timeout, default 30s.
 * @param {string} [args.baseUrl]     Override llama-server URL.
 */
export async function complete({ prompt, grammar, maxTokens, temperature, stop, timeoutMs, baseUrl }) {
  const url = `${resolveBase(baseUrl)}/completion`;
  const body = {
    prompt,
    n_predict: maxTokens ?? 2048,
    temperature: temperature ?? 0.2,
    cache_prompt: true,
  };
  if (grammar) body.grammar = grammar;
  if (Array.isArray(stop) && stop.length) body.stop = stop;

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
    const content = envelope?.content;
    return typeof content === 'string' && content.length > 0 ? content : null;
  } catch {
    clearTimeout(timer);
    return null;
  }
}

/** complete() + JSON.parse convenience for grammar-constrained JSON output. */
export async function completeJSON(args) {
  const raw = await complete(args);
  if (raw == null) return null;
  try {
    return JSON.parse(raw);
  } catch {
    return null;
  }
}

/** Health probe for the llama-server (used by ops/degradation checks). */
export async function llmHealthy(baseUrl) {
  try {
    const res = await fetch(`${resolveBase(baseUrl)}/health`, { method: 'GET' });
    return res.ok;
  } catch {
    return false;
  }
}
