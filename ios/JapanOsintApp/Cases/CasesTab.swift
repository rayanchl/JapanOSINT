import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Cases (roadmap item 14) — the list. The spine of the analyst workflow and the
// screen that supersedes the flat starred list in `SavedTab`.
//
// Owns its own NavigationStack (like `SavedTab` / `CameraDiscoveryTab`) because
// it is mounted as a top-level tab on phone and as a sidebar workspace on iPad;
// in neither position does a parent stack exist to push `CaseDetailView` onto.
//
// Deleting a case is NOT a swipe action here. It is irreversible and takes the
// findings, notes and activity with it, so it lives behind a typed confirmation
// inside `CaseDetailView` — where the user can see what they are about to lose.
// ─────────────────────────────────────────────────────────────────────────────

struct CasesTab: View {
    @EnvironmentObject var store: CaseStore
    @Environment(\.theme) private var theme

    @State private var filter: Filter = .open
    @State private var searchText: String = ""
    @State private var showCreate: Bool = false

    /// Maps onto the server's `?status=` filter. `.all` sends no filter at all.
    enum Filter: String, CaseIterable, Identifiable {
        case open, closed, archived, all
        var id: String { rawValue }
        var label: String {
            switch self {
            case .open:     return "Open"
            case .closed:   return "Closed"
            case .archived: return "Archived"
            case .all:      return "All"
            }
        }
        var apiValue: String? { self == .all ? nil : rawValue }
    }

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                filterPicker
                content
            }
            .themedScreenBackground(theme)
            .navigationTitle("Cases")
            .searchable(text: $searchText,
                        placement: .compatDrawer,
                        prompt: "Search loaded cases")
            .toolbar {
                ToolbarItem(placement: .compatPrimary) {
                    Button { showCreate = true } label: {
                        Image(systemName: "plus.circle.fill")
                    }
                    .accessibilityLabel("New case")
                }
                ToolbarItem(placement: .compatPrimary) {
                    Button { Task { await store.reload() } } label: {
                        Image(systemName: "arrow.clockwise")
                    }
                    .disabled(store.loading)
                }
            }
            .navigationDestination(for: CaseSummary.self) { summary in
                CaseDetailView(caseId: summary.id, initialName: summary.name)
            }
            // Align the store with the segment the picker is showing BEFORE
            // the first fetch, otherwise the list would arrive unfiltered
            // while the control reads "Open". `setFilter` no-ops when the
            // value already matches, so re-entering the tab costs nothing.
            .task {
                await store.setFilter(filter.apiValue)
                await store.loadIfNeeded()
            }
            // Mutations report through the store; a modal is the only way the
            // user sees a failure that happened on a row they already swiped
            // away from.
            .alert("Cases",
                   isPresented: Binding(get: { store.error != nil },
                                        set: { if !$0 { store.error = nil } })) {
                Button("OK", role: .cancel) { store.error = nil }
            } message: {
                Text(store.error ?? "")
            }
            .sheet(isPresented: $showCreate) {
                CaseCreateSheet()
            }
        }
    }

    // MARK: - Chrome

    /// Kept outside the loading/empty branch so the user can always get back to
    /// a status that has rows in it — an empty "Archived" view with no way to
    /// switch back would be a dead end.
    private var filterPicker: some View {
        Picker("Status", selection: $filter) {
            ForEach(Filter.allCases) { f in
                Text(f.label).tag(f)
            }
        }
        .pickerStyle(.segmented)
        .labelsHidden()
        .padding(.horizontal, Space.lg)
        .padding(.top, Space.sm)
        .padding(.bottom, Space.sm)
        .onChange(of: filter) { _, newValue in
            Task { await store.setFilter(newValue.apiValue) }
        }
    }

    @ViewBuilder
    private var content: some View {
        if store.loading && store.cases.isEmpty {
            ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
        } else if store.cases.isEmpty {
            emptyState
        } else if visible.isEmpty {
            noMatches
        } else {
            list
        }
    }

    // MARK: - List

    private var list: some View {
        List {
            ForEach(visible) { summary in
                NavigationLink(value: summary) {
                    CaseRow(summary: summary,
                            itemCount: store.itemCount(for: summary.id),
                            isActive: store.activeCaseId == summary.id)
                }
                .listRowBackground(theme.surface)
                .swipeActions(edge: .trailing, allowsFullSwipe: false) {
                    swipeActions(for: summary)
                }
                .contextMenu { contextActions(for: summary) }
            }
            if store.nextCursor != nil {
                loadMoreRow
            }
        }
        .listStyle(.plain)
        .scrollContentBackground(.hidden)
        .refreshable { await store.reload() }
    }

    /// Fires as soon as it scrolls into view — "load more on scroll end"
    /// without needing a scroll-position observer.
    private var loadMoreRow: some View {
        HStack(spacing: Space.sm) {
            Spacer(minLength: 0)
            if store.loadingMore {
                ProgressView()
            } else {
                Image(systemName: "arrow.down.circle")
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
            }
            Text(store.loadingMore ? "Loading more…" : "Load more cases")
                .font(.caption)
                .foregroundStyle(theme.textMuted)
            Spacer(minLength: 0)
        }
        .padding(.vertical, Space.sm)
        .listRowBackground(Color.clear)
        .onAppear { Task { await store.loadMore() } }
    }

    @ViewBuilder
    private func swipeActions(for summary: CaseSummary) -> some View {
        if summary.status != "archived" {
            Button {
                Task { await store.setStatus(summary.id, to: "archived") }
            } label: {
                Label("Archive", systemImage: "archivebox")
            }
            .tint(theme.warning)
        }
        if summary.status == "open" {
            Button {
                Task { await store.setStatus(summary.id, to: "closed") }
            } label: {
                Label("Close", systemImage: "checkmark.seal")
            }
            .tint(theme.accent)
        } else {
            Button {
                Task { await store.setStatus(summary.id, to: "open") }
            } label: {
                Label("Reopen", systemImage: "folder")
            }
            .tint(theme.success)
        }
    }

    @ViewBuilder
    private func contextActions(for summary: CaseSummary) -> some View {
        Button {
            store.activeCaseId = summary.id
            Haptics.selection()
        } label: {
            Label("Make active case", systemImage: "target")
        }
        Divider()
        if summary.status != "open" {
            Button {
                Task { await store.setStatus(summary.id, to: "open") }
            } label: {
                Label("Reopen", systemImage: "folder")
            }
        }
        if summary.status != "closed" {
            Button {
                Task { await store.setStatus(summary.id, to: "closed") }
            } label: {
                Label("Close", systemImage: "checkmark.seal")
            }
        }
        if summary.status != "archived" {
            Button {
                Task { await store.setStatus(summary.id, to: "archived") }
            } label: {
                Label("Archive", systemImage: "archivebox")
            }
        }
        Divider()
        Button {
            Clipboard.copy(summary.id)
            Haptics.tap()
        } label: {
            Label("Copy case id", systemImage: "doc.on.doc")
        }
    }

    // MARK: - Filtering
    //
    // Search is a LOCAL filter over the rows already loaded, not a server
    // query — /api/cases has no `q` parameter. The placeholder says
    // "Search loaded cases" so the user is never told a case doesn't exist
    // when it is simply on a page we haven't fetched.

    private var visible: [CaseSummary] {
        let q = searchText.trimmingCharacters(in: .whitespaces).lowercased()
        guard !q.isEmpty else { return store.cases }
        return store.cases.filter { c in
            if c.name.lowercased().contains(q) { return true }
            if let s = c.summary, s.lowercased().contains(q) { return true }
            return c.id.lowercased().contains(q)
        }
    }

    // MARK: - Empty states

    private var emptyState: some View {
        ContentUnavailableView {
            Label(filter == .all ? "No cases yet" : "No \(filter.label.lowercased()) cases",
                  systemImage: "folder.badge.plus")
                .foregroundStyle(theme.text)
        } description: {
            Text(filter == .all || filter == .open
                 ? "A case is the unit of work you report on: a named folder with a roster, a pinned evidence set and an immutable history. Pin an intel item, an entity or a breach record into one and the pin keeps a snapshot of what you saw — so the report you export in six weeks still says what you actually read."
                 : "Nothing in this status. Switch to All to see every case in the workspace.")
                .foregroundStyle(theme.textMuted)
        } actions: {
            Button {
                showCreate = true
            } label: {
                Label("New case", systemImage: "plus")
            }
            .buttonStyle(.borderedProminent)
        }
    }

    private var noMatches: some View {
        ContentUnavailableView {
            Label("No matches", systemImage: "line.3.horizontal.decrease.circle")
                .foregroundStyle(theme.text)
        } description: {
            Text("No loaded case matches \"\(searchText)\". Older cases may not be fetched yet — pull to refresh or load more.")
                .foregroundStyle(theme.textMuted)
        }
    }
}

