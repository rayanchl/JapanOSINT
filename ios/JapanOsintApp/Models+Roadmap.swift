import Foundation
import CoreLocation

// ─────────────────────────────────────────────────────────────────────────────
// DTOs for the roadmap backends (items 7, 9, 11, 14–19, 21–25, 27–30, 38).
//
// Kept in their own file rather than appended to Models.swift so this large
// addition never collides with concurrent edits there. Every type mirrors an
// envelope the C backend actually emits — where the server sends `{"data":…}`
// there is a matching `…Envelope`, because the decode site should not have to
// remember which endpoints wrap and which don't.
//
// Naming follows the server's snake_case verbatim rather than being remapped
// to camelCase. That is deliberate: these structs are read next to the C
// headers that define them, and a silent rename is how a field quietly stops
// decoding when the server adds one.
// ─────────────────────────────────────────────────────────────────────────────

// ── Item 14: cases ─────────────────────────────────────────────────────────

struct CaseSummary: Codable, Identifiable, Hashable {
    let id: String
    let name: String
    let summary: String?
    let status: String            // open | closed | archived
    let priority: Int
    let created_by: String?
    let created_at: String
    let updated_at: String
    let closed_at: String?
    /// Served per-row by `GET /api/cases` (casesapi.c:563) so the list can
    /// show a pin count on first paint without opening each case.
    let item_count: Int?

    var isOpen: Bool { status == "open" }
}

/// A pinned reference. `snapshot_json` is a denormalized display copy taken at
/// pin time — deliberately NOT re-read live, because a report must not change
/// after the fact when a scraped source is re-fetched.
struct CaseItemRef: Codable, Identifiable, Hashable {
    let ref_type: String          // intel_item|entity|breach_item|feature|camera|search_run
    let ref_id: String
    let label: String?
    let snapshot: CaseSnapshot?
    let added_by: String?
    let added_at: String

    var id: String { "\(ref_type)|\(ref_id)" }
}

struct CaseSnapshot: Codable, Hashable {
    let origin: String?           // "server" | "client"
    let resolved: Bool?
    let captured_at: String?
    let data: [String: AnyCodable]?

    /// Best-effort display title from whatever the snapshot captured.
    var displayTitle: String? {
        for key in ["title", "name", "canonical", "value", "displayName"] {
            if let v = data?[key]?.value as? String, !v.isEmpty { return v }
        }
        return nil
    }
}

struct CaseActivityEntry: Codable, Identifiable, Hashable {
    let id: Int
    let actor_id: String?
    let kind: String              // created|updated|status_changed|item_pinned|…|comment
    let body: String?
    let target_ref: String?
    let mentions: [String]?
    let ts: String
}

struct CaseMemberRef: Codable, Identifiable, Hashable {
    let user_id: String
    let role: String              // lead | contributor | viewer
    let email: String?
    var id: String { user_id }
}

struct CaseDetail: Codable, Hashable {
    let id: String
    let name: String
    let summary: String?
    let status: String
    let priority: Int
    let created_at: String
    let updated_at: String
    let closed_at: String?
    let item_counts: [String: Int]?
    let members: [CaseMemberRef]?
}

struct CasesEnvelope: Decodable { let data: [CaseSummary]; let page: PageInfo? }
struct CaseEnvelope: Decodable { let data: CaseDetail }
struct CaseItemsEnvelope: Decodable { let data: [CaseItemRef]; let page: PageInfo? }
struct CaseActivityEnvelope: Decodable { let data: [CaseActivityEntry]; let page: PageInfo? }
struct CaseMembersEnvelope: Decodable { let data: [CaseMemberRef] }

/// Keyset page token. Every list endpoint in this backend uses the same
/// base64url `{"p":sort,"u":uid}` cursor, so one shape serves all of them.
///
/// The server's field is `next_cursor` (casesapi.c:140, annotationsapi.c:386,
/// timelineapi.c:536). `cursor` is exposed as an alias because every call site
/// reads it — an earlier version only knew `cursor`, so it decoded to nil and
/// silently pinned every list to a single page.
struct PageInfo: Codable, Hashable {
    let next_cursor: String?
    let has_more: Bool?

