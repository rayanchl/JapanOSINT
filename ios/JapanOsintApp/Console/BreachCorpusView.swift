import SwiftUI
import Charts

/// Operator console for the breach corpus: the whole dataset lifecycle in one
/// screen — stage a dataset by URL, parse the catalog seed without writing,
/// commit it, ingest records, and watch the jobs run.
///
/// Every endpoint here is behind `opgate_check` on the server, so the
/// `isPlatformAdmin` check below is presentation only (same belt-and-suspenders
/// stance as `AdminPanel`). Pushed inside Console's / RootView's stack, so it
/// carries no `NavigationStack` of its own.
struct BreachCorpusView: View {
    @EnvironmentObject var auth: AuthSession
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    private var api: API { apiClient.api }

    // 1 — catalog
    @State private var catalogCount = 0
    @State private var materializedCount = 0
    @State private var runningCollector = false

    // 2 — parse
    @State private var seedPath = ""
    @State private var preview: BreachCatalogPreview?
    @State private var previewing = false

    // 3 — save
    @State private var saving = false
    @State private var loadResult: BreachCatalogLoadResult?
    @State private var confirmLoad = false

    // 4 — ingest (and the fetch that stages a file for it)
    @State private var fetchURL = ""
    @State private var fetching = false
    @State private var ingestSourceId = ""
    @State private var ingestPath = ""
    @State private var ingestType = "auto"
    @State private var materialize = false
    @State private var dryRun = true
    @State private var ingesting = false
    @State private var confirmIngest = false

    // 5 — jobs
    @State private var jobs: [BreachJob] = []
    @State private var pollTask: Task<Void, Never>?

    @State private var errorText: String?
    @State private var noticeText: String?
    @State private var breachSources: [StatusRow] = []

    private let types = ["auto", "email", "username", "phone", "password"]

    /// The server single-flights one fetch and one ingest at a time and answers
    /// 409 otherwise. Deriving the busy state from the polled list means the
    /// button is usually already disabled — but the 409 is still handled, since
    /// polling lags a job started from another device.
    private var fetchBusy: Bool { jobs.contains { $0.kind == "fetch" && $0.isRunning } }
    private var ingestBusy: Bool { jobs.contains { $0.kind == "ingest" && $0.isRunning } }

    var body: some View {
        Group {
            if auth.isPlatformAdmin { content } else { denied }
        }
        .navigationTitle("Breach corpus")
        .compatInlineTitle()
        .themedScreenBackground(theme)
        .task {
            await loadContext()
            startPolling()
        }
        .onDisappear { pollTask?.cancel(); pollTask = nil }
    }

    private var denied: some View {
        ContentUnavailableView(
            "Operator access only",
            systemImage: "lock.shield",
            description: Text("The breach corpus is restricted to the platform operator account.")
        )
    }

