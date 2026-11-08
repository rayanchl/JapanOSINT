import Foundation

// ─────────────────────────────────────────────────────────────────────────────
// Models for the source pages API (core/sourceapi.c, core/source_settings.c,
// core/cadence.c, core/miscapi.c). The web client has read these routes since
// 2026-10-12 (client/src/components/sources/); iOS had none of them, so the
// facts they exist to state — which engine runs a source, how it pages, the
// interval that actually runs and WHY, the engine's own notices — were not
// reachable from the app at all.
//
// Every field is decoded with the non-throwing helpers below: these payloads
// are wide and grow, and a single field the server shapes differently must
// degrade that one value to nil (rendered "not measured"), never fail the
// whole page. Each struct names the server emitter it mirrors.
// ─────────────────────────────────────────────────────────────────────────────

fileprivate extension KeyedDecodingContainer {
    // `try?` flattens the Optional decodeIfPresent returns (SE-0230), so a
    // missing key, a null and a wrongly-typed value all read as nil.
    func spStr(_ k: Key) -> String? { try? decodeIfPresent(String.self, forKey: k) }
    /// cJSON prints every number as a double; an integral one has no decimal
    /// point, but reading through Double accepts both spellings.
    func spDbl(_ k: Key) -> Double? { try? decodeIfPresent(Double.self, forKey: k) }
    func spInt(_ k: Key) -> Int? { spDbl(k).map { Int($0) } }
    func spBool(_ k: Key) -> Bool? { try? decodeIfPresent(Bool.self, forKey: k) }
    func spObj<T: Decodable>(_ t: T.Type, _ k: Key) -> T? { try? decodeIfPresent(T.self, forKey: k) }
    func spStrs(_ k: Key) -> [String] { (try? decodeIfPresent([String].self, forKey: k)) ?? [] }
}

/// A seconds count as an interval a person reads: "45 s", "15 min", "6 h", "2 d".
func fmtInterval(_ sec: Double?) -> String {
    guard let s = sec, s > 0 else { return "on demand" }
    if s < 120 { return "\(Int(s)) s" }
    if s < 7200 { return "\(Int((s / 60).rounded())) min" }
    if s < 172_800 {
        let h = s / 3600
        return h == h.rounded() ? "\(Int(h)) h" : String(format: "%.1f h", h)
    }
    return String(format: "%.1f d", s / 86400)
}

func fmtBytes(_ n: Double?) -> String {
    guard let n else { return "not measured" }
    return ByteCountFormatter.string(fromByteCount: Int64(n), countStyle: .file)
}

// ── shared pieces ───────────────────────────────────────────────────────────

/// One bucket of a facet / histogram. sourceapi.c names the label field per
/// list: `key` (facet_json), `record_type` (record_type_hist), `language`
/// (language_hist) — all three are read here so one type serves every list.
struct SPCount: Decodable, Hashable, Identifiable {
    let key: String
    let label: String?
    let count: Int
    var id: String { key }
    private enum K: String, CodingKey { case key, label, count, record_type, language }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        key = c.spStr(.key) ?? c.spStr(.record_type) ?? c.spStr(.language) ?? "(none)"
        label = c.spStr(.label)
        count = c.spInt(.count) ?? 0
    }
}

/// `{values, shown, total}` — facet_json() (sourceapi.c:581) and
/// record_type_hist() (sourceapi.c:1189). `total` null = not counted.
struct SPFacet: Decodable, Hashable {
    let values: [SPCount]
    let shown: Int?
    let total: Int?
    private enum K: String, CodingKey { case values, shown, total }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        values = c.lossyArray(SPCount.self, forKey: .values)
        shown = c.spInt(.shown)
        total = c.spInt(.total)
    }
}

/// health_json() (sourceapi.c:437) — the source's own answer to "does it
/// work", read from the engine's disclosure records.
struct SPHealth: Decodable, Hashable {
    let state: String?
    let working: Bool?
    let kind: String?
    let outcome: String?
    let note: String?
    let remedy: String?
    let since: String?
    let credentialsRequired: [String]
    let httpStatus: Int?
    let bodyParsed: Bool?
    private enum K: String, CodingKey {
        case state, working, kind, outcome, note, remedy, since
        case credentialsRequired = "credentials_required", httpStatus = "http_status", bodyParsed = "body_parsed"
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        state = c.spStr(.state); working = c.spBool(.working); kind = c.spStr(.kind)
        outcome = c.spStr(.outcome); note = c.spStr(.note); remedy = c.spStr(.remedy)
        since = c.spStr(.since); credentialsRequired = c.spStrs(.credentialsRequired)
        httpStatus = c.spInt(.httpStatus); bodyParsed = c.spBool(.bodyParsed)
    }
}

