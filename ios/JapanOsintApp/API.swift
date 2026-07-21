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

    /// Dedicated session so we control the resource ceiling. `URLSession.shared`
    /// defaults `timeoutIntervalForResource` to 7 days, so the `timeout: nil`
    /// POST path (long collector runs) could otherwise hang effectively
    /// forever if the server stalls without closing the connection. We bound
    /// the whole request leg to 120s and give every request a 30s default
    /// unless the call site overrides it explicitly.
    nonisolated static let session: URLSession = {
        let cfg = URLSessionConfiguration.default
        cfg.timeoutIntervalForRequest = 30
        cfg.timeoutIntervalForResource = 120
        cfg.waitsForConnectivity = true
        return URLSession(configuration: cfg)
    }()

    private nonisolated func makeURL(_ path: String, query: [URLQueryItem] = []) throws -> URL {
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
        do {
            return try await send(url, method: method, body: body, timeout: timeout)
        } catch APIError.http(401, let detail) {
            // Access token expired mid-session. Refresh once (against
            // Supabase directly — not via API, so no recursion). All
            // concurrent 401s coalesce onto ONE shared refresh task so we
            // don't burn Supabase's single-use rotating refresh token.
            let refreshed = await AuthTokenBox.shared.coalescedRefresh()
            guard refreshed else { throw APIError.http(401, detail) }
            // Only auto-replay idempotent reads. Replaying a POST/PUT/PATCH/
            // DELETE could double-create a rule, double-trigger a collector,
            // etc. — surface the auth error and let the caller decide.
            guard method == "GET" else { throw APIError.http(401, detail) }
            return try await send(url, method: method, body: body, timeout: timeout)
        }
    }

    private func send(_ url: URL, method: String,
                      body: Data?, timeout: TimeInterval?) async throws -> Data {
        var req = URLRequest(url: url)
        if let timeout {
            req.timeoutInterval = timeout
        } else {
            // "No client-side timeout" really means "bounded by the slow
            // collector ceiling". The session's 30s `timeoutIntervalForRequest`
            // would kill a legitimately long collector run that hasn't sent
            // bytes yet, so lift the per-request idle budget to the session's
            // 120s resource ceiling — that is now the true hard cap (vs. the
            // old 7-day URLSession.shared default that hung forever).
            req.timeoutInterval = 120
        }
        req.httpMethod = method
        if body != nil { req.setValue("application/json", forHTTPHeaderField: "Content-Type") }
        // Stamp Supabase bearer + active tenant when present. Absent in
        // legacy single-tenant mode — the server ignores both there.
        if let token = AuthTokenBox.shared.accessToken, !token.isEmpty {
            req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        }
        if let tid = AuthTokenBox.shared.tenantId, !tid.isEmpty {
            req.setValue(tid, forHTTPHeaderField: "X-Tenant-Id")
        }
        req.httpBody = body
        let (data, resp) = try await API.session.data(for: req)
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

    /// PATCH helper. Same shape as `put()` — used by alert-rule edits.
    private func patch<T: Decodable>(_ path: String, body: Data,
                                      timeout: TimeInterval? = API.userDefaultTimeout) async throws -> T {
        let url = try makeURL(path)
        let data = try await request(url, method: "PATCH", body: body, timeout: timeout)
        do { return try JSONDecoder().decode(T.self, from: data) }
        catch { throw APIError.decoding(error) }
    }

    /// DELETE helper — body-less, no response expected (204).
    private func delete(_ path: String) async throws {
        let url = try makeURL(path)
        _ = try await request(url, method: "DELETE")
    }

    // ── Identity / tenancy ─────────────────────────────────────────────────
    /// Resolved user + active tenant + memberships. Drives onboarding.
    func me() async throws -> MeResponse {
        try await get("/api/me")
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
    /// Set whether a source is cron-scheduled for the map (`map_cron`) or
    /// only grabbed live from the search tab (`search_only`). Both modes
    /// still write intel_items shown everywhere. Returns the updated row.
    func setSourceSchedule(_ id: String, mode: String) async throws -> DBRowAny {
        let body = try JSONSerialization.data(withJSONObject: ["mode": mode])
        return try await put("/api/sources/\(id)/schedule", body: body)
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

    // ── Platform API keys overlay (operator/admin only) ────────────────────
    // Server-wide default keys. The route is gated to owner|admin; surfaced
    // in Settings → Admin, not the user-facing API Keys tab.
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

    // ── Per-tenant API keys (BYOK) ─────────────────────────────────────────
    // The user-facing API Keys tab. Keys are encrypted per workspace; a
    // workspace with no key of its own falls back to the platform key.
    func tenantKeys() async throws -> TenantKeysEnvelope {
        try await get("/api/tenant-keys")
    }

    /// Reveals THIS workspace's own key only (never the platform value).
    func tenantKeyValue(name: String) async throws -> ApiKeyValue {
        try await get("/api/tenant-keys/\(name)")
    }

    /// Empty string clears the workspace key — collectors then fall back to
    /// the operator's platform key automatically.
    @discardableResult
    func tenantKeySet(name: String, value: String) async throws -> TenantKeyWrite {
        let body = try JSONEncoder().encode(["value": value])
        return try await put("/api/tenant-keys/\(name)", body: body)
    }

    /// Current key-edit policy + the member roster (for the owner picker).
    func keyPolicy() async throws -> KeyPolicyResponse {
        try await get("/api/tenant-keys/policy")
    }

    /// Owner-only. `memberId` required when `policy == "selected_member"`.
    @discardableResult
    func keyPolicySet(policy: String, memberId: String?) async throws -> KeyPolicyState {
        var obj: [String: String] = ["policy": policy]
        if let memberId { obj["memberId"] = memberId }
        let body = try JSONSerialization.data(withJSONObject: obj)
        return try await put("/api/tenant-keys/policy", body: body)
    }

    /// Triggers `node --watch` to respawn the server. The HTTP request races
    /// the process teardown — caller should treat any post-POST connection
    /// reset as expected and switch to /api/health polling for liveness.
    func restartServer() async throws {
        let url = try makeURL("/api/admin/restart")
        _ = try await request(url, method: "POST", body: Data("{}".utf8), timeout: 10)
    }

    // ── Repair worker / self-healing maintenance (operator only) ────────────
    /// Host-wide detect→triage→repair digest over a trailing window (hours).
    func maintenanceDigest(hours: Int = 24) async throws -> MaintenanceDigest {
        try await get("/api/admin/maintenance",
                      query: [URLQueryItem(name: "hours", value: String(hours))])
    }

    /// Full pipeline for one source: recent fetch runs, anomalies (with
    /// triage), and repair attempts (with the proposed patch).
    func maintenanceSourceDetail(_ id: String) async throws -> SourcePipeline {
        try await get("/api/admin/maintenance/source/\(id)")
    }

    /// Apply a staged `verified` url_swap repair — writes the runtime override
    /// and activates it live. Returns the merged result (incl. new_url).
    @discardableResult
    func approveRepair(_ id: Int) async throws -> OkResult {
        try await post("/api/admin/repairs/\(id)/approve", timeout: 15)
    }
    /// Reject a staged repair and resolve its anomaly so the worker stops
    /// re-deriving it.
    @discardableResult
    func rejectRepair(_ id: Int) async throws -> OkResult {
        try await post("/api/admin/repairs/\(id)/reject", timeout: 15)
    }
    /// Lift a circuit-breaker quarantine so the source can be scheduled again.
    @discardableResult
    func unquarantineSource(_ id: String) async throws -> OkResult {
        try await post("/api/admin/sources/\(id)/unquarantine", timeout: 15)
    }
    /// Reset an anomaly's triage so the triage worker re-picks it next cycle.
    @discardableResult
    func requeueAnomaly(_ id: Int) async throws -> OkResult {
        try await post("/api/admin/anomalies/\(id)/requeue", timeout: 15)
    }

    // ── Alerts ─────────────────────────────────────────────────────────────
    func alertsList() async throws -> [AlertRule] {
        let env: AlertRulesEnvelope = try await get("/api/alerts")
        return env.data
    }
    func alertCreate(_ rule: AlertRule) async throws -> AlertRule {
        let body = try JSONEncoder().encode(rule)
        let env: AlertRuleEnvelope = try await post("/api/alerts", body: body, timeout: API.userDefaultTimeout)
        return env.data
    }
    func alertUpdate(_ rule: AlertRule) async throws -> AlertRule {
        let body = try JSONEncoder().encode(rule)
        let env: AlertRuleEnvelope = try await patch("/api/alerts/\(rule.id)", body: body)
        return env.data
    }
    func alertDelete(_ id: String) async throws {
        try await delete("/api/alerts/\(id)")
    }
    func alertMute(_ id: String, durationSec: Int?) async throws {
        let body = try JSONSerialization.data(
            withJSONObject: ["duration_sec": durationSec as Any? ?? "forever"]
        )
        let _: [String: Bool] = try await post("/api/alerts/\(id)/mute", body: body, timeout: 10)
    }
    func alertUnmute(_ id: String) async throws {
        let _: [String: Bool] = try await post("/api/alerts/\(id)/unmute", timeout: 10)
    }
    func alertTest(_ id: String) async throws {
        let _: [String: Bool] = try await post("/api/alerts/\(id)/test", timeout: 10)
    }
    func alertEvents(_ id: String, limit: Int = 100) async throws -> [AlertEvent] {
        let env: AlertEventsEnvelope = try await get(
            "/api/alerts/\(id)/events",
            query: [URLQueryItem(name: "limit", value: String(limit))]
        )
        return env.data
    }

    // ── Workspace members & invites ────────────────────────────────────────
    func membersList() async throws -> WorkspaceMembersResponse {
        try await get("/api/members")
    }
    /// Invite by email. Server adds the membership immediately if the user
    /// already exists, otherwise queues a pending invite claimed on first
    /// sign-in. Returns the raw status string ("invited" / "added" /
    /// "already_member").
    @discardableResult
    func memberInvite(email: String, role: String) async throws -> String {
        let body = try JSONSerialization.data(withJSONObject: ["email": email, "role": role])
        let r: MemberActionResponse = try await post("/api/members/invite", body: body, timeout: API.userDefaultTimeout)
        return r.status ?? "ok"
    }
    func memberSetRole(userId: String, role: String) async throws {
        let body = try JSONSerialization.data(withJSONObject: ["role": role])
        let _: MemberActionResponse = try await patch("/api/members/\(userId)/role", body: body)
    }
    func memberRemove(userId: String) async throws {
        try await delete("/api/members/\(userId)")
    }
    func inviteRevoke(id: String) async throws {
        try await delete("/api/members/invite/\(id)")
    }

    // ── OSINT search + entity graph ────────────────────────────────────────
    func searchAnalyze(_ query: String) async throws -> SearchStartResponse {
        let body = try JSONSerialization.data(withJSONObject: ["query": query])
        return try await post("/api/search/analyze", body: body, timeout: 20)
    }
    func searchSuggest(_ q: String) async throws -> [String] {
        let env: SuggestEnvelope = try await get(
            "/api/search/suggest", query: [URLQueryItem(name: "q", value: q)])
        return env.suggestions
    }
    func searchResults(_ id: String) async throws -> SearchSnapshot {
        try await get("/api/search/results/\(id)")
    }
    func entitySearch(_ q: String, type: String? = nil, limit: Int = 30) async throws -> [EntityHit] {
        var qi = [URLQueryItem(name: "q", value: q), URLQueryItem(name: "limit", value: String(limit))]
        if let type { qi.append(URLQueryItem(name: "type", value: type)) }
        let env: EntitySearchEnvelope = try await get("/api/entities/search", query: qi)
        return env.results
    }
    func entity(_ type: String, _ id: String) async throws -> EntityProfile {
        try await get("/api/entities/\(type)/\(id)")
    }
    func entityGraph(_ type: String, _ id: String, depth: Int = 1) async throws -> EntityGraph {
        try await get("/api/entities/\(type)/\(id)/graph",
                      query: [URLQueryItem(name: "depth", value: String(depth))])
    }
    func entityMentions(_ type: String, _ id: String, limit: Int = 50) async throws -> [EntityMention] {
        let env: MentionsEnvelope = try await get(
            "/api/entities/\(type)/\(id)/mentions",
            query: [URLQueryItem(name: "limit", value: String(limit))])
        return env.mentions
    }

    /// SSE URL + auth headers for the progress stream (consumed by SearchSSE
    /// via URLSession.bytes — which, unlike a browser EventSource, CAN send
    /// the bearer header; the route is also pre-auth so either works).
    func searchStreamRequest(_ requestId: String) -> URLRequest {
        let base = baseURL.trimmingCharacters(in: CharacterSet(charactersIn: "/"))
        var req = URLRequest(url: URL(string: base + "/api/search/stream/\(requestId)")!)
        req.timeoutInterval = 600
        req.setValue("text/event-stream", forHTTPHeaderField: "Accept")
        if let token = AuthTokenBox.shared.accessToken, !token.isEmpty {
            req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        }
        if let tid = AuthTokenBox.shared.tenantId, !tid.isEmpty {
            req.setValue(tid, forHTTPHeaderField: "X-Tenant-Id")
        }
        return req
    }
}
