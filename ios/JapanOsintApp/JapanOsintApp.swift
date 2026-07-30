import SwiftUI
import SwiftData
import CoreSpotlight

@main
struct JapanOsintApp: App {
    @Environment(\.scenePhase) private var scenePhase
    /// Shared SwiftData stack — built once and threaded through every
    /// persistence-aware store via constructor injection. The `modelContainer`
    /// view modifier on `RootView` exposes the same container to any view
    /// using `@Query` or `@Environment(\.modelContext)`.
    private let modelContainer: ModelContainer

    @StateObject private var settings: AppSettings
    @StateObject private var auth: AuthSession
    @StateObject private var apiClient: APIClient
    @StateObject private var ws = WebSocketClient()
    @StateObject private var registry = LayerRegistry()
    @StateObject private var layerCache = LayerFeatureCache()
    @StateObject private var saved: SavedStore
    @StateObject private var intelCache: IntelCache
    @StateObject private var collectorFavs = CollectorFavorites()
    @StateObject private var mapNav = MapNavigation()
    @StateObject private var featureStats = FeatureStats()
    @StateObject private var playback = PlaybackState()

    @StateObject private var cases: CaseStore

    init() {
        let container = AppDataContainer.make()
        self.modelContainer = container
        let appSettings = AppSettings(container: container)
        // CaseStore needs the SAME APIClient instance the rest of the app
        // uses (it carries the auth/tenant headers), so build it into a local
        // first rather than constructing a second client.
        let client = APIClient(settings: appSettings)
        _settings   = StateObject(wrappedValue: appSettings)
        _auth       = StateObject(wrappedValue: AuthSession(settings: appSettings))
        _apiClient  = StateObject(wrappedValue: client)
        _cases      = StateObject(wrappedValue: CaseStore(apiClient: client))
        _saved      = StateObject(wrappedValue: SavedStore(container: container))
        _intelCache = StateObject(wrappedValue: IntelCache(container: container))

        // BGTask handlers MUST be registered before launch completes (roadmap
        // 37). The closure resolves the current API at run time so it tracks a
        // later backend-URL change. No-ops cleanly until the Background Modes
        // capability + Info.plist identifiers are in place.
        BackgroundRefresh.register(apiProvider: { client.api })
    }

    var body: some Scene {
        WindowGroup {
            RootView()
                .environmentObject(settings)
                .environmentObject(auth)
                .environmentObject(apiClient)
                .environmentObject(ws)
                .environmentObject(registry)
                .environmentObject(layerCache)
                .environmentObject(saved)
                .environmentObject(cases)
                .environmentObject(intelCache)
                .environmentObject(collectorFavs)
                .environmentObject(mapNav)
                .environmentObject(featureStats)
                .environmentObject(playback)
                .environmentObject(IntentRouter.shared)
                .environment(\.theme, settings.appTheme.palette)
                .preferredColorScheme(settings.appTheme.colorScheme)
                .tint(settings.appTheme.palette.accent)
                // Cyberpunk: every glyph in the app uses the monospaced
                // design (SF Mono). Cascades to every child Text unless an
                // individual view overrides with `.fontDesign(.default)`.
                // System theme leaves prose alone — only explicit data sites
                // monospace via `.monospacedDigit()` / `Font.system(_, design:
                // .monospaced)` at the call site.
                .fontDesign(settings.appTheme.palette.monospaceAll ? .monospaced : .default)
                .modelContainer(modelContainer)
                .task {
                    // Resolve the auth gate only. The actual fan-out to the
                    // /api/* surfaces (registry bootstrap + WS connect) is
                    // driven from EXACTLY ONE place — the gate→.ready
                    // `onChange` below. `gate` starts at `.loading`, so the
                    // loading→ready transition always fires that handler;
                    // doing the connect here too would double-connect the
                    // socket and double-bootstrap the registry on launch.
                    await auth.bootstrap()
                }
                .onChange(of: auth.gate) { _, gate in
                    // Single source of truth for post-gate fan-out. Fires on
                    // the launch loading→ready transition and on any later
                    // onboarding→ready (fresh sign-in) transition.
                    guard gate == .ready else { return }
                    ws.connect(baseURL: settings.backendBaseURL)
                    Task { await registry.bootstrap(baseURL: settings.backendBaseURL) }
                    onReady()
                }
                // App Intents / Siri / Shortcuts / share-queue drain hand off
                // here (roadmap 36); the shell switches tabs and runs the work.
                .onReceive(IntentRouter.shared.$pending) { dispatchIntent($0) }
                // Permalinks (roadmap 38) and the share extension's deep link
                // (roadmap 36) both arrive as URLs.
                .onOpenURL { handleOpenURL($0) }
                // A Spotlight hit re-enters here with the item's identifier.
                .onContinueUserActivity(CSSearchableItemActionType) { handleSpotlight($0) }
                .onChange(of: scenePhase) { _, phase in
                    guard phase == .active else { return }
                    // Coming to the foreground: pick up anything the share
                    // extension queued, refresh the widget snapshot, and
                    // re-arm the background series.
                    guard auth.gate == .ready else { return }
                    drainShareQueue()
                    Task { await WidgetSnapshotBuilder.refresh(api: apiClient.api) }
                    BackgroundRefresh.scheduleRefresh()
                }
                .onChange(of: settings.backendBaseURL) { _, newURL in
                    guard auth.gate == .ready else { return }
                    // Cached GeoJSON belongs to the previous deployment.
                    layerCache.clearAll()
                    ws.disconnect()
                    ws.connect(baseURL: newURL)
                    Task { await registry.bootstrap(baseURL: newURL) }
                }
        }
        #if os(macOS)
        // Launch at a desktop-appropriate size; the shell pins the minimum.
        // `.contentMinSize` lets the user shrink the window down to the shell's
        // declared minimum frame but no further.
        .defaultSize(width: 1280, height: 832)
        .windowResizability(.contentMinSize)
        #endif
    }

