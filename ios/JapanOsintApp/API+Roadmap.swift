import Foundation

// ─────────────────────────────────────────────────────────────────────────────
// Client for the roadmap backends. An extension in its own file so this large
// addition never collides with concurrent edits to API.swift — the only change
// there was widening the five request helpers from `private` to internal so an
// extension outside that file can reach them.
//
// Two endpoints deliberately return `Data` rather than a decoded type: report
// and export are streamed, chunked, non-JSON responses (Markdown/HTML, CSV,
// GeoJSON), and pretending otherwise would mean buffering and re-encoding a
// document the user is about to hand to a share sheet.
// ─────────────────────────────────────────────────────────────────────────────

extension API {

    private func enc(_ v: Any) throws -> Data {
        try JSONSerialization.data(withJSONObject: v)
    }

    // ── Item 14: cases ─────────────────────────────────────────────────────

    func cases(status: String? = nil, limit: Int = 50,
               cursor: String? = nil) async throws -> CasesEnvelope {
        var q = [URLQueryItem(name: "limit", value: String(limit))]
        if let status { q.append(URLQueryItem(name: "status", value: status)) }
        if let cursor { q.append(URLQueryItem(name: "cursor", value: cursor)) }
        return try await get("/api/cases", query: q)
    }

    func caseCreate(name: String, summary: String? = nil,
                    priority: Int = 0) async throws -> CaseDetail {
        var body: [String: Any] = ["name": name, "priority": priority]
        if let summary { body["summary"] = summary }
        let env: CaseEnvelope = try await post("/api/cases", body: try enc(body),
                                               timeout: 20)
        return env.data
    }

    func caseDetail(_ id: String) async throws -> CaseDetail {
        let env: CaseEnvelope = try await get("/api/cases/\(esc(id))")
        return env.data
    }

    func caseUpdate(_ id: String, fields: [String: Any]) async throws -> CaseDetail {
        let env: CaseEnvelope = try await patch("/api/cases/\(esc(id))",
                                                body: try enc(fields))
        return env.data
    }

    func caseDelete(_ id: String) async throws {
        try await delete("/api/cases/\(esc(id))")
    }

    func caseItems(_ id: String, refType: String? = nil, limit: Int = 100,
                   cursor: String? = nil) async throws -> CaseItemsEnvelope {
        var q = [URLQueryItem(name: "limit", value: String(limit))]
        if let refType { q.append(URLQueryItem(name: "ref_type", value: refType)) }
        if let cursor { q.append(URLQueryItem(name: "cursor", value: cursor)) }
        return try await get("/api/cases/\(esc(id))/items", query: q)
    }

    /// Pin something into a case. `snapshot` is optional — the server
    /// synthesizes a display copy when it is omitted, which is the normal path.
    @discardableResult
    func casePin(_ id: String, refType: String, refId: String,
                 label: String? = nil) async throws -> CaseItemRef? {
        var body: [String: Any] = ["ref_type": refType, "ref_id": refId]
        if let label { body["label"] = label }
        struct One: Decodable { let data: CaseItemRef? }
        let env: One = try await post("/api/cases/\(esc(id))/items",
                                      body: try enc(body), timeout: 20)
        return env.data
    }

    func caseUnpin(_ id: String, refType: String, refId: String) async throws {
        let url = try makeURL("/api/cases/\(esc(id))/items", query: [
            URLQueryItem(name: "ref_type", value: refType),
            URLQueryItem(name: "ref_id", value: refId)
        ])
        _ = try await request(url, method: "DELETE")
    }

    func caseActivity(_ id: String, limit: Int = 100,
                      cursor: String? = nil) async throws -> CaseActivityEnvelope {
        var q = [URLQueryItem(name: "limit", value: String(limit))]
        if let cursor { q.append(URLQueryItem(name: "cursor", value: cursor)) }
        return try await get("/api/cases/\(esc(id))/activity", query: q)
    }

