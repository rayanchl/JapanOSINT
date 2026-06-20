import SwiftUI
import SwiftData

@main
struct JapanOsintApp: App {
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

    init() {
        let container = AppDataContainer.make()
        self.modelContainer = container
        let appSettings = AppSettings(container: container)
        _settings   = StateObject(wrappedValue: appSettings)
        _auth       = StateObject(wrappedValue: AuthSession(settings: appSettings))
        _apiClient  = StateObject(wrappedValue: APIClient(settings: appSettings))
        _saved      = StateObject(wrappedValue: SavedStore(container: container))
        _intelCache = StateObject(wrappedValue: IntelCache(container: container))
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
                .environmentObject(intelCache)
                .environmentObject(collectorFavs)
                .environmentObject(mapNav)
                .environmentObject(featureStats)
                .environmentObject(playback)
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
}