    /// What callers actually read.
    var cursor: String? { next_cursor }
}

// ── Item 15: annotations ───────────────────────────────────────────────────

struct Annotation: Codable, Identifiable, Hashable {
    let id: String
    let case_id: String?
    let ref_type: String
    let ref_id: String
    let body_md: String
    let author_id: String?
    let created_at: String
    let updated_at: String?
    /// Soft delete. Analyst notes are evidence — the row survives deletion and
    /// the UI renders a tombstone rather than hiding that something was here.
    let deleted_at: String?

    var isDeleted: Bool { deleted_at != nil }
}
struct AnnotationsEnvelope: Decodable { let data: [Annotation]; let page: PageInfo? }
struct AnnotationEnvelope: Decodable { let data: Annotation }

// ── Item 18: timeline ──────────────────────────────────────────────────────

struct TimelineBucket: Codable, Hashable, Identifiable {
    let t: String
    let intel: Int
    let mentions: Int
    let alerts: Int
    var id: String { t }
    var total: Int { intel + mentions + alerts }
}

struct TimelineEvent: Codable, Hashable, Identifiable {
    let key: String               // "i:"/"m:"/"a:" prefixed — unique across streams
    let kind: String              // intel | mention | alert
    let ts: String
    let uid: String?
    let title: String?
    let source_id: String?
    let lat: Double?
    let lon: Double?
    var id: String { key }

    var coordinate: CLLocationCoordinate2D? {
        guard let lat, let lon else { return nil }
        return CLLocationCoordinate2D(latitude: lat, longitude: lon)
    }
}

struct TimelinePayload: Decodable {
    let buckets: [TimelineBucket]
    let events: [TimelineEvent]
}
struct TimelineEnvelope: Decodable {
    let data: TimelinePayload
    let page: PageInfo?
    let meta: TimelineMeta?
}
struct TimelineMeta: Decodable, Hashable {
    let since: String?
    let until: String?
    let bucket: String?
    let notes: [String]?
}

// ── Item 11: alert inbox ───────────────────────────────────────────────────

struct AlertInboxEvent: Codable, Identifiable, Hashable {
    let id: String
    let rule_id: String
    let rule_name: String?
    let item_uid: String
    let matched_at: String
    let read_at: String?
    let unread: Bool
    let delivered_channels: [String]
    let item_title: String?
    let item_source_id: String?
    let item_link: String?
}
struct AlertInboxEnvelope: Decodable { let data: [AlertInboxEvent] }
struct UnreadCount: Decodable { let unread: Int }

// ── Item 10: rule preview / backtest ───────────────────────────────────────

struct AlertPreviewMatch: Codable, Identifiable, Hashable {
    let uid: String
    let title: String?
    let source_id: String?
    let published_at: String?
    var id: String { uid }
}
/// The server's shape is `{"data":[…samples…],"page":…,"meta":{"match_count":N,…}}`
/// (see `preview_envelope()` in core/alert_eval.c). An earlier version of this
/// DTO expected top-level `count`/`matches`; because both were optional the
/// decode SUCCEEDED and silently produced nil for each, so the headline number
/// never appeared and the feature looked broken rather than failing loudly.
/// The custom init maps the real envelope and keeps the convenient names.
struct AlertPreviewResult: Decodable {
    let count: Int?
    let matches: [AlertPreviewMatch]?
    let meta: AlertPreviewMeta?

    private enum CodingKeys: String, CodingKey { case data, meta }

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        matches = try c.decodeIfPresent([AlertPreviewMatch].self, forKey: .data)
        meta    = try c.decodeIfPresent(AlertPreviewMeta.self, forKey: .meta)
        count   = meta?.match_count
    }
}
struct AlertPreviewMeta: Decodable, Hashable {
    let match_count: Int?
    let truncated: Bool?
    let scanned: Int?
    let since: String?
    let skipped: String?          // e.g. "llm_mode_unsupported"
}

// ── Item 9: areas of interest ──────────────────────────────────────────────

