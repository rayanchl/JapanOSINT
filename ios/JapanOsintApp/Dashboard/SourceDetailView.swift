import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// One source in full — the iOS counterpart of the web client's source page
// (client/src/components/sources/SourceDetailPage.jsx). Everything on it is
// one GET /api/sources/:id/detail plus the summary and settings routes; a
// value the server did not measure reads "not measured", never a guessed 0.
//
// The dashboard's compact sheet (SourceDashboardTab.SourceDetail) is built
// from a /api/status row, which cannot say which engine runs a source, how it
// pages, why its cadence is what it is, or what the engine itself recorded
// about its last failure. Those are the facts this page is for.
// ─────────────────────────────────────────────────────────────────────────────

private let notMeasured = "not measured"

fileprivate extension JSONValue {
    subscript(_ k: String) -> JSONValue? {
        if case .object(let o) = self { return o[k] }
        return nil
    }
    /// A scalar as display text; nil for null, objects and arrays.
    var text: String? {
        switch self {
        case .string(let s): return s.isEmpty ? nil : s
        case .number(let n):
            if n == n.rounded(), abs(n) < 1e15 { return String(Int64(n)) }
            return String(n)
        case .bool(let b): return b ? "yes" : "no"
        default: return nil
        }
    }
    var number: Double? {
        if case .number(let n) = self { return n }
        return nil
    }
    var truthy: Bool {
        switch self {
        case .bool(let b): return b
        case .number(let n): return n != 0
        default: return false
        }
    }
    var strings: [String] {
        if case .array(let a) = self { return a.compactMap { $0.text } }
        return []
    }
    var pretty: String {
        let enc = JSONEncoder()
        enc.outputFormatting = [.prettyPrinted, .sortedKeys]
        guard let d = try? enc.encode(self), let s = String(data: d, encoding: .utf8) else { return "" }
        return s
    }
}

struct SourceDetailView: View {
    let sourceId: String

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var detail: SourcePageDetail?
    @State private var loadError: String?
    @State private var loading = false
    @State private var summary: SourceSummary?
    @State private var summaryError: String?
    @State private var summaryLoading = false
    @State private var running = false
    @State private var runNote: String?

