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

nonisolated struct LayerSourceRef: Codable, Hashable, Identifiable, Sendable {
    let id: String
    let name: String?
    let type: String?
    let free: Bool?
}

/// Time-coded layer columns advertised by the server. Used by the iOS
/// client only to flag features that fell back to `fetched_at` in replay.
nonisolated struct LayerTemporal: Codable, Hashable, Sendable {
    let field: String
    let fallbackField: String?
}

nonisolated struct LayerDef: Codable, Identifiable, Hashable, Sendable {
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

nonisolated enum Geometry: @unchecked Sendable {
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

nonisolated extension Geometry {
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
nonisolated struct AnyCodable: Codable, Hashable, @unchecked Sendable {
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

nonisolated struct GeoFeature: Identifiable, Equatable, Sendable {
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

nonisolated struct FeatureCollection: Decodable, Sendable {
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

private nonisolated struct RawFeature: Decodable {
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

private nonisolated struct RawGeometry: Decodable {
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
    /// Breach catalog totals. The server emits these only for platform
    /// operators (0 otherwise), so a non-zero `breachTotal` is what tells the
    /// Sources screen that `apis` carries category-"breach" rows.
    let breachTotal: Int?
    let breachMaterialized: Int?
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

// ── API keys (per-tenant BYOK) ─────────────────────────────────────────────

/// One env-var's status for the active workspace. `byok` = the workspace has
/// its own encrypted key. `source`: "tenant" (own key) | "platform" (operator
/// default) | nil (gated — no key anywhere). `set` = resolvable either way.
struct TenantKeyMeta: Decodable, Identifiable, Hashable {
    let name: String
    let role: String          // "required" | "anyOf" | "optional"
    let byok: Bool
    let set: Bool
    let source: String?       // "tenant" | "platform" | nil
    var id: String { name }
}

/// `GET /api/tenant-keys`. `canManage` reflects the owner-chosen edit policy
/// resolved server-side for the caller; the client uses it to gate editing.
struct TenantKeysEnvelope: Decodable {
    let items: [TenantKeyMeta]
    let canManage: Bool
    let policy: String              // owner_only | selected_member | all_members
    let policyMemberId: String?
}

/// `PUT /api/tenant-keys/:name` response — enough to refresh one row without
/// a full reload.
struct TenantKeyWrite: Decodable {
    let name: String
    let byok: Bool
    let set: Bool
    let source: String?
}

/// `PUT /api/tenant-keys/policy` response.
struct KeyPolicyState: Decodable {
    let policy: String
    let memberId: String?
}

struct KeyPolicyMember: Decodable, Identifiable, Hashable {
    let id: String
    let email: String
    let role: String
}

/// `GET /api/tenant-keys/policy`. `members` is the roster the owner picks a
/// delegate from when policy = selected_member.
struct KeyPolicyResponse: Decodable {
    let policy: String
    let memberId: String?
    let members: [KeyPolicyMember]
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
    /// Reliability scoring (roadmap 30), served by /api/status and
    /// /api/intel/sources. `rated == false` means "no fetch history in the
    /// window" — the UI must render that as unrated, never as a zero score.
    /// Optional so cached JSON predating the field still decodes.
    let trust: SourceTrust?
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
    /// Geocoded point, served top-level by the API (intelapi.c) from the
    /// `intel_items.lat/lon` columns — not inside `properties`. Optional/null
    /// for items that were never geocoded.
    let lat: Double?
    let lon: Double?
    let _excerpt: String?
    /// Per-data-point provenance assembled server-side (intelStore.rowToItem):
    /// which collector, the upstream origin URL, this datum's own link,
    /// fetch/publish times, and license/confidence when known. Always sent by
    /// the server but optional here so older cached JSON still decodes.
    let provenance: Provenance?
    /// True when this item was matched only by the auto-translated alt query
    /// (qAlt), not by the user's original q. The Intel tab renders a small
    /// "translated" badge on rows where this is true.
    let via_translation: Bool?

    /// Near-duplicate corroboration, present only when the request asked for
    /// `?collapse=1` (roadmap 25). Optional so every existing call site and
    /// all cached JSON keep decoding unchanged.
    let cluster: ClusterInfo?
    /// Machine translation, present only when the request asked for
    /// `?lang_view=` (roadmap 29). `machine == true` must be labelled as such
    /// in the UI — it is never the source's own words.
    let translation: TranslationInfo?

    var id: String { uid }

    static func == (lhs: IntelItem, rhs: IntelItem) -> Bool { lhs.uid == rhs.uid }
    func hash(into hasher: inout Hasher) { hasher.combine(uid) }
}

/// An entity mentioned in an intel item (from GET /api/intel/items/:uid/entities).
/// Carries the exact `entity_id` so chips push straight into `EntityDetailView`
/// without a re-lookup. Works for both breach records and normal intel items.
struct ItemEntity: Codable, Identifiable, Hashable {
    let entity_id: String
    let type: String
    let value: String
    let label: String?
    var id: String { entity_id }
}
struct ItemEntitiesEnvelope: Decodable { let data: [ItemEntity] }

/// Operator-gated breach-secret reveal (GET /api/intel/items/:uid/reveal).
/// Mirrors breach_index_lookup(reveal=1): per-breach decrypted secret(s), or a
/// `note` for password (hash-only) records.
struct RevealResult: Decodable {
    let found: Bool?
    let count: Int?
    let type: String?
    let note: String?
    let breaches: [RevealBreach]?
}
struct RevealBreach: Decodable, Identifiable, Hashable {
    let breach: String?
    let secret: String?
    var id: String { (breach ?? "") + "|" + (secret ?? "") }
}

// MARK: - Breach corpus (operator only)

/// One async corpus job (`POST /api/admin/breach/{fetch,ingest}`).
///
/// The server keeps these in a fixed in-memory table that is wiped on restart,
/// and single-flights one fetch and one ingest at a time — so this is a live
/// progress view, never an audit trail.
///
/// `started`/`ended` are **unix epoch seconds**, not ISO-8601 strings like the
/// rest of the API (see breach_jobs.c). Decoding them as `String` fails; using
/// them as a `TimeInterval` since 1970 is correct.
struct BreachJob: Decodable, Identifiable, Hashable {
    let jobId: String
    let kind: String            // "fetch" | "ingest"
    let sourceId: String?
    let state: String           // "started" | "running" | "done" | "error"
    let message: String?
    let rowsIn: Int?
    let rowsNew: Int?
    let started: Double?
    let ended: Double?

    var id: String { jobId }
    var isRunning: Bool { state == "running" || state == "started" || state == "queued" }
    var startedAt: Date? { started.map { Date(timeIntervalSince1970: $0) } }
    var endedAt: Date? { ended.map { Date(timeIntervalSince1970: $0) } }

    /// The staged file path, recoverable only by scraping the fetch job's
    /// message ("staged <n> bytes -> <path> (status <code>)"). The server
    /// truncates that path at 120 chars, so treat the result as a suggestion
    /// and keep the field the operator edits writable.
    var stagedPath: String? {
        guard kind == "fetch", let m = message,
              let r = m.range(of: " -> ") else { return nil }
        let tail = String(m[r.upperBound...])
        let path = (tail.components(separatedBy: " (status").first ?? tail)
            .trimmingCharacters(in: .whitespaces)
        return path.isEmpty ? nil : path
    }

    private enum CodingKeys: String, CodingKey {
        case jobId = "job_id", kind, sourceId = "source_id", state, message
        case rowsIn = "rows_in", rowsNew = "rows_new", started, ended
    }
}

struct BreachJobsEnvelope: Decodable { let jobs: [BreachJob] }

/// `GET /api/admin/breach/catalog/preview` — what the seed parser produced,
/// without writing anything.
struct BreachCatalogPreview: Decodable {
    let path: String
    let format: String
    let rowsRead: Int
    let rowsParsed: Int
    let rowsSkipped: Int
    let rowsNew: Int
    let rowsExisting: Int
    let sample: [BreachCatalogRow]

    private enum CodingKeys: String, CodingKey {
        case path, format, sample
        case rowsRead = "rows_read", rowsParsed = "rows_parsed"
        case rowsSkipped = "rows_skipped", rowsNew = "rows_new"
        case rowsExisting = "rows_existing"
    }
}

struct BreachCatalogRow: Decodable, Identifiable, Hashable {
    let breachId: String
    let name: String
    let pwnCount: Int?
    let addedDate: String?
    let breachDate: String?

    var id: String { breachId }

    private enum CodingKeys: String, CodingKey {
        case breachId = "breach_id", name
        case pwnCount = "pwn_count", addedDate = "added_date", breachDate = "breach_date"
    }
}

/// `POST /api/admin/breach/catalog/load`. Only `loaded` and `format` are sent
/// for the JSON manifest path; the TSV path fills in the row counters too.
struct BreachCatalogLoadResult: Decodable {
    let loaded: Int
    let format: String?
    let rowsRead: Int?
    let rowsSkipped: Int?
    let rowsNew: Int?
    let rowsExisting: Int?

    private enum CodingKeys: String, CodingKey {
        case loaded, format
        case rowsRead = "rows_read", rowsSkipped = "rows_skipped"
        case rowsNew = "rows_new", rowsExisting = "rows_existing"
    }
}

/// Mirror of the server `provenance` block (see intelStore.buildProvenance).
/// Every field is optional/null-tolerant so a pruned source or a partial
/// collector never breaks decoding.
struct Provenance: Codable, Hashable {
    let source_id: String?
    let source_name: String?
    let source_name_ja: String?
    let category: String?
    let collection_method: String?   // "api" | "scrape" | "rss" | "dataset" | …
    let source_url: String?          // upstream origin the collector pulls from
    let item_url: String?            // this datum's own upstream link
    let sub_source_id: String?
    let fetched_at: String?
    let published_at: String?
    let license: String?
    let confidence: Double?
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
    /// Spatial alternatives to `bbox` (roadmap 9). At most ONE spatial term
    /// may be present — the server rejects two, because a second shape is an
    /// authoring mistake rather than a conjunction.
    var polygon: [[Double]]?         // ring of [lon, lat]
    var circle: AlertCircle?
    /// Reference to a saved `areas_of_interest` row, so one shape can back
    /// several rules. Resolved tenant-scoped at match time.
    var aoi_id: String?
    /// Entity watchlist terms (roadmap 21).
    var entity_ids: [String]?
    var entity_types: [String]?
    var record_types: [String]?
    /// Query authoring mode. `nil`/"fts" ⇒ the FTS predicate `q` matches new
    /// items; "llm" ⇒ `nl_query` drives the agentic search pipeline (which
    /// also produces an FTS `q` the user chose). Round-trips opaquely.
    var mode: String?
    var nl_query: String?
}

struct AlertCircle: Codable, Hashable {
    var lat: Double
    var lon: Double
    var radius_m: Double
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
    /// Joined from intel_items server-side so the history row can show the
    /// matched item instead of a bare uid. Null when the matched row has
    /// since been pruned.
    let item_title: String?
    let item_source_id: String?
    let item_link: String?
}
struct AlertEventsEnvelope: Decodable { let data: [AlertEvent] }

// ── Test-fire (POST /api/alerts/:id/test) ──────────────────────────────────
// The endpoint really delivers now, so the reply carries a per-channel
// outcome. "The test succeeded" is not useful on its own — an operator is
// testing precisely because they want to know WHICH channel is broken and
// why, so `status`/`http_code`/`error` are surfaced rather than collapsed
// into a single bool.
struct AlertTestChannelResult: Decodable, Identifiable, Hashable {
    let channel_idx: Int
    let type: String
    let target: String
    let status: String          // ok | failed | dead | skipped
    let http_code: Int?
    let error: String?
    var id: Int { channel_idx }
    var ok: Bool { status == "ok" }
}
struct AlertTestData: Decodable, Hashable {
    let rule_id: String
    let rule_name: String?
    let event_id: String
    let results: [AlertTestChannelResult]
}
struct AlertTestResult: Decodable, Hashable {
    let ok: Bool
    let fired: Bool
    let data: AlertTestData

    /// One line an operator can act on, e.g.
    /// "webhook https://… → 502" or "all 2 channels delivered".
    var summary: String {
        let bad = data.results.filter { !$0.ok }
        if bad.isEmpty {
            let n = data.results.count
            return n == 1 ? "Channel delivered"
                          : "All \(n) channels delivered"
        }
        return bad.map { r in
            var s = "\(r.type) \(r.target) → \(r.status)"
            if let c = r.http_code { s += " (HTTP \(c))" }
            if let e = r.error, !e.isEmpty { s += ": \(e)" }
            return s
        }.joined(separator: "\n")
    }
}

// ── Identity / tenancy (GET /api/me) ───────────────────────────────────────

struct MeResponse: Decodable {
    let user: MeUser?
    let tenant: MeTenant?
    let memberships: [MeTenant]?
}
struct MeUser: Decodable, Hashable {
    let id: String
    let email: String?
    let display_name: String?
}
struct MeTenant: Decodable, Hashable, Identifiable {
    let id: String
    let slug: String
    let name: String
    let plan: String?
    let role: String?
}

// ── Workspace members & invites (GET /api/members) ──────────────────────────

/// Assignable workspace roles, mirroring the server's CHECK constraint on
/// `memberships.role`.
enum WorkspaceRole: String, Codable, CaseIterable, Identifiable {
    case owner, admin, analyst, viewer
    var id: String { rawValue }
    var label: String { rawValue.capitalized }
}

struct WorkspaceMember: Decodable, Identifiable, Hashable {
    let user_id: String
    let email: String
    let role: String
    var id: String { user_id }
}

struct WorkspaceInvite: Decodable, Identifiable, Hashable {
    let id: String
    let email: String
    let role: String
    let created_at: String?
}

struct WorkspaceMembersResponse: Decodable {
    let members: [WorkspaceMember]
    let invites: [WorkspaceInvite]
}

/// Shared shape for the member-mutation endpoints (invite / role / remove).
struct MemberActionResponse: Decodable {
    let ok: Bool?
    let status: String?      // "invited" | "added" | "already_member"
    let user_id: String?
    let role: String?
}

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

// MARK: - Repair worker / self-healing maintenance
//
// Mirrors the C backend's detect→triage→repair pipeline. The host-wide digest
// (`GET /api/admin/maintenance`) and the per-source drill-down
// (`GET /api/admin/maintenance/source/:id`) share the `RepairRow`/`AnomalyRow`
// shapes. All lists are server-sourced — empty arrays mean "nothing to show",
// never a placeholder.

/// Host-wide repair digest over a trailing window.
struct MaintenanceDigest: Decodable {
    let generatedAt: String?
    let windowHours: Int?
    let totals: RepairTotals?
    let successByClass: [RepairClassStat]
    let worstSources: [RepairSourceStat]
    let quarantined: [QuarantineRow]
    let urlOverrides: [UrlOverrideRow]
    let autoFixed: [RepairRow]
    let awaitingReview: AwaitingReview?
    let autoDismissed: [RepairRow]
    let needsHuman: [RepairRow]
    let concurrency: ConcurrencySnapshot?

    private enum K: String, CodingKey {
        case generatedAt = "generated_at", windowHours = "window_hours", totals
        case successByClass = "success_by_class", worstSources = "worst_sources"
        case quarantined, urlOverrides = "url_overrides", autoFixed = "auto_fixed"
        case awaitingReview = "awaiting_review", autoDismissed = "auto_dismissed"
        case needsHuman = "needs_human", concurrency
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        generatedAt = try c.decodeIfPresent(String.self, forKey: .generatedAt)
        windowHours = try c.decodeIfPresent(Int.self, forKey: .windowHours)
        totals = try c.decodeIfPresent(RepairTotals.self, forKey: .totals)
        successByClass = (try? c.decode([RepairClassStat].self, forKey: .successByClass)) ?? []
        worstSources = (try? c.decode([RepairSourceStat].self, forKey: .worstSources)) ?? []
        quarantined = (try? c.decode([QuarantineRow].self, forKey: .quarantined)) ?? []
        urlOverrides = (try? c.decode([UrlOverrideRow].self, forKey: .urlOverrides)) ?? []
        autoFixed = (try? c.decode([RepairRow].self, forKey: .autoFixed)) ?? []
        awaitingReview = try c.decodeIfPresent(AwaitingReview.self, forKey: .awaitingReview)
        autoDismissed = (try? c.decode([RepairRow].self, forKey: .autoDismissed)) ?? []
        needsHuman = (try? c.decode([RepairRow].self, forKey: .needsHuman)) ?? []
        concurrency = try c.decodeIfPresent(ConcurrencySnapshot.self, forKey: .concurrency)
    }
}

struct RepairTotals: Decodable {
    let verified: Int?
    let merged: Int?
    let rejected: Int?
    let needsHuman: Int?
    let error: Int?
    private enum K: String, CodingKey {
        case verified, merged, rejected, needsHuman = "needs_human", error
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        verified = try c.decodeIfPresent(Int.self, forKey: .verified)
        merged = try c.decodeIfPresent(Int.self, forKey: .merged)
        rejected = try c.decodeIfPresent(Int.self, forKey: .rejected)
        needsHuman = try c.decodeIfPresent(Int.self, forKey: .needsHuman)
        error = try c.decodeIfPresent(Int.self, forKey: .error)
    }
}

struct RepairClassStat: Decodable, Identifiable {
    let klass: String
    let success: Int?
    let fail: Int?
    let needsHuman: Int?
    let successRate: Double?
    var id: String { klass }
    private enum K: String, CodingKey {
        case klass = "class", success, fail, needsHuman = "needs_human", successRate = "success_rate"
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        klass = (try? c.decode(String.self, forKey: .klass)) ?? "?"
        success = try c.decodeIfPresent(Int.self, forKey: .success)
        fail = try c.decodeIfPresent(Int.self, forKey: .fail)
        needsHuman = try c.decodeIfPresent(Int.self, forKey: .needsHuman)
        successRate = try c.decodeIfPresent(Double.self, forKey: .successRate)
    }
}

struct RepairSourceStat: Decodable, Identifiable {
    let sourceId: String
    let success: Int?
    let fail: Int?
    let successRate: Double?
    var id: String { sourceId }
    private enum K: String, CodingKey {
        case sourceId = "source_id", success, fail, successRate = "success_rate"
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        sourceId = try c.decode(String.self, forKey: .sourceId)
        success = try c.decodeIfPresent(Int.self, forKey: .success)
        fail = try c.decodeIfPresent(Int.self, forKey: .fail)
        successRate = try c.decodeIfPresent(Double.self, forKey: .successRate)
    }
}

struct QuarantineRow: Decodable, Identifiable {
    let sourceId: String
    let name: String?
    let category: String?
    let since: String?
    let until: String?
    let active: Bool?
    let reason: String?
    var id: String { sourceId }
    private enum K: String, CodingKey {
        case sourceId = "source_id", name, category, since, until, active, reason
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        sourceId = try c.decode(String.self, forKey: .sourceId)
        name = try c.decodeIfPresent(String.self, forKey: .name)
        category = try c.decodeIfPresent(String.self, forKey: .category)
        since = try c.decodeIfPresent(String.self, forKey: .since)
        until = try c.decodeIfPresent(String.self, forKey: .until)
        active = try c.decodeIfPresent(Bool.self, forKey: .active)
        reason = try c.decodeIfPresent(String.self, forKey: .reason)
    }
}

struct UrlOverrideRow: Decodable, Identifiable {
    let sourceId: String
    let oldURL: String?
    let newURL: String?
    let anomalyId: Int?
    let createdAt: String?
    var id: String { sourceId + (createdAt ?? "") }
    private enum K: String, CodingKey {
        case sourceId = "source_id", oldURL = "old_url", newURL = "new_url"
        case anomalyId = "anomaly_id", createdAt = "created_at"
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        sourceId = try c.decode(String.self, forKey: .sourceId)
        oldURL = try c.decodeIfPresent(String.self, forKey: .oldURL)
        newURL = try c.decodeIfPresent(String.self, forKey: .newURL)
        anomalyId = try c.decodeIfPresent(Int.self, forKey: .anomalyId)
        createdAt = try c.decodeIfPresent(String.self, forKey: .createdAt)
    }
}

struct AwaitingReview: Decodable {
    let awaitingPr: [RepairRow]
    let awaitingApply: [RepairRow]
    private enum K: String, CodingKey { case awaitingPr = "awaiting_pr", awaitingApply = "awaiting_apply" }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        awaitingPr = (try? c.decode([RepairRow].self, forKey: .awaitingPr)) ?? []
        awaitingApply = (try? c.decode([RepairRow].self, forKey: .awaitingApply)) ?? []
    }
    /// Everything that needs a human decision, regardless of PR-vs-apply split.
    var all: [RepairRow] { awaitingApply + awaitingPr }
}

/// Configured LLM concurrency limits. The C runtime has no shared in-flight
/// gauge, so `inflight`/`waiting` are always 0 — surface as "configured", not
/// live counts.
struct ConcurrencySnapshot: Decodable {
    let heavy: ConcurrencyGauge?
    let mid: ConcurrencyGauge?
}
struct ConcurrencyGauge: Decodable {
    let limit: Int?
    let inflight: Int?
    let waiting: Int?
}

/// One repair attempt. The digest omits `patch`/`gate`/`model`; the per-source
/// detail includes them. `patch` is a JSON string carrying the URL swap.
struct RepairRow: Decodable, Identifiable {
    let id: Int
    let anomalyId: Int?
    let sourceId: String
    let status: String
    let action: String?
    let triageClass: String?
    let prURL: String?
    let createdAt: String?
    let patch: String?
    let gate: String?
    let model: String?

    private enum K: String, CodingKey {
        case id, anomalyId = "anomaly_id", sourceId = "source_id", status, action
        case triageClass = "triage_class", prURL = "pr_url", createdAt = "created_at"
        case patch, gate, model
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        id = try c.decode(Int.self, forKey: .id)
        anomalyId = try c.decodeIfPresent(Int.self, forKey: .anomalyId)
        sourceId = try c.decode(String.self, forKey: .sourceId)
        status = (try? c.decode(String.self, forKey: .status)) ?? "?"
        action = try c.decodeIfPresent(String.self, forKey: .action)
        triageClass = try c.decodeIfPresent(String.self, forKey: .triageClass)
        prURL = try c.decodeIfPresent(String.self, forKey: .prURL)
        createdAt = try c.decodeIfPresent(String.self, forKey: .createdAt)
        patch = try c.decodeIfPresent(String.self, forKey: .patch)
        gate = try c.decodeIfPresent(String.self, forKey: .gate)
        model = try c.decodeIfPresent(String.self, forKey: .model)
    }

    /// A `verified` `url_swap` is the one staged proposition an operator can
    /// approve to apply live.
    var isApprovable: Bool { status == "verified" && action == "url_swap" }

    /// Parsed `patch` payload (old/new URL + whether a runtime override would
    /// actually rewrite traffic).
    var proposedSwap: RepairPatch? {
        guard let patch, let data = patch.data(using: .utf8) else { return nil }
        return try? JSONDecoder().decode(RepairPatch.self, from: data)
    }
}

struct RepairPatch: Decodable {
    let oldURL: String?
    let newURL: String?
    let runtimeOverride: Bool?
    private enum K: String, CodingKey {
        case oldURL = "old_url", newURL = "new_url", runtimeOverride = "runtime_override"
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        oldURL = try c.decodeIfPresent(String.self, forKey: .oldURL)
        newURL = try c.decodeIfPresent(String.self, forKey: .newURL)
        runtimeOverride = try c.decodeIfPresent(Bool.self, forKey: .runtimeOverride)
    }
}

// MARK: Per-source pipeline (`/api/admin/maintenance/source/:id`)

struct SourcePipeline: Decodable {
    let generatedAt: String?
    let source: PipelineSource
    let fetchLog: [FetchRun]
    let anomalies: [AnomalyRow]
    let repairs: [RepairRow]
    private enum K: String, CodingKey {
        case generatedAt = "generated_at", source, fetchLog = "fetch_log", anomalies, repairs
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        generatedAt = try c.decodeIfPresent(String.self, forKey: .generatedAt)
        source = try c.decode(PipelineSource.self, forKey: .source)
        fetchLog = (try? c.decode([FetchRun].self, forKey: .fetchLog)) ?? []
        anomalies = (try? c.decode([AnomalyRow].self, forKey: .anomalies)) ?? []
        repairs = (try? c.decode([RepairRow].self, forKey: .repairs)) ?? []
    }
}

struct PipelineSource: Decodable {
    let id: String
    let name: String?
    let category: String?
    let type: String?
    let url: String?
    let status: String?
    let lastCheck: String?
    let lastSuccess: String?
    let responseTimeMs: Int?
    let recordsCount: Int?
    let errorMessage: String?
    let quarantine: QuarantineState?
    private enum K: String, CodingKey {
        case id, name, category, type, url, status
        case lastCheck = "last_check", lastSuccess = "last_success"
        case responseTimeMs = "response_time_ms", recordsCount = "records_count"
        case errorMessage = "error_message", quarantine
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        id = try c.decode(String.self, forKey: .id)
        name = try c.decodeIfPresent(String.self, forKey: .name)
        category = try c.decodeIfPresent(String.self, forKey: .category)
        type = try c.decodeIfPresent(String.self, forKey: .type)
        url = try c.decodeIfPresent(String.self, forKey: .url)
        status = try c.decodeIfPresent(String.self, forKey: .status)
        lastCheck = try c.decodeIfPresent(String.self, forKey: .lastCheck)
        lastSuccess = try c.decodeIfPresent(String.self, forKey: .lastSuccess)
        responseTimeMs = try c.decodeIfPresent(Int.self, forKey: .responseTimeMs)
        recordsCount = try c.decodeIfPresent(Int.self, forKey: .recordsCount)
        errorMessage = try c.decodeIfPresent(String.self, forKey: .errorMessage)
        quarantine = try c.decodeIfPresent(QuarantineState.self, forKey: .quarantine)
    }
}

struct QuarantineState: Decodable {
    let at: String?
    let until: String?
    let reason: String?
    let active: Bool?
}

struct FetchRun: Decodable, Identifiable {
    let id: Int?
    let timestamp: String?
    let status: String?
    let recordsFetched: Int?
    let durationMs: Int?
    let error: String?
    private enum K: String, CodingKey {
        case id, timestamp, status, recordsFetched = "records_fetched", durationMs = "duration_ms", error
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        id = try c.decodeIfPresent(Int.self, forKey: .id)
        timestamp = try c.decodeIfPresent(String.self, forKey: .timestamp)
        status = try c.decodeIfPresent(String.self, forKey: .status)
        recordsFetched = try c.decodeIfPresent(Int.self, forKey: .recordsFetched)
        durationMs = try c.decodeIfPresent(Int.self, forKey: .durationMs)
        error = try c.decodeIfPresent(String.self, forKey: .error)
    }
    var ok: Bool { (status ?? "").lowercased() == "ok" }
}

struct AnomalyRow: Decodable, Identifiable {
    let id: Int
    let fetchLogId: Int?
    let verdict: String?
    let reason: String?
    let evidence: String?
    let escalationLevel: Int?
    let createdAt: String?
    let resolvedAt: String?
    let resolution: String?
    let triageClass: String?
    let triageConfidence: Double?
    let triageEvidence: String?
    let triageSuggestedFix: String?
    let triagedAt: String?
    let triageModel: String?
    private enum K: String, CodingKey {
        case id, fetchLogId = "fetch_log_id", verdict, reason, evidence
        case escalationLevel = "escalation_level", createdAt = "created_at"
        case resolvedAt = "resolved_at", resolution
        case triageClass = "triage_class", triageConfidence = "triage_confidence"
        case triageEvidence = "triage_evidence", triageSuggestedFix = "triage_suggested_fix"
        case triagedAt = "triaged_at", triageModel = "triage_model"
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        id = try c.decode(Int.self, forKey: .id)
        fetchLogId = try c.decodeIfPresent(Int.self, forKey: .fetchLogId)
        verdict = try c.decodeIfPresent(String.self, forKey: .verdict)
        reason = try c.decodeIfPresent(String.self, forKey: .reason)
        evidence = try c.decodeIfPresent(String.self, forKey: .evidence)
        escalationLevel = try c.decodeIfPresent(Int.self, forKey: .escalationLevel)
        createdAt = try c.decodeIfPresent(String.self, forKey: .createdAt)
        resolvedAt = try c.decodeIfPresent(String.self, forKey: .resolvedAt)
        resolution = try c.decodeIfPresent(String.self, forKey: .resolution)
        triageClass = try c.decodeIfPresent(String.self, forKey: .triageClass)
        triageConfidence = try c.decodeIfPresent(Double.self, forKey: .triageConfidence)
        triageEvidence = try c.decodeIfPresent(String.self, forKey: .triageEvidence)
        triageSuggestedFix = try c.decodeIfPresent(String.self, forKey: .triageSuggestedFix)
        triagedAt = try c.decodeIfPresent(String.self, forKey: .triagedAt)
        triageModel = try c.decodeIfPresent(String.self, forKey: .triageModel)
    }
    var isOpen: Bool { (resolvedAt ?? "").isEmpty }
    var isTriaged: Bool { !(triagedAt ?? "").isEmpty }
}

/// Generic `{ok:true,…}` action response. Extra keys (new_url, status, …) are
/// surfaced when present and ignored otherwise.
struct OkResult: Decodable {
    let ok: Bool?
    let status: String?
    let newURL: String?
    private enum K: String, CodingKey { case ok, status, newURL = "new_url" }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        ok = try c.decodeIfPresent(Bool.self, forKey: .ok)
        status = try c.decodeIfPresent(String.self, forKey: .status)
        newURL = try c.decodeIfPresent(String.self, forKey: .newURL)
    }
}
