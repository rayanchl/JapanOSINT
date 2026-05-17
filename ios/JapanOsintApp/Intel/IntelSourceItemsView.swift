import SwiftUI

/// Level-2 surface: paginated items for a single source.
struct IntelSourceItemsView: View {
    let source: IntelSource

    @EnvironmentObject var apiClient: APIClient
    @EnvironmentObject var intelCache: IntelCache
    @Environment(\.theme) private var theme

    @State private var items: [IntelItem] = []
    @State private var nextCursor: String?
    @State private var loading = false
    @State private var error: String?
    @State private var searchText = ""
    @State private var running = false

    @State private var showFilters = false
    @State private var selectedLanguages: Set<String> = []
    @State private var linkPresence: LinkPresence = .all

    enum LinkPresence: String, FilterChoice {
        case all, hasLink, noLink
        var id: String { rawValue }
        var label: String {
            switch self {
            case .all:     return "All"
            case .hasLink: return "Has link"
            case .noLink:  return "No link"
            }
        }
    }

    var body: some View {
        Group {
            if loading && items.isEmpty {
                ProgressView("Loading items…")
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if items.isEmpty {
                emptyState
            } else {
                list
            }
        }
        .background(theme.surface.ignoresSafeArea())
        .navigationTitle(source.name)
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                Button {
                    Task { await runNow() }
                } label: {
                    if running { ProgressView().controlSize(.mini) }
                    else       { Image(systemName: "play.fill") }
                }
                .disabled(running)
                .accessibilityLabel(running ? "Running…" : "Run \(source.name)")
            }
            ToolbarItem(placement: .topBarTrailing) {
                FilterToolbarButton(isActive: filtersAreActive) {
                    showFilters = true
                }
            }
        }
        .sheet(isPresented: $showFilters) { filtersSheet }
        // Hide the tab bar while a source is open — the top nav back button
        // is the only "go back" we want; the tab bar at the bottom doubles
        // as another navigation affordance and competes visually.
        .toolbar(.hidden, for: .tabBar)
        // Search lives in a bottom inset rather than the nav-bar drawer so
        // that pushing into this view doesn't have to animate the parent's
        // searchable away — the top bar stays clean and snappy.
        .safeAreaInset(edge: .bottom) { searchFooter }
        .onChange(of: searchText) { _, _ in
            // Debounce-ish: small delay so we don't refetch on every keystroke.
            Task {
                try? await Task.sleep(for: .milliseconds(250))
                await reload()
            }
        }
        .refreshable { await reload() }
        .task {
            if items.isEmpty {
                // Paint cached items immediately so the list is non-empty
                // while the live fetch is in flight; the API response then
                // overwrites with fresh data.
                let cached = intelCache.cachedItems(for: source.id)
                if !cached.isEmpty { items = cached }
                await reload()
            }
        }
    }

    private var searchFooter: some View {
        HStack(spacing: 8) {
            Image(systemName: "magnifyingglass")
                .foregroundStyle(theme.textMuted)
            TextField("Search this source", text: $searchText)
                .textFieldStyle(.plain)
                .submitLabel(.search)
            if !searchText.isEmpty {
                Button {
                    searchText = ""
                } label: {
                    Image(systemName: "xmark.circle.fill")
                        .foregroundStyle(theme.textMuted)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Clear search")
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 10)
        .background(.bar)
        .overlay(alignment: .top) {
            Divider()
        }
    }

    private var list: some View {
        List {
            if let error {
                Label(error, systemImage: "exclamationmark.triangle.fill")
                    .font(.caption)
                    .foregroundStyle(.orange)
                    .listRowBackground(Color.orange.opacity(0.12))
                    .accessibilityLabel("Error: \(error)")
            }
            if !items.isEmpty && filteredItems.isEmpty {
                Text("No items match the current filters.")
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
            }
            ForEach(filteredItems) { item in
                NavigationLink(value: item) {
                    IntelItemRow(item: item)
                }
            }
            if nextCursor != nil {
                HStack {
                    Spacer()
                    if loading { ProgressView() }
                    else {
                        Button("Load more") { Task { await loadMore() } }
                            .buttonStyle(.bordered)
                    }
                    Spacer()
                }
            }
        }
        .listStyle(.insetGrouped)
        .navigationDestination(for: IntelItem.self) { item in
            IntelDetail(uid: item.uid, fallbackTitle: item.title ?? item.uid)
        }
    }

    private var emptyState: some View {
        if let error {
            OfflineStateView(
                kind: .error,
                title: "Couldn’t load items.",
                message: error,
                systemImage: "exclamationmark.triangle",
                retry: { Task { await reload() } }
            )
        } else {
            OfflineStateView(
                kind: .empty,
                title: "No items yet.",
                message: nil,
                systemImage: "tray"
            )
        }
    }

    // ── Filtering ───────────────────────────────────────────────────────────

    private var filtersAreActive: Bool {
        !selectedLanguages.isEmpty || linkPresence != .all
    }

    /// Client-side narrowing of the loaded page. Free-text search stays
    /// server-side (`q`) so it spans the full source, not just this page.
    private var filteredItems: [IntelItem] {
        items.filter { item in
            if !selectedLanguages.isEmpty {
                guard let lang = item.language, selectedLanguages.contains(lang) else { return false }
            }
            switch linkPresence {
            case .all:     break
            case .hasLink: if (item.link?.isEmpty ?? true) { return false }
            case .noLink:  if !(item.link?.isEmpty ?? true) { return false }
            }
            return true
        }
    }

    private var availableLanguages: [String] {
        var counts: [String: Int] = [:]
        for i in items { if let l = i.language { counts[l, default: 0] += 1 } }
        return counts.keys.sorted {
            counts[$0]! == counts[$1]! ? $0 < $1 : counts[$0]! > counts[$1]!
        }
    }

    private func toggleLanguage(_ l: String) {
        if selectedLanguages.contains(l) { selectedLanguages.remove(l) }
        else { selectedLanguages.insert(l) }
    }

    private var filtersSheet: some View {
        FilterSheet(
            isActive: filtersAreActive,
            onReset: {
                selectedLanguages.removeAll()
                linkPresence = .all
            },
            onDone: { showFilters = false }
        ) {
            FilterSegmentedSection(title: "Link", selection: $linkPresence)
            FilterMultiSelectSection(
                title: "Language",
                items: availableLanguages,
                emptyText: "No language tags on this page",
                label: { $0.uppercased() },
                count: { l in items.filter { $0.language == l }.count },
                isSelected: { selectedLanguages.contains($0) },
                toggle: { toggleLanguage($0) }
            )
        }
    }

    // ── Data ───────────────────────────────────────────────────────────────

    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            let env = try await apiClient.api
                .intelItems(source: source.id, q: searchText.isEmpty ? nil : searchText, limit: 50, cursor: nil)
            items = env.data
            nextCursor = env.page?.next_cursor
            // Only mirror the unfiltered first page into the cache —
            // search-filtered results would poison the cache for non-search
            // re-opens.
            if searchText.isEmpty {
                intelCache.cacheItems(env.data, for: source.id)
            }
            error = nil
        } catch let err {
            error = err.localizedDescription
        }
    }

    private func runNow() async {
        running = true
        defer { running = false }
        do {
            _ = try await apiClient.api.intelRunSource(source.id)
            Haptics.success()
            await reload()
        } catch {
            // Keep existing items, but tell the user the run didn't kick off.
            self.error = "Run failed: \(error.localizedDescription)"
            Haptics.error()
        }
    }

    private func loadMore() async {
        guard let cursor = nextCursor, !loading else { return }
        loading = true
        defer { loading = false }
        do {
            let env = try await apiClient.api
                .intelItems(source: source.id, q: searchText.isEmpty ? nil : searchText, limit: 50, cursor: cursor)
            items.append(contentsOf: env.data)
            nextCursor = env.page?.next_cursor
            error = nil
        } catch {
            // Keep the items we have; surface why the next page didn't load.
            self.error = "Couldn’t load more: \(error.localizedDescription)"
            Haptics.error()
        }
    }
}