    var body: some View {
        Group {
            if let d = detail {
                ScrollView {
                    VStack(alignment: .leading, spacing: Space.md) {
                        content(d)
                    }
                    .padding()
                }
                .refreshable { await load() }
            } else if let loadError {
                VStack(spacing: Space.sm) {
                    Text(loadError).font(.caption).foregroundStyle(theme.danger)
                        .multilineTextAlignment(.center)
                    Button("Retry") { Task { await load() } }.buttonStyle(.borderedProminent)
                }
                .padding()
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .themedScreenBackground(theme)
        .navigationTitle(detail?.registry.name ?? sourceId)
        .compatInlineTitle()
        .task {
            if detail == nil { await load() }
            if summary == nil && summaryError == nil { await loadSummary(refresh: false) }
        }
    }

    // MARK: - Layout

    @ViewBuilder private func content(_ d: SourcePageDetail) -> some View {
        let reg = d.registry
        header(d)
        if let desc = reg.description, !desc.isEmpty {
            Text(desc).font(.caption).foregroundStyle(theme.textMuted)
        }
        actions(d)
        summarySection(reg.id)
        if let st = d.status, let h = st.health, h.working == false { healthSection(h) }
        statusSection(d)
        if let sch = d.schedule { frequencySection(sch, sourceId: reg.id) }
        engineSection(d)
        if let data = d.data { dataSection(data, reg: reg) }
        runsSection(d)
        noticesSection(d.notices)
        sizeSection(d)
        SourceSettingsSection(sourceId: reg.id)
        provenanceSection(d.provenance)
        Text("detail fetched \(relativeTime(d.fetchedAt))")
            .font(.caption2.monospaced()).foregroundStyle(theme.textMuted)
    }

    private func header(_ d: SourcePageDetail) -> some View {
        let reg = d.registry
        return VStack(alignment: .leading, spacing: Space.xs) {
            HStack(alignment: .firstTextBaseline, spacing: Space.sm) {
                Text(reg.name ?? reg.id).font(.title3.bold()).foregroundStyle(theme.text)
                Pill(text: d.status?.status ?? "never run", tone: statusTone(d.status?.status))
                if d.status?.trustRated == true, let g = d.status?.trustGrade {
                    Pill(text: "grade \(g)", tone: .info)
                }
            }
            if let ja = reg.nameJa, !ja.isEmpty, ja != reg.name {
                Text(ja).font(.subheadline).foregroundStyle(theme.textMuted)
            }
            Text(reg.id).font(.caption2.monospaced()).foregroundStyle(theme.textMuted)
                .textSelection(.enabled)
            HStack(spacing: Space.xs) {
                if let c = reg.category { Pill(text: c) }
                if let t = reg.type { Pill(text: t) }
                if let e = reg.engine { Pill(text: e, tone: .accent) }
                if let k = reg.kind { Pill(text: k) }
            }
            if let key = reg.collector {
                NavigationLink {
                    CollectorDetailView(key: key)
                } label: {
                    Label("Collector: \(reg.collectorLabel ?? key)", systemImage: "square.stack.3d.up")
                        .font(.caption)
                }
                .tint(theme.accent)
            }
        }
    }

    @ViewBuilder private func actions(_ d: SourcePageDetail) -> some View {
        let reg = d.registry
        HStack(spacing: Space.sm) {
            if reg.kind != "internal" {
                Button {
                    Task { await runNow(reg.id) }
                } label: {
                    HStack(spacing: 6) {
                        if running { ProgressView().scaleEffect(0.7) } else { Image(systemName: "play.fill").accessibilityHidden(true) }
                        Text(running ? "Running…" : "Run now")
                    }
                }
                .buttonStyle(.borderedProminent).controlSize(.small).disabled(running)
            }
            NavigationLink {
                IntelSourceItemsView(source: IntelSource(
                    id: reg.id, name: reg.name ?? reg.id, name_ja: reg.nameJa,
                    category: reg.category, description: reg.description, url: reg.url,
                    item_count: d.data?.itemCount ?? 0, last_fetched: d.data?.lastFetched,
                    last_published: d.data?.lastPublished, ttl_ms: nil, parent_id: nil))
            } label: {
                Label("Stored items", systemImage: "list.bullet.rectangle").font(.caption)
            }
            .buttonStyle(.bordered).controlSize(.small)
            if let u = reg.url, let url = URL(string: u), url.scheme == "https" || url.scheme == "http" {
                Link(destination: url) {
                    Label("Upstream", systemImage: "safari").font(.caption)
                }
                .buttonStyle(.bordered).controlSize(.small)
            }
            Spacer(minLength: 0)
        }
        if let n = runNote {
            Text(n).font(.caption2).foregroundStyle(n.hasPrefix("failed") ? theme.danger : theme.textMuted)
        }
    }

    // MARK: - What it is (summary)

    @ViewBuilder private func summarySection(_ id: String) -> some View {
        SPSection(title: "What it is") {
            if summaryLoading && summary == nil {
                SPRow(k: "summary", v: "generating…")
            } else if let s = summary {
                if let text = s.summary {
                    Text(text).font(.caption).foregroundStyle(theme.text)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(8).background(theme.surfaceElevated)
                    if let rt = s.resultType { SPRow(k: "result type", v: rt) }
                    ForEach(Array(s.caveats.enumerated()), id: \.offset) { _, c in
                        SPRow(k: "caveat", v: c, color: theme.warning)
                    }
                    SPRow(k: "from", v: "\(s.factsUsed.map(String.init) ?? "?") measured facts · \(s.model ?? "model unknown")\(s.cached == true ? " · cached" : "") · \(relativeTime(s.generatedAt))")
                } else {
                    // The server's own refusal: an llm status code plus the
                    // facts it would have summarised. Shown as such — no prose
                    // is substituted for a summary that was not generated.
                    SPRow(k: "no summary", v: summaryErrorText(s), color: theme.warning)
                    if let f = s.facts {
                        DisclosureGroup("Measured facts (\(s.factsUsed.map(String.init) ?? "?"))") {
                            Text(f.pretty).font(.caption2.monospaced()).foregroundStyle(theme.textMuted)
                                .textSelection(.enabled)
                                .frame(maxWidth: .infinity, alignment: .leading)
                        }
                        .font(.caption).padding(8).background(theme.surfaceElevated)
                    }
                }
            } else if let e = summaryError {
                SPRow(k: "summary", v: e, color: theme.danger)
            }
            HStack {
                Spacer()
                Button(summaryLoading ? "Generating…" : "Regenerate") {
                    Task { await loadSummary(refresh: true) }
                }
                .font(.caption).disabled(summaryLoading)
            }
            .padding(8).background(theme.surfaceElevated)
        }
    }

    // MARK: - Health / status

    private func healthSection(_ h: SPHealth) -> some View {
        SPSection(title: "Not working") {
            SPRow(k: "state", v: h.state ?? "unknown", color: theme.danger)
            if let s = h.httpStatus {
                SPRow(k: "upstream answered", v: "HTTP \(s)\(h.bodyParsed == false ? " · the body did not parse" : "")")
            }
            if let o = h.outcome { SPRow(k: "engine read it as", v: o) }
            if !h.credentialsRequired.isEmpty {
                SPRow(k: "needs a credential", v: h.credentialsRequired.joined(separator: ", "), color: theme.warning)
            }
            if let n = h.note { SPRow(k: "what happened", v: n) }
            if let r = h.remedy { SPRow(k: "remedy", v: r) }
            if let s = h.since { SPRow(k: "recorded", v: relativeTime(s)) }
        }
    }

    private func statusSection(_ d: SourcePageDetail) -> some View {
        let st = d.status
        return SPSection(title: "Status") {
            SPRow(k: "status", v: st?.status ?? "pending — never run", color: statusColor(st?.status))
            SPRow(k: "last check", v: relativeTime(st?.lastCheck))
            SPRow(k: "last success", v: relativeTime(st?.lastSuccess))
            SPRow(k: "last run", v: lastRunText(st))
            if let e = st?.errorMessage, !e.isEmpty { SPRow(k: "last error", v: e, color: theme.danger) }
            SPRow(k: "trust", v: trustText(st))
            SPRow(k: "credential", v: credentialText(d.credentials))
            if let c = d.credentials, !c.envVars.isEmpty {
                ForEach(c.envVars, id: \.name) { v in
                    SPRow(k: v.role ?? "env", v: "\(v.name) — \(v.set == true ? "set" : "unset")",
                          color: v.set == true ? theme.success : theme.warning)
                }
            }
        }
    }

    // MARK: - Frequency

    @ViewBuilder private func frequencySection(_ sch: SPSchedule, sourceId: String) -> some View {
        SPSection(title: "Frequency") {
            // `effectiveInterval` is what the scheduler keeps after the last run
            // (backoff included); `cadenceBasis` names which figure decided it.
            SPRow(k: "runs every", v: runsEveryText(sch))
            SPRow(k: "declared", v: fmtInterval(sch.declaredInterval))
            SPRow(k: "measured", v: measuredText(sch))
            SPRow(k: "policy", v: sch.policyInterval.map { "\(fmtInterval($0)) (\(sch.policy ?? "declared"))" } ?? "not reported")
            SPRow(k: "set for this source", v: sch.overrideInterval.map { "\(fmtInterval($0)) (outranks the policy)" } ?? "no")
            SPRow(k: "next due", v: nextDueText(sch))
            SPRow(k: "backed off", v: sch.backedOff == true ? "until \(relativeTime(sch.backoffUntil))" : "no",
                  color: sch.backedOff == true ? theme.warning : nil)
            SPRow(k: "quarantined", v: sch.quarantined == true ? "yes (health)" : "no",
                  color: sch.quarantined == true ? theme.danger : nil)
            SPRow(k: "failures / empties", v: "\(sch.consecutiveFailures ?? 0) / \(sch.consecutiveEmpties ?? 0) consecutive")
            if let s = sch.stats30d {
                SPRow(k: "runs (30 d)", v: runs30dText(s))
                SPRow(k: "avg duration", v: s.avgDurationMs.map { "\(Int($0)) ms" } ?? notMeasured)
            }
            if let m = sch.scheduleMode { SPRow(k: "schedule mode", v: m) }
            NavigationLink {
                CadencePolicyView()
            } label: {
                HStack {
                    Text("System-wide cadence policy").font(.caption)
                    Spacer()
                    Image(systemName: "chevron.right").font(.caption2).accessibilityHidden(true)
                }
                .padding(8).background(theme.surfaceElevated)
            }
            .buttonStyle(.plain)
        }
    }

    // MARK: - Engine

    @ViewBuilder private func engineSection(_ d: SourcePageDetail) -> some View {
        let reg = d.registry
        let eng = d.engine
        let isHp = eng?["engine"]?.text == "hpengine"
        SPSection(title: "Engine and mechanism") {
            SPRow(k: "engine", v: reg.engine ?? notMeasured)
            SPRow(k: "kind", v: reg.kind ?? notMeasured)
            SPRow(k: "entity pivot", v: reg.isEntityPivot == true ? "yes — routable by the OSINT dispatcher" : "no")
            if isHp, let e = eng {
                if let m = e["mode"]?.text { SPRow(k: "mode", v: m) }
                let toks = e["entity_tokens"]?.strings ?? []
                SPRow(k: "entity tokens", v: toks.isEmpty ? "none (fixed endpoint)" : toks.joined(separator: " "))
                if let r = e["request"] {
                    SPRow(k: "request", v: "\(r["method"]?.text ?? "GET") \(r["url"]?.text ?? "")", mono: true)
                    if let k = r["key_env"]?.text { SPRow(k: "credential env", v: k) }
                    if let i = r["index_url"]?.text { SPRow(k: "index page", v: i, mono: true) }
                    if let t = r["timeout_ms"]?.text { SPRow(k: "timeout", v: "\(t) ms") }
                    if let p = r["page_delay_ms"]?.text { SPRow(k: "page pacing", v: "\(p) ms") }
                }
                if let p = e["parsing"] {
                    SPRow(k: "array path", v: p["array_path"]?.text ?? "auto-detected (densest array of objects)")
                    SPRow(k: "id keys", v: p["id_keys"]?.text.map { "\($0)  (+ composes, , chooses)" } ?? "default list (id / uid / guid / …)")
                    SPRow(k: "title keys", v: p["title_keys"]?.text ?? "default list (name / title / …)")
                    SPRow(k: "record type", v: p["record_type"]?.text ?? notMeasured)
                }
                if let dh = e["detail_hop"] {
                    SPRow(k: "detail hop", v: dh["declared"]?.truthy == true
                          ? "\(dh["detail_url"]?.text ?? "?") via \(dh["detail_key"]?.text ?? "?")" : "none", mono: dh["declared"]?.truthy == true)
                }
                if let pg = e["paging"] {
                    SPRow(k: "paging", v: pagingText(pg))
                    // A cycling row's page_max is ONE SLICE, not the size of
                    // what it reads — the engine says so in these notes.
                    if let n = pg["page_resume_note"]?.text { SPRow(k: "cycle", v: n, color: theme.accent) }
                    if let n = pg["window_resume_note"]?.text {
                        SPRow(k: "date window", v: windowText(n, pg), color: theme.accent)
                    }
                }
            } else {
                SPRow(k: "registered url", v: reg.url ?? notMeasured, mono: true)
                if let n = eng?["note"]?.text { SPRow(k: "note", v: n) }
            }
            SPRow(k: "host", v: reg.host ?? notMeasured)
            SPRow(k: "host pacing", v: d.pacing?.minGapMs.map { "≥ \(Int($0)) ms between requests" } ?? notMeasured)
            if let e = eng {
                DisclosureGroup("The row as declared (raw)") {
                    Text(e.pretty).font(.caption2.monospaced()).foregroundStyle(theme.textMuted)
                        .textSelection(.enabled)
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
                .font(.caption).padding(8).background(theme.surfaceElevated)
            }
        }
    }

    /// The interval that runs and the figure that decided it. Null
    /// `effectiveInterval` = the scheduler holds no state for the row yet.
    private func runsEveryText(_ sch: SPSchedule) -> String {
        let basis = sch.cadenceBasis.map { " (\($0))" } ?? ""
        if let eff = sch.effectiveInterval { return fmtInterval(eff) + basis }
        if (sch.declaredInterval ?? 0) <= 0 { return "on demand" }
        let planned: Double? = sch.overrideInterval ?? sch.policyInterval
        return "not yet run — " + fmtInterval(planned) + basis
    }

    /// The summary route's refusal: its llm status code, and the HTTP status
    /// the model server answered when there was one.
    private func summaryErrorText(_ s: SourceSummary) -> String {
        var t = s.error ?? "unknown error"
        if let h = s.httpStatus, h > 0 { t += " (HTTP \(h))" }
        return t
    }

    private func lastRunText(_ st: SPStatusBlock?) -> String {
        guard let n = st?.recordsLastRun else { return notMeasured }
        let ms = st?.responseTimeMs.map { String(Int($0)) } ?? "?"
        return "\(n) records in \(ms) ms"
    }

    private func measuredText(_ sch: SPSchedule) -> String {
        guard sch.adaptive == true else { return "not adaptive" }
        guard let m = sch.measuredInterval else { return "adaptive — no dated item yet" }
        return fmtInterval(m) + " (newest stored item)"
    }

    private func nextDueText(_ sch: SPSchedule) -> String {
        if let due = sch.nextDue { return relativeTime(due) + (sch.overdue == true ? " — overdue" : "") }
        return (sch.declaredInterval ?? 0) > 0 ? "not yet run" : "on demand"
    }

    private func runs30dText(_ s: SPStats30d) -> String {
        var t = String(s.runs ?? 0)
        if let r = s.successRate { t += " (\(Int((r * 100).rounded()))% ok)" }
        return t
    }

    private func geocodedText(_ data: SPDataBlock) -> String {
        let g = data.geocoded.map { $0.formatted() } ?? "?"
        let n = data.itemCount.map { $0.formatted() } ?? "?"
        var t = "\(g) of \(n)"
        if let a = data.awaitingGeo { t += " · \(a.formatted()) awaiting geo" }
        return t
    }

    private func lastTransferText(_ last: SPRun?) -> String {
        guard let last else { return "never run" }
        guard let b = last.bytesFetched else { return notMeasured + " (run predates byte accounting)" }
        return fmtBytes(b) + " received"
    }

    private func transfer30dText(_ s: SPStats30d?) -> String {
        if let b = s?.bytes { return "\(fmtBytes(b)) over \(s?.runsWithBytes ?? 0) runs" }
        return (s?.runs ?? 0) > 0 ? notMeasured + " (runs predate byte accounting)" : "no runs in 30 days"
    }

    private func windowText(_ note: String, _ pg: JSONValue) -> String {
        var t = note
        if let d = pg["window_days"]?.text { t += " — \(d) days" }
        if let f = pg["window_from"]?.text { t += ", from \(f)" }
        return t
    }

    private func pagingText(_ pg: JSONValue) -> String {
        let strategy = pg["strategy"]?.text ?? "?"
        if strategy == "single_request" { return "single request" }
        var s = strategy
        if let n = pg["next_path"]?.text { s += " \(n)" }
        if let p = pg["page_param"]?.text { s += " \(p)" }
        if let z = pg["page_size"]?.text { s += ", \(z)/page" }
        if pg["page_zero_based"]?.truthy == true { s += ", 0-based" }
        if let m = pg["page_max"]?.text { s += ", up to \(m) pages per run" }
        return s
    }

    // MARK: - Data

    @ViewBuilder private func dataSection(_ data: SPDataBlock, reg: SPRegistry) -> some View {
        SPSection(title: "Obtained data") {
            SPRow(k: "records stored", v: data.itemCount.map { $0.formatted() } ?? notMeasured)
            SPRow(k: "of which notices", v: data.notices.map { $0.formatted() } ?? notMeasured)
            SPRow(k: "geocoded", v: geocodedText(data))
            SPRow(k: "fetched", v: "\(relativeTime(data.firstFetched)) → \(relativeTime(data.lastFetched))")
            SPRow(k: "published", v: (data.firstPublished != nil || data.lastPublished != nil)
                  ? "\(data.firstPublished ?? "?") → \(data.lastPublished ?? "?")" : notMeasured)
            SPRow(k: "languages", v: data.languages.isEmpty ? "none stored" : data.languages.map { "\($0.key) (\($0.count.formatted()))" }.joined(separator: ", "))
            if let b = data.bbox {
                SPRow(k: "bounding box", v: String(format: "%.3f,%.3f → %.3f,%.3f", b.min_lat, b.min_lon, b.max_lat, b.max_lon))
            }
            SPRow(k: "map layer", v: reg.layer ?? "none")
            SPRow(k: "license", v: reg.license ?? notMeasured)
            if let rt = data.recordTypes { facetRows("record types", rt) }
            if let f = data.fields { facetRows("fields (sample of \(data.fieldSampleSize ?? 0))", f) }
        }
    }

    @ViewBuilder private func facetRows(_ title: String, _ f: SPFacet) -> some View {
        DisclosureGroup("\(title) — \(f.shown ?? f.values.count) shown\(f.total.map { " of \($0)" } ?? "")") {
            VStack(alignment: .leading, spacing: 2) {
                ForEach(f.values) { v in
                    HStack {
                        Text(v.label ?? v.key).font(.caption2.monospaced()).foregroundStyle(theme.text)
                        Spacer()
                        Text(v.count.formatted()).font(.caption2.monospacedDigit()).foregroundStyle(theme.textMuted)
                    }
                }
            }
        }
        .font(.caption).padding(8).background(theme.surfaceElevated)
    }

    // MARK: - Runs / notices / size / provenance

    @ViewBuilder private func runsSection(_ d: SourcePageDetail) -> some View {
        let runs = d.runs?.recent ?? []
        SPSection(title: "Recent runs") {
            if runs.isEmpty {
                SPRow(k: "runs", v: "this source has never run")
            } else {
                ForEach(runs) { r in SPRunRow(run: r) }
            }
            NavigationLink {
                SourceRunsView(sourceId: d.registry.id)
            } label: {
                HStack {
                    Text("\(d.runs?.shown ?? runs.count) shown of \(d.runs?.total.map { $0.formatted() } ?? "an uncounted number") — all runs")
                        .font(.caption)
                    Spacer()
                    Image(systemName: "chevron.right").font(.caption2).accessibilityHidden(true)
                }
                .padding(8).background(theme.surfaceElevated)
            }
            .buttonStyle(.plain)
        }
    }

    @ViewBuilder private func noticesSection(_ n: SPNotices?) -> some View {
        SPSection(title: "Engine notices (latest of each kind)") {
            let latest = n?.latest ?? []
            if latest.isEmpty {
                SPRow(k: "notices", v: "none stored — no fetch failure, shape surprise or truncation recorded")
            } else {
                ForEach(latest) { x in
                    VStack(alignment: .leading, spacing: 2) {
                        HStack {
                            Pill(text: x.kind, tone: x.kind == "collector-fetch-failure" ? .danger : .warning)
                            Spacer()
                            Text(relativeTime(x.fetchedAt)).font(.caption2.monospaced()).foregroundStyle(theme.textMuted)
                        }
                        if let t = x.title { Text(t).font(.caption).foregroundStyle(theme.text) }
                        if let p = x.properties {
                            DisclosureGroup("properties") {
                                Text(p.pretty).font(.caption2.monospaced()).foregroundStyle(theme.textMuted)
                                    .textSelection(.enabled)
                                    .frame(maxWidth: .infinity, alignment: .leading)
                            }
                            .font(.caption2)
                        }
                    }
                    .padding(8).background(theme.surfaceElevated)
                }
            }
            if let counts = n?.counts, !counts.isEmpty {
                SPRow(k: "counts", v: counts.map { "\($0.kind): \($0.count ?? 0)" }.joined(separator: " · "))
            }
        }
    }

    private func sizeSection(_ d: SourcePageDetail) -> some View {
        let s = d.schedule?.stats30d
        let last = d.runs?.recent.first
        return SPSection(title: "Size") {
            SPRow(k: "DB text size", v: "\(fmtBytes(d.data?.dbTextBytes)) (title + body + properties; not transfer size)")
            SPRow(k: "records, 30 d of runs", v: s?.records.map { $0.formatted() } ?? notMeasured)
            SPRow(k: "transfer, last run", v: lastTransferText(last))
            SPRow(k: "transfer, 30 d", v: transfer30dText(s))
        }
    }

    @ViewBuilder private func provenanceSection(_ p: SPProvenance?) -> some View {
        SPSection(title: "Provenance — docs/ rows naming this id") {
            if let note = p?.note {
                SPRow(k: "provenance", v: note)
            } else if (p?.rows ?? []).isEmpty {
                SPRow(k: "provenance", v: "none — this id is in no manifest, probe or verification table (\(p?.filesScanned ?? 0) files scanned)")
            } else {
                ForEach(Array((p?.rows ?? []).enumerated()), id: \.offset) { _, r in
                    SPRow(k: "\(r.file):\(r.line ?? 0)", v: r.text ?? "", mono: true)
                }
                if let total = p?.total, total > (p?.shown ?? 0) {
                    SPRow(k: "shown", v: "\(p?.shown ?? 0) of \(total) rows")
                }
            }
        }
    }

    // MARK: - Text helpers

    private func trustText(_ st: SPStatusBlock?) -> String {
        guard let st, st.trustRated == true else { return "unrated — no runs in the trust window" }
        func pct(_ v: Double?) -> String { v.map { "\(Int(($0 * 100).rounded()))%" } ?? "?" }
        return "grade \(st.trustGrade ?? "?") · reliability \(pct(st.trustReliability)) · success \(pct(st.trustSuccessRate)) · freshness \(pct(st.trustFreshness)) · anomalies \(st.trustAnomalies30d ?? 0) in 30 d"
    }

    private func credentialText(_ c: SPCredentials?) -> String {
        guard let c else { return notMeasured }
        guard let req = c.requiresKey else { return "never classified (no credential table entry)" }
        if !req { return "none required" }
        if c.configured == true { return "configured (\(c.envVars.map(\.name).joined(separator: ", ")))" }
        return "MISSING: \(c.missingVars.joined(separator: ", "))"
    }

    private func statusTone(_ s: String?) -> Pill.Tone {
        switch s {
        case "online": return .success
        case "degraded": return .warning
        case "offline": return .danger
        default: return .neutral
        }
    }
    private func statusColor(_ s: String?) -> Color {
        switch s {
        case "online": return theme.success
        case "degraded": return theme.warning
        case "offline": return theme.danger
        default: return theme.textMuted
        }
    }

    // MARK: - Data

    private func load() async {
        loading = true; defer { loading = false }
        do {
            detail = try await apiClient.api.sourcePageDetail(sourceId)
            loadError = nil
        } catch {
            if detail == nil {
                if case APIError.http(404, _) = error { loadError = "No registered source has the id \(sourceId)." }
                else { loadError = error.localizedDescription }
            }
        }
    }

    private func loadSummary(refresh: Bool) async {
        summaryLoading = true; defer { summaryLoading = false }
        do {
            summary = try await apiClient.api.sourceSummary(sourceId, refresh: refresh)
            summaryError = nil
        } catch {
            summaryError = error.localizedDescription
        }
    }

    private func runNow(_ id: String) async {
        running = true; runNote = nil
        defer { running = false }
        do {
            let r = try await apiClient.api.intelRunSource(id)
            runNote = r.error.map { "failed: \($0)" }
                ?? "ran: \(r.ingested.map(String.init) ?? "?") new rows in \(r.duration_ms.map(String.init) ?? "?") ms"
            await load()
        } catch { runNote = "failed: \(error.localizedDescription)" }
    }
}

// MARK: - Shared building blocks (this file + CollectorsView.swift)

/// A titled block of rows, styled like the dashboard sheet's `section`.
struct SPSection<Content: View>: View {
    let title: String
    @ViewBuilder let content: () -> Content
    @Environment(\.theme) private var theme
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title).font(.caption.bold()).foregroundStyle(theme.textMuted)
            VStack(spacing: 1) { content() }
                .clipShape(RoundedRectangle(cornerRadius: 8))
        }
    }
}

