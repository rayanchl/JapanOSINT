/* core/llm_fault.h — was a NULL from llm_complete() the server or the item?
 *
 * The background pods (anomaly triage, collector repair, entity extraction)
 * call llm_complete(), which returns NULL for an unreachable server, a call
 * that ran out its timeout, a non-2xx, and an empty completion alike. They
 * all used to treat that NULL as a verdict on the ITEM: entity extraction
 * bumped the item's failed_count, and an item that reaches 5 is never
 * retried — so one llama-server restart permanently marked every item it
 * happened to be asked about as unextractable. Triage and repair left the row
 * at the head of their LIMIT and asked about it again next tick, forever.
 *
 * llm_chat_ex() reports the transport verdict, but the completion path these
 * pods use does not, and core/llm.c belongs to another stream of work. So the
 * split is made here with the same two signals llm.c itself uses:
 *   - the call consumed (nearly) its whole budget  -> it timed out
 *   - the server's /health does not answer 2xx     -> it is not there
 * Either one is a TRANSPORT failure: not the item's fault, and a reason to
 * stop the tick rather than spend the rest of the batch on a dead server.
 * Anything else (the server is up, answered quickly, and the answer was empty
 * or unparseable) is a MODEL failure and may be counted against the item. */
#ifndef JO_LLM_FAULT_H
#define JO_LLM_FAULT_H
#include "llm.h"
#include <time.h>

static inline long llm_fault_now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long)t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

/* 1 = transport (server down / timed out), 0 = the model answered badly.
 * Costs one GET /health, and only on a NULL. */
static inline int llm_fault_is_transport(llm_client *llm, long elapsed_ms,
                                         int timeout_ms) {
  if (timeout_ms > 0 && elapsed_ms >= (long)timeout_ms * 9 / 10) return 1;
  return !llm_healthy(llm);
}

#endif
