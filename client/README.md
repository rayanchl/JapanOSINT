# `client/` — the web site

React + Vite + MapLibre GL / deck.gl. This is the site `./launch.sh up` starts
as its **web pod**, against the native C server in [`../native/`](../native/).
(This file used to call the directory dormant — "nothing starts it, nothing
serves it". That stopped being true when `launch.sh` gained the web pod; the
README is now about what the client actually does.)

## How it is served

* `./launch.sh up` (or `./launch.sh web`) runs the **Vite dev server** on
  `:5173` (`WEB_PORT`; `WEB_HOST=0.0.0.0` to expose it on the LAN).
* `vite.config.js` proxies `/api` to the server pod — `JO_API_TARGET`, which
  `launch.sh` sets to the server's `$PORT` (default `http://localhost:4000`).
* `bin/japanosint` serves only `/api`; it has no static-file handler. A
  production bundle (`npm run build` → `dist/`) needs something else in front
  of it that serves the files and forwards `/api`.

## Authentication

`src/auth/session.js` installs a `window.fetch` wrapper
(`installFetchInterceptor`, called from `AuthContext.jsx`) that adds
`Authorization: Bearer <access token>` and `X-Tenant-Id` to every `/api`
request, and on a 401 refreshes the session once and retries. Every
`fetch(apiUrl(...))` in the app goes through it.

Two request kinds cannot, and are handled on their own terms:

* **`EventSource`** sends no headers. The search progress stream
  (`src/store/searchStore.js`) opens with the per-run `?key=` the server
  returns to the run's starter.
* **`<img src="/api/...">`** is fetched by the browser, not by `fetch()`, so it
  would carry no session. Camera images go through
  `src/utils/authedImage.js` (`fetch` → blob → object URL) instead.

## No WebSocket

The native server has no `/ws` endpoint, and `vite.config.js` proxies only
`/api`. `src/hooks/useWebSocket.js` therefore does nothing unless the build
sets `VITE_JO_WEBSOCKET=1` (`WS_ENABLED` in `src/utils/wsUrl.js`), and returns
`available: false` so callers use their HTTP path and say so:

| hook | without `/ws` |
|---|---|
| `useDataSources` | polls `/api/sources` + `/api/sources/stats` every 60 s; "connected" means the last poll succeeded |
| `useCameraDiscoveryStream` | polls `/api/data/cameras/run-status` (4 s during a run, 20 s idle) and re-reads the head of `/api/data/cameras/discovery-feed` when a run finishes; the banner shows the one `ingested` count run-status gives |
| `useCollectorFollowStream` | no live source exists, and `/api/follow/recent` answers 501 — the Follow panel says so |
| `useLayerLoading` | contributes no server-side "working" flags; the client's own in-flight flag (useMapLayers) still drives the layer spinner |
| `useFtsReady` | returns `null` ("unknown"); not mounted anywhere today |

## Routes the client must not assume

Two calls to routes the server never had were removed on 2026-10-06 and are
pinned by `src/utils/__tests__/serverRoutes.test.jsx`:

* `/api/data/cameras/snapshot?url=` → the real
  `/api/data/cameras/proxy?camera_uid=` (`native/core/cameraproxy.c`), which
  takes a camera's uid, never a URL, and serves that record's own image URL
  only when it answers with an image. Embed-blocked aggregator pages are HTML,
  so for those the popup shows the proxy's error rather than a picture — the
  Node server's headless-browser capture was not ported.
* `/api/data/flight-adsb/enrich` → removed; the ADS-B popup shows the route
  only when the feature itself carries origin/destination.

Before adding a call, check the route exists in `native/core/httpd.c`.

## Develop and test

```sh
npm ci
npm run dev          # :3000; launch.sh runs vite with --port $WEB_PORT (5173)
npx vitest run       # unit tests (jsdom)
npx vite build       # the production bundle CI builds
```
