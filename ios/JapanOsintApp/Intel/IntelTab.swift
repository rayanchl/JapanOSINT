import SwiftUI

/// Level-1 surface for non-spatial OSINT sources. The registry is ~61k
/// sources, so this lists ONE SERVER-SEARCHED PAGE at a time
/// (`/api/sources/search`, 50 rows, load more on scroll) — by default only
/// the sources that hold items, with a toggle for the rest. Each row has a Run
/// button for its collector. There is deliberately NO "run all": firing every
/// registered source from a phone is not an action anyone should be one tap
/// from. A search bar runs cross-source FTS via `/api/intel/items`.
struct IntelTab: View {
    @EnvironmentObject var apiClient: APIClient
    @EnvironmentObject var registry: LayerRegistry
    @EnvironmentObject var intelCache: IntelCache
    @Environment(\.theme) private var theme

    @State private var sources: [IntelSource] = []
    @State private var loading = false
    @State private var error: String?
    @State private var searchText = ""
    /// Filled by BilingualSearchModifier — carries the original query plus
    /// Apple's translated counterpart when auto-translate is on.
    @State private var bilingual: BilingualQuery = .empty
    @State private var page: SourcePageInfo?
    @State private var loadingMore = false
    /// Ids of entity pivots among the loaded rows (they cannot be run without
    /// an entity, so their Run button explains that instead of running).
    @State private var pivotIds: Set<String> = []
    @State private var categoryFacet: SourceFacetSet?
    /// Bumped per fetch so a stale answer is dropped.
    @State private var fetchToken = 0
    /// The filter signature the list was last loaded for. Starts at the
    /// default signature so the first appearance loads once, not twice.
    @State private var lastLoadedSignature = "|hasItems"

    @State private var showFilters = false
    @State private var selectedCategory: String?
    @State private var itemPresence: ItemPresence = .hasItems

    /// Which registered sources the catalogue lists. The default is the ones
    /// that hold items (what the tab is for); "All" pages through the whole
    /// registry, empty sources included.
    enum ItemPresence: String, FilterChoice {
        case hasItems, all
        var id: String { rawValue }
        var label: String {
            switch self {
            case .hasItems: return "Has items"
            case .all:      return "All sources"
            }
        }
    }

