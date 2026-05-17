import SwiftUI
import Combine

/// Which key store this view edits.
/// - `.workspace`: the signed-in user's own per-tenant BYOK keys
///   (`/api/tenant-keys`, AES-encrypted server-side). The default — this is
///   the user-facing API Keys tab.
/// - `.platform`: the server-wide operator overlay (`/api/keys`). Reached
///   only from Settings → Admin and gated to owner/admin server-side.
enum KeyScope: Equatable { case workspace, platform }

/// Unified row the list/detail render, mapped from either `TenantKeyMeta`
/// (workspace) or `ApiKeyMeta` (platform). `byok` means "a value is set in
/// *this* store specifically" — the tenant's own key, or an overlay value.
/// `source` is workspace-only: "tenant" | "platform" | nil.
struct KeyRow: Identifiable, Hashable {
    let name: String
    let role: String
    let set: Bool
    let byok: Bool
    let source: String?
    var id: String { name }

    init(name: String, role: String, set: Bool, byok: Bool, source: String?) {
        self.name = name; self.role = role; self.set = set
        self.byok = byok; self.source = source
    }
    init(_ m: TenantKeyMeta) {
        name = m.name; role = m.role; set = m.set; byok = m.byok; source = m.source
    }
    init(_ m: ApiKeyMeta) {
        name = m.name; role = m.role; set = m.set
        byok = m.hasOverlay
        source = m.set ? (m.hasOverlay ? "tenant" : "platform") : nil
    }
}

/// Lists every env-var the server consumes, with status. Tapping a row opens
/// a Face-ID-gated detail sheet for reveal/edit/clear. Search + filter sheet
/// mirror the visual scaffolding used by the cameras and saved tabs.
struct ApiKeysView: View {
    let scope: KeyScope
    init(scope: KeyScope = .workspace) { self.scope = scope }

    @EnvironmentObject var apiClient: APIClient
    @EnvironmentObject var nav: MapNavigation
    @Environment(\.theme) private var theme

    @State private var items: [KeyRow] = []
    /// Owner-policy gate (workspace scope). Platform scope is always editable
    /// — the route itself is admin-gated, so reaching it implies the right.
    @State private var canManage: Bool = true
    @State private var policy: String = ""
    /// Snapshot of `/api/status` used by the detail sheet to show which
    /// sources/collectors reference each key. Best-effort.
    @State private var statusRows: [StatusRow] = []
    @State private var loading: Bool = true
    @State private var error: String?

    @State private var searchText: String = ""
    @State private var statusFilter: StatusFilter = .all
    @State private var roleFilter: Set<String> = []
    @State private var showFilters: Bool = false
    @State private var selected: KeyRow?

    enum StatusFilter: String, FilterChoice {
        case all, set, unset, mine
        var id: String { rawValue }
        var label: String {
            switch self {
            case .all:   return "All"
            case .set:   return "Set"
            case .unset: return "Unset"
            case .mine:  return "Custom"
            }
        }
    }

    private var title: String { scope == .platform ? "Platform Keys" : "API Keys" }

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
        .navigationTitle(title)
        .searchable(
            text: $searchText,
            placement: .navigationBarDrawer(displayMode: .always),
            prompt: "Search keys"
        )
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                FilterToolbarButton(isActive: filtersAreActive) {
                    showFilters = true
                }
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
            if scope == .workspace, let name = nav.pendingApiKeyName {
                await openKey(name)
            }
        }
        .refreshable { await load() }
        .sheet(item: $selected) { row in
            ApiKeyDetailView(row: row,
                             scope: scope,
                             canManage: canManage,
                             statusRows: statusRows) { updated in
                if let i = items.firstIndex(where: { $0.id == updated.id }) {
                    items[i] = updated
                }
            }
        }
        .sheet(isPresented: $showFilters) { filtersSheet }
        .onReceive(nav.$pendingApiKeyName.compactMap { $0 }) { name in
            if scope == .workspace { Task { await openKey(name) } }
        }
    }

    // MARK: - List

    private var listView: some View {
        Form {
            if scope == .workspace && !canManage {
                Section {
                    Label(
                        "Read-only — your workspace owner restricts who can edit keys.",
                        systemImage: "lock.fill"
                    )
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
                }
            }
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

    private func row(_ item: KeyRow) -> some View {
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
                    sourcePill(item)
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

    /// Where the active value comes from. Workspace: "YOUR KEY" vs "ADMIN"
    /// (operator-provided fallback; its value stays hidden from members).
    /// Platform: "OVERLAY" when the operator set a value.
    @ViewBuilder
    private func sourcePill(_ item: KeyRow) -> some View {
        if scope == .workspace {
            if item.byok {
                Pill(text: "YOUR KEY", tone: .accent, size: .md)
            } else if item.source == "platform" {
                Pill(text: "ADMIN", tone: .neutral, size: .md)
            }
        } else if item.byok {
            Pill(text: "OVERLAY", tone: .accent)
        }
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

    private var filteredItems: [KeyRow] {
        let q = searchText.trimmingCharacters(in: .whitespaces).lowercased()
        return items.filter { item in
            switch statusFilter {
            case .all:   break
            case .set:   if !item.set   { return false }
            case .unset: if item.set    { return false }
            case .mine:  if !item.byok  { return false }
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
        FilterSheet(
            isActive: filtersAreActive,
            onReset: {
                statusFilter = .all
                roleFilter.removeAll()
            },
            onDone: { showFilters = false }
        ) {
            FilterSegmentedSection(title: "Status", selection: $statusFilter)
            FilterMultiSelectSection(
                title: "Role",
                items: availableRoles,
                emptyText: "No roles yet",
                label: { $0.capitalized },
                isSelected: { roleFilter.contains($0) },
                toggle: { toggleRole($0) }
            )
        }
    }

    // MARK: - Loading

    private func load() async {
        loading = true
        defer { loading = false }
        let api = apiClient.api
        async let statusTask = api.status()
        do {
            var fresh: [KeyRow] = []
            var manage = true
            var pol = ""
            switch scope {
            case .workspace:
                let env = try await api.tenantKeys()
                fresh = env.items.map(KeyRow.init)
                manage = env.canManage
                pol = env.policy
            case .platform:
                fresh = try await api.apiKeys().map(KeyRow.init)
            }
            let env = try? await statusTask
            await MainActor.run {
                self.items = fresh
                self.canManage = manage
                self.policy = pol
                self.statusRows = env?.apis ?? []
                self.error = nil
            }
        } catch {
            await MainActor.run {
                self.error = error.localizedDescription
            }
        }
    }

    /// Cross-tab nav target (workspace only): open the detail sheet for `name`.
    private func openKey(_ name: String) async {
        if items.isEmpty { await load() }
        if let m = items.first(where: { $0.name == name }) {
            selected = m
        }
        nav.pendingApiKeyName = nil
    }
}
