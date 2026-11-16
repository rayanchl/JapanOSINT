<!-- Dated history moved out of CLAUDE.md on 2026-11-16 (verbatim, unedited): Infrastructure audit W1a-W2a. -->
<!-- CLAUDE.md holds the current rules; where this text contradicts it, CLAUDE.md wins. -->

## Infrastructure audit (W1a–W2a, merged 2026-10-12)

The `claude/audit-fixes-and-embeddings` work, integrated into main with its two
failing tests repaired. What it guarantees, and the knobs it adds:

* **Auth off the loop.** The JWKS is refreshed on its own thread, never fetched
  inside a request; break-glass tokens verify and every use is audited.
* **The audit log is a hash chain** (`audit_events`); verification reports
  unchained rows. Collector runs started through the API are gated by role and
  a per-user budget.
* **Outbound politeness / SSRF** (`core/hostgate.c`): per-host capacity and
  `Retry-After`; the camera proxy refuses LAN, metadata and link-local
  targets, always — there is no LAN opt-in (`JO_CAMERA_ALLOW_LAN` was removed
  2026-10-20; tests use `hostgate_camera_test_allow_lan()`).
  **ffmpeg's own sockets are boxed too** (2026-10-22, `core/netjail.{c,h}`):
  every network run of ffmpeg/ffprobe goes through a per-child forward proxy
  (http/https/hls) or a pinned relay (rtsp/rtsps) that resolves each name once,
  judges every answer under the camera policy and connects to what it judged —
  so a live playlist's later segments and a re-resolved name are judged, not
  just the first URL. On Linux the child also runs in its own network
  namespace (a user namespace when not root) whose only listener is that
  proxy, so an RTSP redirect or a `tcp://` segment has no route at all. Where
  namespaces are unavailable it degrades to proxy-only, and
  `GET /api/admin/ffmpeg/capabilities` → `network_sandbox` says which.
  rtmp/tcp/udp network inputs have no boxed path and are refused.
  `test_netjail.c` decodes a real HLS frame through the box when ffmpeg is
  installed (CI installs it), as root and as an unprivileged user.
* **LLM queue**: bounded lanes (`JO_LLM_QUEUE_MAX`, default 64 waiting; a job
  waits at most `JO_LLM_QUEUE_WAIT_FACTOR` × its timeout, default 4), no retry
  after a timeout, queue time reported apart from model time;
  `JO_EMBED_QUERY_URL` (default `JO_EMBED_URL`) serves query-time embeds on
  their own slot, and both servers' `/v1/models` must agree. Triage and
  translation inputs are FENCED as untrusted text; a run's synthesis row is
  neither translated nor mined for entities. An LLM outage is NOT charged to
  the items it was asked about (only an answer the model gave is).
* **Scheduler**: a boot ramp (`JO_SCHED_RAMP_SEC`, 900 s) instead of a stampede,
  a maintenance lane (`JO_SCHED_MAINT_WORKERS`, 2) that collectors and pods
  never share, real cancellation; `log-retention` prunes fetch_log, anomalies,
  repairs and alert deliveries/events in `JO_RETAIN_BATCH` (5,000) batches.
* **Alerts**: shared-corpus rows meet every tenant's rules, and alert_events is
  UNIQUE(rule_id, item_uid) — one event per (rule, item), even under racing
  double-fires.
* **Breach**: an audited reveal that carries a reason (iOS sends it), paged
  search off the loop, a keyed lookup hash, and a corpus root of
  `$JO_BREACH_DIR`, else `$JO_REPO_ROOT/data/breach`, else the repo the binary
  was built from — never a path written into the source.
* **Detail hops** (hpengine): one admission point, `hp_detail_admit()`, spends a
  per-run budget of a count (default 1,000, was 25) and a clock
  (`JO_HP_DETAIL_SEC`); every record it did not reach carries
  `_detail_pending`, and the run files a `detail-truncation` notice with the
  counts. Every mode deepens, not only JSON.

The concurrency gate (`tools/ci_concurrency_gate.sh`) keeps three `/api/status`
LOOPS running for `LOAD_SEC` (12 s) rather than three single requests: a build
now takes ~0.25 s, which left the old gate passing or failing on timing. CI
runs it with a 0.5 s health limit.