/// sched_json() (sourceapi.c:468-504), plus the fields sourceapi_detail()
/// adds to the detail page's copy (schedule_mode, kind :1461-1462; stats_30d,
/// next_due, overdue :1543-1560). `cadenceBasis` names which figure decided
/// the cadence: "override" | "policy" | "measured" | "declared" | "on demand".
struct SPSchedule: Decodable, Hashable {
    let tracked: Bool?
    let declaredInterval: Double?
    let adaptive: Bool?
    let measuredInterval: Double?
    let policyInterval: Double?
    let policy: String?
    let overrideInterval: Double?
    let cadenceBasis: String?
    let effectiveInterval: Double?
    let consecutiveFailures: Int?
    let consecutiveEmpties: Int?
    let backedOff: Bool?
    let backoffUntil: String?
    let quarantined: Bool?
    let scheduleMode: String?
    let kind: String?
    let stats30d: SPStats30d?
    let nextDue: String?
    let overdue: Bool?
    private enum K: String, CodingKey {
        case tracked, declaredInterval, adaptive, measuredInterval, policyInterval, policy
        case overrideInterval, cadenceBasis, effectiveInterval, consecutiveFailures
        case consecutiveEmpties, backedOff, backoffUntil, quarantined
        case scheduleMode = "schedule_mode", kind, stats30d = "stats_30d"
        case nextDue = "next_due", overdue
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        tracked = c.spBool(.tracked); declaredInterval = c.spDbl(.declaredInterval)
        adaptive = c.spBool(.adaptive); measuredInterval = c.spDbl(.measuredInterval)
        policyInterval = c.spDbl(.policyInterval); policy = c.spStr(.policy)
        overrideInterval = c.spDbl(.overrideInterval); cadenceBasis = c.spStr(.cadenceBasis)
        effectiveInterval = c.spDbl(.effectiveInterval)
        consecutiveFailures = c.spInt(.consecutiveFailures); consecutiveEmpties = c.spInt(.consecutiveEmpties)
        backedOff = c.spBool(.backedOff); backoffUntil = c.spStr(.backoffUntil)
        quarantined = c.spBool(.quarantined); scheduleMode = c.spStr(.scheduleMode)
        kind = c.spStr(.kind); stats30d = c.spObj(SPStats30d.self, .stats30d)
        nextDue = c.spStr(.nextDue); overdue = c.spBool(.overdue)
    }
}

/// runs_json()'s 30-day statistics (sourceapi.c:1362-1384). The average is
/// `avg_duration_ms_30d` on the wire.
struct SPStats30d: Decodable, Hashable {
    let runs: Int?
    let ok: Int?
    let successRate: Double?
    let avgDurationMs: Double?
    let lastRunAt: String?
    let firstRunAt: String?
    let records: Int?
    let bytes: Double?
    let runsWithBytes: Int?
    private enum K: String, CodingKey {
        case runs = "runs_30d", ok = "ok_30d", successRate = "success_rate_30d"
        case avgDurationMs = "avg_duration_ms_30d", lastRunAt = "last_run_at"
        case firstRunAt = "first_run_30d_at", records = "records_30d", bytes = "bytes_30d"
        case runsWithBytes = "runs_with_bytes_30d"
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        runs = c.spInt(.runs); ok = c.spInt(.ok); successRate = c.spDbl(.successRate)
        avgDurationMs = c.spDbl(.avgDurationMs); lastRunAt = c.spStr(.lastRunAt)
        firstRunAt = c.spStr(.firstRunAt); records = c.spInt(.records)
        bytes = c.spDbl(.bytes); runsWithBytes = c.spInt(.runsWithBytes)
    }
}