    var body: some View {
        NavigationStack {
            Group {
                if loading && sources.isEmpty {
                    ProgressView("Loading sources…")
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else if error != nil, sources.isEmpty {
                    OfflineStateView(retry: { Task { await reload() } })
                } else {
                    list
                }
            }
            .themedScreenBackground(theme)
            .navigationTitle("Intel")
            .searchable(
                text: $searchText,
                placement: .compatDrawer,
                prompt: "Search across all sources"
            )
            .modifier(BilingualSearchModifier(query: searchText, bilingual: $bilingual))
            .onSubmit(of: .search) {
                let q = searchText.trimmingCharacters(in: .whitespaces)
                Task { await apiClient.api.recordSearch(kind: "intel",
                    params: ["view": "search", "mode": "fts", "q": q]) }
            }
            .refreshable { await reload() }
            .toolbar {
                ToolbarItem(placement: .compatPrimary) {
                    FilterToolbarButton(isActive: filtersAreActive) {
                        showFilters = true
                    }
                }
            }
            .sheet(isPresented: $showFilters) { filtersSheet }
            // A filter change is a new server query.
            .task(id: filterSignature) {
                if lastLoadedSignature == filterSignature { return }
                await reload()
            }
        }
        .task {
            if sources.isEmpty {
                // Paint instantly from cache so the catalogue is on screen
                // before the network call resolves; refresh in-place once
                // the live response arrives.
                let cached = intelCache.cachedSources()
                if !cached.isEmpty { sources = cached }
                await reload()
            }
        }
    }

    // ── Subviews ────────────────────────────────────────────────────────────

    @ViewBuilder
    private var list: some View {
        if !searchText.isEmpty {
            CrossSourceSearchView(bilingual: bilingual)
        } else {
            catalogueList
        }
    }

    /// The catalogue list: the pages loaded so far, then a "showing N of M"
    /// footer that loads the next page as it scrolls into view.
    private var catalogueList: some View {
        List {
            if !sources.isEmpty && sources.allSatisfy({ $0.item_count == 0 }) {
                Section {
                    VStack(alignment: .leading, spacing: 4) {
                        Label("No items collected yet", systemImage: "info.circle")
                            .font(.caption.weight(.semibold))
                            .foregroundStyle(theme.textMuted)
                        Text("Tap the play button on a source to fetch its data.")
                            .font(.caption2)
                            .foregroundStyle(theme.textMuted)
                    }
                    .padding(.vertical, 4)
                }
            }
            Section {
                ForEach(sources) { src in
                    sourceRow(src)
                }
                if sources.isEmpty && !loading {
                    Text("No sources match the current filters.")
                        .font(.caption)
                        .foregroundStyle(theme.textMuted)
                }
                PagedFooter(shown: sources.count,
                            total: page?.total,
                            hasMore: page?.has_more == true,
                            loading: loadingMore,
                            error: nil,
                            noun: "sources",
                            loadMore: { Task { await loadMore() } })
            }
        }
        .compatInsetGroupedListStyle()
        .navigationDestination(for: IntelSource.self) { src in
            IntelSourceItemsView(source: src)
        }
    }

    /// One catalogue row. Tap-to-drill: NavigationLink wraps the row (with no
    /// visible chevron via opacity 0) — separate hit-target from the Run button
    /// which uses .buttonStyle(.plain).
    private func sourceRow(_ src: IntelSource) -> some View {
        ZStack {
            NavigationLink(value: src) { EmptyView() }.opacity(0)
            IntelSourceRow(source: src,
                           needsEntity: pivotIds.contains(src.id),
                           onRunComplete: {
                Task { await refreshRow(src.id) }
            })
        }
    }

    // ── Filtering (server-side) ─────────────────────────────────────────────

    private var filtersAreActive: Bool {
        selectedCategory != nil || itemPresence != .hasItems
    }

    /// Identity of the active filters; a change is a new server query.
    private var filterSignature: String {
        (selectedCategory ?? "") + "|" + itemPresence.rawValue
    }

    private var availableCategories: [String] {
        (categoryFacet?.values ?? []).compactMap { v in
            let k = v.keyText
            return k.isEmpty ? nil : k
        }
    }

    private func categoryCount(_ c: String) -> Int {
        (categoryFacet?.values ?? []).first(where: { $0.keyText == c })?.count ?? 0
    }

    private var filtersSheet: some View {
        FilterSheet(
            isActive: filtersAreActive,
            onReset: {
                selectedCategory = nil
                itemPresence = .hasItems
            },
            onDone: { showFilters = false }
        ) {
            FilterSegmentedSection(title: "Show", selection: $itemPresence)
            FilterMultiSelectSection(
                title: "Category",
                items: availableCategories,
                emptyText: "No categories loaded yet",
                label: { $0.capitalized },
                count: { categoryCount($0) },
                isSelected: { selectedCategory == $0 },
                toggle: { selectedCategory = (selectedCategory == $0) ? nil : $0 }
            )
        }
    }

    // ── Loading + actions ───────────────────────────────────────────────────

    private func currentQuery() -> SourceSearchQuery {
        var q = SourceSearchQuery()
        q.hasItems = (itemPresence == .hasItems)
        q.category = selectedCategory
        q.sort = "fetched"
        q.desc = true
        return q
    }

    /// First page of the current query. Only the rows on screen are cached.
    private func reload() async {
        loading = true
        fetchToken += 1
        let token = fetchToken
        lastLoadedSignature = filterSignature
        do {
            let env = try await apiClient.api.searchSources(currentQuery(), limit: 50, offset: 0)
            guard token == fetchToken else { return }
            let rows = env.data ?? []
            sources = rows.map(\.asIntelSource)
            pivotIds = Set(rows.filter(\.needsEntity).map(\.id))
            page = env.page
            if let f = env.meta?.facets?.categories { categoryFacet = f }
            intelCache.cacheSources(sources)
            error = nil
        } catch let err {
            guard token == fetchToken else { return }
            error = err.localizedDescription
            // Leave previously-rendered cached sources on screen so the
            // user isn't stranded by a flaky network.
        }
        if token == fetchToken { loading = false }
    }

    private func loadMore() async {
        guard !loadingMore, !loading, page?.has_more == true else { return }
        loadingMore = true
        defer { loadingMore = false }
        let token = fetchToken
        do {
            let env = try await apiClient.api.searchSources(currentQuery(), limit: 50, offset: sources.count)
            guard token == fetchToken else { return }
            let rows = env.data ?? []
            sources += rows.map(\.asIntelSource)
            pivotIds.formUnion(rows.filter(\.needsEntity).map(\.id))
            page = env.page
        } catch let err {
            error = err.localizedDescription
        }
    }

    /// After a manual run, refresh ONLY that row (and its cache entry) instead
    /// of re-downloading and re-caching the catalogue.
    private func refreshRow(_ id: String) async {
        var q = SourceSearchQuery()
        q.q = id
        guard let env = try? await apiClient.api.searchSources(q, limit: 20, offset: 0),
              let fresh = (env.data ?? []).first(where: { $0.id == id }),
              let idx = sources.firstIndex(where: { $0.id == id }) else { return }
        let updated = fresh.asIntelSource
        sources[idx] = updated
        intelCache.upsertSource(updated)
    }
}
