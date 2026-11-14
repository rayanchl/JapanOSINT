import SwiftUI

/// Per-source schedule mode editor. Each data source is either:
///   • Map cron     — the backend scheduler runs it periodically so the
///                     map layer is pre-populated (instant /api/data read).
///   • Search only   — it is NOT cron'd; it's grabbed live only when the
///                     user runs a search. Either way it still writes
///                     intel_items, shown on the map + Intel + search.
///
/// The registry is ~61k sources, so this screen never loads it: it searches
/// the server (`GET /api/sources/search`, 50 rows a page, load more on scroll)
/// and reads each visible row's current mode on demand
/// (`GET /api/sources/:id`, `schedule_mode`). Writes go to
/// `PUT /api/sources/:id/schedule { "mode": "map_cron" | "search_only" }`.
struct SourceScheduleSettingsView: View {
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    private var api: API { apiClient.api }

    @State private var rows: [SourceSearchRow] = []
    @State private var page: SourcePageInfo?
    @State private var searchText = ""
    @State private var loading = true
    @State private var loadingMore = false
    @State private var error: String?
    @State private var savingIds: Set<String> = []
    /// Mode per source id, filled as rows appear ("map_cron" | "search_only").
    @State private var modes: [String: String] = [:]
    /// Bumped per fetch so a stale answer is dropped.
    @State private var fetchToken = 0

    var body: some View {
        Group {
            if loading && rows.isEmpty {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if let error, rows.isEmpty {
                VStack(spacing: 12) {
                    Text(error).font(.caption).foregroundStyle(theme.textMuted)
                    Button("Retry") { Task { await load(reset: true) } }
                }.frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                form
            }
        }
        .themedScreenBackground(theme)
        .navigationTitle("Source scheduling")
        .compatInlineTitle()
        .searchable(text: $searchText, prompt: "Search sources")
        // Debounced server search; each new text cancels the previous task.
        .task(id: searchText) {
            do { try await Task.sleep(nanoseconds: 300_000_000) } catch { return }
            await load(reset: true)
        }
    }

    private var form: some View {
        Form {
            Section {
                Text("Map cron pre-collects on a schedule so the map is ready instantly. Search only skips the schedule and grabs live from the Search tab. Both still feed Intel everywhere.")
                    .font(.caption2).foregroundStyle(theme.textMuted)
            }
            Section {
                ForEach(rows) { row in
                    rowView(row)
                }
                if let error {
                    Text(error).font(.caption2).foregroundStyle(theme.danger)
                }
                PagedFooter(shown: rows.count,
                            total: page?.total,
                            hasMore: page?.has_more == true,
                            loading: loadingMore,
                            error: nil,
                            noun: "sources",
                            loadMore: { Task { await load(reset: false) } })
            } header: {
                Text(searchText.isEmpty ? "Sources" : "Matching sources")
            }
        }
        .compatGroupedForm()
        .scrollContentBackground(.hidden)
    }

    @ViewBuilder private func rowView(_ row: SourceSearchRow) -> some View {
        let binding = Binding<String>(
            get: { modes[row.id] ?? "map_cron" },
            set: { newMode in Task { await setMode(row, newMode) } }
        )
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text(row.displayName).font(.subheadline)
                Spacer()
                if savingIds.contains(row.id) || modes[row.id] == nil {
                    ProgressView().controlSize(.mini)
                }
            }
            Picker("", selection: binding) {
                Text("Map cron").tag("map_cron")
                Text("Search only").tag("search_only")
            }
            .pickerStyle(.segmented)
            .disabled(savingIds.contains(row.id) || modes[row.id] == nil)
        }
        .padding(.vertical, 2)
        // The mode is read when the row scrolls into view, not for 61k rows.
        .task(id: row.id) { await loadMode(row.id) }
    }

    private func loadMode(_ id: String) async {
        if modes[id] != nil { return }
        if let m = try? await api.sourceScheduleMode(id) {
            modes[id] = m
        } else if modes[id] == nil {
            // The route answered without a mode (or failed): the server's
            // default for a source that was never set is map cron.
            modes[id] = "map_cron"
        }
    }

    private func load(reset: Bool) async {
        if !reset && (loadingMore || page?.has_more != true) { return }
        fetchToken += 1
        let token = fetchToken
        if reset { loading = true } else { loadingMore = true }
        var q = SourceSearchQuery()
        q.q = searchText
        // Only rows that have a schedule can be switched between the modes.
        q.sort = "name"
        let offset = reset ? 0 : rows.count
        do {
            let env = try await api.searchSources(q, limit: 50, offset: offset)
            guard token == fetchToken else { return }
            let fresh = env.data ?? []
            rows = reset ? fresh : rows + fresh
            page = env.page
            error = nil
        } catch {
            guard token == fetchToken else { return }
            self.error = "Failed to load sources: \(error.localizedDescription)"
        }
        loading = false
        loadingMore = false
    }

    private func setMode(_ row: SourceSearchRow, _ newMode: String) async {
        let previous = modes[row.id] ?? "map_cron"
        guard previous != newMode else { return }
        modes[row.id] = newMode                  // optimistic
        savingIds.insert(row.id)
        defer { savingIds.remove(row.id) }
        do {
            _ = try await api.setSourceSchedule(row.id, mode: newMode)
        } catch {
            modes[row.id] = previous             // revert on failure
            self.error = "Save failed for \(row.displayName): \(error.localizedDescription)"
        }
    }
}