struct AreaOfInterest: Codable, Identifiable, Hashable {
    let id: String
    let name: String
    let kind: String              // bbox | polygon | circle
    let geometry: AnyCodable?
    /// `[w, s, e, n]` — the server emits the bounding box as a single ARRAY
    /// under the key `bbox` (core/aoiapi.c:243-250), computed for EVERY kind,
    /// not as four scalars. The four `bbox_w/s/e/n` members this replaced
    /// therefore decoded to nil on every row. Null only if the stored columns
    /// are.
    let bbox: [Double]?
    let created_at: String

    /// Ring for a polygon AOI, in CLLocationCoordinate2D order.
    var polygonRing: [CLLocationCoordinate2D]? {
        guard kind == "polygon", let raw = geometry?.value as? [Any] else { return nil }
        return raw.compactMap { pt in
            guard let p = pt as? [Any], p.count >= 2,
                  let lon = (p[0] as? NSNumber)?.doubleValue,
                  let lat = (p[1] as? NSNumber)?.doubleValue else { return nil }
            return CLLocationCoordinate2D(latitude: lat, longitude: lon)
        }
    }

    var circle: (center: CLLocationCoordinate2D, radius: Double)? {
        guard kind == "circle", let d = geometry?.value as? [String: Any],
              let lat = (d["lat"] as? NSNumber)?.doubleValue,
              let lon = (d["lon"] as? NSNumber)?.doubleValue,
              let r = (d["radius_m"] as? NSNumber)?.doubleValue else { return nil }
        return (CLLocationCoordinate2D(latitude: lat, longitude: lon), r)
    }
}
struct AOIEnvelope: Decodable { let data: [AreaOfInterest] }
struct AOIOneEnvelope: Decodable { let data: AreaOfInterest }

// ── Item 21: watchlists ────────────────────────────────────────────────────

struct Watchlist: Codable, Identifiable, Hashable {
    let id: String
    let name: String
    let entity_ids: [String]?
    let rule_id: String?
    let created_at: String
}
struct WatchlistsEnvelope: Decodable { let data: [Watchlist] }

// ── Item 23: entity breach exposure ────────────────────────────────────────

struct EntityExposure: Codable, Hashable {
    let breach_count: Int
    let first_breach_date: String?
    let last_breach_date: String?
    let data_classes: [String]
}

struct EntityBreach: Codable, Identifiable, Hashable {
    let breach_id: String
    let item_uid: String?
    let name: String?
    let title: String?
    let domain: String?
    let breach_date: String?
    let pwn_count: Int?
    let data_classes: [String]
    let verified: Bool?
    let sensitive: Bool?
    var id: String { breach_id }
}
struct EntityBreachesEnvelope: Decodable {
    let data: [EntityBreach]
    let exposure: EntityExposure?
}

// ── Item 19: entity graph canvas ───────────────────────────────────────────

struct GraphNode: Codable, Identifiable, Hashable {
    let id: String
    let type: String
    let value: String
    let label: String
    let mention_count: Int
}

struct GraphEdge: Codable, Hashable, Identifiable {
    let source: String
    let target: String
    let relationship: String      // co_mention | pivot_discovered | asserted
    let weight: Double
    /// Populated once the correlation pod (item 22) has scored the edge.
    let pmi: Double?
    let lift: Double?
    let co_count: Int?
    var id: String { "\(source)|\(target)|\(relationship)" }
}

/// Truncation is reported, never hidden — a canvas that silently drops half a
/// graph teaches an analyst to trust a picture that is lying to them.
struct GraphMeta: Codable, Hashable {
    let node_count: Int
    let max_nodes: Int
    let hubs_collapsed: Int
    let nodes_over_cap: Int
    let truncated: Bool
    let rel_types: String?
}

/// NOTE the name: `EntityGraph` is already taken by `Search/SearchModels.swift`
/// (a different, poorer shape built on `EntityNode` and carrying no `meta`).
/// Two same-named types in one module is a hard compile error, so the richer
/// roadmap-19 payload — the one `GraphCanvasView` renders — is `EntityEgoGraph`.
struct EntityEgoGraph: Decodable {
    let nodes: [GraphNode]
    let edges: [GraphEdge]
    let meta: GraphMeta?
}