    func caseComment(_ id: String, body text: String,
                     mentions: [String] = []) async throws {
        struct Ignored: Decodable {}
        let _: Ignored = try await post("/api/cases/\(esc(id))/activity",
            body: try enc(["body": text, "mentions": mentions]), timeout: 20)
    }

    func caseMembers(_ id: String) async throws -> [CaseMemberRef] {
        let env: CaseMembersEnvelope = try await get("/api/cases/\(esc(id))/members")
        return env.data
    }

    /// Replace the roster. Server-side this is analyst+ OR case lead — a case
    /// `viewer` who could rewrite the roster could promote themselves.
    @discardableResult
    func caseMembersSet(_ id: String,
                        members: [[String: Any]]) async throws -> [CaseMemberRef] {
        let env: CaseMembersEnvelope = try await put("/api/cases/\(esc(id))/members",
                                                     body: try enc(["members": members]))
        return env.data
    }

    // ── Item 16: report (streamed, non-JSON) ───────────────────────────────

    /// Returns the raw document bytes. `format` is "md" or "html" — PDF is
    /// deliberately not a server format; render the HTML on-device instead.
    func caseReport(_ id: String, format: String = "md",
                    sections: [String]? = nil) async throws -> Data {
        var body: [String: Any] = ["format": format]
        if let sections { body["sections"] = sections }
        let url = try makeURL("/api/cases/\(esc(id))/report")
        return try await request(url, method: "POST",
                                 body: try enc(body), timeout: 120)
    }

    // ── Item 7: export (streamed, non-JSON) ────────────────────────────────

    func exportRows(kind: String, format: String,
                    query: [URLQueryItem] = []) async throws -> Data {
        var q = query
        q.append(URLQueryItem(name: "format", value: format))
        let url = try makeURL("/api/export/\(esc(kind))", query: q)
        return try await request(url, timeout: 180)
    }

    // ── Item 15: annotations ───────────────────────────────────────────────

    /// Notes for one reference. `includeDeleted` asks the server for
    /// tombstones too (analyst+ only, silently downgraded for viewers) —
    /// without it, notes deleted in an earlier session simply do not come
    /// back, so the audit trail looks complete when it isn't.
    func annotations(refType: String, refId: String, caseId: String? = nil,
                     limit: Int = 100,
                     includeDeleted: Bool = true) async throws -> [Annotation] {
        var q = [URLQueryItem(name: "ref_type", value: refType),
                 URLQueryItem(name: "ref_id", value: refId),
                 URLQueryItem(name: "limit", value: String(limit))]
        if let caseId { q.append(URLQueryItem(name: "case_id", value: caseId)) }
        if includeDeleted { q.append(URLQueryItem(name: "include_deleted", value: "1")) }
        let env: AnnotationsEnvelope = try await get("/api/annotations", query: q)
        return env.data
    }

    /// Every note in a case, in one request. The server honours `case_id`
    /// without a ref pair; the per-ref call above would otherwise force one
    /// request per pinned finding.
    func caseAnnotations(caseId: String, limit: Int = 200,
                         includeDeleted: Bool = true) async throws -> [Annotation] {
        var q = [URLQueryItem(name: "case_id", value: caseId),
                 URLQueryItem(name: "limit", value: String(limit))]
        if includeDeleted { q.append(URLQueryItem(name: "include_deleted", value: "1")) }
        let env: AnnotationsEnvelope = try await get("/api/annotations", query: q)
        return env.data
    }

    @discardableResult
    func annotationCreate(refType: String, refId: String, bodyMd: String,
                          caseId: String? = nil) async throws -> Annotation {
        var body: [String: Any] = ["ref_type": refType, "ref_id": refId,
                                   "body_md": bodyMd]
        if let caseId { body["case_id"] = caseId }
        let env: AnnotationEnvelope = try await post("/api/annotations",
                                                     body: try enc(body), timeout: 20)
        return env.data
    }

