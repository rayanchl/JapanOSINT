import Foundation
import SwiftData

// MARK: - Saved features

/// Bookmarked map feature. Replaces the old single-blob UserDefaults JSON
/// (`savedFeatures.v1`) which was unbounded and lost the entire list on a
/// single corrupted write. Each row is its own SQLite tuple — corruption is
/// per-row and indexed lookups by `id` are O(log n).
@Model
final class SavedFeature {
    @Attribute(.unique) var id: String
    var layerId: String
    var displayName: String
    var lat: Double
    var lon: Double
    var imageURL: String?
    /// Encoded `[String: AnyCodable]` — stored as Data because SwiftData
    /// won't natively persist a heterogeneous JSON dict.
    var propertiesJSON: Data
    var savedAt: Date

    init(id: String, layerId: String, displayName: String,
         lat: Double, lon: Double, imageURL: String?,
         propertiesJSON: Data, savedAt: Date) {
        self.id = id
        self.layerId = layerId
        self.displayName = displayName
        self.lat = lat
        self.lon = lon
        self.imageURL = imageURL
        self.propertiesJSON = propertiesJSON
        self.savedAt = savedAt
    }

    convenience init(from item: SavedItem) {
        let json = (try? JSONEncoder().encode(item.properties)) ?? Data()
        self.init(
            id: item.id,
            layerId: item.layerId,
            displayName: item.displayName,
            lat: item.lat, lon: item.lon,
            imageURL: item.imageURL,
            propertiesJSON: json,
            savedAt: item.savedAt
        )
    }

    func toSavedItem() -> SavedItem {
        let props = (try? JSONDecoder().decode([String: AnyCodable].self, from: propertiesJSON)) ?? [:]
        return SavedItem(
            id: id, layerId: layerId, displayName: displayName,
            lat: lat, lon: lon, imageURL: imageURL,
            properties: props, savedAt: savedAt
        )
    }
}

// MARK: - App preferences

/// Singleton row holding every user-tunable setting. Replaces the previous
/// mix of `@AppStorage` scalars and JSON-encoded collections in UserDefaults.
/// Field defaults below are the same values the @AppStorage declarations
/// used so a fresh install behaves identically.
@Model
final class AppPreferences {
    /// A single row is enough — we always fetch by this constant.
    @Attribute(.unique) var key: String

    var backendBaseURL: String
    var appThemeRaw: String

    /// Renamed from `liveVehiclesEnabled` when planes/ships moved off the
    /// live overlay. `originalName:` lets SwiftData map the existing column
    /// to the new property — without it, opening an existing store throws
    /// `loadIssueModelContainer` because the schema diff looks destructive.
    @Attribute(originalName: "liveVehiclesEnabled")
    var liveCarriagesEnabled: Bool
    var liveTrainsEnabled: Bool
    var liveSubwaysEnabled: Bool
    var liveBusesEnabled: Bool

    var maxFeaturesPerLayer: Int
    var maxLinesPolygonsPerLayer: Int

    var cameraRefreshSeconds: Int
    var departuresRefreshSeconds: Int
    var apiDefaultTimeoutSeconds: Int

    var followLogMaxEntries: Int
    var dbTablePageSize: Int
    var departuresShown: Int

    var translateButtonEnabled: Bool
    var translateTargetLanguageRaw: String
    /// When true, the Intel-tab and map-search bars run the user's query in
    /// both English and Japanese via Apple's on-device translation and merge
    /// the results. Defaults to true so the feature is discoverable.
    var autoTranslateSearch: Bool = true
    /// When true, every Japanese title/field gets a romaji transcription
    /// appended (under the line for titles, inline-parenthesized for smaller
    /// fields). Defaults to false so quiet UI stays the norm.
    var showRomaji: Bool = false

    /// JSON-encoded `Set<String>`.
    var activeLayerIdsJSON: Data
    /// JSON-encoded `[String: Double]`.
    var layerOpacityJSON: Data
    /// JSON-encoded `Set<String>`.
    var optedOutFollowersJSON: Data

    static let singletonKey = "app.preferences.v1"

    init(key: String = AppPreferences.singletonKey) {
        self.key = key
        self.backendBaseURL = "http://127.0.0.1:4000"
        self.appThemeRaw = "cyberpunk"
        self.liveCarriagesEnabled = false
        self.liveTrainsEnabled = false
        self.liveSubwaysEnabled = false
        self.liveBusesEnabled = false
        self.maxFeaturesPerLayer = 500
        self.maxLinesPolygonsPerLayer = 200
        self.cameraRefreshSeconds = 15
        self.departuresRefreshSeconds = 30
        self.apiDefaultTimeoutSeconds = 25
        self.followLogMaxEntries = 200
        self.dbTablePageSize = 50
        self.departuresShown = 10
        self.translateButtonEnabled = true
        self.translateTargetLanguageRaw = ""
        self.autoTranslateSearch = true
        self.showRomaji = false
        self.activeLayerIdsJSON = Data()
        self.layerOpacityJSON = Data()
        self.optedOutFollowersJSON = Data()
    }
}

// MARK: - Intel cache

/// Cached `/api/intel/sources` row. Stores the raw envelope so the catalogue
/// renders instantly on launch even when offline; refreshed in the background
/// when the API is reachable.
@Model
final class CachedIntelSource {
    @Attribute(.unique) var id: String
    var rawJSON: Data
    var fetchedAt: Date

    init(id: String, rawJSON: Data, fetchedAt: Date) {
        self.id = id
        self.rawJSON = rawJSON
        self.fetchedAt = fetchedAt
    }
}

/// Cached `/api/intel/items` row — keyed by `uid`, partitioned by `sourceId`
/// so per-source views can fetch with a predicate. Pagination still hits the
/// API; the cache only mirrors the most recent page seen.
@Model
final class CachedIntelItem {
    @Attribute(.unique) var uid: String
    var sourceId: String
    var rawJSON: Data
    var fetchedAt: Date

    init(uid: String, sourceId: String, rawJSON: Data, fetchedAt: Date) {
        self.uid = uid
        self.sourceId = sourceId
        self.rawJSON = rawJSON
        self.fetchedAt = fetchedAt
    }
}
