import SwiftUI

/// What the dashboard opens a source detail on. The sheet loads everything
/// else itself, so a deep link (`nav.showSource`) needs only an id.
struct SourceRef: Identifiable, Hashable {
    let id: String
}

/// One source in full, from the server's own routes (mirrors the web client's
/// SourceDetailPage): `/api/sources/:id/detail` (registry, health, schedule,
/// notices, recent runs), `/summary` (a small-model description built only
/// from measured facts), `/settings` (operator-editable, including the
/// interval), `/api/sources/cadence` (the system-wide policy) and
/// `/api/status/:id` (probe state and credentials for key-gated sources).
///
/// Every section states what the server did not say as "—" or "not reported",
/// never a guess.
struct SourceDetailView: View {
    let sourceId: String

    @EnvironmentObject var nav: MapNavigation
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var detail: SourceDetailEnvelope?
    @State private var statusRow: StatusRow?
    @State private var settings: SourceSettingsEnvelope?
    @State private var cadence: CadenceEnvelope?
    @State private var loading = true
    @State private var loadError: String?

    @State private var summary: SourceSummaryResponse?
    @State private var summaryLoading = false
    @State private var summaryError: String?

    @State private var running = false
    @State private var runNote: String?
    @State private var entityText = ""

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                if loading && detail == nil {
                    ProgressView("Loading source…")
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 24)
                } else if let loadError, detail == nil {
                    VStack(spacing: 8) {
                        Text(loadError)
                            .font(.caption)
                            .foregroundStyle(theme.danger)
                            .multilineTextAlignment(.center)
                        Button("Retry") { Task { await loadAll() } }
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 24)
                } else if let d = detail {
                    content(d)
                }
            }
            .padding()
        }
        .themedScreenBackground(theme)
        .navigationTitle(detail?.registry?.name ?? sourceId)
        .compatInlineTitle()
        .toolbar {
            ToolbarItem(placement: .compatPrimary) {
                Button("Close") { dismiss() }
            }
        }
        .task(id: sourceId) { await loadAll() }
        .task(id: sourceId) { await loadSummary(refresh: false) }
    }

    // MARK: - Content

    @ViewBuilder
    private func content(_ d: SourceDetailEnvelope) -> some View {
        header(d)

        if let desc = d.registry?.description, !desc.isEmpty {
            Text(desc)
                .font(.subheadline)
                .foregroundStyle(theme.text)
                .padding(10)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 8))
        }

        summarySection

        healthSection(d)
        runSection(d)
        dataSection(d)
        scheduleSection(d)
        noticesSection(d)
        runsSection(d)
        settingsSection
        cadenceSection
        identitySection(d)

        if let row = statusRow, row.requiresKey == true {
            section("API key") {
                if let env = row.envVars, !env.isEmpty {
                    ForEach(env, id: \.name) { v in envVarRow(v) }
                }
            }
            ProbeActionsView(row: row, onUpdate: { updated in statusRow = updated })
                .padding(10)
                .frame(maxWidth: .infinity)
                .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 8))
        }

        if (d.data?.item_count ?? 0) > 0 {
            NavigationLink {
                IntelSourceItemsView(source: intelSource(d))
            } label: {
                Label("View records", systemImage: "list.bullet.rectangle")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
        }

        if let url = d.registry?.url, let u = URL(string: url) {
            Link(destination: u) {
                Label("Open source URL", systemImage: "safari")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.bordered)
        }
    }

    private func header(_ d: SourceDetailEnvelope) -> some View {
        HStack(alignment: .top, spacing: 10) {
            Circle().fill(stateColor(healthState(d)))
                .frame(width: 14, height: 14)
                .padding(.top, 4)
            VStack(alignment: .leading, spacing: 2) {
                Text(d.registry?.name ?? sourceId)
                    .font(.title3.bold())
                    .foregroundStyle(theme.text)
                if let ja = d.registry?.name_ja, !ja.isEmpty {
                    Text(ja).font(.subheadline).foregroundStyle(theme.textMuted)
                }
                Text(sourceId)
                    .font(.system(.caption2, design: .monospaced))
                    .foregroundStyle(theme.textMuted)
                    .textSelection(.enabled)
            }
            Spacer()
        }
    }

    // MARK: - Summary (model-written, from measured facts only)

    @ViewBuilder
    private var summarySection: some View {
        section("Summary") {
            VStack(alignment: .leading, spacing: 6) {
                if summaryLoading {
                    HStack(spacing: 8) {
                        ProgressView().controlSize(.small)
                        Text("Summarising from measured facts…")
                            .font(.caption).foregroundStyle(theme.textMuted)
                    }
                } else if let s = summary?.summary, !s.isEmpty {
                    Text(s).font(.caption).foregroundStyle(theme.text)
                    if let c = summary?.caveats, !c.isEmpty {
                        ForEach(Array(c.enumerated()), id: \.offset) { _, caveat in
                            Text("• \(caveat)")
                                .font(.caption2).foregroundStyle(theme.textMuted)
                        }
                    }
                    Text(summaryFooter)
                        .font(.caption2).foregroundStyle(theme.textMuted)
                } else if let err = summary?.error {
                    Text(summaryErrorText(err))
                        .font(.caption).foregroundStyle(theme.textMuted)
                    Text("The server returns the reason instead of inventing text. The measured facts are in the sections below.")
                        .font(.caption2).foregroundStyle(theme.textMuted)
                } else if let e = summaryError {
                    Text("Summary request failed: \(e)")
                        .font(.caption).foregroundStyle(theme.textMuted)
                } else {
                    Text("Not loaded").font(.caption).foregroundStyle(theme.textMuted)
                }
                Button {
                    Task { await loadSummary(refresh: true) }
                } label: {
                    Label("Regenerate", systemImage: "arrow.clockwise").font(.caption)
                }
                .buttonStyle(.bordered)
                .controlSize(.small)
                .disabled(summaryLoading)
            }
            .padding(8)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(theme.surfaceElevated)
        }
    }

    private var summaryFooter: String {
        let model = summary?.model ?? "a model"
        let facts = summary?.facts_used ?? 0
        var line = "written by " + model + " from " + String(facts) + " measured facts"
        if summary?.cached == true { line += " (cached)" }
        return line
    }

    private func summaryErrorText(_ err: String) -> String {
        var line = "No summary: " + err
        if let code = summary?.http_status, code > 0 {
            line += " (model answered HTTP " + String(code) + ")"
        }
        return line
    }

    // MARK: - Health

    private func healthState(_ d: SourceDetailEnvelope) -> String {
        d.status?.health?.state ?? d.status?.status ?? "pending"
    }

    @ViewBuilder
    private func healthSection(_ d: SourceDetailEnvelope) -> some View {
        let h = d.status?.health
        section("Health") {
            kv("State", healthState(d), valueColor: stateColor(healthState(d)))
            kv("Last run", d.status?.status ?? "—")
            if let note = h?.note, !note.isEmpty { kv("Why", note) }
            if let remedy = h?.remedy, !remedy.isEmpty { kv("Remedy", remedy) }
            if let creds = h?.credentials_required, !creds.isEmpty {
                kv("Needs", creds.joined(separator: ", "))
            }
            if let code = h?.http_status { kv("HTTP status", String(code)) }
            if let parsed = h?.body_parsed { kv("Body parsed", parsed ? "yes" : "no") }
            if let since = h?.since, !since.isEmpty { kv("Since", relativeTime(since)) }
            if let trust = statusRow?.trust {
                trustRow(trust)
            } else if let t = d.status?.trust {
                kv("Reliability", reliabilityText(t))
            }
            kv("Last check", relativeTime(d.status?.last_check))
            kv("Last success", relativeTime(d.status?.last_success))
            kv("Latency", d.status?.response_time_ms.map { "\(Int($0)) ms" } ?? "—")
            kv("Records, last run", SourceFormat.count(d.status?.records_last_run))
        }
        if let err = d.status?.error_message, !err.isEmpty {
            VStack(alignment: .leading, spacing: 4) {
                Label("Last error", systemImage: "exclamationmark.triangle.fill")
                    .font(.caption.bold())
                    .foregroundStyle(theme.danger)
                Text(err)
                    .font(.caption2)
                    .foregroundStyle(theme.text)
                    .textSelection(.enabled)
            }
            .padding(10)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(theme.danger.opacity(0.1), in: RoundedRectangle(cornerRadius: 8))
        }
    }

    private func reliabilityText(_ t: SourceDetailTrust) -> String {
        guard t.rated == true else { return "unrated" }
        var score = "—"
        if let r = t.reliability { score = String(format: "%.2f", r) }
        return "grade " + (t.grade ?? "—") + " (" + score + ")"
    }

    /// Reliability, laid out like a `kv` row. `TrustBadge` renders an unrated
    /// source as a neutral "—", never as a zero score.
    private func trustRow(_ trust: SourceTrust) -> some View {
        HStack(alignment: .firstTextBaseline) {
            Text("Reliability").font(.caption.bold()).foregroundStyle(theme.textMuted)
                .frame(width: 100, alignment: .leading)
            TrustBadge(trust: trust, sourceName: detail?.registry?.name ?? sourceId)
            Spacer(minLength: 0)
        }
        .padding(8)
        .background(theme.surfaceElevated)
    }

    // MARK: - Run

    private func needsEntity(_ d: SourceDetailEnvelope) -> Bool {
        if let flag = d.registry?.is_entity_pivot { return flag }
        return d.registry?.kind == "pivot"
    }

    /// POST /api/intel/sources/:id/run. An entity pivot only answers for an
    /// entity, so it gets a field and Run stays disabled until one is typed;
    /// the line shown afterwards is the server's own numbers or its error.
    @ViewBuilder
    private func runSection(_ d: SourceDetailEnvelope) -> some View {
        let pivot = needsEntity(d)
        let internalOnly = d.registry?.kind == "internal"
        section("Run") {
            VStack(alignment: .leading, spacing: 8) {
                if pivot {
                    Text("This source is an entity pivot: it answers for one entity at a time and has no scheduled run.")
                        .font(.caption2).foregroundStyle(theme.textMuted)
                    TextField("Entity to look up (name, id, domain…)", text: $entityText)
                        .textFieldStyle(.roundedBorder)
                        .autocorrectionDisabled()
                        .font(.caption)
                }
                HStack {
                    Button {
                        runNow(pivot: pivot)
                    } label: {
                        HStack(spacing: 6) {
                            if running { ProgressView().scaleEffect(0.7) } else { Image(systemName: "play.fill").accessibilityHidden(true) }
                            Text(running ? "Running…" : (pivot ? "Look up" : "Run now"))
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    .controlSize(.small)
                    .disabled(running || internalOnly
                              || (pivot && entityText.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty))
                    if let n = runNote {
                        Text(n)
                            .font(.caption2)
                            .foregroundStyle(runNoteIsProblem(n) ? theme.danger : theme.textMuted)
                    }
                    Spacer()
                }
                if internalOnly {
                    Text("Internal maintenance pod: it has no records to fetch.")
                        .font(.caption2).foregroundStyle(theme.textMuted)
                }
            }
            .padding(8)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(theme.surfaceElevated)
        }
    }

    private func runNoteIsProblem(_ n: String) -> Bool {
        n.hasPrefix("failed") || n.hasPrefix("this source is an entity pivot")
    }

    private func runNow(pivot: Bool) {
        let entity = entityText.trimmingCharacters(in: .whitespacesAndNewlines)
        running = true
        runNote = nil
        Task {
            defer { running = false }
            do {
                let r = try await apiClient.api.intelRunSource(sourceId, entity: pivot ? entity : nil)
                if let e = r.error {
                    runNote = "failed: \(e)"
                } else {
                    let rows = r.ingested.map(String.init) ?? "?"
                    let ms = r.duration_ms.map(String.init) ?? "?"
                    runNote = "ran: \(rows) rows in \(ms) ms"
                    await loadAll(quiet: true)
                }
            } catch {
                runNote = API.runFailureMessage(error)
            }
        }
    }

    // MARK: - Data

    @ViewBuilder
    private func dataSection(_ d: SourceDetailEnvelope) -> some View {
        let x = d.data
        section("Data") {
            kv("Items stored", SourceFormat.count(x?.item_count))
            kv("Geocoded", SourceFormat.count(x?.records_geocoded ?? d.records_geocoded ?? x?.geocoded))
            kv("Awaiting geocode", SourceFormat.count(x?.awaiting_geo))
            kv("Notices", SourceFormat.count(x?.notices))
            kv("Last fetched", relativeTime(x?.last_fetched))
            kv("First fetched", relativeTime(x?.first_fetched))
            kv("Newest published", relativeTime(x?.last_published))
        }
    }

    // MARK: - Schedule

    @ViewBuilder
    private func scheduleSection(_ d: SourceDetailEnvelope) -> some View {
        let s = d.schedule
        let policyText: String = {
            guard let p = s?.policyInterval else { return "—" }
            return SourceFormat.interval(p) + " (" + SourceFormat.policy(s?.policy ?? "policy") + ")"
        }()
        let nextDueText: String = {
            guard let due = s?.next_due else { return "—" }
            return relativeTime(due) + (s?.overdue == true ? " (overdue)" : "")
        }()
        section("Schedule") {
            kv("Declared", SourceFormat.interval(s?.declaredInterval))
            if let m = s?.measuredInterval { kv("Measured", SourceFormat.interval(m)) }
            kv("Policy", policyText)
            if let o = s?.overrideInterval { kv("Set for this source", SourceFormat.interval(o)) }
            kv("Basis", s?.cadenceBasis ?? "—")
            kv("Runs", s?.runInterval.map { SourceFormat.interval($0) } ?? "—")
            kv("Next due", nextDueText)
            kv("Mode", s?.schedule_mode ?? "—")
            kv("Failures in a row", s?.consecutiveFailures.map(String.init) ?? "—")
            kv("Empty runs in a row", s?.consecutiveEmpties.map(String.init) ?? "—")
            if s?.backedOff == true {
                kv("Backed off until", relativeTime(s?.backoffUntil), valueColor: theme.warning)
            }
            if s?.quarantined == true {
                kv("Quarantined", "yes", valueColor: theme.danger)
            }
            if let st = s?.stats_30d {
                kv("Runs, 30 days", "\(st.ok_30d ?? 0) ok of \(st.runs_30d ?? 0)")
                kv("Avg duration", st.avg_duration_ms_30d.map { "\(Int($0)) ms" } ?? "—")
                kv("Records, 30 days", st.records_30d.map { Int($0).formatted() } ?? "—")
            }
        }
    }

    // MARK: - Notices

    @ViewBuilder
    private func noticesSection(_ d: SourceDetailEnvelope) -> some View {
        let counts = d.notices?.counts ?? []
        let latest = d.notices?.latest ?? []
        section("Notices") {
            if counts.isEmpty && latest.isEmpty {
                Text("No collector notices stored for this source.")
                    .font(.caption2).foregroundStyle(theme.textMuted)
                    .padding(8)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(theme.surfaceElevated)
            }
            ForEach(Array(counts.enumerated()), id: \.offset) { _, c in
                kv(NoticeText.label(forKind: c.kind),
                   "\(c.count ?? 0) · latest \(relativeTime(c.latest_at))")
            }
            ForEach(Array(latest.enumerated()), id: \.offset) { _, n in
                VStack(alignment: .leading, spacing: 3) {
                    Text(NoticeText.label(forKind: n.kind))
                        .font(.caption.bold()).foregroundStyle(theme.text)
                    if let t = n.title, !t.isEmpty {
                        Text(t).font(.caption2).foregroundStyle(theme.textMuted)
                    }
                    ForEach(Array(NoticeText.detail(from: n.properties).enumerated()), id: \.offset) { _, line in
                        Text("\(line.label): \(line.text)")
                            .font(.caption2).foregroundStyle(theme.text)
                            .textSelection(.enabled)
                    }
                    Text(relativeTime(n.fetched_at))
                        .font(.caption2).foregroundStyle(theme.textMuted)
                }
                .padding(8)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(theme.surfaceElevated)
            }
        }
    }

    // MARK: - Recent runs

    @ViewBuilder
    private func runsSection(_ d: SourceDetailEnvelope) -> some View {
        let runs = d.runs?.recent ?? []
        section("Recent runs") {
            if runs.isEmpty {
                Text("No runs recorded yet.")
                    .font(.caption2).foregroundStyle(theme.textMuted)
                    .padding(8)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(theme.surfaceElevated)
            }
            ForEach(Array(runs.enumerated()), id: \.offset) { _, r in
                VStack(alignment: .leading, spacing: 2) {
                    HStack {
                        Text(r.status ?? "—")
                            .font(.caption.bold())
                            .foregroundStyle(r.status == "ok" ? theme.success : theme.warning)
                        Text(relativeTime(r.timestamp))
                            .font(.caption2).foregroundStyle(theme.textMuted)
                        Spacer()
                        Text("\(SourceFormat.count(r.records_fetched)) fetched · \(SourceFormat.count(r.stored)) stored")
                            .font(.caption2.monospacedDigit()).foregroundStyle(theme.textMuted)
                    }
                    if let ms = r.duration_ms {
                        Text("\(Int(ms)) ms" + (r.bytes_fetched.map { " · \(Int($0).formatted()) bytes" } ?? ""))
                            .font(.caption2).foregroundStyle(theme.textMuted)
                    }
                    if let e = r.error, !e.isEmpty {
                        Text(e).font(.caption2).foregroundStyle(theme.danger).lineLimit(3)
                    }
                }
                .padding(8)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(theme.surfaceElevated)
            }
            if let total = d.runs?.total, total > runs.count {
                Text("Showing the latest \(runs.count) of \(total.formatted()) runs.")
                    .font(.caption2).foregroundStyle(theme.textMuted)
                    .padding(8)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(theme.surfaceElevated)
            }
        }
    }

    // MARK: - Operator settings

    @ViewBuilder
    private var settingsSection: some View {
        if let st = settings, let rows = st.settings, !rows.isEmpty {
            section("Settings") {
                if st.can_write != true {
                    Text("Only a platform operator can change these. The values below are what this source runs with.")
                        .font(.caption2).foregroundStyle(theme.textMuted)
                        .padding(8)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .background(theme.surfaceElevated)
                }
                ForEach(rows) { row in
                    SourceSettingEditor(
                        sourceId: sourceId,
                        row: row,
                        canWrite: st.can_write == true,
                        onSaved: { updated in
                            settings = updated
                            Task { await loadAll(quiet: true) }
                        })
                }
            }
        }
    }

    // MARK: - Cadence (system-wide)

    @ViewBuilder
    private var cadenceSection: some View {
        if let c = cadence {
            section("System-wide cadence") {
                kv("Policy", c.policy?.mode ?? "—")
                if let g = c.policy?.global_interval_sec, g > 0 {
                    kv("Every source", SourceFormat.interval(Double(g)))
                }
                if let lo = c.policy?.min_interval_sec, lo > 0 {
                    kv("Floor", SourceFormat.interval(Double(lo)))
                }
                if let hi = c.policy?.max_interval_sec, hi > 0 {
                    kv("Ceiling", SourceFormat.interval(Double(hi)))
                }
                if let rt = c.policy?.realtime_max_sec, rt > 0 {
                    kv("Real-time limit", SourceFormat.interval(Double(rt)))
                }
                if let e = c.estimate {
                    kv("Runs a day, declared", SourceFormat.count(e.runs_per_day_declared))
                    kv("Runs a day, policy", SourceFormat.count(e.runs_per_day_effective))
                    kv("Scheduled rows", SourceFormat.count(e.scheduled_rows))
                }
                kv("Set by", c.updated_by ?? c.source ?? "—")
                Text("A source set by hand (Settings above) outranks this policy. The policy itself is edited on the web Sources page.")
                    .font(.caption2).foregroundStyle(theme.textMuted)
                    .padding(8)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(theme.surfaceElevated)
            }
        }
    }

    // MARK: - Identity

    @ViewBuilder
    private func identitySection(_ d: SourceDetailEnvelope) -> some View {
        let r = d.registry
        let collectorText: String = {
            let key = r?.collector ?? "not reported"
            if let label = r?.collector_meta?.label, !label.isEmpty { return label + " (" + key + ")" }
            return key
        }()
        section("Identity") {
            kv("Collector", collectorText)
            kv("Engine", r?.engine ?? "not reported")
            kv("Kind", r?.kind ?? "not reported")
            kv("Category", r?.category ?? "—")
            kv("Type", r?.type ?? "—")
            kv("Host", r?.host ?? "—")
            kv("License", r?.license ?? "—")
            kv("Free tier", r?.free_tier.map { $0 ? "yes" : "no" } ?? "—")
            kv("Layer", r?.layer_id ?? d.layer_id ?? r?.layer ?? "none")
            if let p = d.pacing, let gap = p.min_gap_ms {
                kv("Pacing", "\(Int(gap)) ms between requests to \(p.host ?? "the host")")
            }
        }
    }

    // MARK: - Loading

    /// `quiet` keeps the current content on screen while it refreshes (after a
    /// run or a settings save) instead of flashing the spinner.
    private func loadAll(quiet: Bool = false) async {
        if !quiet { loading = true }
        defer { loading = false }
        let api = apiClient.api
        let id = sourceId
        async let statusTask = api.statusRow(id)
        async let settingsTask = api.sourceSettings(id)
        async let cadenceTask = api.cadencePolicy()
        do {
            detail = try await api.sourceDetail(id)
            loadError = nil
        } catch {
            loadError = error.localizedDescription
        }
        statusRow = try? await statusTask
        settings = try? await settingsTask
        cadence = try? await cadenceTask
    }

    private func loadSummary(refresh: Bool) async {
        summaryLoading = true
        defer { summaryLoading = false }
        do {
            summary = try await apiClient.api.sourceSummary(sourceId, refresh: refresh)
            summaryError = nil
        } catch {
            summaryError = error.localizedDescription
        }
    }

    // MARK: - Helpers

    private func intelSource(_ d: SourceDetailEnvelope) -> IntelSource {
        IntelSource(id: sourceId,
                    name: d.registry?.name ?? sourceId,
                    name_ja: d.registry?.name_ja,
                    category: d.registry?.category,
                    description: d.registry?.description,
                    url: d.registry?.url,
                    item_count: d.data?.item_count ?? 0,
                    last_fetched: d.data?.last_fetched,
                    last_published: d.data?.last_published,
                    ttl_ms: nil,
                    parent_id: nil)
    }

    private func stateColor(_ state: String) -> Color {
        switch state {
        case "ok", "online":        return theme.success
        case "degraded":            return theme.warning
        case "failing", "offline":  return theme.danger
        default:                    return theme.textMuted
        }
    }

    @ViewBuilder
    private func section<Content: View>(_ title: String,
                                        @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title).font(.caption.bold()).foregroundStyle(theme.textMuted)
            VStack(spacing: 1) { content() }
                .clipShape(RoundedRectangle(cornerRadius: 8))
        }
    }

    private func kv(_ k: String, _ v: String, valueColor: Color? = nil) -> some View {
        HStack(alignment: .firstTextBaseline) {
            Text(k).font(.caption.bold()).foregroundStyle(theme.textMuted)
                .frame(width: 100, alignment: .leading)
            Text(v).font(.caption).foregroundStyle(valueColor ?? theme.text)
                .textSelection(.enabled)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding(8)
        .background(theme.surfaceElevated)
    }

    /// One env-var row: monospaced name, role pill, SET/UNSET pill. Tapping it
    /// opens that key's page in the API Keys tab.
    private func envVarRow(_ spec: EnvVarSpec) -> some View {
        Button {
            let target = spec.name
            dismiss()
            nav.showApiKey(target)
        } label: {
            HStack(spacing: 10) {
                Image(systemName: spec.set == true ? "key.fill" : "key.slash")
                    .foregroundStyle(spec.set == true ? theme.success : theme.textMuted)
                    .frame(width: 22)
                    .accessibilityHidden(true)   // restated by the SET/UNSET pill
                Text(spec.name)
                    .font(.system(.body, design: .monospaced).weight(.medium))
                    .foregroundStyle(theme.text)
                    .lineLimit(1)
                if let role = spec.role {
                    Pill(text: role.uppercased(), tone: roleTone(role))
                }
                Spacer()
                Pill(text: spec.set == true ? "SET" : "UNSET",
                     tone: spec.set == true ? .success : .warning, size: .md)
            }
            .padding(.horizontal, 12).padding(.vertical, 8)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(theme.surfaceElevated)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Open API key page for \(spec.name)")
    }

    private func roleTone(_ role: String) -> Pill.Tone {
        switch role {
        case "required": return .accent
        case "anyOf":    return .warning
        default:         return .neutral
        }
    }
}

// ── One operator-editable setting ──────────────────────────────────────────

/// A row of the closed key set `GET /api/sources/:id/settings` publishes
/// (`interval_sec`, `http_body_ceiling_bytes`). The bounds, default, unit and
/// help text are the server's own; the app states them rather than keeping a
/// copy. Switching a setting off keeps the number, so a re-enable does not
/// start from a guess. Writes are operator-gated server-side (403 otherwise).
private struct SourceSettingEditor: View {
    let sourceId: String
    let row: SourceSettingRow
    let canWrite: Bool
    let onSaved: (SourceSettingsEnvelope) -> Void

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var text: String
    @State private var enabled: Bool
    @State private var saving = false
    @State private var error: String?

    init(sourceId: String, row: SourceSettingRow, canWrite: Bool,
         onSaved: @escaping (SourceSettingsEnvelope) -> Void) {
        self.sourceId = sourceId
        self.row = row
        self.canWrite = canWrite
        self.onSaved = onSaved
        self._text = State(initialValue: row.value.map { String(Int($0)) } ?? "")
        self._enabled = State(initialValue: row.enabled == true)
    }

    private var unit: String { row.unit ?? "" }

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text(row.label ?? row.key).font(.caption.bold()).foregroundStyle(theme.text)
                Spacer()
                if canWrite {
                    Toggle("", isOn: $enabled)
                        .labelsHidden()
                        .disabled(saving)
                        .accessibilityLabel("Enable \(row.label ?? row.key)")
                }
            }
            if let help = row.help, !help.isEmpty {
                Text(help).font(.caption2).foregroundStyle(theme.textMuted)
            }
            Text(statusLine).font(.caption2).foregroundStyle(theme.textMuted)
            if canWrite {
                HStack {
                    TextField("value", text: $text)
                        .textFieldStyle(.roundedBorder)
                        .font(.caption.monospacedDigit())
                        #if os(iOS)
                        .keyboardType(.numberPad)
                        #endif
                    Text(unit).font(.caption2).foregroundStyle(theme.textMuted)
                    Button {
                        Task { await save() }
                    } label: {
                        if saving { ProgressView().controlSize(.mini) } else { Text("Save") }
                    }
                    .buttonStyle(.borderedProminent)
                    .controlSize(.small)
                    .disabled(saving || Int(text.trimmingCharacters(in: .whitespaces)) == nil)
                }
            }
            if let error {
                Text(error).font(.caption2).foregroundStyle(theme.danger)
            }
        }
        .padding(8)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.surfaceElevated)
    }

    /// What the code will actually use, with the server's bounds and default.
    private var statusLine: String {
        var parts: [String] = []
        if let e = row.effective { parts.append("in effect: \(Int(e).formatted()) \(unit)") }
        if let d = row.defaultValue {
            parts.append("default \(Int(d).formatted())" + (row.default_basis.map { " (\($0))" } ?? ""))
        }
        if let lo = row.lo, let hi = row.hi {
            parts.append("allowed \(Int(lo).formatted())–\(Int(hi).formatted())")
        }
        return parts.joined(separator: " · ")
    }

    private func save() async {
        guard let v = Int(text.trimmingCharacters(in: .whitespaces)) else { return }
        saving = true
        error = nil
        defer { saving = false }
        do {
            let updated = try await apiClient.api.putSourceSetting(
                sourceId, key: row.key, value: v, enabled: enabled)
            onSaved(updated)
        } catch {
            if case APIError.http(let code, _) = error, code == 403 {
                self.error = "Only a platform operator can change this."
            } else if case APIError.http(let code, _) = error, code == 400 {
                self.error = "The server refused that value (outside \(row.lo.map { Int($0).formatted() } ?? "?")–\(row.hi.map { Int($0).formatted() } ?? "?"))."
            } else {
                self.error = error.localizedDescription
            }
        }
    }
}