    @discardableResult
    func annotationUpdate(_ id: String, bodyMd: String) async throws -> Annotation {
        let env: AnnotationEnvelope = try await patch("/api/annotations/\(esc(id))",
            body: try enc(["body_md": bodyMd]))
        return env.data
    }

    /// Soft delete — returns the tombstone, which the UI renders rather than
    /// hiding. A note that vanished silently would undermine the audit trail.
    @discardableResult
    func annotationDelete(_ id: String) async throws -> Annotation {
        let url = try makeURL("/api/annotations/\(esc(id))")
        let data = try await request(url, method: "DELETE")
        return try JSONDecoder().decode(AnnotationEnvelope.self, from: data).data
    }

    // ── Item 18: timeline ──────────────────────────────────────────────────

    func timeline(since: String? = nil, until: String? = nil,
                  bucket: String = "day", source: String? = nil,
                  caseId: String? = nil, bbox: String? = nil,
                  limit: Int = 100, cursor: String? = nil) async throws -> TimelineEnvelope {
        var q = [URLQueryItem(name: "bucket", value: bucket),
                 URLQueryItem(name: "limit", value: String(limit))]
        if let since { q.append(URLQueryItem(name: "since", value: since)) }
        if let until { q.append(URLQueryItem(name: "until", value: until)) }
        if let source { q.append(URLQueryItem(name: "source", value: source)) }
        if let caseId { q.append(URLQueryItem(name: "case_id", value: caseId)) }
        if let bbox { q.append(URLQueryItem(name: "bbox", value: bbox)) }
        if let cursor { q.append(URLQueryItem(name: "cursor", value: cursor)) }
        return try await get("/api/timeline", query: q)
    }

    // ── Item 11: alert inbox ───────────────────────────────────────────────

    func alertInbox(unreadOnly: Bool = false,
                    limit: Int = 100) async throws -> [AlertInboxEvent] {
        var q = [URLQueryItem(name: "limit", value: String(limit))]
        if unreadOnly { q.append(URLQueryItem(name: "unread", value: "1")) }
        let env: AlertInboxEnvelope = try await get("/api/alert-events", query: q)
        return env.data
    }

    func alertUnreadCount() async throws -> Int {
        let c: UnreadCount = try await get("/api/alert-events/unread-count")
        return c.unread
    }

    func alertMarkRead(_ eventId: String) async throws {
        struct Ignored: Decodable {}
        let _: Ignored = try await post("/api/alert-events/\(esc(eventId))/read",
                                        timeout: 10)
    }

    func alertMarkAllRead() async throws {
        struct Ignored: Decodable {}
        let _: Ignored = try await post("/api/alert-events/read-all", timeout: 15)
    }

    // ── Item 10: rule preview ──────────────────────────────────────────────

    func alertPreview(predicate: [String: Any],
                      since: String? = nil) async throws -> AlertPreviewResult {
        var body: [String: Any] = ["predicate": predicate]
        if let since { body["since"] = since }
        return try await post("/api/alerts/preview", body: try enc(body), timeout: 30)
    }

    // ── Item 9: areas of interest ──────────────────────────────────────────

    func aois() async throws -> [AreaOfInterest] {
        let env: AOIEnvelope = try await get("/api/aoi")
        return env.data
    }

    @discardableResult
    func aoiCreate(name: String, kind: String, geometry: Any) async throws -> AreaOfInterest {
        let env: AOIOneEnvelope = try await post("/api/aoi",
            body: try enc(["name": name, "kind": kind, "geometry": geometry]),
            timeout: 20)
        return env.data
    }

    func aoiDelete(_ id: String) async throws {
        try await delete("/api/aoi/\(esc(id))")
    }

    // ── Item 21: watchlists ────────────────────────────────────────────────

    func watchlists() async throws -> [Watchlist] {
        let env: WatchlistsEnvelope = try await get("/api/watchlists")
        return env.data
    }