/// One key/value row, styled like the dashboard sheet's `kv`.
struct SPRow: View {
    let k: String
    let v: String
    var color: Color? = nil
    var mono: Bool = false
    @Environment(\.theme) private var theme
    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            Text(k).font(.caption.bold()).foregroundStyle(theme.textMuted)
                .frame(width: 110, alignment: .leading)
            Text(v).font(mono ? .caption.monospaced() : .caption).foregroundStyle(color ?? theme.text)
                .textSelection(.enabled)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding(8)
        .background(theme.surfaceElevated)
    }
}

/// One fetch_log row: when, status, fetched/stored, duration, bytes, error.
struct SPRunRow: View {
    let run: SPRun
    @Environment(\.theme) private var theme
    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            HStack(spacing: 6) {
                Circle().fill(run.ok ? theme.success : theme.danger).frame(width: 6, height: 6)
                    .accessibilityHidden(true)
                Text(run.status ?? "?").font(.caption2.bold())
                    .foregroundStyle(run.ok ? theme.success : theme.danger)
                Text(relativeTime(run.timestamp)).font(.caption2.monospaced()).foregroundStyle(theme.textMuted)
                Spacer()
                Text("\(run.recordsFetched.map(String.init) ?? "?") fetched · \(run.stored.map(String.init) ?? "–") stored")
                    .font(.caption2.monospacedDigit()).foregroundStyle(theme.textMuted)
            }
            HStack(spacing: 6) {
                if let ms = run.durationMs { Text("\(ms) ms").font(.caption2.monospacedDigit()) }
                // null bytes = a run from before byte accounting, not zero bytes
                Text(run.bytesFetched == nil ? "bytes not measured" : fmtBytes(run.bytesFetched))
                    .font(.caption2.monospacedDigit())
                Spacer()
            }
            .foregroundStyle(theme.textMuted)
            if let e = run.error, !e.isEmpty {
                Text(e).font(.caption2).foregroundStyle(theme.danger).lineLimit(4).textSelection(.enabled)
            }
        }
        .padding(8)
        .background(theme.surfaceElevated)
        .accessibilityElement(children: .combine)
    }
}

