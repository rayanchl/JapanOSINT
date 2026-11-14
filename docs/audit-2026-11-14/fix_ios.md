# iOS audit fixes (IOS-01 .. IOS-21)

Nothing was compiled (Linux). All paths under /home/user/JapanOSINT/ios/JapanOsintApp. The project uses
file-system-synchronized groups, so the three NEW files are picked up without editing project.pbxproj.
Brace/paren/bracket balance of every edited file was checked with a script; syntax was otherwise re-read by eye.

New files: SourceModels.swift, API+Sources.swift, Dashboard/SourceDetailView.swift.

| ID | Status | Notes / files |
|---|---|---|
| IOS-01 | FIXED | Dashboard uses /api/status?summary=1, /api/collectors, /api/sources/search (50/page, has_more). Dashboard/SourceDashboardTab.swift, API+Sources.swift, SourceModels.swift. Full-dump wrappers `status()`, `sources()`, `intelSources()` removed from API.swift. |
| IOS-02 | FIXED | Per-source dot chart replaced by counts + proportional bar; category chart from the server facet; lists are Lazy; SourceScheduleSettingsView rewritten (paged, lazy per-row mode, no O(n^2) binding). |
| IOS-03 | FIXED | Intel tab pages /api/sources/search (has_items default, "All sources" toggle, server category filter); IntelCache caches only the shown first page (<=100) and `upsertSource` refreshes ONE row after a run; TimelineModel uses one bounded 500-row search and discloses "N of M". |
| IOS-04 | FIXED | "Run all sources" removed from Intel toolbar. Per-collector run on the dashboard keeps its >12 refusal. |
| IOS-05 | FIXED | Detail view shows an entity field for pivots (`registry.is_entity_pivot`/kind==pivot), sends {entity}, Run disabled until typed; 422 needs_entity -> message; collector "run all" skips pivots and says how many; Intel row for a pivot explains instead of running. API.intelRunSource(_:entity:). |
| IOS-06 | FIXED | Separate `longSession` (30 min resource ceiling) for `longRunning: true`; comments rewritten to match code; URLError.timedOut -> "still running on the server"; 409 run_in_flight message (`API.runFailureMessage`, used by all run callers). |
| IOS-07 | FIXED | Email removed. `AuthSession.isOperator` from `API.probeOperator()` (GET /api/media/capabilities; 200 true, 401/403 false, error unknown), cached per user until sign-out. `isPlatformAdmin` now returns it, so call sites are unchanged. |
| IOS-08 | FIXED | `_meta.records_available` / `truncated` read into FeatureStats; map status line shows "N shown of L loaded ... server holds A"; Layers tab shows "sent of held". |
| IOS-09 | FIXED | Restart control, state, doRestart and API.restartServer removed (AdminPanel.swift, API.swift). |
| IOS-10 | FIXED | `API.pathEscape` applied to item, entities, reveal, similar, run, schedule, probe, consent routes. |
| IOS-11 | FIXED | IntelItem.isCollectorNotice = collector-*-notice + fetch-failure + fetch-error; label by kind; reason/remedy/http_status/tls fields rendered (IntelItemRow, NoticeText in SourceModels.swift). |
| IOS-12 | FIXED | Filter sheet is kind / health / last-run status / category from server facets. Old client "Access" (free / needs key) and "Type" filters dropped: the search row has no such fields. |
| IOS-13 | FIXED | SourceDetailView: summary (LLM text or the server's error, never invented), health with why/remedy, run, data, schedule, notices, recent runs, operator-editable settings (interval_sec, http_body_ceiling_bytes; server bounds shown), system cadence display, identity, probe/consent, API-key rows, View records. Cadence is display-only (edit stays on web). |
| IOS-14 | FIXED | waitsForConnectivity = false on both sessions. |
| IOS-15 | FIXED | IntelSourceItemsView: `.task(id: searchText)` debounce + token guard so stale answers are dropped. CrossSourceSearchView already used `.task(id:)`. |
| IOS-16 | PARTLY | ApiKeys: no status dump; detail sheet lists sources the server reports as gated on the key (search health=gated, credentials_required), probe state fetched per row via /api/status/:id. LIMITATION: a source whose key is already set is no longer gated and is not listed (search has no key filter). Breach console: counts from ?summary=1; picker rows are a bounded status window (<=500) at the end of /api/status because breach catalog rows are not in the searchable registry. |
| IOS-17 | FIXED | No targets added. drainShareQueue handles ONE url/text item per call, leaves the rest (and images) queued. |
| IOS-18 | FIXED | searchStreamRequest applies `API.mayCarryCredentials`. |
| IOS-19 | FIXED | BuildConfig backend URL empty in release, debug default kept; onboarding sign-in/social buttons disabled and the server field forced open until a URL is entered. Info.plist insecam.org ATS exception left untouched (needs an owner decision). |
| IOS-20 | PARTLY | Layers copy uses registry.layers.count; the "100+ collectors" copy now says "hundreds of collectors and tens of thousands of sources" (onboarding is pre-auth, so no server count). Threshold copy updated (measured 2026-10-23, operator-tunable). Hardcoded showcase ids / colour tables left as cosmetic. |
| IOS-21 | PARTLY | OAuth force unwrap guarded; "Showing N of M" in all new paged lists, SourcePipeline runs ("latest 12 of N"), cases picker (has_more), alert events (200 cap note, total unknown). IntelSourceItemsView's cursor-paged list is unchanged (no total exposed). |

## Behaviour changes worth knowing
- Breach catalog rows are not in /api/sources/search, so they no longer appear in the Sources list or the Intel tab; Admin > Breach corpus is where they are worked.
- Camera discovery children are no longer nested under their parent in the Intel tab (search rows carry no parent_id).
- Entity-pivot "needs entity" relies on the new server behaviour (HTTP 422 needs_entity); against an older server a pivot run with an entity is still sent {entity}.
- Server field names for `layer_id` / `records_geocoded` in /detail were not found in native/core; both are decoded as optional at several plausible locations (registry.layer_id, top-level layer_id, data.records_geocoded, top-level records_geocoded).
- /api/sources/:id/logs is not used (detail shows the latest 20 runs and the total).

## Unsure it compiles (could not build)
1. SourceDetailView.swift: very large @ViewBuilder function bodies (`content`, `healthSection`, `scheduleSection`) with `let` declarations and `if let` chains; type-checker complexity. `.keyboardType` inside `#if os(iOS)` in a modifier chain (SourceSettingEditor).
2. `async let` with throwing calls and `try? await` later (SourceDetailView.loadAll, SourceDashboardTab.load).
3. SourceModels.swift: SourceSettingRow custom private CodingKeys (`default` keyword remapped to `defaultValue`); `guard var` in NoticeText.label.
4. Key-path-as-function usage: `rows.map(\.asIntelSource)`, `rows.filter(\.needsEntity)`.
5. Tuple-array computed property `statusParts` in SourceDashboardTab and its use in two ForEach(Array(...enumerated())).
6. IntelTab/ SourceScheduleSettingsView: `.task(id:)` + `.searchable` combination; `Form` containing a `PagedFooter` with `.onAppear` (loads next page when shown).
7. OAuth.swift: `ASPresentationAnchor()` fallback (deprecated on iOS 26, warning not error).
8. MapTab.featureCountText reads `visibleFeatures` (computed, expensive) once per status-bar render, as the old text did twice.
9. Models.swift IntelItem new computed properties return a tuple array (`noticeDetail`); IntelItem is Codable/Hashable, which should be unaffected.
10. ApiKeyDetailView: UsedByRow is a `private struct` at file scope using @EnvironmentObject apiClient; `keyName` property unused.
