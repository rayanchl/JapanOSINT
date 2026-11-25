import SwiftUI

/// Level-2 surface: paginated items for a single source.
struct IntelSourceItemsView: View {
    let source: IntelSource

    @EnvironmentObject var apiClient: APIClient
    @EnvironmentObject var intelCache: IntelCache
    @Environment(\.theme) private var theme

    @State private var items: [IntelItem] = []
    @State private var nextCursor: String?
    /// The server's own count of items matching the current server-side
    /// filters (`page.total` / `page.total_gte` from `?total=1`): "10,000+"
    /// when it hit the server's cap, nil when the count was not returned.
    @State private var totalLabel: String?
    @State private var loading = false
    @State private var error: String?
    @State private var searchText = ""
    @State private var running = false
    /// The search text the list was last loaded for, and a counter that lets a
    /// slow answer to an OLD query be dropped instead of overwriting a newer one.
    @State private var lastLoadedSearch = ""
    @State private var reloadToken = 0

    @State private var showFilters = false
    @State private var selectedLanguages: Set<String> = []
    @State private var linkPresence: LinkPresence = .all
    /// `?collapse=1` (roadmap 25). Folded rows are NOT discarded — the survivor
    /// carries the whole cluster and `ClusterBadge` renders it, so turning this
    /// on trades a list of near-identical rows for an explicit corroboration
    /// count you can expand.
    @State private var collapseDuplicates = false
    /// `?lang_view=<code>` (roadmap 29). Empty = off.
    @State private var langView = ""
    @State private var showExport = false
    /// A duplicate tapped inside a `ClusterBadge`, pushed as its own detail.
    @State private var openDuplicateUID: String?

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
        .themedScreenBackground(theme)
        .navigationTitle(source.name)
        .compatInlineTitle()
        .toolbar {
            ToolbarItem(placement: .compatPrimary) {
                Button {
                    Task { await runNow() }
                } label: {
                    if running { ProgressView().controlSize(.mini) }
                    else       { Image(systemName: "play.fill") }
                }
                .disabled(running)
                .accessibilityLabel(running ? "Running…" : "Run \(source.name)")
            }
            ToolbarItem(placement: .compatPrimary) {
                FilterToolbarButton(isActive: filtersAreActive) {
                    showFilters = true
                }
            }
            ToolbarItem(placement: .compatPrimary) {
                Button { showExport = true } label: {
                    Image(systemName: "square.and.arrow.up")
                }
                .accessibilityLabel("Export these items")
            }
        }
        .sheet(isPresented: $showFilters) { filtersSheet }
        // The export runs the SAME filters this screen is showing, so what you
        // get is what you were looking at.
        .sheet(isPresented: $showExport) {
            ExportSheet(
                kind: exportKind,
                filters: serverFilters,
                contextLabel: "\(source.name) · \(source.id)"
                    + (hasLocalOnlyFilters
                       ? " — note: the language / link filters on this screen are applied on-device and are NOT part of the export."
                       : ""))
        }
        // Hide the tab bar while a source is open — the top nav back button
        // is the only "go back" we want; the tab bar at the bottom doubles
        // as another navigation affordance and competes visually.
        .compatHideTabBar()
        // Search lives in a bottom inset rather than the nav-bar drawer so
        // that pushing into this view doesn't have to animate the parent's
        // searchable away — the top bar stays clean and snappy.
        .safeAreaInset(edge: .bottom) { searchFooter }
        // One debounced reload per search text. `.task(id:)` cancels the
        // previous task when the text changes, so a burst of keystrokes sends
        // one request, not one per keystroke (the old Task-per-keystroke let an
        // older response overwrite a newer one).
        .task(id: searchText) {
            if searchText == lastLoadedSearch { return }
            do { try await Task.sleep(for: .milliseconds(250)) } catch { return }
            await reload()
        }
        // Both are server-side post-passes, so flipping either has to refetch —
        // otherwise the toggle reads as "on" over a list that never asked for it.
        .onChange(of: collapseDuplicates) { _, _ in Task { await reload() } }
        .onChange(of: langView) { _, _ in Task { await reload() } }
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
                .onSubmit {
                    let q = searchText.trimmingCharacters(in: .whitespaces)
                    Task { await apiClient.api.recordSearch(kind: "intel",
                        params: ["view": "source", "source": source.id, "q": q]) }
                }
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
            // Rule 2: say how much of what exists this list shows.
            if let totalLabel {
                Text(shownOfTotalText(total: totalLabel))
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
            }
            if !items.isEmpty && filteredItems.isEmpty {
                Text("No items match the current filters.")
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
            }
            ForEach(filteredItems) { item in
                VStack(alignment: .leading, spacing: Space.sm) {
                    NavigationLink(value: item) {
                        IntelItemRow(item: item)
                    }
                    // Roadmap 25. Only present when the row actually carries a
                    // cluster, so an uncollapsed list is unchanged.
                    if let cluster = item.cluster {
                        ClusterBadge(cluster: cluster,
                                     onOpenDuplicate: { dup in
                                         if let uid = dup.uid { openDuplicateUID = uid }
                                     })
                    }
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
        .compatInsetGroupedListStyle()
        .navigationDestination(for: IntelItem.self) { item in
            // The list row is handed through: the detail endpoint has no
            // `collapse` / `lang_view` post-pass, so the cluster and the
            // translation only exist on THIS copy of the item. Dropping it
            // here is exactly how those two features stayed invisible.
            IntelDetail(uid: item.uid,
                        fallbackTitle: item.title ?? item.uid,
                        listItem: item)
        }
        .navigationDestination(item: $openDuplicateUID) { uid in
            IntelDetail(uid: uid, fallbackTitle: uid)
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
            || collapseDuplicates || !langView.isEmpty
    }

    /// True when the screen is narrowed by something the SERVER never sees, so
    /// the export can say plainly that those narrowings are not in the file.
    private var hasLocalOnlyFilters: Bool {
        !selectedLanguages.isEmpty || linkPresence != .all
    }

    /// Breach records list through the ordinary intel endpoint but export
    /// under their own kind (and the exporter requires the source id, which
    /// this screen always has).
    private var exportKind: String {
        source.category == "breach" ? "breach" : "intel"
    }

    /// The filters the server itself applied to this list, forwarded verbatim.
    private var serverFilters: [URLQueryItem] {
        var q = [URLQueryItem(name: "source", value: source.id)]
        let t = searchText.trimmingCharacters(in: .whitespaces)
        if !t.isEmpty { q.append(URLQueryItem(name: "q", value: t)) }
        return q
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
                collapseDuplicates = false
                langView = ""
            },
            onDone: { showFilters = false }
        ) {
            Section {
                Toggle("Group near-duplicates", isOn: $collapseDuplicates)
                Toggle("Translate to English",
                       isOn: Binding(get: { langView == "en" },
                                     set: { langView = $0 ? "en" : "" }))
            } header: {
                Text("Server view")
            } footer: {
                Text("Grouping folds near-identical reports into one row that says how many sources filed it — nothing is dropped, the folded reports are listed under the badge. Translation is machine output and is always labelled as such.")
                    .font(.caption2)
            }
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
        reloadToken += 1
        let token = reloadToken
        let queried = searchText
        lastLoadedSearch = queried
        defer { if token == reloadToken { loading = false } }
        do {
            let env = try await apiClient.api
                .intelItems(source: source.id, q: queried.isEmpty ? nil : queried,
                            limit: 50, cursor: nil,
                            wantTotal: true,
                            collapse: collapseDuplicates,
                            langView: langView.isEmpty ? nil : langView)
            guard token == reloadToken else { return }   // a newer query is in flight
            items = env.data
            nextCursor = env.page?.next_cursor
            totalLabel = env.page?.totalLabel
            // Only mirror the unfiltered first page into the cache —
            // search-filtered results would poison the cache for non-search
            // re-opens. A collapsed or translated page is a VIEW of the corpus,
            // not the corpus, so it is not cached either: replaying it as the
            // plain list would silently hide the folded reports.
            if searchText.isEmpty && !collapseDuplicates && langView.isEmpty {
                intelCache.cacheItems(env.data, for: source.id)
            }
            error = nil
        } catch let err {
            error = err.localizedDescription
        }
    }

    /// "Showing 50 of 1,234 items" (the count is the server's, over the same
    /// server-side filters; the on-device language / link filters and a
    /// collapsed view narrow what is listed further, and that is said too).
    private func shownOfTotalText(total: String) -> String {
        var t = "Showing \(items.count.formatted()) of \(total) items"
        if filteredItems.count != items.count {
            t += " (\(filteredItems.count.formatted()) after the on-device filters)"
        }
        if collapseDuplicates { t += ". Near-duplicates are folded into one row; the total counts them." }
        return t
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
            self.error = "Run: \(API.runFailureMessage(error))"
            Haptics.error()
        }
    }

    private func loadMore() async {
        guard let cursor = nextCursor, !loading else { return }
        loading = true
        defer { loading = false }
        do {
            let env = try await apiClient.api
                .intelItems(source: source.id, q: searchText.isEmpty ? nil : searchText,
                            limit: 50, cursor: cursor,
                            collapse: collapseDuplicates,
                            langView: langView.isEmpty ? nil : langView)
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