    /// entity_ids must be non-empty — an empty watchlist is one that can never
    /// fire, which the server rejects rather than silently accepting.
    @discardableResult
    func watchlistCreate(name: String, entityIds: [String]) async throws -> Watchlist {
        struct One: Decodable { let data: Watchlist }
        let env: One = try await post("/api/watchlists",
            body: try enc(["name": name, "entity_ids": entityIds]), timeout: 20)
        return env.data
    }

    func watchlistDelete(_ id: String) async throws {
        try await delete("/api/watchlists/\(esc(id))")
    }

    // ── Item 23: entity breach exposure ────────────────────────────────────

    func entityBreaches(type: String, id: String,
                        limit: Int = 50) async throws -> EntityBreachesEnvelope {
        try await get("/api/entities/\(esc(type))/\(esc(id))/breaches",
                      query: [URLQueryItem(name: "limit", value: String(limit))])
    }

    // ── Item 19: entity graph ──────────────────────────────────────────────

    /// Named `entityEgoGraph` rather than `entityGraph` because API.swift
    /// already has an `entityGraph(_:_:depth:)` returning the older
    /// `Search/SearchModels.EntityGraph` shape. Two overloads answering the
    /// same question with different types is how a call site quietly gets the
    /// wrong one — so the richer roadmap-19 call is separately named.
    func entityEgoGraph(type: String, id: String, depth: Int = 1,
                        relTypes: [String]? = nil, excludeHubs: Int? = nil,
                        maxNodes: Int? = nil) async throws -> EntityEgoGraph {
        var q = [URLQueryItem(name: "depth", value: String(depth))]
        if let relTypes, !relTypes.isEmpty {
            q.append(URLQueryItem(name: "rel_types", value: relTypes.joined(separator: ",")))
        }
        if let excludeHubs { q.append(URLQueryItem(name: "exclude_hubs", value: String(excludeHubs))) }
        if let maxNodes { q.append(URLQueryItem(name: "max_nodes", value: String(maxNodes))) }
        return try await get("/api/entities/\(esc(type))/\(esc(id))/graph", query: q)
    }

    // ── Item 24: breach monitors ───────────────────────────────────────────

    func breachMonitors() async throws -> [BreachMonitor] {
        let env: BreachMonitorsEnvelope = try await get("/api/breach-monitors")
        return env.data
    }

    /// The plaintext identifier is sent once and never stored server-side —
    /// only its SHA-1 is kept, and nothing echoes it back.
    @discardableResult
    func breachMonitorCreate(kind: String, value: String,
                             label: String? = nil) async throws -> BreachMonitor {
        var body: [String: Any] = ["kind": kind, "value": value]
        if let label { body["label"] = label }
        struct One: Decodable { let data: BreachMonitor }
        let env: One = try await post("/api/breach-monitors",
                                      body: try enc(body), timeout: 30)
        return env.data
    }

    func breachMonitorDelete(_ id: String) async throws {
        try await delete("/api/breach-monitors/\(esc(id))")
    }

    func breachMonitorHits(_ id: String) async throws -> [BreachMonitorHit] {
        let env: BreachMonitorHitsEnvelope =
            try await get("/api/breach-monitors/\(esc(id))/hits")
        return env.data
    }

    // ── Item 38: saved searches, history, permalinks ───────────────────────

    func savedSearches(kind: String? = nil) async throws -> [SavedSearch] {
        var q: [URLQueryItem] = []
        if let kind { q.append(URLQueryItem(name: "kind", value: kind)) }
        let env: SavedSearchesEnvelope = try await get("/api/saved-searches", query: q)
        return env.data
    }

    @discardableResult
    func savedSearchCreate(name: String, kind: String,
                           params: [String: Any]) async throws -> SavedSearch {
        let env: SavedSearchEnvelope = try await post("/api/saved-searches",
            body: try enc(["name": name, "kind": kind, "params": params]), timeout: 20)
        return env.data
    }

    func savedSearchDelete(_ id: String) async throws {
        try await delete("/api/saved-searches/\(esc(id))")
    }