/// One fetch_log row. runs_json() (sourceapi.c:1338-1352) selects id,
/// timestamp, status, records_fetched, stored, duration_ms, error,
/// bytes_fetched; GET /api/sources/:id/logs (miscapi.c:117, `SELECT *`)
/// serves the same column names. `stored`/`bytes_fetched` null = a run from
/// before those columns existed — not zero.
struct SPRun: Decodable, Hashable, Identifiable {
    let rowId: Int?
    let timestamp: String?
    let status: String?
    let recordsFetched: Int?
    let stored: Int?
    let durationMs: Int?
    let error: String?
    let bytesFetched: Double?
    var id: String { rowId.map(String.init) ?? "\(timestamp ?? "?")|\(status ?? "?")" }
    var ok: Bool { (status ?? "").lowercased() == "ok" }
    private enum K: String, CodingKey {
        case rowId = "id", timestamp, status, recordsFetched = "records_fetched", stored
        case durationMs = "duration_ms", error, bytesFetched = "bytes_fetched"
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        rowId = c.spInt(.rowId); timestamp = c.spStr(.timestamp); status = c.spStr(.status)
        recordsFetched = c.spInt(.recordsFetched); stored = c.spInt(.stored)
        durationMs = c.spInt(.durationMs); error = c.spStr(.error); bytesFetched = c.spDbl(.bytesFetched)
    }
}

// ── GET /api/sources/:id/detail ─────────────────────────────────────────────

/// sourceapi_detail() (sourceapi.c:1389-1588).
struct SourcePageDetail: Decodable {
    let registry: SPRegistry
    /// hp_describe_json() (lib/hpengine.c:6379) for an hpengine row — mode,
    /// request, parsing, detail_hop, paging (incl. page_resume_note /
    /// window_resume_note) — else {engine,url,interval,note}. Kept as raw
    /// JSON: it is the row AS DECLARED and grows with every engine field.
    let engine: JSONValue?
    let schedule: SPSchedule?
    let status: SPStatusBlock?
    let data: SPDataBlock?
    let notices: SPNotices?
    let runs: SPRuns?
    let credentials: SPCredentials?
    let pacing: SPPacing?
    let provenance: SPProvenance?
    let fetchedAt: String?
    private enum K: String, CodingKey {
        case registry, engine, schedule, status, data, notices, runs, credentials, pacing, provenance, meta
    }
    private enum MK: String, CodingKey { case fetchedAt = "fetched_at" }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        registry = try c.decode(SPRegistry.self, forKey: .registry)
        engine = c.spObj(JSONValue.self, .engine)
        schedule = c.spObj(SPSchedule.self, .schedule)
        status = c.spObj(SPStatusBlock.self, .status)
        data = c.spObj(SPDataBlock.self, .data)
        notices = c.spObj(SPNotices.self, .notices)
        runs = c.spObj(SPRuns.self, .runs)
        credentials = c.spObj(SPCredentials.self, .credentials)
        pacing = c.spObj(SPPacing.self, .pacing)
        provenance = c.spObj(SPProvenance.self, .provenance)
        let m = try? c.nestedContainer(keyedBy: MK.self, forKey: .meta)
        fetchedAt = m?.spStr(.fetchedAt)
    }
}

/// The `registry` block (sourceapi.c:1398-1424).
struct SPRegistry: Decodable {
    let id: String
    let name: String?
    let nameJa: String?
    let collector: String?
    let collectorLabel: String?
    let collectorParent: String?
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
    let freeTier: Bool?
    let curatedOverlay: Bool?
    let isEntityPivot: Bool?
    private enum K: String, CodingKey {
        case id, name, nameJa = "name_ja", collector, collectorMeta = "collector_meta"
        case category, type, engine, kind, interval, url, host, description, license, layer
        case freeTier = "free_tier", curatedOverlay = "curated_overlay", isEntityPivot = "is_entity_pivot"
    }
    private enum CK: String, CodingKey { case label, parent }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        id = try c.decode(String.self, forKey: .id)
        name = c.spStr(.name); nameJa = c.spStr(.nameJa); collector = c.spStr(.collector)
        let cm = try? c.nestedContainer(keyedBy: CK.self, forKey: .collectorMeta)
        collectorLabel = cm?.spStr(.label); collectorParent = cm?.spStr(.parent)
        category = c.spStr(.category); type = c.spStr(.type); engine = c.spStr(.engine)
        kind = c.spStr(.kind); interval = c.spDbl(.interval); url = c.spStr(.url)
        host = c.spStr(.host); description = c.spStr(.description); license = c.spStr(.license)
        layer = c.spStr(.layer); freeTier = c.spBool(.freeTier)
        curatedOverlay = c.spBool(.curatedOverlay); isEntityPivot = c.spBool(.isEntityPivot)
    }
}

