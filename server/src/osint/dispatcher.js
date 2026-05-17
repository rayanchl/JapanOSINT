// server/src/osint/dispatcher.js
//
// JS port of osint_dispatcher.c. Maps canonical SERVICE_NAME -> handler.
// Handlers return { success, data(JSON string), confidence, error? }.
//
// Only registered names are advertised to the LLM via getServicesList(); the
// analysis grammar enumerates a fixed superset, but the prompt's "Available
// services" line is the implemented subset (faithful to how the C dispatcher
// fed osint_dispatcher_get_services_list() into the prompt). Unregistered
// names dispatch to a graceful not_implemented result so the pipeline and the
// tuned prompt/grammar behaviour stay intact while services port over time.

import { jpCorpusLookup } from './services/jpCorpus.js';
import { ipGeolocation, asnLookup, reverseDns, dnsRecords } from './services/net.js';

// name -> { fn, dedupKey }. dedupKey groups services that share one C handler
// (osint_pipeline.c deduped by handler pointer; we mirror that so e.g. two
// IP services backed by the same call run once).
const REGISTRY = new Map();
function register(names, fn, dedupKey) {
  for (const n of names) REGISTRY.set(n, { fn, dedupKey: dedupKey || names[0] });
}

// Always-on Japan adapter — the search's reach into the JapanOSINT corpus.
register(['JP_CORPUS_LOOKUP'], (entity) => jpCorpusLookup(entity), 'JP_CORPUS_LOOKUP');

// Network OSINT (no API key).
register(['IP_GEOLOCATION'], (e) => ipGeolocation(e), 'IP_GEOLOCATION');
register(['ASN_LOOKUP', 'BGP_LOOKUP'], (e) => asnLookup(e), 'ASN_LOOKUP');
register(['REVERSE_DNS'], (e) => reverseDns(e), 'REVERSE_DNS');
register(['DNS_RECORDS', 'DOMAIN_DNS_INTEL', 'DNS_INTEL'], (e) => dnsRecords(e), 'DNS_RECORDS');

/** Canonical name if known to the grammar/registry, else null. */
export function serviceFromName(name) {
  if (!name) return null;
  const n = String(name).trim().toUpperCase();
  return n.length ? n : null;
}

/** True if a real Node implementation exists (vs not_implemented fallback). */
export function isImplemented(name) {
  return REGISTRY.has(serviceFromName(name));
}

/** Comma-separated implemented service names — fed into the prompts verbatim
 *  like osint_dispatcher_get_services_list(). */
export function getServicesList() {
  return [...REGISTRY.keys()].join(', ');
}

/** Handler dedup key (so the pipeline can collapse same-handler services). */
export function handlerKey(name) {
  return REGISTRY.get(serviceFromName(name))?.dedupKey ?? serviceFromName(name);
}

/**
 * Execute one service against one entity. Never throws; unimplemented or
 * failing services return a structured result so the pipeline continues.
 */
export async function dispatchService(name, entity /* , entityType */) {
  const canon = serviceFromName(name);
  const reg = REGISTRY.get(canon);
  if (!reg) {
    return {
      service: canon, success: false, confidence: 0,
      data: null, error: 'not_implemented',
    };
  }
  try {
    const r = await reg.fn(entity);
    return {
      service: canon,
      success: !!r?.success,
      confidence: Number.isFinite(r?.confidence) ? r.confidence : (r?.success ? 70 : 0),
      data: r?.data ?? null,
      error: r?.error ?? null,
    };
  } catch (err) {
    return { service: canon, success: false, confidence: 0, data: null, error: err?.message || String(err) };
  }
}
