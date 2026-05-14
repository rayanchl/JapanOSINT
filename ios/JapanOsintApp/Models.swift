import Foundation
import CoreLocation

// ── Bounding box / coordinates ─────────────────────────────────────────────

struct BBox: Codable, Hashable, Sendable {
    let minLng: Double
    let minLat: Double
    let maxLng: Double
    let maxLat: Double

    /// JapanOSINT format: "minLng,minLat,maxLng,maxLat"
    var queryString: String {
        "\(minLng),\(minLat),\(maxLng),\(maxLat)"
    }
}

// ── Layers (from /api/layers) ──────────────────────────────────────────────

struct LayerSourceRef: Codable, Hashable, Identifiable, Sendable {
    let id: String
    let name: String?
    let type: String?
    let free: Bool?
}

/// Time-coded layer columns advertised by the server. Used by the iOS
/// client only to flag features that fell back to `fetched_at` in replay.
struct LayerTemporal: Codable, Hashable, Sendable {
    let field: String
    let fallbackField: String?
}

struct LayerDef: Codable, Identifiable, Hashable, Sendable {
    let id: String
    let name: String
    let category: String?
    let sources: [LayerSourceRef]?

    /// Time-slider disposition emitted by /api/layers:
    ///   `temporal` present → time-coded (slider applies)
    ///   `liveOnly == true` → no historical archive, hidden in replay
    ///   neither present     → static (always rendered, even in replay)
    let temporal: LayerTemporal?
    let liveOnly: Bool?

    /// Backend convention: layer id maps to /api/data/<id> for live collector output.
    var dataEndpoint: String { "/api/data/\(id)" }

    /// Human-friendly category (falls back to "Other").
    var categoryLabel: String { category ?? "Other" }

    /// True when this layer has no historical data (vehicle positions etc.).
    /// Such layers are hidden whenever the time slider is in replay.
    var isLiveOnly: Bool { liveOnly == true }

    /// True for static reference data (boundaries, infra dumps) — rendered
    /// unchanged at every slider position.
    var isStatic: Bool { temporal == nil && liveOnly != true }
}

// ── GeoJSON ────────────────────────────────────────────────────────────────

enum Geometry: @unchecked Sendable {
    case point(CLLocationCoordinate2D)
    case lineString([CLLocationCoordinate2D])
    case polygon([[CLLocationCoordinate2D]])      // outer ring + holes
    case multiPoint([CLLocationCoordinate2D])
    case multiLineString([[CLLocationCoordinate2D]])
    case multiPolygon([[[CLLocationCoordinate2D]]])

    /// First coordinate seen (for popup anchor / bbox center).
    var anchor: CLLocationCoordinate2D? {
        switch self {
        case .point(let c): return c
        case .lineString(let cs): return cs.first
        case .polygon(let rings): return rings.first?.first
        case .multiPoint(let cs): return cs.first
        case .multiLineString(let lines): return lines.first?.first
        case .multiPolygon(let polys): return polys.first?.first?.first
        }
    }

    /// Representative coordinate for fast point-in-rect culling.
    /// Mean-of-vertices, not signed-area centroid — sufficient for filtering.
    var centroid: CLLocationCoordinate2D? {
        func mean(_ cs: [CLLocationCoordinate2D]) -> CLLocationCoordinate2D? {
            guard !cs.isEmpty else { return nil }
            let lat = cs.reduce(0.0) { $0 + $1.latitude } / Double(cs.count)
            let lon = cs.reduce(0.0) { $0 + $1.longitude } / Double(cs.count)
            return CLLocationCoordinate2D(latitude: lat, longitude: lon)
        }
        switch self {
        case .point(let c):            return c
        case .multiPoint(let cs):      return mean(cs)
        case .lineString(let cs):      return cs.isEmpty ? nil : cs[cs.count / 2]
        case .multiLineString(let ls): return mean(ls.flatMap { $0 })
        case .polygon(let rings):      return mean(rings.first ?? [])
        case .multiPolygon(let polys): return mean(polys.first?.first ?? [])
        }
    }
}