    func savedSearchRun(_ id: String) async throws {
        struct Ignored: Decodable {}
        let _: Ignored = try await post("/api/saved-searches/\(esc(id))/run", timeout: 20)
    }

    /// Convert a saved intel search into an alert rule. Only `kind == "intel"`
    /// maps cleanly; the server returns a clear 400 for other kinds rather than
    /// producing a rule that matches nothing.
    func savedSearchToAlert(_ id: String, channels: [[String: Any]]) async throws {
        struct Ignored: Decodable {}
        let _: Ignored = try await post("/api/saved-searches/\(esc(id))/to-alert",
            body: try enc(["channels": channels]), timeout: 20)
    }

    func searchHistory(limit: Int = 100) async throws -> [SearchHistoryEntry] {
        let env: SearchHistoryEnvelope = try await get("/api/search-history",
            query: [URLQueryItem(name: "limit", value: String(limit))])
        return env.data
    }

    func searchHistoryClear() async throws {
        try await delete("/api/search-history")
    }

    func permalinkMint(_ state: [String: Any]) async throws -> String {
        let env: PermalinkTokenEnvelope = try await post("/api/permalink",
                                                         body: try enc(state), timeout: 15)
        return env.data.token
    }

    func permalinkResolve(_ token: String) async throws -> [String: AnyCodable] {
        let s: PermalinkState = try await get("/api/permalink/\(esc(token))")
        return s.data
    }

    // ── Item 17: evidence ──────────────────────────────────────────────────

    func evidence(forItem uid: String) async throws -> [EvidenceRecord] {
        let env: EvidenceEnvelope = try await get("/api/intel/items/\(esc(uid))/evidence")
        return env.data
    }

    /// Operator-gated. Returns raw captured bytes — untrusted third-party
    /// content, so never render it as markup.
    func evidenceRaw(_ id: String) async throws -> Data {
        let url = try makeURL("/api/evidence/\(esc(id))/raw")
        return try await request(url, timeout: 60)
    }

    func evidenceVerify() async throws -> EvidenceVerifyResult {
        try await get("/api/evidence/verify")
    }

    // ── Item 27: media assets ──────────────────────────────────────────────

    /// EXIF / pHash / OCR results for an item's images. `GET
    /// /api/intel/items/:uid/media` (media_list_for_item in core/media.h).
    func mediaAssets(forItem uid: String, limit: Int = 50) async throws -> [MediaAsset] {
        let env: MediaAssetsEnvelope = try await get(
            "/api/intel/items/\(esc(uid))/media",
            query: [URLQueryItem(name: "limit", value: String(limit))])
        return env.data
    }

    /// Update a saved search in place (rename / re-pin). The list endpoint
    /// exposes create/delete/run/to-alert; this fills the gap the UI needs.
    @discardableResult
    func savedSearchUpdate(_ id: String, fields: [String: Any]) async throws -> SavedSearch {
        let env: SavedSearchEnvelope = try await patch("/api/saved-searches/\(esc(id))",
                                                       body: try enc(fields))
        return env.data
    }

    // ── Item 28: camera stills ─────────────────────────────────────────────

    func cameraStills(_ cameraId: String,
                      limit: Int = 50) async throws -> CameraStillsEnvelope {
        try await get("/api/cameras/\(esc(cameraId))/stills",
                      query: [URLQueryItem(name: "limit", value: String(limit))])
    }

    func cameraStillRaw(_ stillId: String) async throws -> Data {
        let url = try makeURL("/api/camera-stills/\(esc(stillId))/raw")
        return try await request(url, timeout: 60)
    }

    // ── helpers ────────────────────────────────────────────────────────────

    /// Percent-escape a path segment. Ids here include uids like
    /// `geojson-file|fire-1` and `breach:email:ab…`, whose `|` and `:` must not
    /// reach the URL raw.
    private func esc(_ s: String) -> String {
        s.addingPercentEncoding(withAllowedCharacters: .alphanumerics
            .union(CharacterSet(charactersIn: "-._~"))) ?? s
    }
}
