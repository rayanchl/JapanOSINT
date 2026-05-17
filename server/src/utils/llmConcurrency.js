// Phase 4 — shared LLM concurrency caps.
//
// Inference on the 128GB DDR5 CPU box is memory-bandwidth bound: two big
// models decoding at once don't go 2× — they both crawl. The triage worker
// (12B) and the repair worker (12B fast / 27B hard) run independently, so
// without a *shared* limiter a 27B repair and a 12B triage can stack and
// thrash the box.
//
// Tiers, each a counting semaphore:
//   heavy (27B)  default 1   — only one big job decoding at a time
//   mid   (12B)  default 2   — a couple of mid jobs is fine
// The 4B sanity/detection path stays inline (its own small limiter in
// collectorDetection.js) — it's cheap enough not to gate here.
//
// FIFO waiters so a steady stream of mid jobs can't starve a heavy one.

const LIMITS = {
  heavy: Math.max(1, Number(process.env.LLM_HEAVY_CONCURRENCY || 1)),
  mid: Math.max(1, Number(process.env.LLM_MID_CONCURRENCY || 2)),
};

const state = {
  heavy: { inflight: 0, queue: [] },
  mid: { inflight: 0, queue: [] },
};

function acquire(tier) {
  const s = state[tier];
  if (s.inflight < LIMITS[tier]) {
    s.inflight += 1;
    return Promise.resolve();
  }
  return new Promise((resolve) => s.queue.push(resolve));
}

function release(tier) {
  const s = state[tier];
  const next = s.queue.shift();
  if (next) {
    next(); // hand the slot straight to the next waiter (inflight unchanged)
  } else {
    s.inflight = Math.max(0, s.inflight - 1);
  }
}

/**
 * Run `fn` while holding one slot of `tier` ('heavy' | 'mid'). Unknown tiers
 * run unbounded (fail open — never block a caller because of a typo). The
 * slot is always released, even if `fn` throws.
 */
export async function withLlmSlot(tier, fn) {
  if (tier !== 'heavy' && tier !== 'mid') return fn();
  await acquire(tier);
  try {
    return await fn();
  } finally {
    release(tier);
  }
}

/** Snapshot for the maintenance digest / health endpoint. */
export function llmConcurrencySnapshot() {
  return {
    heavy: { limit: LIMITS.heavy, inflight: state.heavy.inflight, waiting: state.heavy.queue.length },
    mid: { limit: LIMITS.mid, inflight: state.mid.inflight, waiting: state.mid.queue.length },
  };
}
