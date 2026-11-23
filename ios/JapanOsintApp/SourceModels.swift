import Foundation

// ── Paged source surfaces ──────────────────────────────────────────────────
//
// Models for the server's paged and searched source endpoints
// (core/sourceapi.c): GET /api/sources/search, /api/collectors,
// /api/sources/:id/detail, /api/sources/:id/summary, /api/sources/:id/settings,
// /api/sources/cadence and /api/status?summary=1.
//
// The wire keys are snake_case (and a few camelCase in the schedule object),
// and the app decodes with a plain JSONDecoder, so property names match the
// wire verbatim. Every field is optional: a field the server omits or sends
// as null must never fail a whole page of 50 rows.

struct SourcePageInfo: Decodable, Hashable {
    let limit: Int?
    let offset: Int?
    let count: Int?
    let total: Int?
    let has_more: Bool?
}

struct SourceFacetValue: Decodable, Hashable {
    let key: String?
    let label: String?
    let count: Int?
    var keyText: String { key ?? "" }
}

struct SourceFacetSet: Decodable, Hashable {
    let values: [SourceFacetValue]?
    let shown: Int?
    let total: Int?
}

struct SourceFacets: Decodable, Hashable {
    let groups: SourceFacetSet?
    let collectors: SourceFacetSet?
    let categories: SourceFacetSet?
    let engines: SourceFacetSet?
    let kinds: SourceFacetSet?
    let statuses: SourceFacetSet?
    let healths: SourceFacetSet?
}

struct SourceSearchMeta: Decodable, Hashable {
    let facets: SourceFacets?
    let registry_total: Int?
    let snapshot_at: String?
}

/// The server's one answer to "does this source work": `state` is one of
/// ok | failing | gated | degraded | pending, and the rest says why.
struct SourceHealthInfo: Decodable, Hashable {
    let state: String?
    let working: Bool?
    let kind: String?
    let outcome: String?
    let note: String?
    let remedy: String?
    let since: String?
    let credentials_required: [String]?
    let http_status: Int?
    let body_parsed: Bool?
}

struct SourceSearchTrust: Decodable, Hashable {
    let rated: Bool?
    let reliability: Double?
    let grade: String?
}

/// One row of GET /api/sources/search (the compact row every list shares).
struct SourceSearchRow: Decodable, Identifiable, Hashable {
    let id: String
    let name: String?
    let name_ja: String?
    let collector: String?
    let collector_label: String?
    let group: String?
    let group_label: String?
    let category: String?
    let type: String?
    let engine: String?
    /// scheduled | scheduled-pivot | pivot | internal | unreachable
    let kind: String?
    let interval: Double?
    let url: String?
    let host: String?
    let description: String?
    let layer: String?
    let item_count: Int?
    let geocoded: Int?
    let notices: Int?
    let last_fetched: String?
    let status: String?
    let health: SourceHealthInfo?
    let last_check: String?
    let last_success: String?
    let records_last_run: Int?
    let response_time_ms: Double?
    let error_message: String?
    let trust: SourceSearchTrust?

    var displayName: String { name ?? id }

    /// An on-demand entity pivot: it only answers for an entity, so running it
    /// with none stores nothing (the server answers 422 `needs_entity`).
    var needsEntity: Bool { kind == "pivot" }

    var isInternal: Bool { kind == "internal" }

    /// Health state as the server reports it, else the run status.
    var healthState: String { health?.state ?? status ?? "pending" }

    var statusKind: OsintSource.StatusKind {
        switch healthState {
        case "ok", "online":        return .online
        case "degraded":            return .degraded
        case "failing", "offline":  return .offline
        case "gated":               return .gated
        case "pending":             return .pending
        default:                    return .unknown
        }
    }

    /// The Intel tab's row model, built from a search row.
    var asIntelSource: IntelSource {
        IntelSource(id: id,
                    name: name ?? id,
                    name_ja: name_ja,
                    category: category,
                    description: description,
                    url: url,
                    item_count: item_count ?? 0,
                    last_fetched: last_fetched,
                    last_published: nil,
                    ttl_ms: nil,
                    parent_id: nil)
    }
}

