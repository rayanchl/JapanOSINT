import SwiftUI
import Combine

/// Lists every env-var the server consumes, with status. Tapping a row opens
/// a Face-ID-gated detail sheet for reveal/edit/clear. Search + filter sheet
/// mirror the visual scaffolding used by the cameras and saved tabs.
struct ApiKeysView: View {
    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var nav: MapNavigation
    @Environment(\.theme) private var theme

    @State private var items: [ApiKeyMeta] = []
    /// Snapshot of `/api/status` used by the detail sheet to show which
    /// sources/collectors reference each key. Best-effort: failure here
    /// leaves the array empty and the detail's "Used by" section just
    /// shows its empty state.
    @State private var statusRows: [StatusRow] = []
    @State private var loading: Bool = true
    @State private var error: String?

    @State private var searchText: String = ""
    @State private var statusFilter: StatusFilter = .all
    @State private var roleFilter: Set<String> = []
    @State private var showFilters: Bool = false
    @State private var selected: ApiKeyMeta?

    enum StatusFilter: String, CaseIterable, Identifiable {
        case all, set, unset, overlay
        var id: String { rawValue }
        var label: String {
            switch self {
            case .all:     return "All"
            case .set:     return "Set"
            case .unset:   return "Unset"
            case .overlay: return "Overlay"
            }
        }
    }

