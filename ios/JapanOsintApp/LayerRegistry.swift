import SwiftUI
import Combine

/// Loads and caches the layer catalogue (~129 entries) from /api/layers.
/// Falls back to the last successful response cached in UserDefaults so the
/// app launches usable even when the backend is unreachable.
@MainActor
final class LayerRegistry: ObservableObject {
    @Published private(set) var layers: [LayerDef] = []
    @Published private(set) var isLoading = false
    @Published private(set) var lastError: String?

    private let cacheKey = "layerRegistryCache.v1"

    init() {
        if let data = UserDefaults.standard.data(forKey: cacheKey),
           let cached = try? JSONDecoder().decode([LayerDef].self, from: data) {
            layers = cached
        }
    }

    func bootstrap(baseURL: String) async {
        await reload(baseURL: baseURL)
    }

    func reload(baseURL: String) async {
        let api = API(baseURL: baseURL)
        isLoading = true
        lastError = nil
        defer { isLoading = false }
        do {
            let fresh = try await api.layers().sorted { lhs, rhs in
                if lhs.categoryLabel == rhs.categoryLabel { return lhs.name < rhs.name }
                return lhs.categoryLabel < rhs.categoryLabel
            }
            layers = fresh
            if let data = try? JSONEncoder().encode(fresh) {
                UserDefaults.standard.set(data, forKey: cacheKey)
            }
        } catch {
            lastError = error.localizedDescription
        }
    }

    func layer(for id: String) -> LayerDef? {
        layers.first(where: { $0.id == id })
    }

    /// Friendly display labels for layers whose backend-generated name
    /// (kebab → Title-Case) is too verbose or leaks implementation detail.
    /// Anything not in this map falls back to `LayerDef.name` unchanged.
    private static let displayNameOverrides: [String: String] = [
        "unified-trains": "Trains",
        "unified-subways": "Subways",
        "unified-buses": "Buses",
        "unified-ais-ships": "Ships",
        "unified-port-infra": "Ports",
        "unified-station-footprints": "Station Footprints",
        "unified-airports": "Airports",
        "unified-flights": "Flights",
        "unified-highway": "Expressways",
        "mlit-n05-rail-history": "Abandoned Rail",
    ]

    /// Single source of truth for what label to render in the UI.
    func displayName(for layer: LayerDef) -> String {
        Self.displayNameOverrides[layer.id] ?? layer.name
    }

    /// ID-only variant for surfaces that don't have a `LayerDef` in scope
    /// (e.g. saved cards reconstructing from a stored layer id).
    static func displayName(forId id: String) -> String {
        if let override = displayNameOverrides[id] { return override }
        return id
            .split(separator: "-")
            .map { $0.prefix(1).uppercased() + $0.dropFirst() }
            .joined(separator: " ")
    }

    var byCategory: [(category: String, layers: [LayerDef])] {
        Dictionary(grouping: layers, by: \.categoryLabel)
            .map { ($0.key, $0.value.sorted { $0.name < $1.name }) }
            .sorted { $0.category < $1.category }
    }

    // Semantic color hex strings — kept in sync with ThemePalette.cyberpunk
    // so an icon's tint reads the same regardless of which palette is active
    // (still works in light system theme, just less neon).
    private static let semanticOrange = "#ffb347"  // theme.warning
    private static let semanticGreen  = "#00ff88"  // theme.success / accentAlt
    private static let semanticRed    = "#ff3860"  // theme.danger
    private static let semanticBlue   = "#4fc3f7"  // standard layer-blue
    private static let semanticYellow = "#ffd600"
    private static let semanticBrown  = "#8d6e63"

