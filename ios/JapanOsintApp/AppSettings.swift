import SwiftUI
import Combine
import SwiftData

enum AppTheme: String, CaseIterable, Identifiable {
    case cyberpunk
    case system

    var id: String { rawValue }
    var label: String {
        switch self {
        case .cyberpunk: return "Cyberpunk"
        case .system:    return "System"
        }
    }
    var palette: ThemePalette {
        switch self {
        case .cyberpunk: return .cyberpunk
        case .system:    return .system
        }
    }
    var colorScheme: ColorScheme? {
        switch self {
        case .cyberpunk: return .dark
        case .system:    return nil
        }
    }
}

/// SwiftData-backed app settings. Replaces the previous mix of `@AppStorage`
/// scalars and JSON-encoded UserDefaults collections with a single
/// `AppPreferences` row plus published mirrors so SwiftUI bindings still work
/// (`$settings.backendBaseURL`).
///
/// One-shot migration on first launch copies every legacy key out of
/// UserDefaults; after that the store is authoritative.
///
/// The single exception is `apiDefaultTimeoutSeconds`: `API.userDefaultTimeout`
/// is a `nonisolated` static that can't reach the MainActor model context, so
/// every write also mirrors that value back to UserDefaults.
@MainActor
final class AppSettings: ObservableObject {
    private let context: ModelContext
    private let prefs: AppPreferences

    // MARK: - Scalars (formerly @AppStorage)

    @Published var backendBaseURL: String {
        didSet { write(\.backendBaseURL, backendBaseURL) }
    }
    @Published var appThemeRaw: String {
        didSet { write(\.appThemeRaw, appThemeRaw) }
    }
    /// Was `liveVehiclesEnabled` — covered planes + ships + (fallthrough)
    /// trains/subways/buses. Renamed because planes now flow through the
    /// unified-flights layer toggle (always on when the layer is on); the
    /// flag now controls only the "carriage" dots — animated train / subway
    /// / bus markers riding their static route lines.
    @Published var liveCarriagesEnabled: Bool {
        didSet { write(\.liveCarriagesEnabled, liveCarriagesEnabled) }
    }
    @Published var liveTrainsEnabled: Bool {
        didSet { write(\.liveTrainsEnabled, liveTrainsEnabled) }
    }
    @Published var liveSubwaysEnabled: Bool {
        didSet { write(\.liveSubwaysEnabled, liveSubwaysEnabled) }
    }
    @Published var liveBusesEnabled: Bool {
        didSet { write(\.liveBusesEnabled, liveBusesEnabled) }
    }
    @Published var maxFeaturesPerLayer: Int {
        didSet { write(\.maxFeaturesPerLayer, maxFeaturesPerLayer) }
    }
    @Published var maxLinesPolygonsPerLayer: Int {
        didSet { write(\.maxLinesPolygonsPerLayer, maxLinesPolygonsPerLayer) }
    }
    @Published var cameraRefreshSeconds: Int {
        didSet { write(\.cameraRefreshSeconds, cameraRefreshSeconds) }
    }
    @Published var departuresRefreshSeconds: Int {
        didSet { write(\.departuresRefreshSeconds, departuresRefreshSeconds) }
    }
    @Published var apiDefaultTimeoutSeconds: Int {
        didSet {
            write(\.apiDefaultTimeoutSeconds, apiDefaultTimeoutSeconds)
            // Mirrored to UserDefaults so `API.userDefaultTimeout`
            // (nonisolated static) can read it without a model context.
            UserDefaults.standard.set(apiDefaultTimeoutSeconds, forKey: "apiDefaultTimeoutSeconds")
        }
    }
    @Published var followLogMaxEntries: Int {
        didSet { write(\.followLogMaxEntries, followLogMaxEntries) }
    }
    @Published var dbTablePageSize: Int {
        didSet { write(\.dbTablePageSize, dbTablePageSize) }
    }
    @Published var departuresShown: Int {
        didSet { write(\.departuresShown, departuresShown) }
    }
    @Published var translateButtonEnabled: Bool {
        didSet { write(\.translateButtonEnabled, translateButtonEnabled) }
    }
    @Published var translateTargetLanguageRaw: String {
        didSet { write(\.translateTargetLanguageRaw, translateTargetLanguageRaw) }
    }
    @Published var autoTranslateSearch: Bool {
        didSet { write(\.autoTranslateSearch, autoTranslateSearch) }
    }
    @Published var showRomaji: Bool {
        didSet { write(\.showRomaji, showRomaji) }
    }