// MARK: - Every run, paged (GET /api/sources/:id/logs)

struct SourceRunsView: View {
    let sourceId: String
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @State private var rows: [SPRun] = []
    @State private var total: Int?
    @State private var loading = false
    @State private var errorText: String?
    private let pageSize = 100

    private var hasMore: Bool { total.map { rows.count < $0 } ?? false }

    var body: some View {
        ScrollView {
            LazyVStack(alignment: .leading, spacing: 1) {
                Text("\(rows.count.formatted()) of \(total.map { $0.formatted() } ?? "an uncounted number of") runs")
                    .font(.caption.monospacedDigit()).foregroundStyle(theme.textMuted)
                    .padding(.bottom, Space.xs)
                ForEach(rows) { r in SPRunRow(run: r) }
                if let errorText { Text(errorText).font(.caption).foregroundStyle(theme.danger) }
                if hasMore {
                    Button(loading ? "Loading…" : "Load \(pageSize) more") { Task { await loadMore() } }
                        .disabled(loading).padding(.top, Space.sm)
                }
            }
            .padding()
        }
        .themedScreenBackground(theme)
        .navigationTitle("Runs")
        .compatInlineTitle()
        .task { if rows.isEmpty { await loadMore() } }
    }

    private func loadMore() async {
        loading = true; defer { loading = false }
        do {
            let p = try await apiClient.api.sourceLogs(sourceId, limit: pageSize, offset: rows.count)
            rows.append(contentsOf: p.rows)
            total = p.total
            errorText = nil
            // A page that came back empty while `total` says more remain would
            // otherwise offer "load more" for ever; stop offering it.
            if p.rows.isEmpty { total = rows.count }
        } catch { errorText = error.localizedDescription }
    }
}