extension Geometry {
    init?(rawType: String, coords: Any) {
        func coord(_ a: Any) -> CLLocationCoordinate2D? {
            guard let arr = a as? [Any], arr.count >= 2,
                  let lng = (arr[0] as? NSNumber)?.doubleValue ?? (arr[0] as? Double),
                  let lat = (arr[1] as? NSNumber)?.doubleValue ?? (arr[1] as? Double),
                  lat.isFinite, lng.isFinite else { return nil }
            return CLLocationCoordinate2D(latitude: lat, longitude: lng)
        }
        func line(_ a: Any) -> [CLLocationCoordinate2D] {
            (a as? [Any] ?? []).compactMap(coord)
        }
        func poly(_ a: Any) -> [[CLLocationCoordinate2D]] {
            (a as? [Any] ?? []).map(line)
        }
        switch rawType {
        case "Point":
            guard let c = coord(coords) else { return nil }
            self = .point(c)
        case "LineString":
            self = .lineString(line(coords))
        case "Polygon":
            self = .polygon(poly(coords))
        case "MultiPoint":
            self = .multiPoint(line(coords))
        case "MultiLineString":
            self = .multiLineString(poly(coords))
        case "MultiPolygon":
            self = .multiPolygon((coords as? [Any] ?? []).map(poly))
        default:
            return nil
        }
    }
}

/// Arbitrary JSON property bag — passes through to popups verbatim.
/// `Any?` is fundamentally non-Sendable, but instances are immutable after
/// decoding and only ever cross actors as part of frozen FeatureCollection
/// payloads, so `@unchecked` is safe in practice.
struct AnyCodable: Codable, Hashable, @unchecked Sendable {
    let value: Any?

    init(_ value: Any?) { self.value = value }

    init(from decoder: Decoder) throws {
        let c = try decoder.singleValueContainer()
        if c.decodeNil()                      { value = nil }
        else if let v = try? c.decode(Bool.self)    { value = v }
        else if let v = try? c.decode(Int.self)     { value = v }
        else if let v = try? c.decode(Double.self)  { value = v }
        else if let v = try? c.decode(String.self)  { value = v }
        else if let v = try? c.decode([AnyCodable].self) { value = v.map(\.value) }
        else if let v = try? c.decode([String: AnyCodable].self) {
            value = v.mapValues(\.value)
        } else {
            value = nil
        }
    }

    func encode(to encoder: Encoder) throws {
        var c = encoder.singleValueContainer()
        switch value {
        case nil:                try c.encodeNil()
        case let v as Bool:      try c.encode(v)
        case let v as Int:       try c.encode(v)
        case let v as Double:    try c.encode(v)
        case let v as String:    try c.encode(v)
        case let v as [Any]:     try c.encode(v.map(AnyCodable.init))
        case let v as [String: Any]: try c.encode(v.mapValues(AnyCodable.init))
        default:                 try c.encodeNil()
        }
    }

    static func == (lhs: AnyCodable, rhs: AnyCodable) -> Bool {
        String(describing: lhs.value) == String(describing: rhs.value)
    }
    func hash(into hasher: inout Hasher) {
        hasher.combine(String(describing: value))
    }
}

struct GeoFeature: Identifiable, Equatable, Sendable {
    let id: String
    let layerId: String
    let geometry: Geometry
    let properties: [String: AnyCodable]

    static func == (lhs: GeoFeature, rhs: GeoFeature) -> Bool { lhs.id == rhs.id }

    /// Convenience accessors for common property names.
    var displayName: String {
        for k in ["name", "name_ja", "title", "label", "callsign", "id"] {
            if let v = properties[k]?.value as? String, !v.isEmpty { return v }
        }
        return layerId
    }
    var iconHint: String? {
        properties["icon"]?.value as? String
    }
    var imageURL: String? {
        for k in ["thumbnail_url", "image", "thumbnail", "snapshot_url", "photo"] {
            if let v = properties[k]?.value as? String, !v.isEmpty { return v }
        }
        return nil
    }
    var externalLink: String? {
        for k in ["url", "link", "page_url", "website"] {
            if let v = properties[k]?.value as? String, !v.isEmpty { return v }
        }
        return nil
    }
}

struct FeatureCollection: Decodable, Sendable {
    let features: [GeoFeature]
    let meta: [String: AnyCodable]?