    var appTheme: AppTheme {
        get { AppTheme(rawValue: appThemeRaw) ?? .cyberpunk }
        set { appThemeRaw = newValue.rawValue }
    }

    // MARK: - Collections

    @Published var activeLayerIds: Set<String> {
        didSet { writeJSON(activeLayerIds, into: \.activeLayerIdsJSON) }
    }

    /// Per-layer opacity (0...1). Missing entries default to 1.0.
    @Published var layerOpacity: [String: Double] {
        didSet { writeJSON(layerOpacity, into: \.layerOpacityJSON) }
    }

    /// Followers the user has explicitly opted out of (e.g. unchecked
    /// "Show subways" inside the unified-trains expanded panel). When a
    /// parent toggle goes ON we honor the opt-out by NOT adding these.
    @Published var optedOutFollowers: Set<String> {
        didSet { writeJSON(optedOutFollowers, into: \.optedOutFollowersJSON) }
    }

    // MARK: - Init

    init(container: ModelContainer) {
        let ctx = container.mainContext
        let row = Self.fetchOrCreate(in: ctx)
        self.context = ctx
        self.prefs = row

        // Hydrate all @Published from the persisted row. Assigning to a
        // didSet'd property does fire the observer once (writing the same
        // value back), but the in-memory row + context are already up to
        // date so this is a no-op cost.
        self.backendBaseURL            = row.backendBaseURL
        self.appThemeRaw               = row.appThemeRaw
        self.liveCarriagesEnabled      = row.liveCarriagesEnabled
        self.liveTrainsEnabled         = row.liveTrainsEnabled
        self.liveSubwaysEnabled        = row.liveSubwaysEnabled
        self.liveBusesEnabled          = row.liveBusesEnabled
        self.maxFeaturesPerLayer       = row.maxFeaturesPerLayer
        self.maxLinesPolygonsPerLayer  = row.maxLinesPolygonsPerLayer
        self.cameraRefreshSeconds      = row.cameraRefreshSeconds
        self.departuresRefreshSeconds  = row.departuresRefreshSeconds
        self.apiDefaultTimeoutSeconds  = row.apiDefaultTimeoutSeconds
        self.followLogMaxEntries       = row.followLogMaxEntries
        self.dbTablePageSize           = row.dbTablePageSize
        self.departuresShown           = row.departuresShown
        self.translateButtonEnabled    = row.translateButtonEnabled
        self.translateTargetLanguageRaw = row.translateTargetLanguageRaw
        self.autoTranslateSearch       = row.autoTranslateSearch
        self.showRomaji                = row.showRomaji

        self.activeLayerIds = Self.decode(row.activeLayerIdsJSON, default: Set<String>())
        self.layerOpacity   = Self.decode(row.layerOpacityJSON, default: [String: Double]())
        self.optedOutFollowers = Self.decode(row.optedOutFollowersJSON, default: Set<String>())

        // Make sure the nonisolated API-timeout reader sees the current value
        // even if no setter has fired yet this launch.
        UserDefaults.standard.set(self.apiDefaultTimeoutSeconds, forKey: "apiDefaultTimeoutSeconds")

        Self.runOneShotMigrationIfNeeded(into: self)
    }

    // MARK: - Layer helpers (unchanged public API)

    func toggleLayer(_ id: String) {
        if activeLayerIds.contains(id) {
            activeLayerIds.remove(id)
        } else {
            activeLayerIds.insert(id)
        }
        applyHiddenFollowers(triggeredBy: id)
    }

    /// Hidden layers that auto-mirror visible "parent" toggles. Each parent
    /// pulls in its listed followers when enabled; a follower is removed only
    /// when *every* parent that owns it is off (so e.g. station footprints
    /// stay on while either Trains or Subway is on). Users can opt out of a
    /// specific follower via `optedOutFollowers`.
    private static let hiddenFollowers: [String: [String]] = [
        "unified-trains":   ["unified-station-footprints"],
        "unified-subways":  ["unified-station-footprints"],
    ]