// ── Item 30: source reliability ────────────────────────────────────────────

/// `rated == false` means "no fetch history in the window" — a DIFFERENT claim
/// from a bad score, and the UI must render it as "unrated", never as zero.
struct SourceTrust: Codable, Hashable {
    let rated: Bool
    let reliability: Double?
    let grade: String?
    let successRate: Double?
    let freshness: Double?
    let anomalyRate: Double?
    let volumeStability: Double?
    let runs30d: Int?
    let anomalies30d: Int?
    let quarantined: Bool?
}

// ── Item 24: breach exposure monitors ──────────────────────────────────────

struct BreachMonitor: Codable, Identifiable, Hashable {
    let id: String
    let kind: String              // email | domain | username | phone
    let label: String?
    let rule_id: String?
    let created_at: String
    let last_checked_at: String?
    let hit_count: Int?
}
struct BreachMonitorsEnvelope: Decodable { let data: [BreachMonitor] }

/// A monitor hit is NOT shaped like an `EntityBreach`: `monitor_hits()` in
/// core/breach_monitor.c emits the catalog fields under a nested `breach`
/// object, with uid/type/has_secret/count/first_seen at the top level. Reusing
/// EntityBreach here made the decode *throw* (its `data_classes` is
/// non-optional and lives one level down), losing the whole page.
struct BreachMonitorHit: Codable, Identifiable, Hashable {
    let uid: String                 // "breach:<keyid>" — open via the intel route
    let type: String?
    let breach_id: String?
    let has_secret: Bool?
    let count: Int?
    let first_seen: String?
    let breach: BreachCatalogInfo?
    var id: String { uid }
}

struct BreachCatalogInfo: Codable, Hashable {
    let name: String?
    let title: String?
    let domain: String?
    let breach_date: String?
    let pwn_count: Int?
    let data_classes: [String]?
    let verified: Bool?
    let sensitive: Bool?
}

struct BreachMonitorHitsEnvelope: Decodable { let data: [BreachMonitorHit] }

// ── Item 38: saved searches, history, permalinks ───────────────────────────

struct SavedSearch: Codable, Identifiable, Hashable {
    let id: String
    let name: String?
    let kind: String              // intel | osint | entity | breach | map
    let params: [String: AnyCodable]?
    let pinned: Bool
    let created_at: String
    let last_run_at: String?
    let run_count: Int
}
struct SavedSearchesEnvelope: Decodable { let data: [SavedSearch] }
struct SavedSearchEnvelope: Decodable { let data: SavedSearch }

struct SearchHistoryEntry: Codable, Identifiable, Hashable {
    let id: Int
    let kind: String
    let params: [String: AnyCodable]?
    let result_count: Int?
    let ts: String
}
struct SearchHistoryEnvelope: Decodable { let data: [SearchHistoryEntry] }

/// Wrapped: `permalinkapi()` replies `{"data":{"token":…,"version":"v1",…}}`.
struct PermalinkTokenPayload: Decodable {
    let token: String
    let version: String?
}
struct PermalinkTokenEnvelope: Decodable { let data: PermalinkTokenPayload }
struct PermalinkState: Decodable { let data: [String: AnyCodable] }

// ── Item 17: evidence / chain of custody ───────────────────────────────────

struct EvidenceRecord: Codable, Identifiable, Hashable {
    let id: String
    let captured_at: String
    let source_id: String
    let request_url: String?
    let request_method: String?
    let response_status: Int?
    let content_sha256: String
    let content_bytes: Int?
    let content_type: String?
    let chain_seq: Int?
    /// False once the LRU reaper has unlinked the blob. The ROW survives on
    /// purpose — deleting it would break the hash chain and be
    /// indistinguishable from tampering — so the UI says "evicted", not "gone".
    let blob_present: Bool?
}
struct EvidenceEnvelope: Decodable { let data: [EvidenceRecord] }

struct EvidenceVerifyResult: Decodable {
    let ok: Bool
    let checked: Int?
    let broken_at: Int?
    let reason: String?
}