/// The detail page's `status` block (sourceapi.c:1465-1495). Its trust object
/// is snake_case, unlike /api/status's camelCase `SourceTrust`.
struct SPStatusBlock: Decodable {
    let status: String?
    let health: SPHealth?
    let lastCheck: String?
    let lastSuccess: String?
    let responseTimeMs: Double?
    let recordsLastRun: Int?
    let errorMessage: String?
    let trustRated: Bool?
    let trustGrade: String?
    let trustReliability: Double?
    let trustSuccessRate: Double?
    let trustFreshness: Double?
    let trustAnomalies30d: Int?
    private enum K: String, CodingKey {
        case status, health, lastCheck = "last_check", lastSuccess = "last_success"
        case responseTimeMs = "response_time_ms", recordsLastRun = "records_last_run"
        case errorMessage = "error_message", trust
    }
    private enum TK: String, CodingKey {
        case rated, grade, reliability, successRate = "success_rate", freshness, anomalies30d = "anomalies_30d"
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        status = c.spStr(.status); health = c.spObj(SPHealth.self, .health)
        lastCheck = c.spStr(.lastCheck); lastSuccess = c.spStr(.lastSuccess)
        responseTimeMs = c.spDbl(.responseTimeMs); recordsLastRun = c.spInt(.recordsLastRun)
        errorMessage = c.spStr(.errorMessage)
        let t = try? c.nestedContainer(keyedBy: TK.self, forKey: .trust)
        trustRated = t?.spBool(.rated); trustGrade = t?.spStr(.grade)
        trustReliability = t?.spDbl(.reliability); trustSuccessRate = t?.spDbl(.successRate)
        trustFreshness = t?.spDbl(.freshness); trustAnomalies30d = t?.spInt(.anomalies30d)
    }
}

/// The `data` block (sourceapi.c:1497-1536).
struct SPDataBlock: Decodable {
    let itemCount: Int?
    let geocoded: Int?
    let notices: Int?
    let lastFetched: String?
    let awaitingGeo: Int?
    let firstFetched: String?
    let firstPublished: String?
    let lastPublished: String?
    let dbTextBytes: Double?
    let bbox: SPBBox?
    let recordTypes: SPFacet?
    let languages: [SPCount]
    let fieldSampleSize: Int?
    let fields: SPFacet?
    private enum K: String, CodingKey {
        case itemCount = "item_count", geocoded, notices, lastFetched = "last_fetched"
        case awaitingGeo = "awaiting_geo", firstFetched = "first_fetched"
        case firstPublished = "first_published", lastPublished = "last_published"
        case dbTextBytes = "db_text_bytes", bbox, recordTypes = "record_types", languages
        case fieldInventory = "field_inventory"
    }
    private enum FK: String, CodingKey { case sampleSize = "sample_size", fields }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        itemCount = c.spInt(.itemCount); geocoded = c.spInt(.geocoded); notices = c.spInt(.notices)
        lastFetched = c.spStr(.lastFetched); awaitingGeo = c.spInt(.awaitingGeo)
        firstFetched = c.spStr(.firstFetched); firstPublished = c.spStr(.firstPublished)
        lastPublished = c.spStr(.lastPublished); dbTextBytes = c.spDbl(.dbTextBytes)
        bbox = c.spObj(SPBBox.self, .bbox); recordTypes = c.spObj(SPFacet.self, .recordTypes)
        languages = c.lossyArray(SPCount.self, forKey: .languages)
        let f = try? c.nestedContainer(keyedBy: FK.self, forKey: .fieldInventory)
        fieldSampleSize = f?.spInt(.sampleSize)
        fields = f?.spObj(SPFacet.self, .fields)
    }
}

struct SPBBox: Decodable, Hashable {
    let min_lat: Double
    let max_lat: Double
    let min_lon: Double
    let max_lon: Double
}

