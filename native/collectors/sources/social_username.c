/* collectors/osint/sources/social_username.c — SOCIAL_USERNAME.
 *
 * Single username-pivot service, fusing the former SHERLOCK_SEARCH,
 * SOCIAL_ANALYZER (Maigret), SOCIAL_INTEL and the per-platform deep lookups
 * (REDDIT/TWITTER/FACEBOOK/INSTAGRAM/LINKEDIN_LOOKUP). On-demand; ctx->entity =
 * a username/handle. Each constituent keeps its own probing/detection and is
 * invoked via social_fuse.h; rows dedupe naturally on remote_key
 * ("profile:<platform>:<username>"). Honest-empty when nothing is confirmed. */
#include "source.h"
#include "social_fuse.h"

static int run(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return 0;
  int t = 0;
  t += jo_social_intel_run(ctx, sink);      /* GitHub/Reddit/Keybase deep + shallow */
  t += jo_social_platforms_run(ctx, sink);  /* Reddit/Twitter/FB/IG/LinkedIn deep */
  t += jo_sherlock_run(ctx, sink);          /* 511-site presence (soft-404 aware) */
  t += jo_maigret_run(ctx, sink);           /* Maigret site DB presence */
  /* run() must return 0 for success — core/scheduler.c does
   *   status = rc == 0 ? "ok" : "error"
   * and feeds that to anomaly_detect(). Returning the emitted count made every
   * successful run (91 real profile rows for `torvalds`) log as an error and
   * walk the source toward quarantine. Only a negative is a failure. */
  return t >= 0 ? 0 : -1;
}

static const source_def social_username_def = {
  .id = "SOCIAL_USERNAME", .collector = "osint",
  .name = "Username OSINT", .name_ja = "ユーザー名OSINT",
  .update_interval_sec = 0, .run = run,
  .category = "social", .type = "api",
  .url = "internal://osint/social-username",
  .description = "Username pivot: cross-platform profiles (GitHub/Reddit/Keybase/Twitter/…) + presence enumeration across the Sherlock (511) and Maigret site databases.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(social_username_def)