    /// Reverse index: follower → set of parents that own it. Built once.
    private static let parentsOf: [String: Set<String>] = {
        var result: [String: Set<String>] = [:]
        for (parent, followers) in hiddenFollowers {
            for f in followers { result[f, default: []].insert(parent) }
        }
        return result
    }()

    /// Followers list for a given parent (UI uses this to know whether to
    /// render the FEATURES section in the expanded layer row).
    func followers(of parentId: String) -> [String] {
        Self.hiddenFollowers[parentId] ?? []
    }

    func followerEnabled(_ followerId: String) -> Bool {
        !optedOutFollowers.contains(followerId)
    }

    /// Toggle a follower's opt-out state. Updates activeLayerIds in lockstep
    /// so the map reflects the choice immediately.
    func toggleFollowerOptOut(_ followerId: String, parentId: String) {
        if optedOutFollowers.contains(followerId) {
            optedOutFollowers.remove(followerId)
            if activeLayerIds.contains(parentId) {
                activeLayerIds.insert(followerId)
            }
        } else {
            optedOutFollowers.insert(followerId)
            activeLayerIds.remove(followerId)
        }
    }

    private func applyHiddenFollowers(triggeredBy id: String) {
        guard let followers = Self.hiddenFollowers[id] else { return }
        for follower in followers {
            if activeLayerIds.contains(id) {
                if !optedOutFollowers.contains(follower) {
                    activeLayerIds.insert(follower)
                }
            } else {
                let stillOwned = (Self.parentsOf[follower] ?? [])
                    .contains { $0 != id && activeLayerIds.contains($0) }
                if !stillOwned { activeLayerIds.remove(follower) }
            }
        }
    }

    func opacity(for id: String) -> Double { layerOpacity[id] ?? 1.0 }

    func setOpacity(_ value: Double, for id: String) {
        layerOpacity[id] = max(0, min(1, value))
    }

    func enableAll(_ ids: [String]) {
        activeLayerIds.formUnion(ids)
    }
    func disableAll() {
        activeLayerIds.removeAll()
    }

    /// Wipe the persisted layer registry cache so the next launch fetches fresh.
    /// Caller is responsible for triggering an immediate `LayerRegistry.reload`
    /// if a refresh is also wanted right now.
    func clearLayerRegistryCache() {
        UserDefaults.standard.removeObject(forKey: "layerRegistryCache.v1")
    }

    // MARK: - SwiftData plumbing

    private func write<T>(_ keyPath: ReferenceWritableKeyPath<AppPreferences, T>, _ value: T) {
        prefs[keyPath: keyPath] = value
        save()
    }

    private func writeJSON<T: Encodable>(_ value: T,
                                         into keyPath: ReferenceWritableKeyPath<AppPreferences, Data>) {
        let data = (try? JSONEncoder().encode(value)) ?? Data()
        prefs[keyPath: keyPath] = data
        save()
    }

    private func save() {
        do { try context.save() }
        catch { /* single-row writes shouldn't take down the UI */ }
    }

    private static func decode<T: Decodable>(_ data: Data, default fallback: T) -> T {
        guard !data.isEmpty else { return fallback }
        return (try? JSONDecoder().decode(T.self, from: data)) ?? fallback
    }

    private static func fetchOrCreate(in context: ModelContext) -> AppPreferences {
        let key = AppPreferences.singletonKey
        let descriptor = FetchDescriptor<AppPreferences>(
            predicate: #Predicate { $0.key == key }
        )
        if let existing = try? context.fetch(descriptor).first {
            return existing
        }
        let row = AppPreferences()
        context.insert(row)
        try? context.save()
        return row
    }

    // MARK: - One-shot UserDefaults migration

