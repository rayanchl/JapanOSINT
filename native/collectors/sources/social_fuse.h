/* social_fuse.h — entry points for the fused social/identity services.
 *
 * The username-pivot collectors (Sherlock, Maigret, social_intel, the
 * per-platform deep lookups) and the email-pivot collectors (EmailRep,
 * email validator, HIBP breach) used to register one service ID each. They
 * are now fused into two input-typed services:
 *
 *   SOCIAL_USERNAME (social_username.c) — username pivot
 *   SOCIAL_EMAIL    (social_search.c)   — email pivot (+ username parity)
 *
 * Each former collector keeps its probing/detection logic but no longer
 * REGISTER_SOURCEs; its run is exposed here so the two hosts can aggregate
 * them. Every jo_*_run reads ctx->entity and emits per-record rows. */
#ifndef JO_SOCIAL_FUSE_H
#define JO_SOCIAL_FUSE_H
#include "../../source.h"

/* username-pivot providers */
int jo_social_intel_run(const source_ctx *ctx, intel_sink *sink);
int jo_social_platforms_run(const source_ctx *ctx, intel_sink *sink);
int jo_sherlock_run(const source_ctx *ctx, intel_sink *sink);
int jo_maigret_run(const source_ctx *ctx, intel_sink *sink);

/* email-pivot providers */
int jo_email_reputation_run(const source_ctx *ctx, intel_sink *sink);
int jo_email_validator_run(const source_ctx *ctx, intel_sink *sink);
int jo_breach_checker_run(const source_ctx *ctx, intel_sink *sink);

#endif /* JO_SOCIAL_FUSE_H */
