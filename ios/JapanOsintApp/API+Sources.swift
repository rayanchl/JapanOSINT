import Foundation

// ── Paged source endpoints (core/sourceapi.c) ──────────────────────────────
//
// The registry is ~61k sources. Nothing here downloads it: the dashboard asks
// for tiles (`/api/status?summary=1`), the collector tree (`/api/collectors`)
// and one searched page of rows at a time (`/api/sources/search`), exactly as
// the web client's Sources page does.

/// Filters of `GET /api/sources/search`. Empty / nil means "no filter".
struct SourceSearchQuery: Equatable {
    var q: String = ""
    var group: String? = nil
    var collector: String? = nil
    var category: String? = nil
    var engine: String? = nil
    var kind: String? = nil
    var status: String? = nil
    var health: String? = nil
    var hasItems: Bool = false
    var sort: String? = nil
    var desc: Bool = false

    /// True when anything narrows the registry.
    var isFiltered: Bool {
        !q.trimmingCharacters(in: .whitespaces).isEmpty
            || group != nil || collector != nil || category != nil
            || engine != nil || kind != nil || status != nil
            || health != nil || hasItems
    }
}

extension API {
    /// One page of the registry, searched and filtered on the server.
    /// `limit` is clamped to 500 by the server; the default is the web's 50.
    func searchSources(_ query: SourceSearchQuery,
                       limit: Int = 50, offset: Int = 0) async throws -> SourceSearchEnvelope {
        var qs: [URLQueryItem] = [
            URLQueryItem(name: "limit", value: String(limit)),
            URLQueryItem(name: "offset", value: String(offset)),
        ]
        let q = query.q.trimmingCharacters(in: .whitespaces)
        if !q.isEmpty { qs.append(URLQueryItem(name: "q", value: q)) }
        if let v = query.group { qs.append(URLQueryItem(name: "group", value: v)) }
        if let v = query.collector { qs.append(URLQueryItem(name: "collector", value: v)) }
        if let v = query.category { qs.append(URLQueryItem(name: "category", value: v)) }
        if let v = query.engine { qs.append(URLQueryItem(name: "engine", value: v)) }
        if let v = query.kind { qs.append(URLQueryItem(name: "kind", value: v)) }
        if let v = query.status { qs.append(URLQueryItem(name: "status", value: v)) }
        if let v = query.health { qs.append(URLQueryItem(name: "health", value: v)) }
        if query.hasItems { qs.append(URLQueryItem(name: "has_items", value: "1")) }
        if let v = query.sort { qs.append(URLQueryItem(name: "sort", value: v)) }
        if query.desc { qs.append(URLQueryItem(name: "desc", value: "1")) }
        return try await get("/api/sources/search", query: qs)
    }

    /// Every collector and group with its tallies (a few hundred rows).
    func collectorTree() async throws -> CollectorsEnvelope {
        try await get("/api/collectors")
    }

    /// Fleet counters only — no rows are built or sent.
    func statusSummary() async throws -> StatusSummaryEnvelope {
        try await get("/api/status", query: [URLQueryItem(name: "summary", value: "1")])
    }

    /// A bounded window of `/api/status` rows (the server states the bound in
    /// `view`). Only used where the rows are not in the searchable registry
    /// (the operator-only breach catalog is appended after the registry rows).
    func statusWindow(limit: Int, offset: Int) async throws -> StatusEnvelope {
        try await get("/api/status", query: [
            URLQueryItem(name: "limit", value: String(limit)),
            URLQueryItem(name: "offset", value: String(offset)),
        ])
    }

    /// One source as a `StatusRow` (probe state, credentials, trust).
    func statusRow(_ id: String) async throws -> StatusRow {
        try await get("/api/status/\(API.pathEscape(id))")
    }

    func sourceDetail(_ id: String) async throws -> SourceDetailEnvelope {
        try await get("/api/sources/\(API.pathEscape(id))/detail")
    }

    /// The small-model summary. Slow on a cold cache (it calls a model), so
    /// it gets its own generous timeout.
    func sourceSummary(_ id: String, refresh: Bool = false) async throws -> SourceSummaryResponse {
        let url = try makeURL("/api/sources/\(API.pathEscape(id))/summary",
                              query: refresh ? [URLQueryItem(name: "refresh", value: "1")] : [])
        let data = try await request(url, timeout: 45)
        do { return try JSONDecoder().decode(SourceSummaryResponse.self, from: data) }
        catch { throw APIError.decoding(error) }
    }

    func sourceSettings(_ id: String) async throws -> SourceSettingsEnvelope {
        try await get("/api/sources/\(API.pathEscape(id))/settings")
    }

    /// Operator-gated (403 otherwise). `key` is one of the closed key set the
    /// settings route lists (`interval_sec`, `http_body_ceiling_bytes`).
    func putSourceSetting(_ id: String, key: String, value: Int,
                          enabled: Bool) async throws -> SourceSettingsEnvelope {
        let body = try JSONSerialization.data(withJSONObject: [
            "key": key, "value": value, "enabled": enabled,
        ] as [String: Any])
        return try await put("/api/sources/\(API.pathEscape(id))/settings", body: body)
    }

    /// The system-wide cadence policy and what it costs in runs a day.
    func cadencePolicy() async throws -> CadenceEnvelope {
        try await get("/api/sources/cadence")
    }

    /// `schedule_mode` of one source (`GET /api/sources/:id`, the sources row).
    func sourceScheduleMode(_ id: String) async throws -> String? {
        let row: DBRowAny = try await get("/api/sources/\(API.pathEscape(id))")
        return row.raw["schedule_mode"]?.value as? String
    }

    /// Asks the cheapest operator-gated route whether the caller is a platform
    /// operator, as the web client does (client/src/auth/AuthContext.jsx):
    /// `true` on 200, `false` on 401/403, `nil` when the answer is unknown (a
    /// network error — never guess a role from a failure).
    func probeOperator() async -> Bool? {
        do {
            let url = try makeURL("/api/media/capabilities")
            _ = try await request(url, timeout: 10)
            return true
        } catch APIError.http(let code, _) where code == 401 || code == 403 {
            return false
        } catch {
            return nil
        }
    }
}
