import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Watchlists (roadmap 21) — list, create, delete.
//
// A watchlist is sugar over an alert rule whose predicate is
// `{"entity_ids":[…]}`: creating one creates the rule, deleting one deletes the
// rule and its events. That is why deletion here is destructive and says so.
//
// `entity_ids` MUST be non-empty. This is not a nicety — an empty list compiles
// to a predicate with no entity term, which matches every item in the tenant,
// so the server rejects it with `entity_ids_required`. The create form enforces
// the same rule locally and explains WHY, rather than leaving the user to
// discover it through a 400.
//
// Mount inside a NavigationStack (like `AlertsTab`); the create form presents
// itself as a sheet with its own stack.
// ─────────────────────────────────────────────────────────────────────────────

struct WatchlistsView: View {
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var lists: [Watchlist] = []
    @State private var loading = false
    @State private var error: String?
    @State private var showCreate = false

    var body: some View {
        Group {
            if loading && lists.isEmpty {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if lists.isEmpty {
                emptyState
            } else {
                list
            }
        }
        .themedScreenBackground(theme)
        .navigationTitle("Watchlists")
        .toolbar {
            ToolbarItem(placement: .compatPrimary) {
                Button { showCreate = true } label: {
                    Image(systemName: "plus.circle.fill")
                }
                .accessibilityLabel("New watchlist")
            }
            ToolbarItem(placement: .compatPrimary) {
                Button { Task { await reload() } } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .disabled(loading)
                .accessibilityLabel("Refresh watchlists")
            }
        }
        .task { if lists.isEmpty { await reload() } }
        .sheet(isPresented: $showCreate) {
            WatchlistEditor(onCreate: { created in
                lists.insert(created, at: 0)
            })
        }
        .alert("Watchlists error",
               isPresented: Binding(get: { error != nil && !lists.isEmpty },
                                    set: { if !$0 { error = nil } })) {
            Button("OK", role: .cancel) { error = nil }
        } message: {
            Text(error ?? "")
        }
    }

    // MARK: - List

    private var list: some View {
        List {
            Section {
                ForEach(lists) { wl in
                    row(wl)
                }
            } footer: {
                Text("Deleting a watchlist also deletes the alert rule behind it and that rule's firing history.")
                    .font(.caption2)
            }
        }
        .compatInsetGroupedListStyle()
        .scrollContentBackground(.hidden)
        .refreshable { await reload() }
    }

    private func row(_ wl: Watchlist) -> some View {
        let ids = wl.entity_ids ?? []
        return VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: Space.sm) {
                Image(systemName: "person.2.fill")
                    .font(.body)
                    .foregroundStyle(theme.accent)
                    .frame(width: 26, height: 26)
                    .background(theme.accent.opacity(0.14),
                                in: RoundedRectangle(cornerRadius: Radius.sm))
                Text(wl.name)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
                    .lineLimit(1)
                Spacer(minLength: 0)
            }
            HStack(spacing: Space.xs) {
                // A stored watchlist can never legitimately be empty; if one
                // reads back empty, say so rather than showing "0 entities" as
                // if that were a normal state.
                if ids.isEmpty {
                    Pill(text: "NO ENTITIES", tone: .danger,
                         icon: "exclamationmark.triangle", maxWidth: 150)
                } else {
                    Pill(text: "\(ids.count) ENTIT\(ids.count == 1 ? "Y" : "IES")",
                         tone: .accent, icon: "person.text.rectangle", maxWidth: 150)
                }
                Text(relativeTime(wl.created_at))
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
                Spacer(minLength: 0)
            }
            if !ids.isEmpty {
                Text(ids.prefix(4).joined(separator: ", ")
                     + (ids.count > 4 ? " +\(ids.count - 4) more" : ""))
                    .font(.caption2.monospaced())
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(2)
            }
        }
        .padding(.vertical, 2)
        .listRowBackground(theme.surface)
        .swipeActions(edge: .trailing) {
            Button(role: .destructive) {
                Task { await delete(wl) }
            } label: {
                Label("Delete", systemImage: "trash")
            }
        }
        .contextMenu {
            Button {
                Clipboard.copy(wl.id)
            } label: {
                Label("Copy watchlist id", systemImage: "number")
            }
            if let ruleId = wl.rule_id, !ruleId.isEmpty {
                Button {
                    Clipboard.copy(ruleId)
                } label: {
                    Label("Copy rule id", systemImage: "bell")
                }
            }
            Button(role: .destructive) {
                Task { await delete(wl) }
            } label: {
                Label("Delete", systemImage: "trash")
            }
        }
    }

    private var emptyState: some View {
        ContentUnavailableView {
            Label("No watchlists", systemImage: "person.2.fill")
                .foregroundStyle(theme.text)
        } description: {
            Text("Pick the entities you care about — a person, a domain, an IP — and get told whenever new intel mentions any of them.")
                .foregroundStyle(theme.textMuted)
        } actions: {
            Button("Create a watchlist") { showCreate = true }
                .buttonStyle(.borderedProminent)
            if let error {
                Text(error)
                    .font(.caption2)
                    .foregroundStyle(theme.danger)
            }
        }
    }

    // MARK: - Actions

    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            lists = try await apiClient.api.watchlists()
            error = nil
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }

    private func delete(_ wl: Watchlist) async {
        let snapshot = lists
        lists.removeAll { $0.id == wl.id }
        do {
            try await apiClient.api.watchlistDelete(wl.id)
            Haptics.warning()
        } catch let e {
            lists = snapshot
            error = e.localizedDescription
            Haptics.error()
        }
    }
}