struct SourceSearchEnvelope: Decodable {
    let data: [SourceSearchRow]?
    let page: SourcePageInfo?
    let meta: SourceSearchMeta?
}

// ── Collectors (GET /api/collectors) ───────────────────────────────────────

struct CollectorStatusCounts: Decodable, Hashable {
    let online: Int?
    let degraded: Int?
    let offline: Int?
    let pending: Int?
}

/// One collector, or one GROUP of collectors (`is_group`, `collectors` = the
/// member keys). Groups come back in `CollectorsEnvelope.groups`.
struct CollectorSummary: Decodable, Identifiable, Hashable {
    let key: String
    let label: String?
    let label_ja: String?
    let description: String?
    let parent: String?
    let is_group: Bool?
    let group: String?
    let group_label: String?
    let sources: Int?
    let scheduled: Int?
    let pivots: Int?
    let unreachable: Int?
    let items: Int?
    let notices: Int?
    let last_fetched: String?
    let status: CollectorStatusCounts?
    let categories: SourceFacetSet?
    let collectors: [String]?
    let collector_count: Int?

    var id: String { key }
    var displayName: String { (label?.isEmpty == false) ? label! : key }
    var sourceCount: Int { sources ?? 0 }
    var onlineCount: Int { status?.online ?? 0 }
    var degradedCount: Int { status?.degraded ?? 0 }
    var offlineCount: Int { status?.offline ?? 0 }
    var pendingCount: Int { status?.pending ?? 0 }
    /// The dominant category (the facet list is sorted by count).
    var topCategory: String? { categories?.values?.first?.key }

    /// Worst-of aggregate, as the old client-side `Collector` computed it.
    var aggregateStatus: OsintSource.StatusKind {
        let n = sourceCount
        if n > 0, offlineCount == n { return .offline }
        if degradedCount > 0 || offlineCount > 0 { return .degraded }
        if onlineCount > 0 { return .online }
        return .pending
    }
}

struct CollectorsMeta: Decodable, Hashable {
    let total: Int?
    let sources: Int?
    let registry_total: Int?
    let groups: Int?
}

struct CollectorsEnvelope: Decodable {
    let data: [CollectorSummary]?
    let groups: [CollectorSummary]?
    let meta: CollectorsMeta?
}

// ── GET /api/status?summary=1 ──────────────────────────────────────────────

struct StatusViewInfo: Decodable, Hashable {
    let total: Int?
    let shown: Int?
    let offset: Int?
    let limit: Int?
    let truncated: Bool?
    let note: String?
}

/// `?summary=1` answers the counters and an empty `apis` array, so this model
/// declares only the counters (`StatusEnvelope` requires `apis`).
struct StatusSummaryEnvelope: Decodable {
    let summary: StatusSummary?
    let view: StatusViewInfo?
}

// ── GET /api/sources/:id/detail ────────────────────────────────────────────

struct SourceCollectorMeta: Decodable, Hashable {
    let key: String?
    let label: String?
    let label_ja: String?
    let description: String?
    let parent: String?
}

struct SourceDetailRegistry: Decodable, Hashable {
    let id: String?
    let name: String?
    let name_ja: String?
    let collector: String?
    let collector_meta: SourceCollectorMeta?
    let category: String?
    let type: String?
    let engine: String?
    let kind: String?
    let interval: Double?
    let url: String?
    let host: String?
    let description: String?
    let license: String?
    let layer: String?
    let layer_id: String?
    let free_tier: Bool?
    let curated_overlay: Bool?
    let is_entity_pivot: Bool?
}

struct SourceRunStats: Decodable, Hashable {
    let runs_30d: Int?
    let ok_30d: Int?
    let success_rate_30d: Double?
    let avg_duration_ms_30d: Double?
    let last_run_at: String?
    let records_30d: Double?
    let bytes_30d: Double?
}

