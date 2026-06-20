import SwiftUI

/// Build and run a workspace query — either a direct FTS query over existing
/// intel, or a full LLM agentic-search run (which ingests new intel into the
/// DB as usual). Either can be promoted to an alert rule via the same
/// `AlertEditor` the Alerts tab uses, so both query types converge on one
/// `AlertRule` (carrying `predicate.mode`).
struct WorkspaceQueryView: View {
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @StateObject private var searchStore = SearchStore()

    @State private var mode = "fts"            // "fts" | "llm"
    @State private var q = ""
    @State private var nlQuery = ""
    @State private var results: [IntelItem] = []
    @State private var running = false
    @State private var suggesting = false
    @State private var error: String?
    @State private var draft: AlertRule?

    private var api: API { apiClient.api }

    var body: some View {
        Form {
            Section {
                Picker("Query type", selection: $mode) {
                    Text("FTS").tag("fts")
                    Text("LLM pipeline").tag("llm")
                }
                .pickerStyle(.segmented)

                if mode == "llm" {
                    TextField("Natural-language query (e.g. phishing targeting JP banks)",
                              text: $nlQuery)
                        .compatNoAutocap()
                    Button {
                        Task { await suggestFTS() }
                    } label: {
                        Label(suggesting ? "Suggesting…" : "Suggest FTS query",
                              systemImage: "sparkles")
                    }
                    .disabled(suggesting || nlQuery.trimmingCharacters(in: .whitespaces).isEmpty)
                }

                TextField("FTS query (e.g. phishing AND tld:.jp)", text: $q)
                    .compatNoAutocap()
                    .autocorrectionDisabled()
                    .font(.system(.body, design: .monospaced))

                HStack(spacing: Space.sm) {
                    Button {
                        Task { await run() }
                    } label: {
                        HStack(spacing: Space.xs) {
                            if running { ProgressView().controlSize(.small) }
                            Text(running ? "Running…" : "Run")
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(running || !canRun)
                    Spacer()
                }
            } header: {
                Text("Query")
            } footer: {
                Text(mode == "llm"
                     ? "Runs the agentic search pipeline on your natural-language query, ingesting new intel. “Suggest FTS query” drafts the FTS string the LLM would use — edit it, then save as an alert."
                     : "Searches existing intel via full-text search. Save it as an alert to get pinged on new matches.")
                    .font(.caption2)
            }

            Section {
                HStack {
                    Button {
                        draft = makeAlertDraft()
                    } label: {
                        Text("Save as alert")
                    }
                    .buttonStyle(.bordered)
                    .disabled(!canRun)
                    Spacer()
                }
            } footer: {
                Text("Opens the alert editor pre-filled with this query. Add a delivery channel there to finish.")
                    .font(.caption2)
            }

            if mode == "llm" {
                llmRunsSection
            } else if !results.isEmpty {
                ftsResultsSection
            }

            if let error {
                Section { Text(error).font(.caption).foregroundStyle(theme.danger) }
            }
        }
        .compatGroupedForm()
        .themedScreenBackground(theme)
        .navigationTitle("Queries")
        .compatInlineTitle()
        .sheet(item: $draft) { rule in
            AlertEditor(rule: rule, onSave: { _ in draft = nil })
        }
    }

    // MARK: - FTS results

    private var ftsResultsSection: some View {
        Section {
            ForEach(results) { item in
                VStack(alignment: .leading, spacing: 3) {
                    Text(item.title ?? item._excerpt ?? "(untitled)")
                        .font(.subheadline.weight(.medium))
                        .foregroundStyle(theme.text)
                        .lineLimit(2)
                    HStack(spacing: Space.sm) {
                        Text(item.source_id)
                            .font(.caption2.monospaced())
                            .foregroundStyle(theme.accent)
                        if let pub = item.published_at {
                            Text(pub).font(.caption2).foregroundStyle(theme.textMuted).lineLimit(1)
                        }
                    }
                    if let s = item.summary ?? item.body, !s.isEmpty {
                        Text(s).font(.caption2).foregroundStyle(theme.textMuted).lineLimit(2)
                    }
                }
                .padding(.vertical, 2)
            }
        } header: {
            Text("Results (\(results.count))")
        }
    }

    // MARK: - LLM runs

    private var llmRunsSection: some View {
        Section {
            if searchStore.active.isEmpty && searchStore.completed.isEmpty {
                Text("No runs yet.").font(.caption).foregroundStyle(theme.textMuted)
            }
            ForEach(searchStore.active) { run in
                runRow(query: run.query,
                       detail: run.snapshot?.phase ?? "queued",
                       done: false)
            }
            ForEach(searchStore.completed) { run in
                runRow(query: run.query,
                       detail: completedDetail(run.snapshot),
                       done: true)
            }
        } header: {
            Text("Pipeline runs")
        }
    }

    private func completedDetail(_ snap: SearchSnapshot?) -> String {
        if let n = snap?.results?.services?.count, n > 0 {
            return "completed · \(n) service\(n == 1 ? "" : "s")"
        }
        return "completed"
    }

    private func runRow(query: String, detail: String, done: Bool) -> some View {
        HStack(spacing: Space.md) {
            if done {
                Image(systemName: "checkmark.circle.fill").foregroundStyle(theme.success)
            } else {
                ProgressView().controlSize(.small)
            }
            VStack(alignment: .leading, spacing: 2) {
                Text(query).font(.subheadline).lineLimit(1)
                Text(detail).font(.caption2).foregroundStyle(theme.textMuted).lineLimit(1)
            }
            Spacer()
        }
    }

    // MARK: - Logic

    private var canRun: Bool {
        mode == "llm"
            ? !nlQuery.trimmingCharacters(in: .whitespaces).isEmpty
            : !q.trimmingCharacters(in: .whitespaces).isEmpty
    }

    private func run() async {
        error = nil
        if mode == "llm" {
            await searchStore.start(nlQuery, api: api)
        } else {
            let tq = q.trimmingCharacters(in: .whitespaces)
            running = true
            defer { running = false }
            do {
                results = try await api.intelItems(q: tq, limit: 50).data
            } catch let e {
                error = e.localizedDescription
            }
        }
    }

    private func suggestFTS() async {
        let nl = nlQuery.trimmingCharacters(in: .whitespaces)
        guard !nl.isEmpty else { return }
        suggesting = true
        defer { suggesting = false }
        do {
            let suggestions = try await api.searchSuggest(nl)
            if let first = suggestions.first(where: { !$0.trimmingCharacters(in: .whitespaces).isEmpty }) {
                q = first
                error = nil
                Haptics.success()
            } else {
                error = "No FTS suggestion returned"
            }
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func makeAlertDraft() -> AlertRule {
        var pred = AlertPredicate()
        pred.mode = mode
        let tq = q.trimmingCharacters(in: .whitespaces)
        if !tq.isEmpty { pred.q = tq }
        if mode == "llm" {
            let nl = nlQuery.trimmingCharacters(in: .whitespaces)
            if !nl.isEmpty { pred.nl_query = nl }
        }
        var rule = AlertRule.blank
        rule.predicate = pred
        return rule
    }
}