    var body: some View {
        Group {
            if loading && items.isEmpty {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if let error, items.isEmpty {
                OfflineStateView(retry: { Task { await load() } })
                    .overlay(alignment: .bottom) {
                        Text(error)
                            .font(.caption2)
                            .foregroundStyle(theme.textMuted)
                            .padding(.bottom, 24)
                    }
            } else {
                listView
            }
        }
        .background(theme.surface.ignoresSafeArea())
        .navigationTitle("API Keys")
        .searchable(
            text: $searchText,
            placement: .navigationBarDrawer(displayMode: .always),
            prompt: "Search keys"
        )
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                Button { showFilters = true } label: {
                    Image(systemName: filtersAreActive
                          ? "line.3.horizontal.decrease.circle.fill"
                          : "line.3.horizontal.decrease.circle")
                }
                .accessibilityLabel("Filters")
            }
            ToolbarItem(placement: .topBarTrailing) {
                Button { Task { await load() } } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .accessibilityLabel("Reload")
            }
        }
        .task {
            await load()
            // Console may have pushed us in response to `showApiKey(_:)` —
            // the published value can land before this view mounts, so the
            // `.onReceive` below would miss the initial value. Consume it here.
            if let name = nav.pendingApiKeyName { await openKey(name) }
        }
        .refreshable { await load() }
        .sheet(item: $selected) { meta in
            ApiKeyDetailView(meta: meta, statusRows: statusRows) { updated in
                if let i = items.firstIndex(where: { $0.id == updated.id }) {
                    items[i] = updated
                }
            }
        }
        .sheet(isPresented: $showFilters) { filtersSheet }
        .onReceive(nav.$pendingApiKeyName.compactMap { $0 }) { name in
            Task { await openKey(name) }
        }
    }

    // MARK: - List

    private var listView: some View {
        Form {
            if filteredItems.isEmpty {
                Text(searchText.isEmpty
                     ? "No keys match the current filters."
                     : "No keys match \"\(searchText)\"")
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
                    .frame(maxWidth: .infinity)
                    .listRowBackground(Color.clear)
            } else {
                ForEach(filteredItems) { item in
                    Button { selected = item } label: { row(item) }
                        .buttonStyle(.plain)
                }
            }
        }
    }

    private func row(_ item: ApiKeyMeta) -> some View {
        HStack(spacing: 10) {
            Image(systemName: item.set ? "key.fill" : "key.slash")
                .foregroundStyle(item.set ? theme.success : theme.textMuted)
                .frame(width: 22)
            VStack(alignment: .leading, spacing: 2) {
                Text(item.name)
                    .font(.system(.body, design: .monospaced).weight(.medium))
                    .foregroundStyle(theme.text)
                    .lineLimit(1)
                HStack(spacing: 4) {
                    rolePill(item.role)
                    if item.hasOverlay { overlayPill }
                }
            }
            Spacer()
            statusPill(item.set)
            Image(systemName: "chevron.right")
                .font(.caption2)
                .foregroundStyle(theme.textMuted)
        }
        .contentShape(Rectangle())
    }

    private func rolePill(_ role: String) -> some View {
        Pill(text: role.uppercased(), tone: roleTone(role))
    }

    private var overlayPill: some View {
        Pill(text: "OVERLAY", tone: .accent)
    }

    private func statusPill(_ set: Bool) -> some View {
        Pill(text: set ? "SET" : "UNSET", tone: set ? .success : .warning, size: .md)
    }

    private func roleTone(_ role: String) -> Pill.Tone {
        switch role {
        case "required": return .accent
        case "anyOf":    return .warning
        default:         return .neutral
        }
    }

    // MARK: - Filtering

    private var filtersAreActive: Bool {
        statusFilter != .all || !roleFilter.isEmpty
    }

    private var filteredItems: [ApiKeyMeta] {
        let q = searchText.trimmingCharacters(in: .whitespaces).lowercased()
        return items.filter { item in
            switch statusFilter {
            case .all:     break
            case .set:     if !item.set         { return false }
            case .unset:   if item.set          { return false }
            case .overlay: if !item.hasOverlay  { return false }
            }
            if !roleFilter.isEmpty, !roleFilter.contains(item.role) {
                return false
            }
            if !q.isEmpty, !item.name.lowercased().contains(q) {
                return false
            }
            return true
        }
    }

    private var availableRoles: [String] {
        Array(Set(items.map(\.role))).sorted { a, b in
            // required → anyOf → optional, matching server-side ranking.
            let rank: [String: Int] = ["required": 0, "anyOf": 1, "optional": 2]
            return (rank[a] ?? 99) < (rank[b] ?? 99)
        }
    }

    private func toggleRole(_ role: String) {
        if roleFilter.contains(role) { roleFilter.remove(role) }
        else { roleFilter.insert(role) }
    }

    // MARK: - Filters sheet

    private var filtersSheet: some View {
        NavigationStack {
            Form {
                Section("Status") {
                    Picker("Status", selection: $statusFilter) {
                        ForEach(StatusFilter.allCases) { Text($0.label).tag($0) }
                    }
                    .pickerStyle(.segmented)
                }
                Section("Role") {
                    if availableRoles.isEmpty {
                        Text("No roles yet")
                            .foregroundStyle(theme.textMuted)
                    } else {
                        ForEach(availableRoles, id: \.self) { role in
                            Button { toggleRole(role) } label: {
                                HStack {
                                    Text(role.capitalized)
                                        .foregroundStyle(theme.text)
                                    Spacer()
                                    if roleFilter.contains(role) {
                                        Image(systemName: "checkmark")
                                            .foregroundStyle(theme.accent)
                                    }
                                }
                            }
                        }
                    }
                }
                if filtersAreActive {
                    Section {
                        Button("Reset filters", role: .destructive) {
                            statusFilter = .all
                            roleFilter.removeAll()
                        }
                    }
                }
            }
            .navigationTitle("Filters")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { showFilters = false }
                }
            }
        }
        .presentationDetents([.medium, .large])
    }

    // MARK: - Loading

    private func load() async {
        loading = true
        defer { loading = false }
        let api = API(baseURL: settings.backendBaseURL)
        // Fetch keys + status concurrently. Status is best-effort — if it
        // fails we still show the keys list, the detail sheet's "Used by"
        // section just won't have data to display.
        async let keysTask = api.apiKeys()
        async let statusTask = api.status()
        do {
            let fresh = try await keysTask
            let env = try? await statusTask
            await MainActor.run {
                self.items = fresh
                self.statusRows = env?.apis ?? []
                self.error = nil
            }
        } catch {
            await MainActor.run {
                self.error = error.localizedDescription
            }
        }
    }

    /// Cross-tab nav target: open the detail sheet for `name`. If the keys
    /// list hasn't loaded yet, fetch first so we can resolve the metadata.
    private func openKey(_ name: String) async {
        if items.isEmpty { await load() }
        if let m = items.first(where: { $0.name == name }) {
            selected = m
        }
        nav.pendingApiKeyName = nil
    }
}