    enum K: String, CodingKey { case features, _meta }

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: K.self)
        let raw = try c.decodeIfPresent([RawFeature].self, forKey: .features) ?? []
        // layerId is set later by the caller (we don't know it here).
        self.features = raw.compactMap { $0.toGeoFeature(layerId: "") }
        self.meta = try c.decodeIfPresent([String: AnyCodable].self, forKey: ._meta)
    }

    init(features: [GeoFeature], meta: [String: AnyCodable]? = nil) {
        self.features = features
        self.meta = meta
    }

    func tagged(layerId: String) -> FeatureCollection {
        FeatureCollection(
            features: features.map {
                GeoFeature(id: "\(layerId)|\($0.id)", layerId: layerId,
                           geometry: $0.geometry, properties: $0.properties)
            },
            meta: meta
        )
    }
}

private struct RawFeature: Decodable {
    let id: AnyCodable?
    let geometry: RawGeometry?
    let properties: [String: AnyCodable]?

    func toGeoFeature(layerId: String) -> GeoFeature? {
        guard let geom = geometry?.parsed() else { return nil }
        let fid: String = {
            if let v = id?.value as? String { return v }
            if let v = id?.value as? Int    { return String(v) }
            if let p = properties,
               let v = p["id"]?.value as? String { return v }
            return UUID().uuidString
        }()
        return GeoFeature(id: fid, layerId: layerId,
                          geometry: geom, properties: properties ?? [:])
    }
}

private struct RawGeometry: Decodable {
    let type: String?
    let coordinates: AnyCodable?

    func parsed() -> Geometry? {
        guard let type, let coords = coordinates?.value else { return nil }
        return Geometry(rawType: type, coords: coords)
    }
}

// ── Sources (from /api/sources and /api/status) ────────────────────────────

struct OsintSource: Codable, Identifiable, Hashable {
    let id: String
    let name: String?
    let type: String?
    let category: String?
    let status: String?           // online | degraded | offline | pending
    let url: String?
    let last_check: String?
    let last_success: String?
    let response_time_ms: Double?
    let records_count: Int?
    let error_message: String?

    var statusKind: StatusKind { StatusKind(rawValue: status ?? "") ?? .unknown }

    enum StatusKind: String {
        case online, degraded, offline, pending, gated, unknown
    }
}

struct StatusEnvelope: Decodable {
    let summary: StatusSummary?
    let apis: [StatusRow]
}

struct StatusSummary: Decodable {
    let total: Int?
    let online: Int?
    let degraded: Int?
    let offline: Int?
    let pending: Int?
    let gated: Int?
    let requiresKey: Int?
    let configured: Int?
    let missingKey: Int?
    let working: Int?
}

/// Backend may emit `envVars` as either `["VAR_NAME", ...]` (legacy) or
/// `[{name, set, role}, ...]` (current). This decoder accepts both shapes.
struct EnvVarSpec: Decodable, Hashable {
    let name: String
    let set: Bool?
    let role: String?      // "required" | "optional" | "anyOf"

    init(from decoder: Decoder) throws {
        if let s = try? decoder.singleValueContainer().decode(String.self) {
            self.name = s; self.set = nil; self.role = nil
            return
        }
        let c = try decoder.container(keyedBy: K.self)
        self.name = try c.decode(String.self, forKey: .name)
        self.set  = try c.decodeIfPresent(Bool.self,   forKey: .set)
        self.role = try c.decodeIfPresent(String.self, forKey: .role)
    }
    private enum K: String, CodingKey { case name, set, role }
}

struct StatusRow: Decodable, Identifiable, Hashable {
    let id: String
    let name: String?
    let nameJa: String?
    let type: String?
    let category: String?
    let url: String?
    let description: String?
    let free: Bool?
    let layer: String?
    let status: String?
    let lastCheck: String?
    let lastSuccess: String?
    let responseTimeMs: Double?
    let recordsCount: Int?
    let errorMessage: String?
    let requiresKey: Bool?
    let configured: Bool?
    let envVars: [EnvVarSpec]?
    let missingVars: [String]?
    let probeConsent: Bool?
    let gated: Bool?