    private var content: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Space.lg) {
                if let errorText {
                    banner(errorText, tone: .danger, icon: "exclamationmark.triangle.fill")
                }
                if let noticeText {
                    banner(noticeText, tone: .success, icon: "checkmark.circle.fill")
                }
                catalogCard
                parseCard
                saveCard
                ingestCard
                jobsCard
            }
            .padding(Space.lg)
            .frame(maxWidth: 720, alignment: .leading)
            .frame(maxWidth: .infinity, alignment: .top)
        }
    }

    // MARK: - 1. Catalog

    private var catalogCard: some View {
        card(1, "Catalog", hint: "Breaches known to the server, from the local seed file.") {
            HStack(alignment: .firstTextBaseline, spacing: Space.md) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(catalogCount.formatted())
                        .font(.title2.bold().monospacedDigit())
                        .foregroundStyle(theme.text)
                    Text("catalogued").font(.caption2).foregroundStyle(theme.textMuted)
                }
                VStack(alignment: .leading, spacing: 2) {
                    Text(materializedCount.formatted())
                        .font(.title2.bold().monospacedDigit())
                        .foregroundStyle(theme.danger)
                    Text("with records").font(.caption2).foregroundStyle(theme.textMuted)
                }
                Spacer()
            }
            Button {
                Haptics.tap()
                Task { await runCollector() }
            } label: {
                HStack(spacing: 6) {
                    if runningCollector { ProgressView().controlSize(.small) }
                    else { Image(systemName: "play.fill") }
                    Text(runningCollector ? "Running…" : "Run collector")
                }
            }
            .buttonStyle(.bordered)
            .disabled(runningCollector)
            Text("Runs the breach-corpus collector, which loads the seed straight into the catalog — the same thing step 3 does, on the server's daily schedule.")
                .font(.caption2).foregroundStyle(theme.textMuted)
        }
    }

    // MARK: - 2. Parse

    private var parseCard: some View {
        card(2, "Parse", hint: "Reads and normalizes the seed. Writes nothing.") {
            TextField("Seed path (blank = server default)", text: $seedPath)
                .textFieldStyle(.roundedBorder)
                .font(.system(.caption, design: .monospaced))
                .autocorrectionDisabled()

            Button {
                Haptics.tap()
                Task { await runPreview() }
            } label: {
                HStack(spacing: 6) {
                    if previewing { ProgressView().controlSize(.small) }
                    else { Image(systemName: "doc.text.magnifyingglass") }
                    Text(previewing ? "Parsing…" : "Preview parse")
                }
            }
            .buttonStyle(.borderedProminent)
            .disabled(previewing)

            if let p = preview {
                VStack(alignment: .leading, spacing: Space.sm) {
                    HStack(spacing: Space.xs) {
                        Pill(text: "\(p.rowsRead) read", tone: .neutral)
                        Pill(text: "\(p.rowsParsed) parsed", tone: .accent)
                        Pill(text: "\(p.rowsSkipped) skipped", tone: .warning)
                    }
                    HStack(spacing: Space.xs) {
                        Pill(text: "\(p.rowsNew) new", tone: .success)
                        Pill(text: "\(p.rowsExisting) existing", tone: .neutral)
                    }
                    Text(p.path)
                        .font(.system(.caption2, design: .monospaced))
                        .foregroundStyle(theme.textMuted)
                        .lineLimit(2).truncationMode(.middle)
                    sampleTable(p.sample)
                    if p.rowsSkipped > 0 {
                        Text("Skipped rows are the header plus names that normalize to an empty id (non-Latin catalog names have no usable slug).")
                            .font(.caption2).foregroundStyle(theme.textMuted)
                    }
                }
            }
        }
    }

    private func sampleTable(_ rows: [BreachCatalogRow]) -> some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack(spacing: Space.sm) {
                Text("ID").frame(maxWidth: .infinity, alignment: .leading)
                Text("ACCOUNTS").frame(width: 76, alignment: .trailing)
                Text("BREACHED").frame(width: 62, alignment: .trailing)
            }
            .font(Typography.display(9, weight: .semibold))
            .tracking(0.8)
            .foregroundStyle(theme.textMuted)
            .padding(.vertical, 4)

            ForEach(rows) { r in
                HStack(spacing: Space.sm) {
                    VStack(alignment: .leading, spacing: 1) {
                        Text(r.breachId)
                            .font(.system(.caption2, design: .monospaced))
                            .foregroundStyle(theme.text)
                            .lineLimit(1).truncationMode(.middle)
                        Text(r.name)
                            .font(.caption2)
                            .foregroundStyle(theme.textMuted)
                            .lineLimit(1)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    Text(r.pwnCount.map { $0.formatted() } ?? "—")
                        .font(.system(.caption2, design: .monospaced).monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                        .frame(width: 76, alignment: .trailing)
                    Text(r.breachDate ?? "—")
                        .font(.system(.caption2, design: .monospaced))
                        .foregroundStyle(theme.textMuted)
                        .frame(width: 62, alignment: .trailing)
                }
                .padding(.vertical, 3)
                Divider().opacity(0.25)
            }
        }
        .padding(Space.sm)
        .background(theme.surface, in: RoundedRectangle(cornerRadius: Radius.sm))
    }

    // MARK: - 3. Save

    private var saveCard: some View {
        card(3, "Save", hint: "Commits the parsed catalog into breach_meta.") {
            Button {
                Haptics.tap(.medium)
                confirmLoad = true
            } label: {
                HStack(spacing: 6) {
                    if saving { ProgressView().controlSize(.small) }
                    else { Image(systemName: "square.and.arrow.down") }
                    Text(saving ? "Loading…" : "Load into catalog")
                }
            }
            .buttonStyle(.borderedProminent)
            .disabled(saving || preview == nil)
            .confirmationDialog("Load the catalog?",
                                isPresented: $confirmLoad,
                                titleVisibility: .visible) {
                Button("Load") { Haptics.tap(.medium); Task { await runLoad() } }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text(preview.map {
                    "\($0.rowsNew) new and \($0.rowsExisting) existing entries will be written. Existing rows are updated in place."
                } ?? "")
            }

            if preview == nil {
                Text("Run a preview first so you can see what will be written.")
                    .font(.caption2).foregroundStyle(theme.textMuted)
            }
            if let r = loadResult {
                HStack(spacing: Space.xs) {
                    Pill(text: "\(r.loaded) loaded", tone: .success, icon: "checkmark.circle.fill")
                    if let n = r.rowsNew { Pill(text: "\(n) new", tone: .accent) }
                    if let f = r.format { Pill(text: f, tone: .neutral) }
                }
            }
        }
    }

    // MARK: - 4. Ingest

    private var ingestCard: some View {
        card(4, "Ingest records",
             hint: "Stage a leaked-record dataset, then index it. Catalog entries alone carry no records.") {
            Text("SOURCE").font(Typography.display(9, weight: .semibold))
                .tracking(0.8).foregroundStyle(theme.textMuted)
            sourcePicker

            Text("STAGE FROM URL").font(Typography.display(9, weight: .semibold))
                .tracking(0.8).foregroundStyle(theme.textMuted)
                .padding(.top, 2)
            TextField("https://…", text: $fetchURL)
                .textFieldStyle(.roundedBorder)
                .font(.system(.caption, design: .monospaced))
                .autocorrectionDisabled()
            Button {
                Haptics.tap()
                Task { await runFetch() }
            } label: {
                HStack(spacing: 6) {
                    if fetching { ProgressView().controlSize(.small) }
                    else { Image(systemName: "arrow.down.circle") }
                    Text(fetching ? "Staging…" : "Stage dataset")
                }
            }
            .buttonStyle(.bordered)
            .disabled(fetching || fetchBusy || fetchURL.isEmpty || ingestSourceId.isEmpty)
            if fetchBusy {
                Text("A fetch is already running.").font(.caption2).foregroundStyle(theme.warning)
            }

            Text("FILE").font(Typography.display(9, weight: .semibold))
                .tracking(0.8).foregroundStyle(theme.textMuted)
                .padding(.top, 2)
            TextField("Path on the server", text: $ingestPath)
                .textFieldStyle(.roundedBorder)
                .font(.system(.caption, design: .monospaced))
                .autocorrectionDisabled()
            Text("Filled in from a completed fetch when the server reports it. That message is truncated at 120 characters, so check the path before ingesting.")
                .font(.caption2).foregroundStyle(theme.textMuted)

            Picker("Type", selection: $ingestType) {
                ForEach(types, id: \.self) { Text($0).tag($0) }
            }
            .pickerStyle(.segmented)

            Toggle("Dry run", isOn: $dryRun)
                .font(.caption)
            Toggle("Materialize records", isOn: $materialize)
                .font(.caption)
            Text("Materializing writes one searchable row per identity — roughly 4 rows per record, and tens of gigabytes on a 100M-record corpus. Leave it off to build only the hashed index.")
                .font(.caption2).foregroundStyle(theme.textMuted)

            Button {
                Haptics.tap(.medium)
                if materialize && !dryRun { confirmIngest = true } else { Task { await runIngest() } }
            } label: {
                HStack(spacing: 6) {
                    if ingesting { ProgressView().controlSize(.small) }
                    else { Image(systemName: "tray.and.arrow.down.fill") }
                    Text(ingesting ? "Starting…" : "Start ingest")
                }
            }
            .buttonStyle(.borderedProminent)
            .disabled(ingesting || ingestBusy || ingestSourceId.isEmpty || ingestPath.isEmpty)
            .confirmationDialog("Materialize records?",
                                isPresented: $confirmIngest,
                                titleVisibility: .visible) {
                Button("Ingest and materialize", role: .destructive) {
                    Task { await runIngest() }
                }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("This writes searchable rows for every identity in the dataset and can consume a large amount of disk. Run a dry run first if you haven't.")
            }
            if ingestBusy {
                Text("An ingest is already running.").font(.caption2).foregroundStyle(theme.warning)
            }
        }
    }

    /// Free-text id with a menu of catalogued breaches — the catalog is ~1k
    /// entries, so a plain Picker would be unusable, and an operator often
    /// knows the slug already.
    private var sourcePicker: some View {
        HStack(spacing: Space.sm) {
            TextField("breach id", text: $ingestSourceId)
                .textFieldStyle(.roundedBorder)
                .font(.system(.caption, design: .monospaced))
                .autocorrectionDisabled()
            Menu {
                ForEach(breachSuggestions, id: \.id) { s in
                    Button("\(s.name ?? s.id)") { ingestSourceId = s.id }
                }
            } label: {
                Image(systemName: "list.bullet")
            }
            .disabled(breachSuggestions.isEmpty)
        }
    }

    /// Prefix-matched against what the operator has typed, capped so the menu
    /// never tries to render the whole catalog.
    private var breachSuggestions: [StatusRow] {
        let q = ingestSourceId.lowercased()
        let pool = q.isEmpty ? breachSources : breachSources.filter {
            $0.id.lowercased().contains(q) || ($0.name ?? "").lowercased().contains(q)
        }
        return Array(pool.prefix(25))
    }

    // MARK: - 5. Jobs

    private var jobsCard: some View {
        card(5, "Jobs", hint: "In-memory on the server — the list resets when it restarts.") {
            if jobs.isEmpty {
                Text("No jobs yet.").font(.caption).foregroundStyle(theme.textMuted)
            } else {
                let charted = jobs.filter { ($0.rowsNew ?? 0) > 0 }
                if !charted.isEmpty {
                    Chart(charted) { job in
                        BarMark(
                            x: .value("New rows", job.rowsNew ?? 0),
                            y: .value("Job", "\(job.sourceId ?? "—") · \(job.jobId.prefix(6))")
                        )
                        .foregroundStyle(jobColor(job.state))
                    }
                    .frame(height: CGFloat(max(80, charted.count * 22)))
                }
                VStack(spacing: Space.sm) {
                    ForEach(jobs) { job in jobRow(job) }
                }
            }
        }
    }

    private func jobRow(_ job: BreachJob) -> some View {
        VStack(alignment: .leading, spacing: 3) {
            HStack(spacing: Space.xs) {
                Pill(text: job.kind, tone: job.kind == "ingest" ? .accent : .info)
                Pill(text: job.state, tone: jobTone(job.state),
                     icon: job.isRunning ? "arrow.triangle.2.circlepath" : nil)
                Text(job.sourceId ?? "—")
                    .font(.system(.caption2, design: .monospaced))
                    .foregroundStyle(theme.text)
                    .lineLimit(1)
                Spacer()
                Text(elapsed(job)).font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
            }
            HStack(spacing: Space.sm) {
                if let i = job.rowsIn {
                    Text("in \(i.formatted())").font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
                if let n = job.rowsNew {
                    Text("new \(n.formatted())").font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.success)
                }
                if let p = job.stagedPath {
                    Button("Use file") { ingestPath = p }
                        .font(.caption2)
                        .buttonStyle(.plain)
                        .foregroundStyle(theme.accent)
                }
            }
            if let m = job.message, !m.isEmpty {
                Text(m).font(.caption2).foregroundStyle(theme.textMuted)
                    .lineLimit(2).truncationMode(.middle)
            }
        }
        .padding(Space.sm)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.surface, in: RoundedRectangle(cornerRadius: Radius.sm))
    }

    private func jobTone(_ state: String) -> Pill.Tone {
        switch state {
        case "done":  return .success
        case "error": return .danger
        case "running", "started": return .accent
        default: return .neutral
        }
    }

    private func jobColor(_ state: String) -> Color {
        switch state {
        case "done":  return theme.success
        case "error": return theme.danger
        case "running", "started": return theme.accent
        default: return theme.textMuted
        }
    }

    /// Epoch seconds, not ISO — see `BreachJob`.
    private func elapsed(_ job: BreachJob) -> String {
        guard let start = job.startedAt else { return "—" }
        let end = job.endedAt ?? Date()
        let secs = Int(max(0, end.timeIntervalSince(start)))
        if secs < 60 { return "\(secs)s" }
        return "\(secs / 60)m \(secs % 60)s"
    }

    // MARK: - Actions

    private func loadContext() async {
        do {
            let env = try await api.status()
            catalogCount = env.summary?.breachTotal ?? 0
            materializedCount = env.summary?.breachMaterialized ?? 0
            breachSources = env.apis.filter { $0.category == "breach" }
        } catch {
            errorText = API.breachErrorMessage(error)
        }
    }

    private func runCollector() async {
        runningCollector = true
        defer { runningCollector = false }
        errorText = nil; noticeText = nil
        do {
            _ = try await api.intelRunSource("breach-corpus")
            await loadContext()
            noticeText = "Collector run complete — catalog now \(catalogCount.formatted()) entries."
            Haptics.success()
        } catch {
            errorText = API.breachErrorMessage(error)
            Haptics.error()
        }
    }

    private func runPreview() async {
        previewing = true
        defer { previewing = false }
        errorText = nil; noticeText = nil
        do {
            preview = try await api.breachCatalogPreview(
                path: seedPath.isEmpty ? nil : seedPath, limit: 50)
        } catch {
            preview = nil
            errorText = API.breachErrorMessage(error)
            Haptics.error()
        }
    }

    private func runLoad() async {
        saving = true
        defer { saving = false }
        errorText = nil; noticeText = nil
        do {
            let r = try await api.loadBreachCatalog(
                path: seedPath.isEmpty ? nil : seedPath, format: "auto")
            loadResult = r
            await loadContext()
            noticeText = "Loaded \(r.loaded.formatted()) catalog entries."
            Haptics.success()
        } catch {
            errorText = API.breachErrorMessage(error)
            Haptics.error()
        }
    }

    private func runFetch() async {
        fetching = true
        defer { fetching = false }
        errorText = nil; noticeText = nil
        do {
            _ = try await api.startBreachFetch(sourceId: ingestSourceId, url: fetchURL)
            await refreshJobs()
            noticeText = "Fetch started."
        } catch {
            errorText = API.breachErrorMessage(error)
            Haptics.error()
        }
    }

    private func runIngest() async {
        ingesting = true
        defer { ingesting = false }
        errorText = nil; noticeText = nil
        do {
            _ = try await api.startBreachIngest(
                sourceId: ingestSourceId,
                path: ingestPath,
                type: ingestType == "auto" ? "" : ingestType,
                materialize: materialize,
                dryRun: dryRun)
            await refreshJobs()
            noticeText = dryRun ? "Dry-run ingest started — nothing will be written."
                                : "Ingest started."
        } catch {
            errorText = API.breachErrorMessage(error)
            Haptics.error()
        }
    }

    private func refreshJobs() async {
        if let j = try? await api.breachJobs() { jobs = j }
    }

    /// Fast cadence only while something is actually running; otherwise a slow
    /// heartbeat so an idle screen isn't polling every two seconds.
    private func startPolling() {
        pollTask?.cancel()
        pollTask = Task {
            while !Task.isCancelled {
                await refreshJobs()
                let busy = jobs.contains { $0.isRunning }
                try? await Task.sleep(for: .seconds(busy ? 2 : 15))
            }
        }
    }

    // MARK: - Chrome

    @ViewBuilder
    private func card<Content: View>(_ n: Int, _ title: String, hint: String,
                                     @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            HStack(spacing: Space.sm) {
                Text("\(n)")
                    .font(Typography.display(11, weight: .semibold).monospacedDigit())
                    .foregroundStyle(theme.accent)
                    .frame(width: 20, height: 20)
                    .background(theme.accent.opacity(0.12), in: Circle())
                Text(title.uppercased())
                    .font(Typography.display(10, weight: .semibold))
                    .tracking(1.2)
                    .foregroundStyle(theme.textMuted)
            }
            Text(hint).font(.caption2).foregroundStyle(theme.textMuted)
            content()
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(Space.md)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.md))
    }

    private func banner(_ text: String, tone: Pill.Tone, icon: String) -> some View {
        let color: Color = tone == .danger ? theme.danger : theme.success
        return HStack(alignment: .top, spacing: Space.sm) {
            Image(systemName: icon).font(.caption).foregroundStyle(color)
            Text(text).font(.caption).foregroundStyle(theme.text)
                .frame(maxWidth: .infinity, alignment: .leading)
                .textSelection(.enabled)
        }
        .padding(Space.sm)
        .background(color.opacity(0.12), in: RoundedRectangle(cornerRadius: Radius.sm))
    }
}