/// notices_json() (sourceapi.c:1286-1327): per-kind counts and the newest
/// notice of each kind verbatim.
struct SPNotices: Decodable {
    let counts: [SPNoticeCount]
    let latest: [SPNotice]
    private enum K: String, CodingKey { case counts, latest }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        counts = c.lossyArray(SPNoticeCount.self, forKey: .counts)
        latest = c.lossyArray(SPNotice.self, forKey: .latest)
    }
}
struct SPNoticeCount: Decodable, Hashable, Identifiable {
    let kind: String
    let count: Int?
    let latestAt: String?
    var id: String { kind }
    private enum K: String, CodingKey { case kind, count, latestAt = "latest_at" }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        kind = c.spStr(.kind) ?? "?"; count = c.spInt(.count); latestAt = c.spStr(.latestAt)
    }
}
struct SPNotice: Decodable, Hashable, Identifiable {
    let kind: String
    let title: String?
    let fetchedAt: String?
    let uid: String?
    let properties: JSONValue?
    var id: String { uid ?? kind }
    private enum K: String, CodingKey { case kind, title, fetchedAt = "fetched_at", uid, properties }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        kind = c.spStr(.kind) ?? "?"; title = c.spStr(.title); fetchedAt = c.spStr(.fetchedAt)
        uid = c.spStr(.uid); properties = c.spObj(JSONValue.self, .properties)
    }
}

/// runs_json() (sourceapi.c:1329-1360): the newest 20 runs and how many exist.
struct SPRuns: Decodable {
    let recent: [SPRun]
    let shown: Int?
    let total: Int?
    private enum K: String, CodingKey { case recent, shown, total }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        recent = c.lossyArray(SPRun.self, forKey: .recent)
        shown = c.spInt(.shown); total = c.spInt(.total)
    }
}

/// statusapi_cred_json() → add_cred_status() (statusapi.c:103-160).
/// requiresKey null + credentialStatus "unknown" = never classified, which is
/// a different claim from "no key needed".
struct SPCredentials: Decodable {
    let requiresKey: Bool?
    let configured: Bool?
    let credentialStatus: String?
    let envVars: [EnvVarSpec]
    let missingVars: [String]
    private enum K: String, CodingKey { case requiresKey, configured, credentialStatus, envVars, missingVars }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        requiresKey = c.spBool(.requiresKey); configured = c.spBool(.configured)
        credentialStatus = c.spStr(.credentialStatus)
        envVars = c.lossyArray(EnvVarSpec.self, forKey: .envVars)
        missingVars = c.spStrs(.missingVars)
    }
}

/// The `pacing` block (sourceapi.c:1566-1573): core/hostgate.c's gap for the host.
struct SPPacing: Decodable {
    let host: String?
    let minGapMs: Double?
    private enum K: String, CodingKey { case host, minGapMs = "min_gap_ms" }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        host = c.spStr(.host); minGapMs = c.spDbl(.minGapMs)
    }
}

/// prov_json() (sourceapi.c:1159-1186): docs/ rows naming this id.
struct SPProvenance: Decodable {
    let filesScanned: Int?
    let rows: [SPProvRow]
    let shown: Int?
    let total: Int?
    let note: String?
    private enum K: String, CodingKey { case filesScanned = "files_scanned", rows, shown, total, note }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        filesScanned = c.spInt(.filesScanned); rows = c.lossyArray(SPProvRow.self, forKey: .rows)
        shown = c.spInt(.shown); total = c.spInt(.total); note = c.spStr(.note)
    }
}
struct SPProvRow: Decodable, Hashable {
    let file: String
    let line: Int?
    let text: String?
    private enum K: String, CodingKey { case file, line, text }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        file = c.spStr(.file) ?? "?"; line = c.spInt(.line); text = c.spStr(.text)
    }
}

// ── GET /api/sources/:id/summary ────────────────────────────────────────────