    /// Gated rows take precedence over the underlying probe status — they
    /// haven't been probed and shouldn't be coloured as online/offline.
    var statusKind: OsintSource.StatusKind {
        if gated == true { return .gated }
        return OsintSource.StatusKind(rawValue: status ?? "") ?? .unknown
    }
}

// ── API keys (overlay) ─────────────────────────────────────────────────────

/// Metadata for a single env-var the server reads. `set` reflects whether
/// process.env currently has a non-empty value (overlay or .env). `hasOverlay`
/// is true when the value comes from the user-edited overlay file (i.e. the
/// iOS tab has written it).
struct ApiKeyMeta: Decodable, Identifiable, Hashable {
    let name: String
    let role: String          // "required" | "anyOf" | "optional"
    let set: Bool
    let hasOverlay: Bool
    var id: String { name }
}

struct ApiKeyValue: Decodable {
    let name: String
    let value: String?
}

// ── Geocoding ──────────────────────────────────────────────────────────────

struct GeocodeHit: Codable, Identifiable, Hashable {
    let lat: Double
    let lon: Double
    let display_name: String?
    let type: String?
    let source: String?
    /// True when this hit was matched only by the auto-translated alt query
    /// (qAlt), not by the user's original q. Set by the server when running
    /// in bilingual mode; nil/false for ordinary single-query results.
    let via_translation: Bool?

    var id: String { "\(lat),\(lon),\(display_name ?? "")" }
    var coordinate: CLLocationCoordinate2D {
        CLLocationCoordinate2D(latitude: lat, longitude: lng)
    }
    var lng: Double { lon }
}

struct GeocodeResponse: Decodable {
    let results: [GeocodeHit]
    let provider: String?
}

struct ReverseGeocodeResponse: Decodable {
    let lat: Double?
    let lon: Double?
    let display_name: String?
    let display_name_ja: String?
    let display_name_en: String?
    let source: String?
}

// ── Intel (non-spatial sources) ────────────────────────────────────────────

struct IntelSource: Codable, Identifiable, Hashable {
    let id: String
    let name: String
    let name_ja: String?
    let category: String?
    let description: String?
    let url: String?
    let item_count: Int
    let last_fetched: String?
    let last_published: String?
    let ttl_ms: Int?
}

struct IntelRunResult: Decodable {
    let ran: Bool
    let source_id: String
    let ingested: Int?
    let kind: String?
    let duration_ms: Int?
    let error: String?
}

struct IntelSourcesEnvelope: Decodable {
    let data: [IntelSource]
    let meta: IntelSourcesMeta?
}

struct IntelSourcesMeta: Decodable {
    let total: Int?
    let fetched_at: String?
}

struct IntelItem: Codable, Identifiable, Hashable {
    let uid: String
    let source_id: String
    let title: String?
    let summary: String?
    let body: String?
    let link: String?
    let author: String?
    let language: String?
    let published_at: String?
    let fetched_at: String?
    let tags: [String]?
    let properties: [String: AnyCodable]?
    let _excerpt: String?
    /// True when this item was matched only by the auto-translated alt query
    /// (qAlt), not by the user's original q. The Intel tab renders a small
    /// "translated" badge on rows where this is true.
    let via_translation: Bool?
    var id: String { uid }

    static func == (lhs: IntelItem, rhs: IntelItem) -> Bool { lhs.uid == rhs.uid }
    func hash(into hasher: inout Hasher) { hasher.combine(uid) }
}

struct IntelItemsEnvelope: Decodable {
    let data: [IntelItem]
    let page: IntelPage?
}

struct IntelPage: Decodable {
    let next_cursor: String?
    let limit: Int?
    let total: Int?
}

struct IntelItemEnvelope: Decodable {
    let data: IntelItem
}

// ── Alerts ─────────────────────────────────────────────────────────────────

struct AlertChannel: Codable, Hashable, Identifiable {
    enum Kind: String, Codable, CaseIterable, Identifiable {
        case email, webhook
        var id: String { rawValue }
        var label: String { self == .email ? "Email" : "Webhook" }
    }
    let type: Kind
    var target: String
    var secret: String?    // webhook only; server returns "••••" on reads
    var id: String { "\(type.rawValue):\(target)" }
}