    /// Per-layer color override. Used for layers whose meaning has an obvious
    /// color (water → blue, fire → red, vegetation → green). Anything not in
    /// this map falls back to the deterministic-hash palette below.
    private static let colorOverrides: [String: String] = [
        // User-requested explicit pins
        "rice-paddies":         semanticOrange,  // carrot
        "tea-zones":            "#2e7d32",       // matcha-leaf dark green
        "red-light-zones":      semanticRed,
        "sento-public-baths":   semanticBlue,
        "vending-machines":     semanticRed,
        "wifi-networks":        semanticBlue,
        "onsen-map":            semanticRed,     // hot water

        // Other obvious semantic pins
        "river":                semanticBlue,
        "water-infra":          semanticBlue,
        "dam-water-level":      semanticBlue,
        "ocean":                semanticBlue,
        "jma-tide":             semanticBlue,
        "jma-ocean-temp":       semanticBlue,
        "jma-ocean-wave":       semanticBlue,
        "nowphas-wave":         semanticBlue,
        "tsunami":              semanticBlue,
        "submarine-cables":     semanticBlue,

        "volcano":              semanticRed,
        "fire-station-map":     semanticRed,
        "earthquake":           semanticRed,
        "jma-intensity":        semanticRed,
        "jshis-seismic":        semanticRed,
        "hi-net":               semanticRed,
        "k-net":                semanticRed,
        "warnings":             semanticRed,
        "hazard":               semanticRed,
        "hazard-map-portal":    semanticRed,
        "drone-nofly":          semanticRed,
        "yakuza-hq":            semanticRed,
        "wanted-persons":       semanticRed,
        "bird-flu-outbreaks":   semanticRed,
        "bear-encounters":      semanticBrown,
        "phone-scam-hotspots":  semanticRed,
        "crime":                semanticRed,
        "kanagawa-police":      semanticRed,
        "jpcert-alerts":        semanticRed,
        "ipa-alerts":           semanticRed,

        "national-parks":       semanticGreen,
        "sakura-front":         "#f8bbd0",  // sakura pink

        "electrical-grid":      semanticYellow,
        "energy":               semanticYellow,
        "ev-charging":          semanticYellow,
        "wind-turbines":        semanticGreen,
        "nuclear-facilities":   semanticYellow,
        "radiation":            semanticYellow,

        "weather":              semanticBlue,
        "air-quality":          semanticBlue,
    ]

    /// A stable color per layer for icon tinting. Uses semantic overrides
    /// when defined, else a deterministic hash of the id over a fixed palette.
    func color(for layerId: String) -> Color {
        if let hex = Self.colorOverrides[layerId] { return Color(hex: hex) }

        let palette: [String] = [
            "#ff4444", "#4fc3f7", "#66bb6a", "#ffb74d", "#ffd600", "#ce93d8",
            "#4dd0e1", "#aed581", "#42a5f5", "#ef5350", "#78909c", "#f06292",
            "#1da1f2", "#ff9800", "#8bc34a", "#26a69a", "#7e57c2", "#bdbdbd"
        ]
        var h: UInt32 = 2_166_136_261
        for byte in layerId.utf8 {
            h ^= UInt32(byte)
            h &*= 16_777_619
        }
        return Color(hex: palette[Int(h % UInt32(palette.count))])
    }