/// sourceapi_summary() (sourceapi_summary.c:300-375). Either a summary
/// (summary, result_type, caveats, model, facts_used, generated_at, cached,
/// served_at) or — when the model could not be reached or answered nothing
/// usable — `error` (an llm status code) plus the measured `facts`, never
/// prose that was not generated.
struct SourceSummary: Decodable {
    let summary: String?
    let resultType: String?
    let caveats: [String]
    let model: String?
    let factsUsed: Int?
    let generatedAt: String?
    let cached: Bool?
    let error: String?
    let httpStatus: Int?
    let attemptedAt: String?
    let facts: JSONValue?
    private enum K: String, CodingKey {
        case summary, resultType = "result_type", caveats, model, factsUsed = "facts_used"
        case generatedAt = "generated_at", cached, error, httpStatus = "http_status"
        case attemptedAt = "attempted_at", facts
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        summary = c.spStr(.summary); resultType = c.spStr(.resultType); caveats = c.spStrs(.caveats)
        model = c.spStr(.model); factsUsed = c.spInt(.factsUsed); generatedAt = c.spStr(.generatedAt)
        cached = c.spBool(.cached); error = c.spStr(.error); httpStatus = c.spInt(.httpStatus)
        attemptedAt = c.spStr(.attemptedAt); facts = c.spObj(JSONValue.self, .facts)
    }
}

// ── GET/PUT /api/sources/:id/settings ───────────────────────────────────────

/// source_settings_json() (source_settings.c:166-259). The key set is CLOSED
/// and every bound, default and help text comes from the server — the client
/// holds no second opinion about what a knob accepts. `can_write` is decided
/// by the route that enforces the gate (httpd.c, opgate_check).
struct SourceSettingsEnvelope: Decodable {
    let canWrite: Bool
    let settings: [SourceSetting]
    private enum K: String, CodingKey { case canWrite = "can_write", settings }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        canWrite = c.spBool(.canWrite) ?? false
        settings = c.lossyArray(SourceSetting.self, forKey: .settings)
    }
}

/// Keys today: `http_body_ceiling_bytes` (unit bytes) and `interval_sec`
/// (unit seconds; absent for an entity pivot, and its `default` is per source
/// — what runs without an override — with `default_basis` saying why).
struct SourceSetting: Decodable, Identifiable, Hashable {
    let key: String
    let label: String?
    let help: String?
    let unit: String?
    let min: Double?
    let max: Double?
    let defaultValue: Double?
    let defaultBasis: String?
    let value: Double?
    let enabled: Bool
    let configured: Bool?
    let effective: Double?
    let updatedAt: String?
    let updatedBy: String?
    var id: String { key }
    private enum K: String, CodingKey {
        case key, label, help, unit, min, max, defaultValue = "default", defaultBasis = "default_basis"
        case value, enabled, configured, effective, updatedAt = "updated_at", updatedBy = "updated_by"
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        key = try c.decode(String.self, forKey: .key)
        label = c.spStr(.label); help = c.spStr(.help); unit = c.spStr(.unit)
        min = c.spDbl(.min); max = c.spDbl(.max); defaultValue = c.spDbl(.defaultValue)
        defaultBasis = c.spStr(.defaultBasis); value = c.spDbl(.value)
        enabled = c.spBool(.enabled) ?? false; configured = c.spBool(.configured)
        effective = c.spDbl(.effective); updatedAt = c.spStr(.updatedAt); updatedBy = c.spStr(.updatedBy)
    }
}

// ── GET/PUT /api/sources/cadence ────────────────────────────────────────────

/// cadence_json() (cadence.c:194-231). The response carries no `can_write`:
/// the PUT is platform-operator gated (httpd.c:2438-2455) and a refusal comes
/// back as 403, which the view names.
struct CadencePolicyEnvelope: Decodable {
    let globalIntervalSec: Int
    let minIntervalSec: Int
    let maxIntervalSec: Int
    let mode: String?
    let source: String?
    let updatedAt: String?
    let updatedBy: String?
    let scheduledRows: Int?
    let runsPerDayDeclared: Int?
    let runsPerDayEffective: Int?
    let fastestEffectiveSec: Double?
    let slowestEffectiveSec: Double?
    let estimateBasis: String?
    private enum K: String, CodingKey { case policy, source, updatedAt = "updated_at", updatedBy = "updated_by", estimate }
    private enum PK: String, CodingKey {
        case global = "global_interval_sec", min = "min_interval_sec", max = "max_interval_sec", mode
    }
    private enum EK: String, CodingKey {
        case rows = "scheduled_rows", declared = "runs_per_day_declared", effective = "runs_per_day_effective"
        case fastest = "fastest_effective_sec", slowest = "slowest_effective_sec", basis
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        let p = try? c.nestedContainer(keyedBy: PK.self, forKey: .policy)
        globalIntervalSec = p?.spInt(.global) ?? 0
        minIntervalSec = p?.spInt(.min) ?? 0
        maxIntervalSec = p?.spInt(.max) ?? 0
        mode = p?.spStr(.mode)
        source = c.spStr(.source); updatedAt = c.spStr(.updatedAt); updatedBy = c.spStr(.updatedBy)
        let e = try? c.nestedContainer(keyedBy: EK.self, forKey: .estimate)
        scheduledRows = e?.spInt(.rows); runsPerDayDeclared = e?.spInt(.declared)
        runsPerDayEffective = e?.spInt(.effective); fastestEffectiveSec = e?.spDbl(.fastest)
        slowestEffectiveSec = e?.spDbl(.slowest); estimateBasis = e?.spStr(.basis)
    }
}

