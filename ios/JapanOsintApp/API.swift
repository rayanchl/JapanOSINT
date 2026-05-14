import Foundation

enum APIError: LocalizedError {
    case badURL
    case http(Int, String)
    case decoding(Error)

    var errorDescription: String? {
        switch self {
        case .badURL: return "Bad backend URL"
        case .http(let code, let body): return "HTTP \(code): \(body.prefix(200))"
        case .decoding(let e): return "Decoding error: \(e.localizedDescription)"
        }
    }
}

struct API: Sendable {
    let baseURL: String

    /// Re-read every call so the user-chosen value in Settings takes effect
    /// without restart. Falls back to 25s when no value has been written yet.
    /// Marked `nonisolated` so the API's `nonisolated` `data(for:)` path can
    /// reference it as a default-argument expression without crossing actors.
    nonisolated static var userDefaultTimeout: TimeInterval {
        let raw = UserDefaults.standard.integer(forKey: "apiDefaultTimeoutSeconds")
        return raw > 0 ? TimeInterval(raw) : 25
    }

    private func makeURL(_ path: String, query: [URLQueryItem] = []) throws -> URL {
        let trimmed = baseURL.trimmingCharacters(in: CharacterSet(charactersIn: "/"))
        guard var comps = URLComponents(string: trimmed + path) else {
            throw APIError.badURL
        }
        if !query.isEmpty { comps.queryItems = query }
        guard let url = comps.url else { throw APIError.badURL }
        return url
    }

    /// Pass `timeout: nil` to skip client-side timeout enforcement entirely
    /// — only the server's internal collector timeouts will bound the wait.
    /// Used by `post()` for long-running endpoints like intel collector runs
    /// (Wayback CDX, GitHub leaks, etc.) which can take 30–60 s legitimately.
    private func request(_ url: URL, method: String = "GET",
                         body: Data? = nil, timeout: TimeInterval? = API.userDefaultTimeout) async throws -> Data {
        var req = URLRequest(url: url)
        if let timeout {
            req.timeoutInterval = timeout
        } else {
            // No client-side timeout: pick a large value that effectively
            // never fires. URLSession honours timeoutIntervalForResource on
            // its config (default 7 days) — this caps the request leg.
            req.timeoutInterval = 24 * 60 * 60
        }
        req.httpMethod = method
        if body != nil { req.setValue("application/json", forHTTPHeaderField: "Content-Type") }
        req.httpBody = body
        let (data, resp) = try await URLSession.shared.data(for: req)
        guard let http = resp as? HTTPURLResponse else { throw APIError.http(-1, "no response") }
        guard (200..<300).contains(http.statusCode) else {
            throw APIError.http(http.statusCode, String(data: data, encoding: .utf8) ?? "")
        }
        return data
    }

    private func get<T: Decodable>(_ path: String, query: [URLQueryItem] = []) async throws -> T {
        let url = try makeURL(path, query: query)
        let data = try await request(url)
        do { return try JSONDecoder().decode(T.self, from: data) }
        catch { throw APIError.decoding(error) }
    }

    /// POST helper. By default, no client-side timeout — the server's own
    /// per-collector AbortController bounds the wait. Pass `timeout:` to
    /// re-enable a client cap if needed.
    private func post<T: Decodable>(_ path: String, query: [URLQueryItem] = [],
                                     body: Data? = nil, timeout: TimeInterval? = nil) async throws -> T {
        let url = try makeURL(path, query: query)
        let data = try await request(url, method: "POST", body: body ?? Data(), timeout: timeout)
        do { return try JSONDecoder().decode(T.self, from: data) }
        catch { throw APIError.decoding(error) }
    }

    /// PUT helper. Mirrors `post()` but uses the user-default timeout since
    /// PUTs in this app go to lightweight metadata endpoints (api-keys store,
    /// future preferences) rather than long-running collector triggers.
    private func put<T: Decodable>(_ path: String, body: Data,
                                    timeout: TimeInterval? = API.userDefaultTimeout) async throws -> T {
        let url = try makeURL(path)
        let data = try await request(url, method: "PUT", body: body, timeout: timeout)
        do { return try JSONDecoder().decode(T.self, from: data) }
        catch { throw APIError.decoding(error) }
    }