struct AlertPredicate: Codable, Hashable {
    var q: String?
    var source_ids: [String]?
    var tags_any: [String]?
    var tags_all: [String]?
    var bbox: [Double]?              // [w, s, e, n]
    var record_types: [String]?
}

struct AlertRule: Codable, Identifiable, Hashable {
    let id: String
    var name: String
    var enabled: Bool
    var predicate: AlertPredicate
    var channels: [AlertChannel]
    var dedup_window_sec: Int
    var storm_cap_per_hour: Int
    var muted_until: String?
    let created_at: String?
    let updated_at: String?
}

struct AlertRuleEnvelope: Decodable { let data: AlertRule }
struct AlertRulesEnvelope: Decodable { let data: [AlertRule] }

struct AlertEvent: Decodable, Identifiable, Hashable {
    let id: String
    let item_uid: String
    let matched_at: String
    let delivered_channels: [String]
    let suppressed: Int
    let reason: String?
}
struct AlertEventsEnvelope: Decodable { let data: [AlertEvent] }

// ── Database explorer ──────────────────────────────────────────────────────

struct DBTable: Decodable, Identifiable, Hashable {
    var id: String { name }
    let name: String
    let row_count: Int
    let columns: [DBColumn]
}

struct DBColumn: Decodable, Hashable {
    let name: String
    let type: String
}

struct DBPage: Decodable {
    let name: String
    let columns: [DBColumn]
    let rows: [[String: AnyCodable]]
    let total: Int
    let limit: Int
    let offset: Int
}

struct SchedulerJob: Decodable, Identifiable, Hashable {
    let id: String
    let cron: String?
    let description: String?
    let last_run: String?
    let next_run: String?
}

struct SchedulerEnvelope: Decodable {
    let jobs: [SchedulerJob]
    let sources: [DBRowAny]
}

struct DBRowAny: Decodable, Identifiable, Hashable {
    let raw: [String: AnyCodable]
    var id: String {
        if let v = raw["id"]?.value as? String { return v }
        if let v = raw["id"]?.value as? Int { return String(v) }
        return UUID().uuidString
    }
    init(from decoder: Decoder) throws {
        raw = try [String: AnyCodable](from: decoder)
    }
}

// ── Follow panel events (WebSocket + GET /api/follow/recent) ───────────────

struct FollowEvent: Decodable, Identifiable, Hashable {
    let id: String
    let event_id: String?
    let phase: String?            // request | response | error
    let method: String?
    let url: String?
    let status: Int?
    let bytes: Int?
    let duration_ms: Double?
    let collector: String?
    let record_count: Int?
    let timestamp: String?

    private enum CodingKeys: String, CodingKey {
        case event_id, phase, method, url, status, bytes, duration_ms, collector, record_count, timestamp
    }

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        self.event_id     = try c.decodeIfPresent(String.self, forKey: .event_id)
        self.phase        = try c.decodeIfPresent(String.self, forKey: .phase)
        self.method       = try c.decodeIfPresent(String.self, forKey: .method)
        self.url          = try c.decodeIfPresent(String.self, forKey: .url)
        self.status       = try c.decodeIfPresent(Int.self, forKey: .status)
        self.bytes        = try c.decodeIfPresent(Int.self, forKey: .bytes)
        self.duration_ms  = try c.decodeIfPresent(Double.self, forKey: .duration_ms)
        self.collector    = try c.decodeIfPresent(String.self, forKey: .collector)
        self.record_count = try c.decodeIfPresent(Int.self, forKey: .record_count)
        self.timestamp    = try c.decodeIfPresent(String.self, forKey: .timestamp)
        self.id           = self.event_id ?? UUID().uuidString
    }
}

struct FollowEnvelope: Decodable {
    let count: Int?
    let events: [FollowEvent]
}

// ── Camera discovery events ────────────────────────────────────────────────

struct CameraEvent: Decodable, Identifiable, Hashable {
    let id: String
    let kind: String?            // "new" | "updated"
    let url: String?
    let lat: Double?
    let lon: Double?
    let title: String?
    let snapshot_url: String?
    let timestamp: String?
    /// Full raw properties dict from the backend feature (camera_type,
    /// discovery_channels, operator, first_seen_at, …). Optional because
    /// older code paths may not have it; carries through to the popup so
    /// the Properties section isn't reduced to a single timestamp row.
    let properties: [String: AnyCodable]?