    // MARK: - Platform integration (roadmap 36/37)

    /// Fired once the auth gate reaches `.ready`. Seeds the widget snapshot,
    /// (re)builds the Spotlight index, and arms background refresh. All three
    /// no-op cleanly until their capabilities are configured in Xcode.
    @MainActor
    private func onReady() {
        Task { await WidgetSnapshotBuilder.refresh(api: apiClient.api) }
        SpotlightIndexer.index(cases: cases.cases, saved: saved.items)
        BackgroundRefresh.scheduleRefresh()
        BackgroundRefresh.scheduleSync()
    }

    /// Route an intent action into the running UI, then clear it so it can't
    /// replay on the next publish.
    @MainActor
    private func dispatchIntent(_ action: IntentAction?) {
        guard let action else { return }
        switch action {
        case .search(let q):            mapNav.showSearch(q)
        case .exposure(let identifier): mapNav.showSearch(identifier)
        }
        IntentRouter.shared.pending = nil
    }

    /// Share-extension deep link → drain the queue; permalink → resolve and, for
    /// a search permalink, route to the Search tab. Full permalink state
    /// application (map camera + layers + time window) is a later pass.
    @MainActor
    private func handleOpenURL(_ url: URL) {
        if url.scheme?.lowercased() == "japanosint",
           url.host?.lowercased() == "share" {
            drainShareQueue()
            return
        }
        if let token = PermalinkResolver.token(from: url) {
            Task { @MainActor in
                if let state = try? await PermalinkResolver.resolve(token, api: apiClient.api),
                   let q = (state["q"]?.value as? String)
                        ?? (state["query"]?.value as? String),
                   !q.isEmpty {
                    mapNav.showSearch(q)
                }
            }
        }
    }

    /// Consume queued share items. URL/text start a search; images are left
    /// queued because the on-device media pipeline (roadmap 27) isn't present
    /// yet — dropping what the user shared would be worse than deferring it.
    @MainActor
    private func drainShareQueue() {
        for item in ShareQueue.pending() {
            switch item.kind {
            case .url, .text:
                mapNav.showSearch(item.value)
                ShareQueue.remove(item)
            case .image:
                continue
            }
        }
    }

    /// Open the case or saved item behind a Spotlight hit.
    @MainActor
    private func handleSpotlight(_ activity: NSUserActivity) {
        guard let uid = activity.userInfo?[CSSearchableItemActivityIdentifier] as? String,
              let route = SpotlightIndexer.route(uid) else { return }
        switch route.domain {
        case SpotlightIndexer.caseDomain:
            mapNav.selectedTab = AppTab.cases.rawValue
            cases.activeCaseId = route.id
        case SpotlightIndexer.savedDomain:
            mapNav.consolePath = [.saved]
            mapNav.selectedTab = AppTab.console.rawValue
        default:
            break
        }
    }
}
