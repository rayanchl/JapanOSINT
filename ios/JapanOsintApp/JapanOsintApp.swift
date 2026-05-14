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
    @StateObject private var ws = WebSocketClient()
    @StateObject private var registry = LayerRegistry()
    @StateObject private var saved: SavedStore
    @StateObject private var intelCache: IntelCache
    @StateObject private var collectorFavs = CollectorFavorites()
    @StateObject private var mapNav = MapNavigation()
    @StateObject private var featureStats = FeatureStats()
    @StateObject private var playback = PlaybackState()

    init() {
        let container = AppDataContainer.make()
        self.modelContainer = container
        _settings   = StateObject(wrappedValue: AppSettings(container: container))
        _saved      = StateObject(wrappedValue: SavedStore(container: container))
        _intelCache = StateObject(wrappedValue: IntelCache(container: container))
    }

    var body: some Scene {
        WindowGroup {
            RootView()
                .environmentObject(settings)
                .environmentObject(ws)
                .environmentObject(registry)
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
                    await registry.bootstrap(baseURL: settings.backendBaseURL)
                    ws.connect(baseURL: settings.backendBaseURL)
                }
                .onChange(of: settings.backendBaseURL) { _, newURL in
                    ws.disconnect()
                    ws.connect(baseURL: newURL)
                    Task { await registry.bootstrap(baseURL: newURL) }
                }
        }
    }
}
