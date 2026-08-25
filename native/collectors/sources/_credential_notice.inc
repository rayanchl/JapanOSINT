/* collectors/sources/_credential_notice.inc — the ONE way a collector says
 * "I am configured correctly, I have no credential, so I fetched nothing."
 *
 * WHY THIS FILE EXISTS. Twelve credential-gated collectors each degraded to a
 * bare `fprintf(stderr, "… gated …"); return 0;`. To every consumer of the
 * data that is indistinguishable from a source that ran, spent a request and
 * found nothing:
 *
 *     fetch_log      status='ok', records=0
 *     /api/status    last run OK
 *     anomaly triage nothing to see
 *
 * — which is the same invisible nothing as an EMPTY_RESULTSET source, and it
 * is the failure mode CLAUDE.md rule 1 names explicitly: "A failure degrades
 * to an explicit error / not_found / 'needs credential' note, never to
 * invented content." A log line nobody reads is not a disclosure (the same
 * argument docs/SOURCE_EXHAUSTIVENESS.md makes about truncation).
 *
 * THE TENSION WITH R6, AND HOW IT IS RESOLVED. SOURCE_AUTHORING_CONTRACT.md's
 * R6 said "do not emit a row saying a key is needed", and it was right about
 * the thing it was guarding: a gated source must never emit a FINDING — a row
 * that a map pin, a feed or a synthesis prompt would read as an observation
 * about the world. That is fabricated data.
 *
 * A status notice is not a finding, and the tree already had the shape for it:
 * `dam_water_level.c` reports its own not-implemented state as one
 * `record_type = "collector-status-notice"` row rather than spending a request
 * to throw the answer away. This helper is that shape, generalised. Consumers
 * filter on record_type, so the notice never reaches a findings surface, and:
 *
 *   - `remote_key` is the CONSTANT "collector-needs-credential", so the sink
 *     upserts it. A source gated for a year holds exactly ONE row, not one per
 *     scheduler tick. (This matters: several of these run every 30 minutes.)
 *   - the row carries no observation of any kind — no coordinates, no counts,
 *     no upstream content. Only which env var is missing and where to get it.
 *   - the collector still returns 0. Returning -1 makes scheduler_run_source
 *     write fetch_log status='error' and open a collector_anomaly on every
 *     tick, so an unconfigured source would look permanently broken and would
 *     bury real breakages (bear_encounters.c documented that incident).
 *
 * Included, never compiled standalone; `-iquote collectors/sources` is in
 * CFLAGS so any collector at any depth can `#include "_credential_notice.inc"`.
 * The includer must already have source.h and third_party/cJSON.h. */
#ifndef JO_CREDENTIAL_NOTICE_INC
#define JO_CREDENTIAL_NOTICE_INC

#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Emit the notice. Returns 0 always — "gated" is a state, not a run failure —
 * so the whole degradation is one line at a call site:
 *
 *     if (!key) return jo_needs_credential(sink, "fofa-jp", "FOFA",
 *                                          (const char *[]){"FOFA_API_KEY",
 *                                                           "FOFA_EMAIL", NULL},
 *                                          FOFA_BASE,
 *                                          "free account at fofa.info");
 *
 *   sink      the run's sink (NULL is tolerated — the notice is then skipped).
 *   source_id the registered `.id`, used in the log line and stored as
 *             sub_source_id so the row is attributable without a join.
 *   display   human name of the upstream ("FOFA", "e-Stat 労働力調査").
 *   env_names NULL-terminated array of the env var names that would unblock
 *             it, in the order the collector tries them. At least one.
 *   endpoint  the URL that would have been called, or NULL.
 *   how       one clause on how an operator obtains the credential, or NULL.
 */
static inline int jo_needs_credential(intel_sink *sink,
                                      const char *source_id,
                                      const char *display,
                                      const char *const *env_names,
                                      const char *endpoint,
                                      const char *how) {
  if (!source_id) source_id = "collector";
  if (!display)   display   = source_id;

  /* One stderr line, as before — the log is still the operator's fastest
   * signal; the record is what makes it survivable and queryable. */
  fprintf(stderr, "[%s] gated (%s)\n", source_id,
          (env_names && env_names[0]) ? env_names[0] : "credential unset");

  if (!sink || !sink->emit) return 0;

  cJSON *p = cJSON_CreateObject();
  if (!p) return 0;
  cJSON_AddStringToObject(p, "status", "needs_credential");
  cJSON_AddStringToObject(p, "source_id", source_id);
  cJSON_AddStringToObject(p, "upstream", display);

  cJSON *envs = cJSON_CreateArray();
  if (envs) {
    for (const char *const *e = env_names; e && *e; e++)
      cJSON_AddItemToArray(envs, cJSON_CreateString(*e));
    cJSON_AddItemToObject(p, "env_vars_required", envs);
  }
  if (endpoint) cJSON_AddStringToObject(p, "endpoint", endpoint);
  if (how)      cJSON_AddStringToObject(p, "how_to_obtain", how);

  /* Say plainly that this is an accounting row about the collector, not an
   * observation, and that the shortfall is total: no request was spent. */
  cJSON_AddNumberToObject(p, "records_used", 0);
  cJSON_AddStringToObject(p, "records_available",
                          "unknown — no request was made without a credential");
  cJSON_AddBoolToObject(p, "request_spent", 0);

  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char title[320], summary[512], envlist[256];
  envlist[0] = 0;
  for (const char *const *e = env_names; e && *e; e++) {
    size_t used = strlen(envlist);
    snprintf(envlist + used, sizeof envlist - used, "%s%s",
             used ? ", " : "", *e);
  }
  snprintf(title, sizeof title, "%s — credential required, nothing collected",
           display);
  snprintf(summary, sizeof summary,
           "%s is registered and reachable but has no credential configured, "
           "so no request was made and no record exists for this run. Set %s%s%s.",
           display, envlist[0] ? envlist : "the collector's API key",
           how ? " — " : "", how ? how : "");

  intel_item note = {0};
  note.remote_key      = "collector-needs-credential";
  note.title           = title;
  note.summary         = summary;
  note.link            = endpoint;
  note.lang            = "en";
  note.record_type     = "collector-status-notice";
  note.sub_source_id   = source_id;
  note.properties_json = pj ? pj : "{}";
  note.tags_json       = "[\"collector-status\",\"needs-credential\"]";
  sink->emit(sink, &note);
  free(pj);
  return 0;
}

#endif /* JO_CREDENTIAL_NOTICE_INC */