    static func == (lhs: CameraEvent, rhs: CameraEvent) -> Bool { lhs.id == rhs.id }
    func hash(into hasher: inout Hasher) { hasher.combine(id) }

    /// Convenience accessor for an optional string property on the underlying
    /// raw feature dict. Returns nil for missing or empty values.
    func propString(_ key: String) -> String? {
        guard let v = properties?[key]?.value as? String, !v.isEmpty else { return nil }
        return v
    }

    /// First entry from `discovery_channels` (array form) or the legacy single
    /// `discovery_channel` string. Used by the feed resolver to gate behavior.
    var firstDiscoveryChannel: String? {
        if let arr = properties?["discovery_channels"]?.value as? [Any],
           let first = arr.first as? String, !first.isEmpty {
            return first
        }
        if let s = properties?["discovery_channels"]?.value as? String, !s.isEmpty {
            return s
        }
        return propString("discovery_channel")
    }
}

extension CameraEvent {
    /// Build a CameraEvent from the WS `camera_discovered` envelope. The
    /// server wraps a GeoJSON Feature under `camera`, so we unwrap it and
    /// rename keys to match the flat struct.
    static func fromBroadcast(envelope: [String: Any]) -> CameraEvent? {
        guard let camera = envelope["camera"] as? [String: Any] else { return nil }
        return fromRawFeature(camera, kindOverride: envelope["kind"] as? String)
    }

    /// Build a CameraEvent from a typed GeoFeature returned by /api/data/cameras.
    /// `kind` is nil for REST-seeded cameras (no new/updated badge).
    static func fromFeature(_ f: GeoFeature) -> CameraEvent? {
        let p = f.properties
        func str(_ k: String) -> String? {
            guard let v = p[k]?.value as? String, !v.isEmpty else { return nil }
            return v
        }
        guard let id = (p["camera_uid"]?.value as? String), !id.isEmpty else { return nil }
        let coord = f.geometry.anchor
        return CameraEvent(
            id: id,
            kind: nil,
            url: str("url"),
            lat: coord?.latitude,
            lon: coord?.longitude,
            title: str("name") ?? str("title"),
            snapshot_url: str("thumbnail_url") ?? str("snapshot_url"),
            timestamp: str("last_seen_at") ?? str("timestamp"),
            properties: f.properties
        )
    }

    /// Shared flattener over the raw JSON Feature dict (used by the WS path
    /// where we don't have a typed GeoFeature handy).
    private static func fromRawFeature(_ feature: [String: Any], kindOverride: String?) -> CameraEvent? {
        let properties = (feature["properties"] as? [String: Any]) ?? [:]
        guard let id = (properties["camera_uid"] as? String), !id.isEmpty else { return nil }

        var lat: Double?
        var lon: Double?
        if let geom = feature["geometry"] as? [String: Any],
           let coords = geom["coordinates"] as? [Any], coords.count >= 2 {
            lon = (coords[0] as? NSNumber)?.doubleValue ?? (coords[0] as? Double)
            lat = (coords[1] as? NSNumber)?.doubleValue ?? (coords[1] as? Double)
        }

        func str(_ k: String) -> String? {
            guard let v = properties[k] as? String, !v.isEmpty else { return nil }
            return v
        }

        // Re-encode then decode the raw properties dict via AnyCodable so the
        // dynamic [String:Any] payload becomes [String:AnyCodable] without
        // having to handcraft per-type bridging.
        let typedProps: [String: AnyCodable]? = {
            guard let data = try? JSONSerialization.data(withJSONObject: properties) else { return nil }
            return try? JSONDecoder().decode([String: AnyCodable].self, from: data)
        }()

        return CameraEvent(
            id: id,
            kind: kindOverride,
            url: str("url"),
            lat: lat,
            lon: lon,
            title: str("name") ?? str("title"),
            snapshot_url: str("thumbnail_url") ?? str("snapshot_url"),
            timestamp: str("last_seen_at") ?? str("timestamp"),
            properties: typedProps
        )
    }
}