    /// On first launch after the SwiftData migration ships, copy every legacy
    /// key out of UserDefaults into the new store. Subsequent launches skip
    /// this entirely. Old keys are left intact so a downgrade still works.
    private static func runOneShotMigrationIfNeeded(into settings: AppSettings) {
        let defaults = UserDefaults.standard
        let migrationKey = "appSettings.migratedToSwiftData.v1"
        guard !defaults.bool(forKey: migrationKey) else { return }
        defer { defaults.set(true, forKey: migrationKey) }

        // Scalars
        if let v = defaults.string(forKey: "backendBaseURL") { settings.backendBaseURL = v }
        if let v = defaults.string(forKey: "appThemeRaw")    { settings.appThemeRaw = v }
        // Pre-rename pull: legacy `liveVehiclesEnabled` covered the same
        // territory as the new `liveCarriagesEnabled` (it gated trains /
        // subways / buses already; planes/ships were the part that didn't
        // belong). Carry the value over so users keep their setting.
        if defaults.object(forKey: "liveVehiclesEnabled") != nil {
            settings.liveCarriagesEnabled = defaults.bool(forKey: "liveVehiclesEnabled")
        }
        if defaults.object(forKey: "liveTrainsEnabled") != nil {
            settings.liveTrainsEnabled = defaults.bool(forKey: "liveTrainsEnabled")
        }
        if defaults.object(forKey: "liveSubwaysEnabled") != nil {
            settings.liveSubwaysEnabled = defaults.bool(forKey: "liveSubwaysEnabled")
        }
        if defaults.object(forKey: "liveBusesEnabled") != nil {
            settings.liveBusesEnabled = defaults.bool(forKey: "liveBusesEnabled")
        }
        if let v = positiveInt(defaults, "mapMaxFeaturesPerLayer") {
            settings.maxFeaturesPerLayer = v
        }
        if let v = positiveInt(defaults, "mapMaxLinesPolygonsPerLayer") {
            settings.maxLinesPolygonsPerLayer = v
        }
        if let v = positiveInt(defaults, "cameraRefreshSeconds") {
            settings.cameraRefreshSeconds = v
        }
        if let v = positiveInt(defaults, "departuresRefreshSeconds") {
            settings.departuresRefreshSeconds = v
        }
        if let v = positiveInt(defaults, "apiDefaultTimeoutSeconds") {
            settings.apiDefaultTimeoutSeconds = v
        }
        if let v = positiveInt(defaults, "followLogMaxEntries") {
            settings.followLogMaxEntries = v
        }
        if let v = positiveInt(defaults, "dbTablePageSize") {
            settings.dbTablePageSize = v
        }
        if let v = positiveInt(defaults, "departuresShown") {
            settings.departuresShown = v
        }
        if defaults.object(forKey: "translateButtonEnabled") != nil {
            settings.translateButtonEnabled = defaults.bool(forKey: "translateButtonEnabled")
        }
        if let v = defaults.string(forKey: "translateTargetLanguageRaw") {
            settings.translateTargetLanguageRaw = v
        }
        if defaults.object(forKey: "autoTranslateSearch") != nil {
            settings.autoTranslateSearch = defaults.bool(forKey: "autoTranslateSearch")
        }
        if defaults.object(forKey: "showRomaji") != nil {
            settings.showRomaji = defaults.bool(forKey: "showRomaji")
        }

        // Collections
        if let data = defaults.data(forKey: "activeLayerIds"),
           let set = try? JSONDecoder().decode(Set<String>.self, from: data) {
            settings.activeLayerIds = set
        }
        if let data = defaults.data(forKey: "layerOpacity"),
           let dict = try? JSONDecoder().decode([String: Double].self, from: data) {
            settings.layerOpacity = dict
        }
        if let data = defaults.data(forKey: "optedOutFollowers"),
           let set = try? JSONDecoder().decode(Set<String>.self, from: data) {
            settings.optedOutFollowers = set
        }
    }

    private static func positiveInt(_ defaults: UserDefaults, _ key: String) -> Int? {
        // `integer(forKey:)` returns 0 for missing keys, which collides with a
        // legitimately-saved zero. Probe via `object(forKey:)` first.
        guard defaults.object(forKey: key) != nil else { return nil }
        let value = defaults.integer(forKey: key)
        return value > 0 ? value : nil
    }
}