// MARK: - Row

/// One case. Name, status, priority, pinned-item count (when known) and the
/// relative `updated_at`, which is what the list is sorted by.
struct CaseRow: View {
    let summary: CaseSummary
    /// nil == not yet known (see `CaseStore.itemCounts`). Renders nothing
    /// rather than a fabricated 0.
    let itemCount: Int?
    let isActive: Bool

    @Environment(\.theme) private var theme

    var body: some View {
        HStack(alignment: .top, spacing: Space.md) {
            Image(systemName: CaseStatus.icon(summary.status))
                .font(.body)
                .foregroundStyle(iconColor)
                .frame(width: 26, height: 26)
                .background(iconColor.opacity(0.14),
                            in: RoundedRectangle(cornerRadius: Radius.sm))

            VStack(alignment: .leading, spacing: 3) {
                Text(summary.name)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
                    .lineLimit(2)

                if let s = summary.summary, !s.isEmpty {
                    Text(s)
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                        .lineLimit(2)
                }

                HStack(spacing: Space.sm - 2) {
                    Pill(text: CaseStatus.label(summary.status).uppercased(),
                         tone: CaseStatus.tone(summary.status),
                         icon: CaseStatus.icon(summary.status),
                         maxWidth: 120)
                    if summary.priority > 0 {
                        Pill(text: CasePriority.label(summary.priority).uppercased(),
                             tone: CasePriority.tone(summary.priority),
                             icon: "exclamationmark.triangle.fill",
                             maxWidth: 120)
                    }
                    if isActive {
                        Pill(text: "ACTIVE", tone: .accent, icon: "target", maxWidth: 90)
                    }
                    Spacer(minLength: 0)
                }

                HStack(spacing: Space.xs) {
                    if let itemCount {
                        Text("\(itemCount) pinned")
                            .font(.caption2.monospacedDigit())
                            .foregroundStyle(theme.textMuted)
                        Text("·")
                            .font(.caption2)
                            .foregroundStyle(theme.textMuted)
                    }
                    Text("updated \(caseRelativeTime(summary.updated_at))")
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                    Spacer(minLength: 0)
                }
            }

            Spacer(minLength: 0)
        }
        .padding(.vertical, 2)
    }