// MARK: - Settings (GET/PUT /api/sources/:id/settings)

/// Every bound, default and explanation comes from the server with the
/// values; the client holds no second opinion about what a knob accepts. A
/// non-operator sees the controls disabled with the reason, rather than a
/// form that is then refused with 403.
struct SourceSettingsSection: View {
    let sourceId: String
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @State private var env: SourceSettingsEnvelope?
    @State private var errorText: String?
    @State private var drafts: [String: String] = [:]
    @State private var enabledDrafts: [String: Bool] = [:]
    @State private var busy: String?
    @State private var note: String?

    var body: some View {
        // A VStack, not a Group: a Group whose `if` renders nothing has no
        // view for `.task` to attach to, so the settings would never load.
        VStack(alignment: .leading, spacing: 0) {
            if let env, !env.settings.isEmpty {
                SPSection(title: "Settings") {
                    if !env.canWrite {
                        SPRow(k: "read-only", v: "changing a source setting is an audited operator action")
                    }
                    ForEach(env.settings) { s in settingRow(s, canWrite: env.canWrite) }
                    if let note { SPRow(k: "result", v: note, color: note.hasPrefix("saved") ? theme.success : theme.danger) }
                }
            } else if let errorText {
                SPSection(title: "Settings") { SPRow(k: "settings", v: errorText, color: theme.danger) }
            }
        }
        .task { if env == nil { await load() } }
    }

