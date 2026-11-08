import Foundation

// ─────────────────────────────────────────────────────────────────────────────
// Client for the source pages API (models: Models+SourcePages.swift). Its own
// file for the same reason API+Roadmap.swift is: an addition this size never
// collides with concurrent edits to API.swift.
//
// Why these and not /api/status: GET /api/status serves the WHOLE registry in
// one body (sourceapi.h measures it at 20 MB / 8 s; ~58,000 rows today). These
// routes are server-searched and paged, or one source / one collector, so a
// screen that needs one row or a search pays for one row or a page.
// ─────────────────────────────────────────────────────────────────────────────

extension API {

    /// GET /api/sources/:id/detail — 404 when no source has that id.
    func sourcePageDetail(_ id: String) async throws -> SourcePageDetail {
        try await get("/api/sources/\(esc(id))/detail")
    }

    /// GET /api/sources/:id/summary[?refresh=1]. The model call can take tens
    /// of seconds server-side (a 25 s LLM budget), so the client waits longer
    /// than the default.
    func sourceSummary(_ id: String, refresh: Bool = false) async throws -> SourceSummary {
        var q: [URLQueryItem] = []
        if refresh { q.append(URLQueryItem(name: "refresh", value: "1")) }
        let url = try makeURL("/api/sources/\(esc(id))/summary", query: q)
        let data = try await request(url, timeout: 60)
        do { return try JSONDecoder().decode(SourceSummary.self, from: data) }
        catch { throw APIError.decoding(error) }
    }

    /// GET /api/sources/:id/settings — readable by any signed-in user; the
    /// envelope's `can_write` says whether THIS caller may change it.
    func sourceSettings(_ id: String) async throws -> SourceSettingsEnvelope {
        try await get("/api/sources/\(esc(id))/settings")
    }

    /// PUT /api/sources/:id/settings {key, value, enabled}. Operator-gated
    /// (403 `operator_required`); out-of-range is a 400 `value_out_of_range`,
    /// never a clamp. Answers the refreshed settings envelope.
    func setSourceSetting(_ id: String, key: String, value: Double,
                          enabled: Bool) async throws -> SourceSettingsEnvelope {
        let body = try JSONSerialization.data(withJSONObject: [
            "key": key, "value": Int64(value), "enabled": enabled,
        ] as [String: Any])
        return try await put("/api/sources/\(esc(id))/settings", body: body)
    }

    /// GET /api/sources/cadence — the system-wide cadence policy.
    func cadencePolicy() async throws -> CadencePolicyEnvelope {
        try await get("/api/sources/cadence")
    }

    /// PUT /api/sources/cadence — platform operator. 0 clears a field.
    func setCadencePolicy(globalSec: Int, minSec: Int, maxSec: Int) async throws -> CadencePolicyEnvelope {
        let body = try JSONSerialization.data(withJSONObject: [
            "global_interval_sec": globalSec, "min_interval_sec": minSec, "max_interval_sec": maxSec,
        ])
        return try await put("/api/sources/cadence", body: body)
    }

    /// GET /api/sources/search — the registry filtered and paged on the
    /// server (sourceapi.h sourceapi_query). `limit` is clamped to 500 there.
    func sourceSearch(q: String? = nil, collector: String? = nil, health: String? = nil,
                      limit: Int = 50, offset: Int = 0) async throws -> SourceSearchPage {
        var qi = [URLQueryItem(name: "limit", value: String(limit)),
                  URLQueryItem(name: "offset", value: String(offset))]
        if let q, !q.isEmpty { qi.append(URLQueryItem(name: "q", value: q)) }
        if let collector, !collector.isEmpty { qi.append(URLQueryItem(name: "collector", value: collector)) }
        if let health, !health.isEmpty { qi.append(URLQueryItem(name: "health", value: health)) }
        return try await get("/api/sources/search", query: qi)
    }

    /// GET /api/collectors — every collector key with its tallies, and groups.
    func collectorsIndex() async throws -> CollectorsEnvelope {
        try await get("/api/collectors")
    }

    /// GET /api/collectors/:key — a collector (or a grp_* group) with one
    /// page of its sources; `q`/`limit`/`offset` page that list.
    func collectorDetail(_ key: String, q: String? = nil, limit: Int = 50,
                         offset: Int = 0) async throws -> CollectorDetailEnvelope {
        var qi = [URLQueryItem(name: "limit", value: String(limit)),
                  URLQueryItem(name: "offset", value: String(offset))]
        if let q, !q.isEmpty { qi.append(URLQueryItem(name: "q", value: q)) }
        return try await get("/api/collectors/\(esc(key))", query: qi)
    }

    /// GET /api/sources/:id/logs?limit=&offset= — every fetch_log row of a
    /// source, newest first (limit clamped to 500 server-side).
    func sourceLogs(_ id: String, limit: Int = 50, offset: Int = 0) async throws -> SourceLogsPage {
        try await get("/api/sources/\(esc(id))/logs", query: [
            URLQueryItem(name: "limit", value: String(limit)),
            URLQueryItem(name: "offset", value: String(offset)),
        ])
    }

    /// The source pages' refusals, named rather than dumped: the settings PUT
    /// answers `operator_required` / `value_out_of_range` / `unknown_key`, the
    /// cadence PUT a 403 or its parser's own message.
    static func sourcePagesErrorMessage(_ error: Error) -> String {
        guard case let APIError.http(code, body) = error else { return error.localizedDescription }
        struct Body: Decodable { let error: String? }
        let parsed = body.data(using: .utf8).flatMap { try? JSONDecoder().decode(Body.self, from: $0) }
        switch (code, parsed?.error) {
        case (403, _):                    return "Platform operator access required — this change is an audited operator action."
        case (400, "value_out_of_range"?): return "The server refused that value: it is outside this setting's range."
        case (400, "unknown_key"?):        return "The server does not offer this setting."
        case (404, _):                    return "No registered source has this id."
        default:                          return parsed?.error ?? (body.isEmpty ? "HTTP \(code)" : body)
        }
    }
}