    // ── Layers / Sources / Status ──────────────────────────────────────────
    func health() async throws -> Data {
        try await request(try makeURL("/api/health"))
    }
    func layers() async throws -> [LayerDef] {
        try await get("/api/layers")
    }
    func sources() async throws -> [DBRowAny] {
        try await get("/api/sources")
    }
    func status() async throws -> StatusEnvelope {
        try await get("/api/status")
    }

    /// One-shot manual probe for a keyed source. Server uses the configured
    /// API key as the auth header so the request actually reaches the API
    /// instead of returning 401/403. Result is the refreshed StatusRow.
    func probeSource(_ id: String) async throws -> StatusRow {
        try await post("/api/status/\(id)/probe")
    }

    /// Persist whether the scheduler is allowed to auto-probe a keyed
    /// source. `allow:false` puts the row back into the gated bucket.
    func setProbeConsent(_ id: String, allow: Bool) async throws -> StatusRow {
        let body = try JSONSerialization.data(withJSONObject: ["allow": allow])
        return try await post("/api/status/\(id)/consent", body: body)
    }

    /// Backfill for the Camera Discovery view. Returns historical discovery
    /// events flattened into the same `CameraEvent` shape the WS path emits
    /// so callers can merge both feeds into one list.
    func cameraDiscoveryFeed(limit: Int = 500, cursor: String? = nil, channel: String? = nil)
    async throws -> (events: [CameraEvent], cursor: String?) {
        var q: [URLQueryItem] = [URLQueryItem(name: "limit", value: String(limit))]
        if let cursor { q.append(URLQueryItem(name: "cursor", value: cursor)) }
        if let channel { q.append(URLQueryItem(name: "channel", value: channel)) }
        let url = try makeURL("/api/data/cameras/discovery-feed", query: q)
        let data = try await request(url)
        guard let obj = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw APIError.decoding(NSError(domain: "discovery-feed", code: -1,
                                            userInfo: [NSLocalizedDescriptionKey: "bad JSON"]))
        }
        let envelopes = (obj["events"] as? [[String: Any]]) ?? []
        let events = envelopes.compactMap { CameraEvent.fromBroadcast(envelope: $0) }
        return (events, obj["cursor"] as? String)
    }

    // ── GeoJSON for one layer ──────────────────────────────────────────────
    /// `nonisolated` + detached decode keep multi-MB GeoJSON deserialisation
    /// off the MainActor so the map stays responsive during heavy fetches.
    /// No bbox: each layer is fetched once-and-forever; viewport movement
    /// no longer drives reloads.
    nonisolated func data(for layer: LayerDef,
                          at: Date? = nil,
                          windowSeconds: Int? = nil) async throws -> FeatureCollection {
        var query: [URLQueryItem] = []
        if let at {
            let f = ISO8601DateFormatter()
            f.formatOptions = [.withInternetDateTime]
            query.append(URLQueryItem(name: "at", value: f.string(from: at)))
        }
        if let w = windowSeconds {
            query.append(URLQueryItem(name: "window", value: String(w)))
        }
        let url = try makeURL(layer.dataEndpoint, query: query)
        let bytes = try await request(url, timeout: 40)
        let layerId = layer.id
        return try await Task.detached(priority: .userInitiated) {
            do {
                let raw = try JSONDecoder().decode(FeatureCollection.self, from: bytes)
                return raw.tagged(layerId: layerId)
            } catch {
                throw APIError.decoding(error)
            }
        }.value
    }

    // ── Intel (non-spatial sources) ────────────────────────────────────────
    func intelSources() async throws -> IntelSourcesEnvelope {
        try await get("/api/intel/sources")
    }
    func intelItems(source: String? = nil,
                    q: String? = nil,
                    qAlt: String? = nil,
                    lang: String? = nil,
                    since: String? = nil,
                    limit: Int = 50,
                    cursor: String? = nil) async throws -> IntelItemsEnvelope {
        var qs: [URLQueryItem] = [URLQueryItem(name: "limit", value: String(limit))]
        if let source { qs.append(URLQueryItem(name: "source", value: source)) }
        if let q, !q.isEmpty { qs.append(URLQueryItem(name: "q", value: q)) }
        if let qAlt, !qAlt.isEmpty { qs.append(URLQueryItem(name: "qAlt", value: qAlt)) }
        if let lang { qs.append(URLQueryItem(name: "lang", value: lang)) }
        if let since { qs.append(URLQueryItem(name: "since", value: since)) }
        if let cursor { qs.append(URLQueryItem(name: "cursor", value: cursor)) }
        return try await get("/api/intel/items", query: qs)
    }
    func intelItem(uid: String) async throws -> IntelItem {
        let env: IntelItemEnvelope = try await get("/api/intel/items/\(uid)")
        return env.data
    }
    /// User-initiated trigger. Server runs the named intel collector and
    /// upserts items synchronously; the response carries the result count.
    /// No client-side timeout — long-running collectors (Wayback CDX,
    /// GitHub leaks, …) are bounded by their own internal AbortController
    /// timeouts on the server side. The user keeps seeing the spinner
    /// until the server actually replies.
    func intelRunSource(_ id: String) async throws -> IntelRunResult {
        try await post("/api/intel/sources/\(id)/run")
    }

    // ── Geocoding ──────────────────────────────────────────────────────────
    func geocode(query: String, queryAlt: String? = nil) async throws -> GeocodeResponse {
        var qs: [URLQueryItem] = [URLQueryItem(name: "q", value: query)]
        if let queryAlt, !queryAlt.isEmpty {
            qs.append(URLQueryItem(name: "qAlt", value: queryAlt))
        }
        return try await get("/api/geocode", query: qs)
    }
    func reverseGeocode(lat: Double, lon: Double) async throws -> ReverseGeocodeResponse {
        try await get("/api/geocode/reverse", query: [
            URLQueryItem(name: "lat", value: String(lat)),
            URLQueryItem(name: "lon", value: String(lon))
        ])
    }

    // ── Database ───────────────────────────────────────────────────────────
    func dbTables() async throws -> [DBTable] {
        try await get("/api/db/tables")
    }
    func dbRows(table: String, query: String, orderBy: String?, orderDir: String,
                limit: Int = 50, offset: Int = 0) async throws -> DBPage {
        var q: [URLQueryItem] = [
            URLQueryItem(name: "limit",  value: String(limit)),
            URLQueryItem(name: "offset", value: String(offset))
        ]
        if !query.isEmpty { q.append(URLQueryItem(name: "q", value: query)) }
        if let ob = orderBy {
            q.append(URLQueryItem(name: "orderBy",  value: ob))
            q.append(URLQueryItem(name: "orderDir", value: orderDir))
        }
        return try await get("/api/db/tables/\(table)", query: q)
    }
    func scheduler() async throws -> SchedulerEnvelope {
        try await get("/api/db/scheduler")
    }

    // ── Follow / Cameras / Triggers ────────────────────────────────────────
    func recentFollow(limit: Int = 200) async throws -> FollowEnvelope {
        try await get("/api/follow/recent", query: [URLQueryItem(name: "limit", value: String(limit))])
    }
    func triggerCameraDiscovery() async throws {
        let url = try makeURL("/api/data/cameras/trigger")
        _ = try await request(url, method: "POST", body: Data("{}".utf8), timeout: 15)
    }

    // ── API keys overlay ───────────────────────────────────────────────────
    func apiKeys() async throws -> [ApiKeyMeta] {
        try await get("/api/keys")
    }

    func apiKeyValue(name: String) async throws -> ApiKeyValue {
        try await get("/api/keys/\(name)")
    }

    /// Pass an empty string to clear the overlay (server falls back to the
    /// .env-baked value if any).
    @discardableResult
    func apiKeySet(name: String, value: String) async throws -> ApiKeyMeta {
        let body = try JSONEncoder().encode(["value": value])
        return try await put("/api/keys/\(name)", body: body)
    }

    /// Triggers `node --watch` to respawn the server. The HTTP request races
    /// the process teardown — caller should treat any post-POST connection
    /// reset as expected and switch to /api/health polling for liveness.
    func restartServer() async throws {
        let url = try makeURL("/api/admin/restart")
        _ = try await request(url, method: "POST", body: Data("{}".utf8), timeout: 10)
    }
}
