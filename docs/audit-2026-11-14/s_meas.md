# s_meas (partial, in progress)

## Setup
- libcurl 8.11.1 with --enable-websockets built at scratchpad/meas/curl (shared; run with LD_LIBRARY_PATH); the unchanged server binary links it at runtime. Repo default build untouched.
- Proxy control: curl 8.11.1 through HTTPS_PROXY to wss://echo.websocket.org, wss://jetstream2.us-west.bsky.network, wss://certstream.calidog.io: all `HTTP/1.1 101 Switching Protocols`. The proxy carries WebSocket.

## 1. WebSocket live
- bluesky-jetstream-jp: `[sched] bluesky-jetstream-jp run rc=0 records=17 6243ms stored=17` (second run 14/14).
- certstream-jp: handshake 101 OK, then ZERO bytes in 12 s (curl: "Operation timed out after 12001 ms with 0 bytes received"): the public calidog server is silent. Defect found: both collectors ignored ws_collect()'s return, so a failed handshake or silent socket read as `rc=0 records=0` (rule 1). Fixed: -1 + stderr reason when ws_collect <= 0 and nothing oversize; scheduler then files collector-fetch-failure (verified: `[sched] certstream-jp run rc=-1`, fetch-failure item stored). Residual: the scheduler's generic wording says "made no HTTP request at all" because WS traffic is not counted in requests=; not edited.
- Oversize path: `JO_WS_MAX_MSG_BYTES=1024` bluesky run: 63 "[ws] message exceeds ... dropped" lines, `records=10 stored=11 notices=1`; notice record_type collector-truncation-notice, reason "websocket messages over the per-message ceiling were dropped whole", messages_dropped=63, bytes_dropped=106540, max_message_bytes=1024.

## 2. SRC-05 (in progress)
- ROOT CAUSE of the 14 "died" rows: tools/audit_registry_emit.py imposed its 1024 MB file ceiling with `sh -c 'ulimit -f 1048576'`; /bin/sh is dash, whose -f counts 512-byte blocks, so the ceiling was 512 MB. A run storing 100,000 rows makes a ~575 MB DB; SIGXFSZ (shell_rc -25) killed it silently. Reproduced under strace: `ftruncate(3, 537280512) = -1 EFBIG`, `+++ killed by SIGXFSZ +++`. Unlimited direct run: rc=0, 240 s, peak RSS 362 MB (not memory). Fixed: setrlimit in bytes.
- SECOND defect behind the 11 af-dportal-trans-* rows: `trans_country_code=BW` is ignored by d-portal (rule 4d FILTER_IGNORED): count(*) with that filter = 25,453,092 (whole table), first rows were HT/KE/MY. The rows emitted 100,000 records of OTHER countries; the walk was unordered over a 25M-row table, hence the varying 58..332 uid collisions (drift). Correct filter `trans_country=BW` -> 53,521; KE 468,787; NG 474,294; ZA 234,231; ET 482,306; GH 299,514. `orderby=aid,trans_id` honoured (control `orderby=notacolumn` is ignored, real columns reorder). Fixed in 11 rows. First-pass verdicts for them (slow19_before_dportal_fix.tsv) are therefore of the wrong collection.