// ── /api/sources/search, /api/collectors, /api/collectors/:key ──────────────

/// row_json() (sourceapi.c:506-561) — the compact row every source list shares.
struct SourceSearchRow: Decodable, Identifiable, Hashable {
    let id: String
    let name: String?
    let nameJa: String?
    let collector: String?
    let collectorLabel: String?
    let group: String?
    let groupLabel: String?
    let category: String?
    let type: String?
    let engine: String?
    let kind: String?
    let interval: Double?
    let host: String?
    let itemCount: Int?
    let notices: Int?
    let lastFetched: String?
    let status: String?
    let health: SPHealth?
    let recordsLastRun: Int?
    let errorMessage: String?
    let trustGrade: String?
    let sched: SPSchedule?
    private enum K: String, CodingKey {
        case id, name, nameJa = "name_ja", collector, collectorLabel = "collector_label"
        case group, groupLabel = "group_label", category, type, engine, kind, interval, host
        case itemCount = "item_count", notices, lastFetched = "last_fetched", status, health
        case recordsLastRun = "records_last_run", errorMessage = "error_message", trust, sched
    }
    private enum TK: String, CodingKey { case grade }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        id = try c.decode(String.self, forKey: .id)
        name = c.spStr(.name); nameJa = c.spStr(.nameJa); collector = c.spStr(.collector)
        collectorLabel = c.spStr(.collectorLabel); group = c.spStr(.group); groupLabel = c.spStr(.groupLabel)
        category = c.spStr(.category); type = c.spStr(.type); engine = c.spStr(.engine)
        kind = c.spStr(.kind); interval = c.spDbl(.interval); host = c.spStr(.host)
        itemCount = c.spInt(.itemCount); notices = c.spInt(.notices); lastFetched = c.spStr(.lastFetched)
        status = c.spStr(.status); health = c.spObj(SPHealth.self, .health)
        recordsLastRun = c.spInt(.recordsLastRun); errorMessage = c.spStr(.errorMessage)
        let t = try? c.nestedContainer(keyedBy: TK.self, forKey: .trust)
        trustGrade = t?.spStr(.grade)
        sched = c.spObj(SPSchedule.self, .sched)
    }
    static func == (a: SourceSearchRow, b: SourceSearchRow) -> Bool { a.id == b.id }
    func hash(into h: inout Hasher) { h.combine(id) }
}

/// search_obj() (sourceapi.c:686-764): {data, page:{limit,offset,count,total,
/// has_more}, meta:{facets, fetched_at, snapshot_at, registry_total, filters}}.
struct SourceSearchPage: Decodable {
    let rows: [SourceSearchRow]
    let page: OffsetPage?
    let registryTotal: Int?
    private enum K: String, CodingKey { case data, page, meta }
    private enum MK: String, CodingKey { case registryTotal = "registry_total" }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        rows = c.lossyArray(SourceSearchRow.self, forKey: .data)
        page = c.spObj(OffsetPage.self, .page)
        let m = try? c.nestedContainer(keyedBy: MK.self, forKey: .meta)
        registryTotal = m?.spInt(.registryTotal)
    }
}