    private var iconColor: Color {
        switch summary.status {
        case "open":     return theme.success
        case "archived": return theme.accentAlt
        default:         return theme.textMuted
        }
    }
}

// MARK: - Create sheet

/// New case. Name is the only required field; the server trims it and rejects
/// anything over 200 bytes, so over-length input surfaces as a server error
/// rather than being silently cut here.
struct CaseCreateSheet: View {
    /// Fires with the created case so a caller (e.g. the picker) can chain.
    var onCreated: ((CaseDetail) -> Void)? = nil

    @EnvironmentObject var store: CaseStore
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    @State private var name: String = ""
    @State private var summary: String = ""
    @State private var priority: Int = 0
    @State private var saving: Bool = false
    @State private var error: String?

    private var trimmedName: String {
        name.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("Case name", text: $name)
                        .font(.subheadline)
                } header: {
                    Text("Name")
                } footer: {
                    Text("What you'd call this investigation. Required.")
                }

                Section {
                    TextField("What is this case about?", text: $summary, axis: .vertical)
                        .lineLimit(3...8)
                        .font(.subheadline)
                } header: {
                    Text("Summary")
                } footer: {
                    Text("Becomes the executive summary section of the exported report.")
                }

                Section {
                    Picker("Priority", selection: $priority) {
                        ForEach(CasePriority.range, id: \.self) { p in
                            Text("\(p) · \(CasePriority.label(p))").tag(p)
                        }
                    }
                } header: {
                    Text("Priority")
                }

                if let error {
                    Section {
                        Text(error)
                            .font(.caption)
                            .foregroundStyle(theme.danger)
                    }
                }
            }
            .compatGroupedForm()
            .themedScreenBackground(theme)
            .navigationTitle("New case")
            .compatInlineTitle()
            .toolbar {
                ToolbarItem(placement: .compatLeading) {
                    Button("Cancel") { dismiss() }
                }
                ToolbarItem(placement: .compatPrimary) {
                    Button {
                        Task { await save() }
                    } label: {
                        if saving {
                            ProgressView().controlSize(.small)
                        } else {
                            Text("Create")
                        }
                    }
                    .disabled(saving || trimmedName.isEmpty)
                }
            }
        }
        .compatSheetSizing(.form)
    }

    private func save() async {
        saving = true
        defer { saving = false }
        do {
            let detail = try await store.create(name: trimmedName,
                                                summary: summary,
                                                priority: priority)
            error = nil
            onCreated?(detail)
            dismiss()
        } catch let e {
            error = e.localizedDescription
            Haptics.error()
        }
    }
}