    /// Hand-picked SF Symbol per layer id. Keeps the substring heuristic
    /// below as a fallback for any layer that isn't explicitly mapped, so
    /// newly added layers still get a sensible icon.
    private static let symbolOverrides: [String: String] = [
        // Environment / weather / seismic
        "weather":             "cloud.sun.fill",
        "earthquake":          "waveform.path.ecg",
        "jma-intensity":       "waveform.path.ecg",
        "jma-ocean-temp":      "thermometer.medium",
        "jma-ocean-wave":      "water.waves",
        "jma-tide":            "water.waves",
        "nowphas-wave":        "water.waves",
        "hi-net":              "waveform.path.badge.plus",
        "k-net":               "waveform.path",
        "jshis-seismic":       "waveform.path.ecg",
        "volcano":             "flame.fill",
        "tsunami":             "water.waves",
        "bear-encounters":     "pawprint.fill",
        "bird-flu-outbreaks":  "bird.fill",
        "sakura-front":        "leaf.fill",
        "wdcgg-co2":           "smoke.fill",
        "air-quality":         "wind",
        "radiation":           "atom",
        "ocean":               "water.waves",
        "warnings":            "exclamationmark.triangle.fill",

        // Safety / hazard / crime
        "hazard":              "exclamationmark.triangle.fill",
        "hazard-map-portal":   "exclamationmark.triangle.fill",
        "bosai-shelter":       "house.lodge.fill",
        "aed-map":             "heart.text.square.fill",
        "koban-map":           "shield.lefthalf.filled",
        "fire-station-map":    "flame.fill",
        "crime":               "exclamationmark.shield.fill",
        "emergency":           "cross.case.fill",
        "drone-nofly":         "airplane.circle.fill",
        "jcg-patrol":          "shield.righthalf.filled",
        "phone-scam-hotspots": "phone.badge.waveform.fill",
        "wanted-persons":      "person.fill.questionmark",
        "yakuza-hq":           "exclamationmark.shield.fill",
        "red-light-zones":     "light.beacon.max.fill",
        "kanagawa-police":     "shield.lefthalf.filled",

        // Transport
        "transport":                  "tram.fill",
        "full-transport":             "tram.fill",
        "unified-trains":             "tram.fill",
        "unified-subways":            "tram.fill",
        "unified-buses":              "bus",
        "unified-ais-ships":          "ferry.fill",
        "unified-port-infra":         "ferry.fill",
        "unified-stations":           "tram.fill",
        "unified-station-footprints": "square.dashed",
        "unified-airports":           "airplane",
        "unified-flights":            "airplane",
        "unified-highway":            "car.2.fill",
        "highway-traffic":            "car.2.fill",
        "jartic-traffic":             "car.2.fill",
        "bus-routes":                 "bus.fill",
        "ferry-routes":               "ferry.fill",
        "lighthouse-map":             "light.beacon.max",
        "haneda-flights":             "airplane.departure",
        "narita-flights":             "airplane.departure",
        "kansai-flights":             "airplane.departure",
        "mlit-n02-stations":          "tram.fill",
        "mlit-n05-rail-history":      "tram.fill",
        "mlit-n07-bus-routes":        "bus.fill",
        "mlit-p02-airports":          "airplane",
        "mlit-p11-bus-stops":         "bus",
        "mlit-c02-ports":             "ferry.fill",
        "osm-transport-buses":        "bus",
        "osm-transport-subways":      "tram.fill",
        "osm-transport-trains":       "tram.fill",
        "osm-transport-ports":        "ferry.fill",
        "maritime":                   "ferry.fill",
        "maritime-ais":               "ferry.fill",
        "marine-traffic":             "ferry.fill",
        "vessel-finder":              "ferry.fill",
        "gtfs-jp":                    "bus.doubledecker",
        "aviation":                   "airplane",
        "flight-adsb":                "airplane",

        // Infrastructure / energy / utilities
        "infrastructure":          "wrench.and.screwdriver.fill",
        "electrical-grid":         "bolt.fill",
        "gas-network":             "flame.fill",
        "water-infra":             "drop.fill",
        "cell-towers":             "antenna.radiowaves.left.and.right",
        "5g-coverage":             "antenna.radiowaves.left.and.right",
        "nuclear-facilities":      "atom",
        "gas-stations":            "fuelpump.fill",
        "ev-charging":             "bolt.car.fill",
        "dam-water-level":         "drop.triangle.fill",
        "internet-exchanges":      "server.rack",
        "submarine-cables":        "cable.connector",
        "data-centers":            "server.rack",
        "petroleum-stockpile":     "drop.degreesign",
        "wind-turbines":           "wind",
        "amateur-radio-repeaters": "dot.radiowaves.left.and.right",
        "famous-places":           "star.fill",
        "telecom":                 "antenna.radiowaves.left.and.right",
        "energy":                  "bolt.fill",
        "parking-facilities":      "parkingsign.square.fill",
        "river":                   "drop.fill",

        // Industry
        "auto-plants":        "car.side.fill",
        "steel-mills":        "hammer.fill",
        "petrochemical":      "flask.fill",
        "refineries":         "flask.fill",
        "semiconductor-fabs": "cpu.fill",
        "shipyards":          "ferry.fill",

        // Health
        "hospital-map": "cross.case.fill",
        "pharmacy-map": "pills.fill",
        "health":       "cross.case.fill",

        // Government
        "government-buildings": "building.columns.fill",
        "city-halls":           "building.columns.fill",
        "courts-prisons":       "scale.3d",
        "embassies":            "flag.fill",
        "houjin-bangou":        "doc.text.magnifyingglass",

        // Defense
        "jsdf-bases":           "shield.fill",
        "usfj-bases":           "flag.checkered",
        "radar-sites":          "dot.radiowaves.up.forward",
        "coast-guard-stations": "ferry.fill",
        "radar":                "dot.radiowaves.up.forward",

        // Marketplace / commercial / leisure
        "classifieds":          "tag.fill",
        "real-estate":          "house.fill",
        "job-boards":           "briefcase.fill",
        "mlit-transaction":     "yensign.circle.fill",
        "convenience-stores":   "cart.fill",
        "tabelog-restaurants":  "fork.knife",
        "mercari-trending":     "bag.fill",
        "suumo-rental-density": "house.fill",
        "karaoke-chains":       "mic.fill",
        "pachinko-density":     "circle.dotted",
        "manga-net-cafes":      "book.fill",
        "themed-cafes":         "cup.and.saucer.fill",
        "vending-machines":     "cabinet.fill",
        "onsen-map":            "thermometer.variable.and.figure",
        "sento-public-baths":   "drop.fill",
        "ski-resorts":          "snowflake",
        "racetracks":           "flag.checkered",
        "stadiums":             "sportscourt.fill",

        // Tourism / culture
        "shrine-temple":    "building.columns.fill",
        "castles":          "building.fill",
        "museums":          "books.vertical.fill",
        "anime-pilgrimage": "sparkles",
        "unesco-heritage":  "globe.asia.australia.fill",
        "national-parks":   "tree.fill",

        // Cyber
        "cameras":              "video.fill",
        "shodan-iot":           "network",
        "insecam-webcams":      "video.fill",
        "wifi-networks":        "wifi",
        "censys-japan":         "network",
        "nicter-darknet":       "moon.stars.fill",
        "nicter-stats":         "chart.bar.fill",
        "tor-exit-nodes":       "globe.badge.chevron.backward",
        "google-dorking":       "magnifyingglass",
        "fofa-jp":              "magnifyingglass.circle.fill",
        "quake360-jp":          "magnifyingglass.circle.fill",
        "urlscan-jp":           "link.circle.fill",
        "wayback-jp":           "clock.arrow.circlepath",
        "github-leaks-jp":      "terminal.fill",
        "grayhat-buckets":      "externaldrive.fill",
        "ipa-alerts":           "exclamationmark.shield.fill",
        "jpcert-alerts":        "exclamationmark.shield",
        "certstream-jp":        "lock.shield.fill",
        "strava-heatmap-bases": "figure.run",
        "greynoise-jp":         "network.badge.shield.half.filled",
        "cyber":                "network",
        "camera-discovery":     "video.fill",

        // Social / news
        "social":            "bubble.left.and.bubble.right.fill",
        "twitter-geo":       "bubble.left.and.bubble.right.fill",
        "facebook-geo":      "bubble.left.fill",
        "chan-5ch":          "bubble.left.and.bubble.right",
        "bird-makeup-jp":    "bird.fill",
        "misskey-timeline":  "bubble.left.fill",
        "note-com-trending": "square.and.pencil",
        "hatena-bookmark":   "bookmark.fill",
        "gdelt":             "globe.americas.fill",
        "news-feed":         "newspaper.fill",

        // Statistics / economy
        "estat-census":     "person.3.fill",
        "resas-population": "person.3.fill",
        "resas-tourism":    "suitcase.fill",
        "resas-industry":   "building.2.fill",
        "population":       "person.3.fill",
        "economy":          "yensign.circle.fill",
        "edinet-filings":   "doc.text.fill",
        "landprice":        "yensign.square.fill",
        "landuse":          "map.fill",

        // Geospatial / basemap
        "basemap":          "map.fill",
        "elevation":        "mountain.2.fill",
        "geocode":          "mappin.and.ellipse",
        "google-my-maps":   "mappin.circle.fill",
        "admin-boundaries": "square.dashed.inset.filled",
        "poi":              "mappin.and.ellipse",

        // Agriculture / food
        "tea-zones":          "leaf.fill",
        "rice-paddies":       "carrot.fill",
        "wineries-craftbeer": "wineglass.fill",
        "sake-breweries":     "wineglass.fill",
        "wagyu-ranches":      "fork.knife",
        "fish-markets":       "fish.fill",

        // Misc
        "japan-post-offices": "envelope.fill",
        "manhole-covers":     "circle.grid.cross.fill",

        // Satellite
        "satellite":                 "globe.asia.australia.fill",
        "satellite-ground-stations": "dot.radiowaves.left.and.right",
        "satellite-imagery":         "photo.fill",
        "satellite-tracking":        "scope",
    ]

