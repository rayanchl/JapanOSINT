import Foundation
import Combine
import CoreLocation

/// Tab indices for the top-level shell. Stored as Int on `selectedTab` so the
/// existing `Int` binding stays — the enum is just there to keep references
/// readable (no more `selectedTab = 9` magic-number lines).
enum AppTab: Int { case map = 0, intel, saved, console }

/// Destinations inside the Console hub (the catch-all tab for everything that
/// isn't Map / Intel / Saved). The hub uses a `NavigationStack(path:)` so
/// cross-tab actions like "open this API key" can deep-link straight in.
enum ConsoleDestination: Hashable {
    case sources, database, scheduler, cameras, followLog, apiKeys, settings
}

/// Cross-tab coordinator. Tabs other than Map (Camera Discovery, etc.) push
/// a coordinate via `showOnMap(_:)` which switches to the Map tab and lets
/// `MapTab` consume `pendingFlyTo` to animate the camera.
@MainActor
final class MapNavigation: ObservableObject {
    @Published var selectedTab: Int = AppTab.map.rawValue
    @Published var consolePath: [ConsoleDestination] = []
    @Published var pendingFlyTo: CLLocationCoordinate2D?
    /// When non-nil, the Map tab consumes this on arrival and presents the
    /// matching feature popup. Lets cross-tab "Show on map" actions show the
    /// point's info even if the underlying data layer isn't toggled on.
    @Published var pendingPresent: GeoFeature?
    /// When non-nil, the Sources tab observes this and opens the
    /// matching SourceDetail sheet (also expanding its parent collector).
    /// Cleared by the consumer after the sheet is presented.
    @Published var pendingSourceId: String?
    /// When non-nil, the API Keys tab observes this and opens the matching
    /// ApiKeyDetailView sheet. Cleared by the consumer after presentation.
    @Published var pendingApiKeyName: String?

    func showOnMap(_ coord: CLLocationCoordinate2D, feature: GeoFeature? = nil) {
        pendingPresent = feature
        pendingFlyTo = coord
        selectedTab = AppTab.map.rawValue
    }

    /// Switch to the Console tab and push Sources, then publish the pending
    /// source id. Order matters: pushing first means SourceDashboardTab is
    /// mounted before the `pendingSourceId` value lands, so its `.task`
    /// consumption sees the value on first appear.
    func showSource(_ sourceId: String) {
        selectedTab = AppTab.console.rawValue
        consolePath = [.sources]
        pendingSourceId = sourceId
    }

    func showApiKey(_ name: String) {
        selectedTab = AppTab.console.rawValue
        consolePath = [.apiKeys]
        pendingApiKeyName = name
    }
}