/// coll_json() (sourceapi.c:794-827), and a group object from groups_json()
/// (:899-919), which adds `collectors` (member keys) and `collector_count`.
struct CollectorSummary: Decodable, Identifiable, Hashable {
    let key: String
    let label: String?
    let description: String?
    let parent: String?
    let isGroup: Bool?
    let group: String?
    let groupLabel: String?
    let sources: Int
    let scheduled: Int?
    let pivots: Int?
    let internalCount: Int?
    let unreachable: Int?
    let items: Int?
    let notices: Int?
    let lastFetched: String?
    let online: Int?
    let degraded: Int?
    let offline: Int?
    let pending: Int?
    let memberKeys: [String]
    var id: String { key }
    private enum K: String, CodingKey {
        case key, label, description, parent, isGroup = "is_group", group, groupLabel = "group_label"
        case sources, scheduled, pivots, internalCount = "internal", unreachable, items, notices
        case lastFetched = "last_fetched", status, collectors
    }
    private enum SK: String, CodingKey { case online, degraded, offline, pending }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        key = try c.decode(String.self, forKey: .key)
        label = c.spStr(.label); description = c.spStr(.description); parent = c.spStr(.parent)
        isGroup = c.spBool(.isGroup); group = c.spStr(.group); groupLabel = c.spStr(.groupLabel)
        sources = c.spInt(.sources) ?? 0; scheduled = c.spInt(.scheduled); pivots = c.spInt(.pivots)
        internalCount = c.spInt(.internalCount); unreachable = c.spInt(.unreachable)
        items = c.spInt(.items); notices = c.spInt(.notices); lastFetched = c.spStr(.lastFetched)
        let s = try? c.nestedContainer(keyedBy: SK.self, forKey: .status)
        online = s?.spInt(.online); degraded = s?.spInt(.degraded)
        offline = s?.spInt(.offline); pending = s?.spInt(.pending)
        memberKeys = c.spStrs(.collectors)
    }
    static func == (a: CollectorSummary, b: CollectorSummary) -> Bool { a.key == b.key }
    func hash(into h: inout Hasher) { h.combine(key) }
}

/// sourceapi_collectors() (sourceapi.c:921-959): {data, groups, meta}.
struct CollectorsEnvelope: Decodable {
    let collectors: [CollectorSummary]
    let groups: [CollectorSummary]
    let total: Int?
    let sources: Int?
    let registryTotal: Int?
    let unassignedCollectors: Int?
    private enum K: String, CodingKey { case data, groups, meta }
    private enum MK: String, CodingKey {
        case total, sources, registryTotal = "registry_total", unassigned = "unassigned_collectors"
    }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        collectors = c.lossyArray(CollectorSummary.self, forKey: .data)
        groups = c.lossyArray(CollectorSummary.self, forKey: .groups)
        let m = try? c.nestedContainer(keyedBy: MK.self, forKey: .meta)
        total = m?.spInt(.total); sources = m?.spInt(.sources)
        registryTotal = m?.spInt(.registryTotal); unassignedCollectors = m?.spInt(.unassigned)
    }
}

/// sourceapi_collector() (sourceapi.c:1002-1061): {collector, children,
/// member_keys, is_group, groups (id-prefix facet, null for a group),
/// sources (a search page restricted to the key)}.
struct CollectorDetailEnvelope: Decodable {
    let collector: CollectorSummary
    let children: [CollectorSummary]
    let isGroup: Bool?
    let idPrefixes: SPFacet?
    let sources: SourceSearchPage?
    private enum K: String, CodingKey { case collector, children, isGroup = "is_group", groups, sources }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        collector = try c.decode(CollectorSummary.self, forKey: .collector)
        children = c.lossyArray(CollectorSummary.self, forKey: .children)
        isGroup = c.spBool(.isGroup)
        idPrefixes = c.spObj(SPFacet.self, .groups)
        sources = c.spObj(SourceSearchPage.self, .sources)
    }
}

// ── GET /api/sources/:id/logs ───────────────────────────────────────────────

/// miscapi_source_logs() (miscapi.c:117-178): {data, page:{limit, offset,
/// total}, meta}. This page block carries no `count`/`has_more`, so "more"
/// is measured as offset + rows < total.
struct SourceLogsPage: Decodable {
    let rows: [SPRun]
    let limit: Int?
    let offset: Int?
    let total: Int?
    private enum K: String, CodingKey { case data, page }
    private enum PK: String, CodingKey { case limit, offset, total }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: K.self)
        rows = c.lossyArray(SPRun.self, forKey: .data)
        let p = try? c.nestedContainer(keyedBy: PK.self, forKey: .page)
        limit = p?.spInt(.limit); offset = p?.spInt(.offset); total = p?.spInt(.total)
    }
}