// ── Item 27: media assets ──────────────────────────────────────────────────

struct MediaAsset: Codable, Identifiable, Hashable {
    let id: String
    let url: String
    let sha256: String?
    let bytes: Int?
    let content_type: String?
    let width: Int?
    let height: Int?
    let exif: [String: AnyCodable]?
    let ocr_text: String?
    let analyzed_at: String?

    /// EXIF GPS, when the image carried one. This is how an item with no
    /// coordinates of its own gets on the map.
    ///
    /// The server nests this: `media_exif_to_json()` in core/media.c emits
    /// `{"gps":{"lat":…,"lon":…}}`, NOT flat `gps_lat`/`gps_lon`. An earlier
    /// version of this accessor read the flat form and therefore returned nil
    /// for every geotagged image — the feature silently did nothing. The flat
    /// form is still accepted as a fallback in case the shape ever changes.
    ///
    /// (0,0) is rejected: null island is overwhelmingly a zeroed field rather
    /// than a real fix in the Gulf of Guinea.
    var exifCoordinate: CLLocationCoordinate2D? {
        func num(_ v: Any?) -> Double? {
            if let n = v as? NSNumber { return n.doubleValue }
            if let d = v as? Double { return d }
            if let s = v as? String { return Double(s) }
            return nil
        }
        var lat: Double?, lon: Double?
        if let gps = exif?["gps"]?.value as? [String: Any] {
            lat = num(gps["lat"]); lon = num(gps["lon"])
        }
        if lat == nil { lat = num(exif?["gps_lat"]?.value) }
        if lon == nil { lon = num(exif?["gps_lon"]?.value) }
        guard let la = lat, let lo = lon,
              la.isFinite, lo.isFinite,
              la >= -90, la <= 90, lo >= -180, lo <= 180,
              !(la == 0 && lo == 0) else { return nil }
        return CLLocationCoordinate2D(latitude: la, longitude: lo)
    }
}
struct MediaAssetsEnvelope: Decodable { let data: [MediaAsset] }

// ── Item 28: camera stills ─────────────────────────────────────────────────

struct CameraStill: Codable, Identifiable, Hashable {
    let id: String
    let captured_at: String
    let sha256: String
    let bytes: Int?
    let width: Int?
    let height: Int?
    /// nil means "cannot tell" (no image decoder on the server), NOT "no
    /// change". The UI must not render nil as unchanged.
    let scene_changed: Int?
    let blob_present: Bool?
}
struct CameraStillsEnvelope: Decodable {
    let data: [CameraStill]
    let meta: CameraStillsMeta?
}
struct CameraStillsMeta: Decodable, Hashable {
    let scene_change_available: Bool?
    let bytes_preserved: Bool?
}

// ── Item 25 / 29: dedup + translation, as they ride on intel items ─────────

/// Attached to an intel item when `?collapse=1` folded near-duplicates.
/// The duplicate list is a FEATURE — "20 independent sources reported this"
/// is corroboration, and hiding it would throw away the signal.
struct ClusterInfo: Codable, Hashable {
    let cluster_id: String?
    let cluster_size: Int?
    let cluster_source_count: Int?
    let duplicates: [ClusterDuplicate]?
    let duplicates_truncated: Bool?
}
/// Both fields are optional because simhash.c writes `cJSON_CreateNull()` for
/// either when the underlying column is NULL — and a single null would
/// otherwise fail the decode of the WHOLE enclosing intel item, losing the
/// article to save a duplicate row.
struct ClusterDuplicate: Codable, Hashable, Identifiable {
    let source_id: String?
    let uid: String?
    var id: String { uid ?? "\(source_id ?? "?")-unknown" }
}

/// Attached when `?lang_view=` requested a translation. `machine: true` is
/// load-bearing — machine output must be labelled as such, never presented as
/// the source's own words.
struct TranslationInfo: Codable, Hashable {
    let lang: String
    let machine: Bool
    let engine: String?
    let translated_at: String?
    let title: String?
    let summary: String?
    let body: String?
}