    private func fmt(_ unit: String?, _ v: Double?) -> String {
        guard let v else { return notMeasured }
        switch unit {
        case "bytes": return fmtBytes(v)
        case "seconds": return fmtInterval(v)
        default: return v == v.rounded() ? String(Int64(v)) : String(v)
        }
    }

    private func boundsText(_ s: SourceSetting) -> String {
        var t = "\(fmt(s.unit, s.min)) – \(fmt(s.unit, s.max)), default \(fmt(s.unit, s.defaultValue))"
        if let b = s.defaultBasis { t += " (\(b))" }
        return t
    }

    /// What the collector will actually use — the toggle and the number do
    /// not say that between them.
    private func effectText(_ s: SourceSetting) -> String {
        var t = "in effect: " + fmt(s.unit, s.effective)
        if let by = s.updatedBy { t += " · set by \(by)" }
        if let at = s.updatedAt { t += " " + relativeTime(at) }
        return t
    }

    @ViewBuilder private func settingRow(_ s: SourceSetting, canWrite: Bool) -> some View {
        let enabled = Binding<Bool>(
            get: { enabledDrafts[s.key] ?? s.enabled },
            set: { enabledDrafts[s.key] = $0 })
        let text = Binding<String>(
            get: { drafts[s.key] ?? s.value.map { String(Int64($0)) } ?? "" },
            set: { drafts[s.key] = $0 })
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                VStack(alignment: .leading, spacing: 1) {
                    Text(s.label ?? s.key).font(.caption.bold()).foregroundStyle(theme.text)
                    Text(s.key).font(.caption2.monospaced()).foregroundStyle(theme.textMuted)
                }
                Spacer()
                Toggle("active", isOn: enabled).labelsHidden().disabled(!canWrite)
                    .accessibilityLabel("\(s.label ?? s.key) active")
            }
            if let h = s.help { Text(h).font(.caption2).foregroundStyle(theme.textMuted) }
            HStack {
                TextField("value (\(s.unit ?? ""))", text: text)
                    .font(.caption.monospaced())
                    .textFieldStyle(.roundedBorder)
                    .compatKeyboardNumber()
                    .disabled(!canWrite)
                Button(busy == s.key ? "Saving…" : "Save") { Task { await save(s) } }
                    .buttonStyle(.borderedProminent).controlSize(.small)
                    .disabled(!canWrite || busy != nil || Double(text.wrappedValue) == nil)
            }
            Text(boundsText(s))
                .font(.caption2).foregroundStyle(theme.textMuted)
            Text(effectText(s))
                .font(.caption2).foregroundStyle(theme.text)
        }
        .padding(8)
        .background(theme.surfaceElevated)
    }

    private func load() async {
        do { env = try await apiClient.api.sourceSettings(sourceId); errorText = nil }
        catch { errorText = API.sourcePagesErrorMessage(error) }
    }

    private func save(_ s: SourceSetting) async {
        guard let v = Double(drafts[s.key] ?? s.value.map { String(Int64($0)) } ?? "") else { return }
        busy = s.key; defer { busy = nil }
        do {
            env = try await apiClient.api.setSourceSetting(sourceId, key: s.key, value: v,
                                                          enabled: enabledDrafts[s.key] ?? s.enabled)
            drafts.removeValue(forKey: s.key)
            enabledDrafts.removeValue(forKey: s.key)
            note = "saved \(s.label ?? s.key)"
        } catch {
            // The server's own refusal, named — value_out_of_range and
            // operator_required each mean something specific.
            note = API.sourcePagesErrorMessage(error)
        }
    }
}