struct SourceDetailSchedule: Decodable, Hashable {
    let tracked: Bool?
    let declaredInterval: Double?
    let adaptive: Bool?
    let measuredInterval: Double?
    let policyInterval: Double?
    let policy: String?
    let overrideInterval: Double?
    let cadenceBasis: String?
    let runInterval: Double?
    let effectiveInterval: Double?
    let consecutiveFailures: Int?
    let consecutiveEmpties: Int?
    let backedOff: Bool?
    let backoffUntil: String?
    let quarantined: Bool?
    let schedule_mode: String?
    let kind: String?
    let next_due: String?
    let overdue: Bool?
    let stats_30d: SourceRunStats?
}

struct SourceDetailTrust: Decodable, Hashable {
    let rated: Bool?
    let reliability: Double?
    let grade: String?
    let success_rate: Double?
    let runs_30d: Int?
    let anomalies_30d: Int?
}

struct SourceDetailStatus: Decodable, Hashable {
    let status: String?
    let health: SourceHealthInfo?
    let last_check: String?
    let last_success: String?
    let response_time_ms: Double?
    let records_last_run: Int?
    let error_message: String?
    let trust: SourceDetailTrust?
}

struct SourceDetailData: Decodable, Hashable {
    let item_count: Int?
    let geocoded: Int?
    let records_geocoded: Int?
    let notices: Int?
    let last_fetched: String?
    let awaiting_geo: Int?
    let first_fetched: String?
    let first_published: String?
    let last_published: String?
}

struct SourceNoticeCount: Decodable, Hashable {
    let kind: String?
    let count: Int?
    let latest_at: String?
}

struct SourceNoticeLatest: Decodable, Hashable {
    let kind: String?
    let title: String?
    let fetched_at: String?
    let uid: String?
    let properties: [String: AnyCodable]?
}

struct SourceDetailNotices: Decodable, Hashable {
    let counts: [SourceNoticeCount]?
    let latest: [SourceNoticeLatest]?
}

struct SourceRunRow: Decodable, Hashable {
    let id: Int?
    let timestamp: String?
    let status: String?
    let records_fetched: Int?
    let stored: Int?
    let duration_ms: Double?
    let error: String?
    let bytes_fetched: Double?
}

struct SourceDetailRuns: Decodable, Hashable {
    let recent: [SourceRunRow]?
    let shown: Int?
    let total: Int?
}

struct SourceDetailPacing: Decodable, Hashable {
    let host: String?
    let min_gap_ms: Double?
}

struct SourceDetailEnvelope: Decodable {
    let registry: SourceDetailRegistry?
    let schedule: SourceDetailSchedule?
    let status: SourceDetailStatus?
    let data: SourceDetailData?
    let notices: SourceDetailNotices?
    let runs: SourceDetailRuns?
    let pacing: SourceDetailPacing?
    /// Newer servers also publish the layer id at the top level.
    let layer_id: String?
    let records_geocoded: Int?
}

// ── GET /api/sources/:id/summary ───────────────────────────────────────────

/// A small-model summary built only from measured facts. On any model failure
/// the server answers `error` (and the facts) instead of prose — never
/// invented text — so `summary == nil` with an `error` is a normal state.
struct SourceSummaryResponse: Decodable {
    let summary: String?
    let result_type: String?
    let caveats: [String]?
    let model: String?
    let cached: Bool?
    let generated_at: String?
    let error: String?
    let http_status: Int?
    let facts_used: Int?
}

// ── GET/PUT /api/sources/:id/settings ──────────────────────────────────────

struct SourceSettingRow: Decodable, Identifiable, Hashable {
    let key: String
    let label: String?
    let help: String?
    let unit: String?
    let lo: Double?
    let hi: Double?
    let defaultValue: Double?
    let default_basis: String?
    let value: Double?
    let enabled: Bool?
    let configured: Bool?
    let effective: Double?
    let updated_at: String?
    let updated_by: String?

    var id: String { key }

    private enum CodingKeys: String, CodingKey {
        case key, label, help, unit
        case lo = "min"
        case hi = "max"
        case defaultValue = "default"
        case default_basis, value, enabled, configured, effective, updated_at, updated_by
    }
}