    /// SF Symbol per layer. Tries the explicit map first, then falls back
    /// to substring heuristics so newly added (un-mapped) layers still get
    /// a sensible icon.
    func symbol(for layerId: String) -> String {
        if let mapped = Self.symbolOverrides[layerId] { return mapped }

        let id = layerId.lowercased()
        // Intel-source heuristics first — these need to win over more
        // generic substring matches below (e.g. "phishing" is more specific
        // than a fallthrough to a generic icon).
        if id.contains("phish") { return "envelope.badge.fill" }
        if id.contains("cve") || id.contains("ghsa") || id.contains("vuln") || id.contains("trickest") { return "ladybug.fill" }
        if id.contains("malware") || id.contains("ioc") || id.contains("urlhaus") || id.contains("threatfox") { return "shield.lefthalf.filled" }
        if id.contains("certstream") || id.contains("crtsh") { return "lock.shield.fill" }
        if id.contains("leak") || id.contains("buckets") || id.contains("grayhat") { return "doc.text.magnifyingglass" }
        if id.contains("github") || id.contains("poc") { return "hammer.fill" }
        if id.contains("rss") || id.contains("news") || id.contains("nhk") { return "newspaper.fill" }
        if id.contains("protest") || id.contains("tmp-protest") { return "megaphone.fill" }
        if id.contains("edinet") || id.contains("filing") || id.contains("disclosure") { return "doc.richtext" }
        if id.contains("censys") || id.contains("greynoise") || id.contains("fofa") || id.contains("quake") { return "network" }

        if id.contains("earthquake") || id.contains("seismic") { return "waveform.path.ecg" }
        if id.contains("plane") || id.contains("flight") || id.contains("adsb") { return "airplane" }
        if id.contains("ship") || id.contains("ais") || id.contains("marine") || id.contains("vessel") { return "ferry" }
        if id.contains("train") || id.contains("subway") || id.contains("transit") || id.contains("station") { return "tram.fill" }
        if id.contains("bus") { return "bus" }
        if id.contains("camera") { return "video.fill" }
        if id.contains("nuclear") || id.contains("radiation") { return "atom" }
        if id.contains("weather") { return "cloud.sun.fill" }
        if id.contains("crime") || id.contains("police") || id.contains("koban") { return "shield.lefthalf.filled" }
        if id.contains("hospital") || id.contains("health") || id.contains("ndb") { return "cross.case.fill" }
        if id.contains("tower") || id.contains("cell") || id.contains("5g") { return "antenna.radiowaves.left.and.right" }
        if id.contains("food") || id.contains("restaurant") { return "fork.knife" }
        if id.contains("hotel") || id.contains("tourism") { return "bed.double.fill" }
        if id.contains("river") || id.contains("water") || id.contains("flood") { return "drop.fill" }
        if id.contains("shrine") || id.contains("temple") { return "building.columns.fill" }
        if id.contains("anime") || id.contains("pilgrimage") { return "sparkles" }
        if id.contains("yakuza") || id.contains("crime") { return "exclamationmark.triangle.fill" }
        if id.contains("tor") || id.contains("shodan") || id.contains("iot") { return "network" }
        if id.contains("real-estate") || id.contains("rent") || id.contains("land") || id.contains("price") { return "house.fill" }
        if id.contains("population") || id.contains("density") { return "person.3.fill" }
        if id.contains("satellite") || id.contains("himawari") { return "globe.asia.australia.fill" }
        if id.contains("solar") || id.contains("turbine") || id.contains("power") || id.contains("grid") || id.contains("ev") { return "bolt.fill" }
        if id.contains("data-center") || id.contains("datacenter") { return "server.rack" }
        if id.contains("twitter") || id.contains("x-geo") || id.contains("social") || id.contains("facebook") { return "bubble.left.and.bubble.right.fill" }
        if id.contains("classified") || id.contains("market") || id.contains("job") { return "tag.fill" }
        if id.contains("buildings") || id.contains("plateau") { return "building.2.fill" }
        if id.contains("shelter") || id.contains("bosai") { return "house.lodge.fill" }
        if id.contains("aed") { return "heart.text.square.fill" }
        if id.contains("jsdf") || id.contains("base") || id.contains("defense") { return "scope" }
        return "mappin.and.ellipse"
    }
}
