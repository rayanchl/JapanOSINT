# iOS verification pass 2 (2026-11-25)

## Toolchain
Swift 6.2.1 (swift.org tarball, `download.swift.org`, ubuntu24.04 x86_64; not refused), extracted outside the repo under
`/tmp/claude-0/-home-user-JapanOSINT/c20195ab-72c3-5bbf-bc8d-73ece67936f3/scratchpad/swift/tc62/`. (6.0.3 was tried first and
rejected the app's `nonisolated struct` syntax, so 6.2.1 is the version that matches Xcode 26.)
`swiftc --version` -> `Swift version 6.2.1 (swift-6.2.1-RELEASE) Target: x86_64-unknown-linux-gnu`.
No SwiftUI/UIKit/MapKit/SwiftData on Linux, so only the Foundation-only slice can be type-checked.

## Checks run
1. `swiftc -frontend -parse` on EVERY .swift under ios/ (154 files, incl. ios/Unbuilt): `parsed 154 files, 0 with syntax errors`.
2. `swiftc -typecheck -swift-version 5` of the Foundation-only slice (Models, Models+Roadmap, SourceModels, API, API+Sources,
   API+Roadmap, Auth/SupabaseAuth, Auth/AuthTokenBox, BuildConfig, Search/SearchModels, Map/IsochroneModels, Cameras/*,
   Translation/Romaji) with an out-of-repo `CoreLocation` stub (`CLLocationCoordinate2D`). Exact output:
   ```
   API.swift:50:13: error: cannot assign to property: 'waitsForConnectivity' is a get-only property
   API.swift:61:13: error: cannot assign to property: 'waitsForConnectivity' is a get-only property
   ```
   Those two are swift-corelibs-foundation lacking a setter (IOS-14 sets it false on Apple platforms where it is settable);
   not app defects. Everything else in the slice type-checks.
   Excluded because Linux Foundation lacks the API: Shared/DateFormatting.swift (`RelativeDateTimeFormatter`),
   Auth/PlatformAdmin.swift (extends `AuthSession`, which imports Combine/SwiftUI-side types), Widgets files.
3. Full COMPILE + LINK + RUN of that slice (minus the two `waitsForConnectivity` lines, removed in a scratch copy) with
   `/…/scratchpad/decode/main.swift` decoding the server's shapes (field names taken from native/core/sourceapi.c,
   statusapi.c, cadence.c). Output:
   ```
   PASS sources/search?key= (meta.credential)
   PASS sources/search (no credential)
   PASS status?summary=1
   PASS sources/cadence (realtime_max_sec)
   PASS sources/:id/detail (sourceKind)
   PASS sources/:id/settings
   PASS collectors
   ALL DECODE CHECKS PASSED
   ```
   This found real gaps, now fixed in `SourceModels.swift`: `meta.credential` (name/sources/shown/basis), `registry.sourceKind`
   on /detail, and `estimate.realtime_exempt_rows` were not decoded at all.
4. Manual review (REVIEW, not compilation) of the 31 Swift files changed since 2026-11-13 for the SwiftUI-dependent files:
   a script listed every capitalised identifier in each file that is not declared anywhere in ios/; all residuals are Apple
   framework types or string-literal words (no undefined app types). A second grep confirmed ~30 newly introduced
   helpers (runFailureMessage, pathEscape, mayCarryCredentials, longSession, isOperator/probeOperator, PagedFooter,
   statusWindow, upsertSource, ...) each have exactly one declaration and callers; removed wrappers (`status()`,
   `sources()`, `intelSources()`, `restartServer`) have no remaining callers.
5. CI: `.github/workflows/ci.yml` already had `ios-build` (macos-latest, `-scheme JapanOsintApp`, `CODE_SIGNING_ALLOWED=NO`,
   iOS device + macOS). Added a `Build (iOS Simulator)` step (`generic/platform=iOS Simulator`). YAML parses
   (`yaml.safe_load`; steps: Build (iOS), Build (iOS Simulator), Build (macOS)). Never run (no macOS here).

## Per IOS id
Status words: compiled+typechecked = in the Linux slice above; syntax-checked = parse only; reviewed only = review + parse.
Every SwiftUI file is at best "syntax-checked" + review. Nothing was compiled with Xcode.

| ID | Status |
| --- | --- |
| IOS-01 | models/API compiled+typechecked+decoded (Dashboard/SourceDashboardTab.swift view code: syntax-checked, reviewed only) |
| IOS-02 | syntax-checked, reviewed only |
| IOS-03 | IntelCache/IntelTab/TimelineModel: syntax-checked, reviewed only |
| IOS-04 | syntax-checked, reviewed only |
| IOS-05 | API.intelRunSource compiled+typechecked; SourceDetailView syntax-checked, reviewed only |
| IOS-06 | API.swift compiled+typechecked (session/runFailureMessage); behaviour not exercised |
| IOS-07 | PlatformAdmin/AuthSession: syntax-checked, reviewed only (`API.probeOperator` compiled) |
| IOS-08 | FeatureStats/MapTab/LayersTab: syntax-checked, reviewed only |
| IOS-09 | syntax-checked, reviewed only (removal; no callers remain) |
| IOS-10 | `API.pathEscape` compiled+typechecked |
| IOS-11 | `NoticeText`, `IntelItem.isCollectorNotice/noticeDetail` compiled+typechecked; IntelItemRow syntax-checked |
| IOS-12 | syntax-checked, reviewed only |
| IOS-13 | models decoded (detail/settings/cadence); SourceDetailView large @ViewBuilder bodies: syntax-checked, reviewed only (type-checker complexity is the open risk) |
| IOS-14 | compiled on Linux only after removing the property (Linux lacks the setter): reviewed only for Apple platforms |
| IOS-15 | syntax-checked, reviewed only |
| IOS-16 | DONE. `searchSources(key:)` -> `/api/sources/search?key=NAME` compiled; `SourceSearchMeta.credential` now decoded (PASS against the server's shape); ApiKeyDetailView "Used by N" now uses `meta.credential.sources` (falls back to `page.total`). View: syntax-checked, reviewed only |
| IOS-17 | NOT DONE: needs Xcode. pbxproj is hand-maintained (one target, no XcodeGen/Tuist), so nothing was hand-edited. Widgets, Share extension and the app-side feeders (SharedSnapshot, SearchActivityAttributes, WidgetSnapshotBuilder, BackgroundRefresh) moved by `git mv` to `ios/Unbuilt/`; README there states the Xcode steps; all app references removed (`drainShareQueue`, widget refresh, BGTask registration, scenePhase hook, `japanosint://share` handling, `BGTaskSchedulerPermittedIdentifiers`/`UIBackgroundModes` in Info.plist). syntax-checked + reviewed only |
| IOS-18 | API.swift compiled+typechecked |
| IOS-19 | BuildConfig compiled+typechecked; onboarding view syntax-checked, reviewed only |
| IOS-20 | DONE in source, reviewed only. Onboarding intro copy no longer claims "hundreds of collectors / tens of thousands of sources / real time" (neutral copy; live counts are the server numbers on the Sources tab after sign-in); onboarding layer-tile showcase is filtered to ids the server's `/api/layers` returned (empty until loaded; also removes a modulo-by-zero when empty). Layers copy already used `registry.layers.count`. Not done: decorative `AnimatedMapMock` strings / colour tables (cosmetic) |
| IOS-21 | DONE in source, reviewed only. `IntelSourceItemsView` requests `total=1` (server route `/api/intel/items`, `page.total` exact or `page.total_gte` at JO_TOTAL_CAP=10000, `intelapi.c` ~643-716; nothing added server-side) and shows "Showing N of M items" (or "M+"), noting on-device filters and collapse. `IntelPage.totalLabel` compiled |

## Not done / honest caveats
- No Xcode build was possible; the two ios-build CI jobs are the first real compile of every SwiftUI file.
- Large `@ViewBuilder` bodies (SourceDetailView) may hit "expression too complex" on a real compiler.
- IOS-17 targets are not added (needs Xcode, see ios/Unbuilt/README.md).