struct SourceSettingsEnvelope: Decodable {
    let can_write: Bool?
    let settings: [SourceSettingRow]?
}

// ── GET /api/sources/cadence ───────────────────────────────────────────────

struct CadencePolicy: Decodable, Hashable {
    let global_interval_sec: Int?
    let min_interval_sec: Int?
    let max_interval_sec: Int?
    let realtime_max_sec: Int?
    let mode: String?
}

struct CadenceEstimate: Decodable, Hashable {
    let scheduled_rows: Int?
    let runs_per_day_declared: Int?
    let runs_per_day_effective: Int?
    let fastest_effective_sec: Int?
    let slowest_effective_sec: Int?
}

struct CadenceEnvelope: Decodable {
    let policy: CadencePolicy?
    let source: String?
    let updated_at: String?
    let updated_by: String?
    let estimate: CadenceEstimate?
}

// ── Formatting helpers shared by the source screens ────────────────────────

enum SourceFormat {
    /// The scheduler's cadence policy for a row (`schedule.policy`). "realtime"
    /// is a row declared at or below `sched.realtime_max_sec`: a global
    /// interval does not apply to it.
    static func policy(_ mode: String?) -> String {
        switch mode ?? "" {
        case "", "declared": return "declared"
        case "global":   return "global (system-wide interval)"
        case "clamped":  return "clamped (floor/ceiling)"
        case "realtime": return "real-time (exempt from the global interval)"
        case let other: return other
        }
    }

    /// "every 6 h" / "every 90 s" / "on demand".
    static func interval(_ seconds: Double?) -> String {
        guard let s = seconds, s > 0 else { return "on demand" }
        let n = Int(s)
        if n % 86400 == 0 { return "every \(n / 86400) d" }
        if n % 3600 == 0 { return "every \(n / 3600) h" }
        if n % 60 == 0 { return "every \(n / 60) min" }
        return "every \(n) s"
    }

    /// "1,234" with the locale's grouping; "—" when not measured.
    static func count(_ n: Int?) -> String {
        guard let n else { return "—" }
        return n.formatted()
    }
}

/// Reads the explanation out of a `collector-*-notice` / fetch-failure
/// record's `properties`: why it fired and what to do about it, plus the HTTP
/// status and TLS cause the engine measured when it carries them. Nothing is
/// composed beyond what the server wrote; an empty result means the record
/// carried none of those fields.
enum NoticeText {
    static func detail(from props: [String: AnyCodable]?) -> [(label: String, text: String)] {
        guard let p = props else { return [] }
        var out: [(label: String, text: String)] = []
        func read(_ key: String) -> String? {
            guard let v = p[key]?.value else { return nil }
            if let s = v as? String { return s.isEmpty ? nil : s }
            if let i = v as? Int { return String(i) }
            if let d = v as? Double { return String(d) }
            if let b = v as? Bool { return b ? "yes" : "no" }
            return nil
        }
        for key in ["reason", "note", "error", "message", "outcome"] {
            if let t = read(key) { out.append((label: key, text: t)); break }
        }
        if let t = read("http_status") { out.append((label: "http status", text: t)) }
        if let t = read("tls_cause") { out.append((label: "tls cause", text: t)) }
        if let t = read("tls_detail") { out.append((label: "tls detail", text: t)) }
        if let t = read("remedy") { out.append((label: "remedy", text: t)) }
        return out
    }

    /// Short label for a notice record type.
    static func label(forKind rt: String?) -> String {
        guard var t = rt else { return "notice" }
        switch t {
        case "collector-status-notice":     return "needs credential"
        case "collector-truncation-notice": return "truncated"
        case "collector-fetch-failure",
             "collector-fetch-error":       return "fetch failed"
        case "collector-shape-notice":      return "unexpected shape"
        default:
            if t.hasPrefix("collector-") { t.removeFirst("collector-".count) }
            if t.hasSuffix("-notice") { t.removeLast("-notice".count) }
            return t.replacingOccurrences(of: "-", with: " ")
        }
    }
}
