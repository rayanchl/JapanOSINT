# `client/` — React web app (NOT the live client, NOT served, NOT started)

**Status: dormant. Read this before you spend a day on it.**

The live client of this product is the SwiftUI iOS app in [`../ios/`](../ios/).
This directory is a React + Vite + MapLibre GL web app from the Node era. It
still builds on its own with `npm run dev`, but nothing in the running system
references it.

Measured 2026-08-09, against this checkout:

| Claim | Evidence |
|---|---|
| ~14.7k lines of app code | 14,678 LOC of `.js`/`.jsx` under `client/src` (21,452 including CSS and config) |
| Nothing starts it | `../launch.sh` contains **zero** occurrences of the string `client` |
| Nothing serves it | `native/core/httpd.c` has no static-file handler; its catch-all is `reply_json(c, 404, "{\"error\":\"not_found\"}")` (`httpd.c:2256`, `:2259`), so every non-`/api` path 404s |
| Its realtime layer targets a server that was never ported | `useDataSources.js:68`, `useCameraDiscoveryStream.js:141` and `useCollectorFollowStream.js:134` all call `useWebSocket('/ws')`; `httpd.c` implements no `/ws` endpoint at all |
| Effectively unmaintained | exactly one commit in the last 30 days touched `client/`, and it was a native-side camera fix that swept these files along |

## What is NOT wrong with it

An earlier audit recorded that the first ten `LAYER_DEFINITIONS` entries
(`earthquake`, `weather`, `transport`, `air-quality`, `radiation`,
`population`, `landprice`, `river`, `crime`, `gdelt`) had no collector behind
them. **That is not true**, and it was re-measured on 2026-08-09: every one of
those layers has at least one registered, implemented C source carrying that
`layer` id in `native/core/source_registry.gen.c` — 5 for `earthquake`, 6 for
`weather`, 9 for `population`, 7 for `landprice`, and so on. `/api/data/<layer>`
is served generically by `dataapi_layer()` (`httpd.c:2223`).

So the data side is largely there. What is missing is the *plumbing*: nobody
serves the bundle, and the WebSocket half of the app has no server.

## If you want to revive it

Roughly, in order:

1. Serve the built bundle — either add a static handler to `core/httpd.c` for
   non-`/api` paths, or put a reverse proxy in front and drop the assumption
   that the API and the UI share an origin.
2. Replace the `/ws` hooks. The C server pushes over SSE, not WebSocket; the
   three `useWebSocket('/ws')` call sites need porting to the SSE endpoints the
   binary actually exposes, or `/ws` needs implementing (`native/lib/ws.c`
   exists on the *client* side — it needs Homebrew curl on macOS, see
   [`../docs/BUILD.md`](../docs/BUILD.md)).
3. Re-verify each `LAYER_DEFINITIONS` endpoint against a running server rather
   than against any document, including this one.

## Why it is still here

14,678 lines is not a maintainer's decision to make on someone's behalf, and
several of these components (the layer panel, the source dashboard) are the
only worked-out design for screens the iOS app does not have yet. Deleting it
is a product call, not a cleanup call. What is not acceptable is the previous
state: a root README advertising it as *the* product while nothing built,
served or started it.