// MARK: - System-wide cadence policy (GET/PUT /api/sources/cadence)

struct CadencePolicyView: View {
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @State private var policy: CadencePolicyEnvelope?
    @State private var globalText = ""
    @State private var minText = ""
    @State private var maxText = ""
    @State private var errorText: String?
    @State private var note: String?
    @State private var saving = false

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Space.md) {
                if let p = policy {
                    SPSection(title: "Policy in force") {
                        SPRow(k: "mode", v: p.mode ?? "?")
                        SPRow(k: "global", v: p.globalIntervalSec > 0 ? fmtInterval(Double(p.globalIntervalSec)) : "off")
                        SPRow(k: "floor", v: p.minIntervalSec > 0 ? fmtInterval(Double(p.minIntervalSec)) : "off")
                        SPRow(k: "ceiling", v: p.maxIntervalSec > 0 ? fmtInterval(Double(p.maxIntervalSec)) : "off")
                        SPRow(k: "set from", v: setFromText(p))
                    }
                    SPSection(title: "What it costs") {
                        SPRow(k: "scheduled rows", v: p.scheduledRows.map { $0.formatted() } ?? notMeasured)
                        SPRow(k: "runs/day declared", v: p.runsPerDayDeclared.map { $0.formatted() } ?? notMeasured)
                        SPRow(k: "runs/day in force", v: p.runsPerDayEffective.map { $0.formatted() } ?? notMeasured)
                        SPRow(k: "fastest / slowest", v: "\(fmtInterval(p.fastestEffectiveSec)) / \(fmtInterval(p.slowestEffectiveSec))")
                        if let b = p.estimateBasis { SPRow(k: "basis", v: b) }
                    }
                    SPSection(title: "Change (platform operator; 0 clears a field)") {
                        field("global (s)", $globalText)
                        field("floor (s)", $minText)
                        field("ceiling (s)", $maxText)
                        HStack {
                            Spacer()
                            Button(saving ? "Saving…" : "Save policy") { Task { await save() } }
                                .buttonStyle(.borderedProminent).controlSize(.small)
                                .disabled(saving || Int(globalText) == nil || Int(minText) == nil || Int(maxText) == nil)
                        }
                        .padding(8).background(theme.surfaceElevated)
                    }
                    Text("A source's own interval (Settings › Collection interval on its page) outranks this policy.")
                        .font(.caption2).foregroundStyle(theme.textMuted)
                }
                if let note { Text(note).font(.caption).foregroundStyle(note.hasPrefix("saved") ? theme.success : theme.danger) }
                if let errorText { Text(errorText).font(.caption).foregroundStyle(theme.danger) }
                if policy == nil && errorText == nil { ProgressView().frame(maxWidth: .infinity) }
            }
            .padding()
        }
        .themedScreenBackground(theme)
        .navigationTitle("Cadence policy")
        .compatInlineTitle()
        .task { if policy == nil { await load() } }
    }

    private func setFromText(_ p: CadencePolicyEnvelope) -> String {
        var t = p.source ?? "?"
        if let by = p.updatedBy { t += " by \(by)" }
        if let at = p.updatedAt { t += " " + relativeTime(at) }
        return t
    }

    private func field(_ label: String, _ text: Binding<String>) -> some View {
        HStack {
            Text(label).font(.caption.bold()).foregroundStyle(theme.textMuted).frame(width: 110, alignment: .leading)
            TextField("0", text: text)
                .font(.caption.monospaced()).textFieldStyle(.roundedBorder)
                .compatKeyboardNumber()
        }
        .padding(8).background(theme.surfaceElevated)
    }

    private func apply(_ p: CadencePolicyEnvelope) {
        policy = p
        globalText = String(p.globalIntervalSec)
        minText = String(p.minIntervalSec)
        maxText = String(p.maxIntervalSec)
    }

    private func load() async {
        do { apply(try await apiClient.api.cadencePolicy()); errorText = nil }
        catch { errorText = error.localizedDescription }
    }

    private func save() async {
        guard let g = Int(globalText), let mn = Int(minText), let mx = Int(maxText) else { return }
        saving = true; defer { saving = false }
        do {
            apply(try await apiClient.api.setCadencePolicy(globalSec: g, minSec: mn, maxSec: mx))
            note = "saved — the running scheduler re-plans without a restart"
        } catch {
            note = API.sourcePagesErrorMessage(error)
        }
    }
}
