import SwiftUI

/// Per-source schedule mode editor. Each data source is either:
///   • Map cron     — the backend scheduler runs it periodically so the
///                     map layer is pre-populated (instant /api/data read).
///   • Search only   — it is NOT cron'd; it's grabbed live only when the
///                     user runs a search. Either way it still writes
///                     intel_items, shown on the map + Intel + search.
///
/// Backed by `GET /api/sources` (rows carry `schedule_mode`) and
/// `PUT /api/sources/:id/schedule { "mode": "map_cron" | "search_only" }`.
struct SourceScheduleSettingsView: View {
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    private var api: API { apiClient.api }

    private struct Row: Identifiable, Hashable {
        let id: String
        let name: String
        let category: String
        var mode: String          // "map_cron" | "search_only"
    }

    @State private var rows: [Row] = []
    @State private var loading = true
    @State private var error: String?
    @State private var savingIds: Set<String> = []

    private var grouped: [(category: String, rows: [Row])] {
        Dictionary(grouping: rows, by: { $0.category })
            .map { (category: $0.key, rows: $0.value.sorted { $0.name < $1.name }) }
            .sorted { $0.category < $1.category }
    }

    var body: some View {
        Group {
            if loading {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if let error {
                VStack(spacing: 12) {
                    Text(error).font(.caption).foregroundStyle(theme.textMuted)
                    Button("Retry") { Task { await load() } }
                }.frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                form
            }
        }
        .themedScreenBackground(theme)
        .navigationTitle("Source scheduling")
        .compatInlineTitle()
        .task { await load() }
    }

    private var form: some View {
        Form {
            Section {
                Text("Map cron pre-collects on a schedule so the map is ready instantly. Search only skips the schedule and grabs live from the Search tab. Both still feed Intel everywhere.")
                    .font(.caption2).foregroundStyle(theme.textMuted)
            }
            ForEach(grouped, id: \.category) { group in
                Section(group.category.isEmpty ? "Uncategorized" : group.category) {
                    ForEach(group.rows) { row in
                        rowView(row)
                    }
                }
            }
        }
        .compatGroupedForm()
        .scrollContentBackground(.hidden)
    }

    @ViewBuilder private func rowView(_ row: Row) -> some View {
        let binding = Binding<String>(
            get: { rows.first(where: { $0.id == row.id })?.mode ?? row.mode },
            set: { newMode in Task { await setMode(row, newMode) } }
        )
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text(row.name).font(.subheadline)
                Spacer()
                if savingIds.contains(row.id) {
                    ProgressView().controlSize(.mini)
                }
            }
            Picker("", selection: binding) {
                Text("Map cron").tag("map_cron")
                Text("Search only").tag("search_only")
            }
            .pickerStyle(.segmented)
            .disabled(savingIds.contains(row.id))
        }
        .padding(.vertical, 2)
    }

    private func load() async {
        loading = true; error = nil
        do {
            let raw = try await api.sources()
            rows = raw.compactMap { r in
                guard let id = r.raw["id"]?.value as? String else { return nil }
                let name = (r.raw["name"]?.value as? String) ?? id
                let cat  = (r.raw["category"]?.value as? String) ?? ""
                let mode = (r.raw["schedule_mode"]?.value as? String) ?? "map_cron"
                return Row(id: id, name: name, category: cat, mode: mode)
            }
        } catch {
            self.error = "Failed to load sources: \(error.localizedDescription)"
        }
        loading = false
    }

    private func setMode(_ row: Row, _ newMode: String) async {
        guard let idx = rows.firstIndex(where: { $0.id == row.id }),
              rows[idx].mode != newMode else { return }
        let previous = rows[idx].mode
        rows[idx].mode = newMode                 // optimistic
        savingIds.insert(row.id)
        defer { savingIds.remove(row.id) }
        do {
            _ = try await api.setSourceSchedule(row.id, mode: newMode)
        } catch {
            if let i = rows.firstIndex(where: { $0.id == row.id }) {
                rows[i].mode = previous          // revert on failure
            }
            self.error = "Save failed for \(row.name): \(error.localizedDescription)"
        }
    }
}
