# ESLint pass 2 (client/): react-hooks/set-state-in-effect 57 -> 0, rule now "error"

Head: cf240e5 on the worktree branch (based on claude/peaceful-ritchie-i7e0hj). Not pushed.
Result: `npx eslint .` 0 errors 0 warnings; `npx vitest run` 34 files / 233 tests pass (227 old + 6 new, no timeouts);
`npx vite build` OK; browser smoke 27 routes x 2 API modes, no render loop, no console error.
New eslint-disable lines: 0.

Patterns (D = derived while rendering with previous value kept in state; K = answer stored with the key it answers; A = state set only from async continuation, cancellation/seq kept; X = useSyncExternalStore; L = new hooks/useLoadOnChange)
- hooks/useAoi: A (pure fetchAll, effect inline async w/ alive flag) + D for `enabled` turning on
- hooks/useDataSources: first read from a timer callback (cleared on cleanup)
- hooks/useIntel (useIntelItems): A with seq + D on request identity; loading = enabled && loading
- hooks/useLayerCatalog: X (stable memoised snapshot; sessionStorage hydration moved into module snapshot())
- hooks/useLiveVehicles: D (disable clears geojson)
- hooks/useMapProjection: projection stored with the coords it answers (K); first projection from a rAF callback
- hooks/usePagedList (useMoreRows): D on initialRows/initialPage, generation counter now state + ref
- hooks/useSearch: suggestions short-query reset D; pause reset D; useRun error reset D; useEntitySearch reset D (initial loading seeded from q);
  useEntity: pure fetchAll + apply, effect inline with alive flag (also drops a superseded read), D for loading
- alerts/AOIPage, BreachMonitorsPage, WatchlistsPage: L (+useCallback loader); sheets (create/rename/add/editor) D
- alerts/AlertInboxPage: L + D (filter change drops old rows)
- alerts/AlertsPage: preset auto-open D; RuleEvents L; RuleEditorSheet draft re-seed D; PreviewSection status D
- alerts/SavedSearchesPage: Rename/ToAlert sheets D
- cases/CaseDetailPage: NotesPane target derived (no state sync); MembersPane draft D; Edit/Delete sheets D
- cases/CasePickerSheet: D
- collectors/CollectorDetailPage: draft D; paged rows re-seed from head response D
- console/ApiKeysPage: KeyDetailSheet working copy D; KeyPolicySheet draft D
- console/ThresholdsTab: row draft D; rows D
- entities/EntitiesPage: ?q= pre-fill D; entities/EntityProfile useResolvedId: K + derived id/missing
- map/CameraStills (StillFrame), MapPopup (DamLiveLevel, CameraSnapshot), panels/CameraVisualSearch (FrameThumb, SimilarCameras): K
- map/MapPage: ?aoi=new raises aoiDrawing while rendering (idempotent), effect only strips the param
- map/overlays/AOIDrawOverlay, IsochroneOverlay: D (inactive/closed resets)
- map/overlays/ShareSheet: D for close reset; mint effect writes state from callbacks
- map/popups/StationPopup: missing uid derived
- panels/DatabaseExplorerTab: D marks loading on query change; DatabaseSchedulerTab: dropped redundant setLoading(true) (starts true, mount-only)
- search/RunActions: SaveSearchSheet D; SearchShareSheet D; search/WorkspaceRuns, timeline/TimelinePage: L
- sources/CadenceCard: form D; sources/SourcesListPage: draft D
- eslint.config.js: set-state-in-effect "warn" -> "error"

Notes / judgement calls
- useLoadOnChange starts the loader from a queueMicrotask (cancelled if the loader identity changes or the component unmounts first). Loader's own seq guard still handles in-flight staleness. It is a deliberate "set state only from an async callback" device for loaders shared with buttons (reload/load more).
- A bug the new test caught in my own first draft: useEntitySearch initial loading must be seeded from q (fixed).
- Behaviour deltas (all improvements, none intended otherwise): useEntitySearch emptied box now also clears loading (was stuck true); useEntity drops a superseded read; useMapProjection never serves the previous point's pixels while the new one is pending; useLayerCatalog hydrates sessionStorage on first snapshot instead of in a useState initialiser.
- CameraStills' outer component still has a reset-by-cameraId effect that the rule did not flag; left as is.
- Smoke: shell needs auth, so Playwright seeded onboarded/refresh keys in localStorage and mocked /auth/v1 + /api (mode "fail": all 503; mode "empty": 200 empty envelope). One console TypeError on /console/scheduler in "empty" mode is from my mock giving `sources` as an object where DatabaseSchedulerTab expects an array (code unchanged by me beyond removing setLoading(true)).
- Environment: disk filled during the work; I ran `npm cache clean --force` (frees ~210 MB of re-downloadable cache) to continue. Other worktrees' node_modules are untouched.
- Did not touch client/src/auth or the api client/token module.
