# Web client audit fixes (client/ only)

Results: `npx vitest run` 32 files / 216 tests pass (was 29/200). `npx vite build --outDir <scratchpad>/dist-web` OK;
main chunk 2.84 MB -> 475 KB (MapPage is its own 2.4 MB lazy chunk). node_modules/dist are gitignored; no build output in repo.
Note: another process committed my in-progress client edits as WIP commits (3861d3f/29cbff2); I ran no git commands.

| ID | Status | Files / notes |
|---|---|---|
| WEB-01 | FIXED | new sources/RunControl.jsx (isPivotKind, runSource, useCanRun, RunControl). Pivot kinds (pivot, scheduled-pivot) get entity input + Run (sends {entity}); button disabled until entity typed, no request sent without one; 422 needs_entity shown inline; never "ran: 0 new rows" for a pivot without entity. Used in SourceDetailPage, IntelPage (cards), IntelSourceItemsPage. |
| WEB-02 | FIXED | SourcesListPage: canEditCadence = auth.isPlatformAdmin === true. |
| WEB-03 | FIXED | IntelSourceItemsPage -> /api/sources/:id/detail; IntelPage SourcesView -> usePagedList(/api/sources/search has_items=1, sort, category facet, PagedFooter); Timeline source select -> Typeahead (new ui/Typeahead.jsx) on /api/sources/search. |
| WEB-04 | FIXED | SourceDetailPage: dead link removed; "full run history" pages /api/sources/:id/logs in place via usePagedList + PagedFooter (RunHistory). |
| WEB-05 | FIXED | SourcesPanel: ?summary=1 counters + paged search + per-row /api/status/:id on open; AdminPage breach: ?summary=1 + search?category=breach; SettingsPage schedule + probe consent: paged search, per-row lazy GET /api/sources/:id and /api/status/:id. ApiKeysPage: PARTLY - "used by" now comes from the paged health=gated search (only sources WAITING on a credential; stated in-band) because no cheap route lists already-configured keyed sources. |
| WEB-06 | FIXED | Run gated on isPlatformAdmin||canManageWorkspace (read-only note otherwise); summary regenerate, probe/consent and schedule gated on isPlatformAdmin with read-only notes. AdminPage "Re-run collector" left (operator-only page) - on a pivot it now just gets the server's 422 as an action error. |
| WEB-07 | FIXED | SourcesListPage: "pivots only" quick filter (kind=pivot) + PivotFinder (semantic search on /api/sources/semantic, 503 shown as such, entity form per pivot hit). |
| WEB-08 | FIXED | Collector filter is a Typeahead on /api/collectors?q=. |
| WEB-09 | FIXED | Detail shows d.layer_id with a link to /?layer=<id> (MapPage now reads ?layer= and switches it on) and "no geocoded records: not on the map" when records_geocoded is 0. |
| WEB-10 | FIXED | layerCatalog.loadProgress + LayerPanel "Loading N of M records (page P)" from _meta. |
| WEB-11 | FIXED | hooks/__tests__/layerDefinitionsAllowList.test.js: frozen allow-list of the 237 current definitions (fallback untouched). |
| WEB-12 | FIXED | useCollectorFollowStream no longer calls /api/follow/recent; FollowPanel history-error UI removed. |
| WEB-13 | NOT FIXED | tokens stay in localStorage; no session-only path exists in session.js, and httpOnly cookies need server support. |
| WEB-14 | FIXED | session.js: 401 with no refresh token (and a token we attached) clears tokens and fires session-lost. Test auth/__tests__/session401.test.js. |
| WEB-15 | FIXED | useLayerCatalog cache keyed by tenant and cleared on sign-out/account change (localData.clearLocalUserData). |
| WEB-16 | FIXED | CollectorDetailPage Row/Table hoisted to module scope. |
| WEB-17 | FIXED (broad) | aria-labels, table captions/scope, aria-pressed on source, collector, intel, settings, panel screens. Not an exhaustive a11y pass. |
| WEB-18 | FIXED | App.jsx lazy MapPage. |
| WEB-19 | FIXED | useApi poll and useDataSources pause while document.hidden, refresh on return. |
| WEB-20 | FIXED | sources/__tests__/pivotRun.test.jsx (pivot run flow, 422, read-only, cadence gate, IntelPage paging), session401, allow-list, loadProgress. |
| DOC-02 | FIXED | ThresholdsTab copy replaced with neutral operator-tunable / measured 2026-10-23 text. |