// MARK: - Create form

/// Sheet-presented create form. The only two things it can get wrong are a
/// blank name and an empty entity list, and both are blocked with the reason
/// spelled out — never with a silently disabled button.
struct WatchlistEditor: View {
    let onCreate: (Watchlist) -> Void

    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    /// Explicit rather than memberwise: the synthesized init would put the
    /// closure ahead of the environment-backed properties, which makes trailing
    /// closure syntax at the call site bind to the wrong parameter.
    init(onCreate: @escaping (Watchlist) -> Void) {
        self.onCreate = onCreate
    }

    @State private var name = ""
    @State private var query = ""
    @State private var hits: [EntityHit] = []
    @State private var selected: [EntityHit] = []
    @State private var searching = false
    @State private var searchError: String?
    @State private var saving = false
    @State private var error: String?

    var body: some View {
        NavigationStack {
            Form {
                Section("Watchlist") {
                    TextField("Name", text: $name)
                }

                Section {
                    if selected.isEmpty {
                        Text("No entities yet.")
                            .font(.caption)
                            .foregroundStyle(theme.textMuted)
                    } else {
                        ForEach(selected) { hit in
                            selectedRow(hit)
                        }
                    }
                } header: {
                    HStack {
                        Text("Watching")
                        Spacer()
                        Text("\(selected.count)")
                            .monospacedDigit()
                            .foregroundStyle(selected.isEmpty ? theme.danger : theme.textMuted)
                    }
                } footer: {
                    Text("At least one entity is required. A watchlist with none would carry no entity term at all, which matches every incoming item — so it is refused rather than quietly turned into a firehose.")
                        .font(.caption2)
                }

                Section {
                    TextField("Search entities (name, domain, IP…)", text: $query)
                        .compatNoAutocap()
                        .autocorrectionDisabled()
                        .font(.system(.body, design: .monospaced))
                        .fontDesign(.monospaced)

                    if searching {
                        HStack(spacing: Space.sm) {
                            ProgressView().controlSize(.mini)
                            Text("Searching…")
                                .font(.caption)
                                .foregroundStyle(theme.textMuted)
                        }
                    }
                    if let searchError {
                        Text(searchError)
                            .font(.caption2)
                            .foregroundStyle(theme.danger)
                    }
                    ForEach(availableHits) { hit in
                        resultRow(hit)
                    }
                    if !searching, searchError == nil,
                       availableHits.isEmpty,
                       !query.trimmingCharacters(in: .whitespaces).isEmpty {
                        Text("No entities match “\(query)”. Entities appear once they've been extracted from an ingested item.")
                            .font(.caption2)
                            .foregroundStyle(theme.textMuted)
                    }
                } header: {
                    Text("Add entities")
                }

                if let error {
                    Section {
                        Text(error)
                            .font(.caption)
                            .foregroundStyle(theme.danger)
                    }
                }
            }
            .navigationTitle("New watchlist")
            .compatGroupedForm()
            .themedScreenBackground(theme)
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Cancel") { dismiss() }
                }
                ToolbarItem(placement: .compatPrimary) {
                    Button("Create") { Task { await create() } }
                        .disabled(saving)
                }
            }
            // Debounced entity search: a changed query cancels the in-flight
            // task, so the sleep below is the debounce.
            .task(id: query) {
                await runSearch()
            }
        }
        .compatSheetSizing(.large)
    }

    // MARK: - Rows

    private func selectedRow(_ hit: EntityHit) -> some View {
        HStack(spacing: Space.sm) {
            Pill(text: hit.type.uppercased(), tone: .info, maxWidth: 90)
            Text(hit.value)
                .font(.caption.monospaced())
                .foregroundStyle(theme.text)
                .lineLimit(1)
            Spacer(minLength: 0)
            Button {
                selected.removeAll { $0.entity_id == hit.entity_id }
                Haptics.selection()
            } label: {
                Image(systemName: "minus.circle.fill")
                    .foregroundStyle(theme.danger)
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Remove \(hit.value)")
        }
    }

    private func resultRow(_ hit: EntityHit) -> some View {
        Button {
            selected.append(hit)
            Haptics.selection()
        } label: {
            HStack(spacing: Space.sm) {
                Pill(text: hit.type.uppercased(), tone: .info, maxWidth: 90)
                Text(hit.value)
                    .font(.caption.monospaced())
                    .foregroundStyle(theme.text)
                    .lineLimit(1)
                Spacer(minLength: 0)
                if let n = hit.mention_count {
                    Text("\(n)")
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
                Image(systemName: "plus.circle")
                    .foregroundStyle(theme.accent)
            }
        }
        .buttonStyle(.plain)
    }

    /// Hits not already picked — re-adding the same id is a no-op the server
    /// would strip anyway, so don't offer it.
    private var availableHits: [EntityHit] {
        let taken = Set(selected.map(\.entity_id))
        return hits.filter { !taken.contains($0.entity_id) }
    }

    // MARK: - Search

    private func runSearch() async {
        let q = query.trimmingCharacters(in: .whitespaces)
        guard q.count >= 2 else {
            hits = []
            searchError = nil
            return
        }
        try? await Task.sleep(for: .milliseconds(350))
        if Task.isCancelled { return }
        searching = true
        defer { searching = false }
        do {
            let results = try await apiClient.api.entitySearch(q, limit: 25)
            if Task.isCancelled { return }
            hits = results
            searchError = nil
        } catch let e {
            if Task.isCancelled { return }
            hits = []
            searchError = e.localizedDescription
        }
    }

    // MARK: - Create

    private func create() async {
        let trimmed = name.trimmingCharacters(in: .whitespaces)
        if trimmed.isEmpty {
            error = "Name is required."
            Haptics.error()
            return
        }
        if selected.isEmpty {
            error = "Add at least one entity — an empty watchlist would match every item, so the server refuses it."
            Haptics.error()
            return
        }
        saving = true
        defer { saving = false }
        do {
            let created = try await apiClient.api.watchlistCreate(
                name: trimmed,
                entityIds: selected.map(\.entity_id))
            onCreate(created)
            Haptics.success()
            dismiss()
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }
}
