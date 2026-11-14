import SwiftUI
import Charts
import Combine

/// The Sources dashboard.
///
/// The registry is ~61k sources, so nothing here downloads it. It asks the
/// server for exactly what is on screen, the way the web client's Sources
/// page does:
///
///   · the tiles        — `GET /api/status?summary=1` (counters only)
///   · the collector tree — `GET /api/collectors` (a few hundred tallies)
///   · rows             — `GET /api/sources/search`, 50 at a time, with
///                        `has_more` driving "load more" on scroll
///   · search + filters — the same search route (`q`, `kind`, `status`,
///                        `health`, `category`), so a search covers the whole
///                        registry instead of whatever happened to be loaded
///
/// A bounded list always says how much it is showing out of how much exists.
struct SourceDashboardTab: View {
    @EnvironmentObject var apiClient: APIClient
    @EnvironmentObject var favs: CollectorFavorites
    @EnvironmentObject var nav: MapNavigation
    @Environment(\.theme) private var theme

    /// Rows per request, as on the web Sources page.
    static let pageSize = 50

    // Fleet counters and the collector tree.
    @State private var summary: StatusSummary?
    @State private var collectorList: [CollectorSummary] = []
    @State private var registryTotal: Int?
    @State private var overviewFacets: SourceFacets?
    @State private var loading = false
    @State private var errorMessage: String?

    @State private var search = ""
    @State private var selected: SourceRef?
    @State private var expanded: Set<String> = []
    /// Groups whose fold of single-source collectors is open (see collectorsList).
    @State private var singlesOpen: Set<String> = []
    @State private var refreshing: Set<String> = []
    @State private var refreshError: [String: String] = [:]

    // Rows of each expanded collector, paged on demand.
    @State private var collectorRows: [String: [SourceSearchRow]] = [:]
    @State private var collectorPages: [String: SourcePageInfo] = [:]
    @State private var collectorBusy: Set<String> = []
    @State private var collectorErrors: [String: String] = [:]

    // Flat results of a search or a filter, paged on demand.
    @State private var flatRows: [SourceSearchRow] = []
    @State private var flatPage: SourcePageInfo?
    @State private var flatLoading = false
    @State private var flatError: String?
    /// Bumped per fetch; a response whose token is stale is dropped, so a slow
    /// answer to an old query can never overwrite a newer one.
    @State private var flatToken = 0

    // Server-side filters (single choice each, from the registry's facets).
    @State private var showFilters = false
    @State private var kindFilter: String?
    @State private var statusFilter: String?
    @State private var healthFilter: String?
    @State private var categoryFilter: String?

    var body: some View {
        Group {
            if collectorList.isEmpty && summary == nil && !loading && errorMessage != nil {
                // Matches the Database tab's offline state exactly — full-
                // area `OfflineStateView` with no outline / no red banner.
                OfflineStateView(retry: { Task { await load() } })
            } else {
                ScrollView {
                    VStack(alignment: .leading, spacing: Space.lg - 2) {
                        if collectorList.isEmpty && !loading {
                            emptyState
                        }
                        if isFlat {
                            // Matching collectors first (a handful, filtered
                            // by name locally), then the server's source hits.
                            if isSearching { collectorsList }
                            flatResults
                            summaryCards
                        } else {
                            summaryCards
                            if summary != nil {
                                statusBreakdown
                                categoryChart
                                if hasBreaches { breachStrip }
                            }
                            collectorsList
                        }
                    }
                    .padding()
                }
                .refreshable { await load() }
            }
        }
        .themedScreenBackground(theme)
        .navigationTitle("Sources")
        .toolbar {
            ToolbarItem(placement: .compatPrimary) {
                FilterToolbarButton(isActive: filtersAreActive) {
                    showFilters = true
                }
            }
            ToolbarItem(placement: .compatPrimary) {
                Button { Task { await load() } } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .disabled(loading)
                .accessibilityLabel("Reload collectors")
            }
        }
        .searchable(text: $search, prompt: "Search sources and collectors")
        .sheet(isPresented: $showFilters) { filtersSheet }
        .task {
            if collectorList.isEmpty { await load() }
            // Cross-tab deep link: when Console pushed us in response to a
            // `showSource(_:)` call, the value was published before we mounted
            // — `.onReceive` would miss it. Consume the current value here.
            if let id = nav.pendingSourceId { openSource(id) }
        }
        // Search text and filters drive ONE debounced server query; changing
        // either cancels the previous one (`.task(id:)`), so keystrokes never
        // pile up requests.
        .task(id: flatKey) { await runFlatSearch() }
        .onReceive(nav.$pendingSourceId.compactMap { $0 }) { id in
            openSource(id)
        }
        .sheet(item: $selected) { ref in
            NavigationStack {
                SourceDetailView(sourceId: ref.id)
            }
            .compatSheetSizing(.medium)
        }
    }

    // MARK: - Summary cards

    private var summaryCards: some View {
        let s = summary
        let cols = [GridItem(.flexible(), spacing: 8), GridItem(.flexible(), spacing: 8)]
        return LazyVGrid(columns: cols, spacing: 8) {
            statCard("Total",    s?.total ?? registryTotal ?? 0, color: theme.accent,  icon: "square.stack.3d.up.fill")
            statCard("Online",   s?.online ?? 0,         color: theme.success, icon: "checkmark.circle.fill")
            statCard("Degraded", s?.degraded ?? 0,       color: theme.warning, icon: "exclamationmark.triangle.fill")
            statCard("Offline",  s?.offline ?? 0,        color: theme.danger,  icon: "xmark.octagon.fill")
            statCard("Gated",    s?.gated ?? 0,          color: theme.textMuted, icon: "lock.fill")
            statCard("Pending",  s?.pending ?? 0,        color: theme.textMuted, icon: "clock")
            if hasBreaches {
                statCard("Breaches", breachTotal, color: theme.danger,
                         icon: "lock.trianglebadge.exclamationmark")
            }
        }
    }

    // MARK: - Breach catalog
    //
    // The server inlines the breach catalog into /api/status for platform
    // operators only, so these are 0 for everyone else and the whole section
    // simply doesn't render. No client-side role check is needed — or wanted,
    // since the server is the actual gate. The catalog entries are not part of
    // the searchable registry; Admin → Breach corpus is where they are worked.

    private var breachTotal: Int { summary?.breachTotal ?? 0 }
    private var breachMaterialized: Int { summary?.breachMaterialized ?? 0 }
    private var hasBreaches: Bool { breachTotal > 0 }

    /// Breaches get their own compact strip rather than a bar in the category
    /// chart: at ~1k entries against a few hundred real collectors, one Breach
    /// bar would flatten every other category into invisibility.
    private var breachStrip: some View {
        let pending = max(0, breachTotal - breachMaterialized)
        let frac = breachTotal > 0 ? Double(breachMaterialized) / Double(breachTotal) : 0
        return VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 6) {
                Image(systemName: "lock.trianglebadge.exclamationmark")
                    .font(.caption).foregroundStyle(theme.danger)
                    .accessibilityHidden(true)   // "Breach corpus" follows
                Text("Breach corpus").font(.headline).foregroundStyle(theme.text)
                Spacer()
                Text("\(breachTotal)")
                    .font(.caption.monospacedDigit()).foregroundStyle(theme.textMuted)
            }
            GeometryReader { geo in
                HStack(spacing: 2) {
                    if breachMaterialized > 0 {
                        RoundedRectangle(cornerRadius: 2).fill(theme.danger)
                            .frame(width: max(2, geo.size.width * frac))
                    }
                    RoundedRectangle(cornerRadius: 2).fill(theme.textMuted.opacity(0.25))
                }
            }
            .frame(height: 8)
            HStack(spacing: Space.sm) {
                Pill(text: "\(breachMaterialized) with records", tone: .danger,
                     icon: "tray.full.fill")
                Pill(text: "\(pending) catalog only", tone: .neutral, icon: "clock")
            }
            Text("Catalog entries come from the local seed. Records arrive only after an operator ingest.")
                .font(.caption2).foregroundStyle(theme.textMuted)
        }
        .padding(12)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 12))
    }

    private var emptyState: some View {
        VStack(spacing: 8) {
            Image(systemName: "chart.pie")
                .font(.largeTitle)
                .foregroundStyle(theme.textMuted)
                .accessibilityHidden(true)   // decorative empty-state mark
            Text("No sources reported.")
                .font(.subheadline)
                .foregroundStyle(theme.text)
            Text("Backend reachable? Try Settings → Check connection.")
                .font(.caption)
                .foregroundStyle(theme.textMuted)
            Button("Retry") { Task { await load() } }
                .buttonStyle(.borderedProminent)
        }
        .frame(maxWidth: .infinity)
        .padding(20)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 12))
    }

    private func statCard(_ label: String, _ value: Int, color: Color, icon: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            HStack(spacing: Space.xs) {
                Image(systemName: icon)
                    .font(.caption2)
                    .foregroundStyle(color)
                    .accessibilityHidden(true)   // the label beside it says it
                Text(label).font(.caption2).foregroundStyle(theme.textMuted)
            }
            Text("\(value)")
                .font(.title3.bold().monospacedDigit())
                .foregroundStyle(color)
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(label): \(value)")
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(Space.md - 2)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.md))
    }

    // MARK: - Status breakdown (counts, not one dot per source)

    private var statusParts: [(label: String, count: Int, color: Color)] {
        let s = summary
        return [
            (label: "Online",   count: s?.online ?? 0,   color: theme.success),
            (label: "Degraded", count: s?.degraded ?? 0, color: theme.warning),
            (label: "Offline",  count: s?.offline ?? 0,  color: theme.danger),
            (label: "Gated",    count: s?.gated ?? 0,    color: theme.textMuted),
            (label: "Pending",  count: s?.pending ?? 0,  color: theme.textMuted.opacity(0.5)),
        ]
    }

    private var statusBreakdown: some View {
        // Bound ONCE: the parts array is rebuilt on every access.
        let parts = statusParts
        let total = max(1, parts.reduce(0) { $0 + $1.count })
        return VStack(alignment: .leading, spacing: 8) {
            Text("Status").font(.headline).foregroundStyle(theme.text)
            GeometryReader { geo in
                HStack(spacing: 2) {
                    ForEach(Array(parts.enumerated()), id: \.offset) { _, p in
                        if p.count > 0 {
                            RoundedRectangle(cornerRadius: 2).fill(p.color)
                                .frame(width: max(2, geo.size.width * CGFloat(p.count) / CGFloat(total)))
                        }
                    }
                }
            }
            .frame(height: 8)
            LazyVGrid(columns: [GridItem(.adaptive(minimum: 96), spacing: 6, alignment: .leading)],
                      alignment: .leading, spacing: 6) {
                ForEach(Array(parts.enumerated()), id: \.offset) { _, p in
                    HStack(spacing: 4) {
                        Circle().fill(p.color).frame(width: 8, height: 8)
                            .accessibilityHidden(true)
                        Text("\(p.label) \(p.count.formatted())")
                            .font(.caption2.monospacedDigit())
                            .foregroundStyle(theme.textMuted)
                    }
                }
            }
            Text("Counts for the whole registry, from the server. Search or filter to see the sources themselves.")
                .font(.caption2).foregroundStyle(theme.textMuted)
        }
        .padding(12)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 12))
    }

    // MARK: - Category chart (the registry's own facet, top 12)

    private struct CategoryBucket: Identifiable {
        let name: String
        let count: Int
        var id: String { name }
    }

    private var categoryBuckets: [CategoryBucket] {
        let values = overviewFacets?.categories?.values ?? []
        var out: [CategoryBucket] = []
        for v in values {
            let name = v.keyText
            // Breaches are excluded deliberately — see `breachStrip`.
            if name.isEmpty || name == "breach" { continue }
            out.append(CategoryBucket(name: name, count: v.count ?? 0))
            if out.count == 12 { break }
        }
        return out
    }

    @ViewBuilder
    private var categoryChart: some View {
        // Bound once — read twice per body pass otherwise.
        let buckets = categoryBuckets
        if !buckets.isEmpty {
            VStack(alignment: .leading, spacing: 6) {
                Text("Sources by category (top 12)").font(.headline).foregroundStyle(theme.text)
                Chart(buckets) { b in
                    BarMark(
                        x: .value("Sources", b.count),
                        y: .value("Category", b.name)
                    )
                    .foregroundStyle(theme.accent)
                }
                .frame(height: CGFloat(max(140, buckets.count * 18)))
            }
            .padding(12)
            .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 12))
        }
    }

    // MARK: - Search + filters (all on the server)

    private var trimmedSearch: String { search.trimmingCharacters(in: .whitespaces) }
    private var isSearching: Bool { !trimmedSearch.isEmpty }

    private var filtersAreActive: Bool {
        kindFilter != nil || statusFilter != nil || healthFilter != nil || categoryFilter != nil
    }

    /// A search or any filter switches the screen from the collector tree to
    /// the server's flat, paged result list.
    private var isFlat: Bool { isSearching || filtersAreActive }

    private var flatQuery: SourceSearchQuery {
        var q = SourceSearchQuery()
        q.q = trimmedSearch
        q.kind = kindFilter
        q.status = statusFilter
        q.health = healthFilter
        q.category = categoryFilter
        return q
    }

    /// Identity of the current query; `.task(id:)` re-runs when it changes.
    private var flatKey: String {
        [trimmedSearch, kindFilter ?? "", statusFilter ?? "",
         healthFilter ?? "", categoryFilter ?? ""].joined(separator: "|")
    }

    private func runFlatSearch() async {
        guard isFlat else {
            flatToken += 1          // drop any answer still in flight
            flatRows = []
            flatPage = nil
            flatError = nil
            flatLoading = false
            return
        }
        // Debounce: a keystroke cancels this task before the request is sent.
        do { try await Task.sleep(nanoseconds: 300_000_000) } catch { return }
        await fetchFlat(reset: true)
    }

    private func fetchFlat(reset: Bool) async {
        let api = apiClient.api
        let query = flatQuery
        let offset = reset ? 0 : flatRows.count
        flatToken += 1
        let token = flatToken
        flatLoading = true
        do {
            let env = try await api.searchSources(query, limit: Self.pageSize, offset: offset)
            guard token == flatToken else { return }
            let rows = env.data ?? []
            flatRows = reset ? rows : flatRows + rows
            flatPage = env.page
            flatError = nil
        } catch {
            guard token == flatToken else { return }
            flatError = error.localizedDescription
        }
        flatLoading = false
    }

    private var flatResults: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text(isSearching ? "Matching sources" : "Filtered sources")
                    .font(.headline)
                    .foregroundStyle(theme.text)
                Spacer()
                if let t = flatPage?.total {
                    Text("\(t.formatted()) match")
                        .font(.caption.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
            }
            if flatRows.isEmpty && !flatLoading && flatError == nil && flatPage != nil {
                Text("No source matches. The server searched the whole registry.")
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
                    .padding(.vertical, 8)
            }
            LazyVStack(spacing: 1) {
                ForEach(flatRows) { src in
                    SourceRow(row: src) { selected = SourceRef(id: src.id) }
                }
            }
            .clipShape(RoundedRectangle(cornerRadius: 10))
            PagedFooter(shown: flatRows.count,
                        total: flatPage?.total,
                        hasMore: flatPage?.has_more == true,
                        loading: flatLoading,
                        error: flatError,
                        noun: "sources",
                        loadMore: { Task { await fetchFlat(reset: false) } })
        }
    }

    // Facet helpers for the filter sheet. They read the registry-wide facets
    // from the unfiltered overview, so every option stays selectable.

    private func facetItems(_ set: SourceFacetSet?) -> [String] {
        (set?.values ?? []).compactMap { v in
            let k = v.keyText
            return k.isEmpty ? nil : k
        }
    }

    private func facetCount(_ set: SourceFacetSet?, _ key: String) -> Int {
        (set?.values ?? []).first(where: { $0.keyText == key })?.count ?? 0
    }

    private func choose(_ current: String?, _ v: String) -> String? {
        current == v ? nil : v
    }

    private var filtersSheet: some View {
        let f = overviewFacets
        return FilterSheet(
            isActive: filtersAreActive,
            onReset: {
                kindFilter = nil
                statusFilter = nil
                healthFilter = nil
                categoryFilter = nil
            },
            onDone: { showFilters = false }
        ) {
            FilterMultiSelectSection(
                title: "Kind",
                items: facetItems(f?.kinds),
                emptyText: "No facets loaded yet",
                label: { $0 },
                count: { facetCount(f?.kinds, $0) },
                isSelected: { kindFilter == $0 },
                toggle: { kindFilter = choose(kindFilter, $0) }
            )
            FilterMultiSelectSection(
                title: "Health",
                items: facetItems(f?.healths),
                emptyText: "No facets loaded yet",
                label: { $0 },
                count: { facetCount(f?.healths, $0) },
                isSelected: { healthFilter == $0 },
                toggle: { healthFilter = choose(healthFilter, $0) }
            )
            FilterMultiSelectSection(
                title: "Last run status",
                items: facetItems(f?.statuses),
                emptyText: "No facets loaded yet",
                label: { $0 },
                count: { facetCount(f?.statuses, $0) },
                isSelected: { statusFilter == $0 },
                toggle: { statusFilter = choose(statusFilter, $0) }
            )
            FilterMultiSelectSection(
                title: "Category",
                items: facetItems(f?.categories),
                emptyText: "No facets loaded yet",
                label: { $0.capitalized },
                count: { facetCount(f?.categories, $0) },
                isSelected: { categoryFilter == $0 },
                toggle: { categoryFilter = choose(categoryFilter, $0) }
            )
        }
    }

    // MARK: - Collectors list (from /api/collectors)

    /// Collectors matching the search box by name, key or description. The
    /// tree is a few hundred rows, so this is cheap; the SOURCES inside a
    /// collector are never held until it is opened.
    private var collectors: [CollectorSummary] {
        let s = trimmedSearch.lowercased()
        let all = collectorList.filter { $0.is_group != true }
        let filtered = s.isEmpty ? all : all.filter { c in
            c.key.lowercased().contains(s)
                || (c.label ?? "").lowercased().contains(s)
                || (c.description ?? "").lowercased().contains(s)
        }
        return filtered.sorted { lhs, rhs in
            let lf = favs.contains(lhs.id)
            let rf = favs.contains(rhs.id)
            if lf != rf { return lf && !rf }   // favs first
            return lhs.displayName.lowercased() < rhs.displayName.lowercased()
        }
    }

    /// Collectors sectioned by their server-side GROUP (collectors.def
    /// headings), groups ordered by source count, each group's collectors in
    /// the `collectors` order (favourites first, then name). Every collector
    /// is in exactly one section; one the server could not place sits under
    /// "Unassigned" rather than being dropped.
    private struct CollectorGroupSection: Identifiable {
        let id: String
        let label: String
        let collectors: [CollectorSummary]
        var sourceCount: Int { collectors.reduce(0) { $0 + $1.sourceCount } }
    }
    private func sections(of items: [CollectorSummary]) -> [CollectorGroupSection] {
        var byGroup: [String: [CollectorSummary]] = [:]
        var labels: [String: String] = [:]
        for c in items {
            let g = c.group ?? "grp_unassigned"
            byGroup[g, default: []].append(c)
            labels[g] = c.group_label ?? g
        }
        return byGroup.map { CollectorGroupSection(id: $0.key, label: labels[$0.key] ?? $0.key, collectors: $0.value) }
            .sorted { lhs, rhs in
                if lhs.sourceCount != rhs.sourceCount { return lhs.sourceCount > rhs.sourceCount }
                return lhs.label < rhs.label
            }
    }

    private var collectorsList: some View {
        // Bound once: filtering, grouping and sorting the tree per read.
        let items = collectors
        let secs = sections(of: items)
        return VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text("Collectors (\(items.count)) in \(secs.count) groups")
                    .font(.headline.monospacedDigit())
                    .foregroundStyle(theme.text)
                Spacer()
                Text("\((registryTotal ?? 0).formatted()) sources")
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
            }
            VStack(spacing: 8) {
                ForEach(secs) { section in
                  HStack {
                    Text(section.label)
                        .font(.subheadline.bold())
                        .foregroundStyle(section.id == "grp_unassigned" ? theme.danger : theme.text)
                    Spacer()
                    Text("\(section.collectors.count) collector\(section.collectors.count == 1 ? "" : "s") · \(section.sourceCount.formatted()) sources")
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                  }
                  .padding(.top, 6)
                  .accessibilityAddTraits(.isHeader)
                  // Collectors holding exactly ONE source (the per-country
                  // hand-written beats) fold into one row per group, so a group
                  // with 26 of them reads as a list rather than 26 rows. Nothing
                  // is dropped: the fold says how many it holds and opens in place.
                  let multi = section.collectors.filter { $0.sourceCount != 1 }
                  let singles = section.collectors.filter { $0.sourceCount == 1 }
                  ForEach(multi) { c in
                    collectorRow(c)
                  }
                  if !singles.isEmpty {
                    Button {
                        if singlesOpen.contains(section.id) { singlesOpen.remove(section.id) } else { singlesOpen.insert(section.id) }
                    } label: {
                        HStack {
                            Image(systemName: singlesOpen.contains(section.id) ? "chevron.down" : "chevron.right")
                                .font(.caption2)
                            Text("\(singles.count) single-source collector\(singles.count == 1 ? "" : "s")")
                                .font(.caption.monospacedDigit())
                            Spacer()
                        }
                        .foregroundStyle(theme.textMuted)
                        .padding(.vertical, 4)
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel("\(singles.count) single-source collectors in \(section.label)")
                    if singlesOpen.contains(section.id) {
                        ForEach(singles) { c in
                            collectorRow(c)
                        }
                    }
                  }
                }
            }
        }
    }

    private func collectorRow(_ c: CollectorSummary) -> some View {
        CollectorRow(
            collector: c,
            rows: collectorRows[c.id] ?? [],
            page: collectorPages[c.id],
            isFavorite: favs.contains(c.id),
            isExpanded: expanded.contains(c.id),
            isLoadingRows: collectorBusy.contains(c.id),
            rowsError: collectorErrors[c.id],
            isRefreshing: refreshing.contains(c.id),
            refreshError: refreshError[c.id],
            toggleFavorite: { favs.toggle(c.id) },
            toggleExpand: { toggleExpand(c.id) },
            loadMore: { Task { await loadCollectorRows(c.id, reset: false) } },
            refresh: { Task { await refresh(c.id) } },
            selectSource: { selected = SourceRef(id: $0.id) }
        )
    }

    /// Cross-tab nav target: open the source detail for `id`. The sheet loads
    /// the source itself, so nothing has to be searched for first.
    private func openSource(_ id: String) {
        selected = SourceRef(id: id)
        nav.pendingSourceId = nil
    }

    private func toggleExpand(_ id: String) {
        if expanded.contains(id) {
            expanded.remove(id)
        } else {
            expanded.insert(id)
            if collectorRows[id] == nil {
                Task { await loadCollectorRows(id, reset: true) }
            }
        }
    }

    /// One page of one collector's sources, from the same search route (the
    /// `collector` filter). `reset` starts again from the first page.
    private func loadCollectorRows(_ key: String, reset: Bool) async {
        if collectorBusy.contains(key) { return }
        if !reset, collectorPages[key]?.has_more != true { return }
        collectorBusy.insert(key)
        defer { collectorBusy.remove(key) }
        let have = reset ? 0 : (collectorRows[key]?.count ?? 0)
        var q = SourceSearchQuery()
        q.collector = key
        do {
            let env = try await apiClient.api.searchSources(q, limit: Self.pageSize, offset: have)
            let rows = env.data ?? []
            collectorRows[key] = reset ? rows : (collectorRows[key] ?? []) + rows
            collectorPages[key] = env.page
            collectorErrors.removeValue(forKey: key)
        } catch {
            collectorErrors[key] = error.localizedDescription
        }
    }

    /// Runs the sources of ONE collector, one at a time, through the same
    /// POST /api/intel/sources/:id/run the web client uses. Bounded to
    /// `runAllMax` sources: a 500-source beat is not something to fire from a
    /// phone, and the button says so instead of silently running the first N.
    /// Entity pivots are skipped (they need an entity; the server would
    /// answer 422), and the result says how many.
    static let runAllMax = 12
    private func refresh(_ collectorId: String) async {
        guard let c = collectorList.first(where: { $0.id == collectorId }),
              c.sourceCount <= Self.runAllMax else { return }
        refreshing.insert(collectorId)
        refreshError.removeValue(forKey: collectorId)
        defer { refreshing.remove(collectorId) }

        var q = SourceSearchQuery()
        q.collector = collectorId
        var failures: [String] = []
        do {
            let env = try await apiClient.api.searchSources(q, limit: Self.runAllMax, offset: 0)
            var skipped = 0
            for row in env.data ?? [] {
                if row.category == "breach" || row.isInternal { continue }
                if row.needsEntity { skipped += 1; continue }
                do { _ = try await apiClient.api.intelRunSource(row.id) }
                catch { failures.append("\(row.id): \(API.runFailureMessage(error))") }
            }
            if skipped > 0 {
                failures.append("\(skipped) entity pivot\(skipped == 1 ? "" : "s") skipped: open one to run it with an entity")
            }
        } catch {
            failures.append(error.localizedDescription)
        }
        if !failures.isEmpty { refreshError[collectorId] = failures.joined(separator: "\n") }
        await load()
    }

    // MARK: - Loading

    private func load() async {
        let api = apiClient.api
        loading = true
        defer { loading = false }
        async let summaryTask = api.statusSummary()
        async let treeTask = api.collectorTree()
        async let facetTask = api.searchSources(SourceSearchQuery(), limit: 1)
        do {
            let sumEnv = try await summaryTask
            let treeEnv = try await treeTask
            summary = sumEnv.summary
            collectorList = treeEnv.data ?? []
            registryTotal = treeEnv.meta?.registry_total ?? treeEnv.meta?.sources
            errorMessage = nil
        } catch {
            errorMessage = error.localizedDescription
        }
        // The facets only decorate (category chart, filter options); a
        // failure there must not hide the tree.
        if let f = try? await facetTask {
            overviewFacets = f.meta?.facets
            if registryTotal == nil { registryTotal = f.meta?.registry_total }
        }
        // Open collectors re-read their first page so counts and health are
        // current after a reload or a run.
        for key in expanded { await loadCollectorRows(key, reset: true) }
        if isFlat { await fetchFlat(reset: true) }
    }
}

// ── A bounded list's footer ────────────────────────────────────────────────

/// "Showing N of M" plus load-more for any server-paged list. A list that is
/// not showing everything must say how much it is showing out of how much
/// exists; the footer also loads the next page as it scrolls into view.
struct PagedFooter: View {
    let shown: Int
    let total: Int?
    let hasMore: Bool
    let loading: Bool
    let error: String?
    let noun: String
    let loadMore: () -> Void

    @Environment(\.theme) private var theme

    private var countLine: String {
        if let total {
            return "Showing \(shown.formatted()) of \(total.formatted()) \(noun)"
        }
        return "Showing \(shown.formatted()) \(noun) (total not reported)"
    }

    var body: some View {
        VStack(spacing: 6) {
            if shown > 0 || hasMore {
                Text(countLine)
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
            }
            if loading {
                ProgressView().controlSize(.small)
            }
            if let error {
                Text(error)
                    .font(.caption2)
                    .foregroundStyle(theme.danger)
                    .multilineTextAlignment(.center)
                Button("Retry", action: loadMore)
                    .buttonStyle(.bordered)
                    .controlSize(.small)
            } else if hasMore && !loading {
                Button("Load more", action: loadMore)
                    .buttonStyle(.bordered)
                    .controlSize(.small)
            }
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 6)
        // Scrolling the footer into view asks for the next page.
        .onAppear {
            if hasMore && !loading && error == nil { loadMore() }
        }
    }
}

struct CollectorRow: View {
    let collector: CollectorSummary
    let rows: [SourceSearchRow]
    let page: SourcePageInfo?
    let isFavorite: Bool
    let isExpanded: Bool
    let isLoadingRows: Bool
    let rowsError: String?
    let isRefreshing: Bool
    let refreshError: String?
    let toggleFavorite: () -> Void
    let toggleExpand: () -> Void
    let loadMore: () -> Void
    let refresh: () -> Void
    let selectSource: (SourceSearchRow) -> Void

    @Environment(\.theme) private var theme

    var body: some View {
        VStack(spacing: 0) {
            header
            if isExpanded {
                expandedBody
            }
        }
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 10))
    }

    private var header: some View {
        Button(action: toggleExpand) {
            HStack(spacing: 10) {
                Button(action: toggleFavorite) {
                    Image(systemName: isFavorite ? "star.fill" : "star")
                        .foregroundStyle(isFavorite ? theme.warning : theme.textMuted)
                }
                .buttonStyle(.plain)
                .accessibilityLabel(isFavorite ? "Unfavorite" : "Favorite")

                Circle().fill(statusColor)
                    .frame(width: 10, height: 10)

                VStack(alignment: .leading, spacing: 1) {
                    Text(collector.displayName)
                        .font(.subheadline.bold())
                        .foregroundStyle(theme.text)
                        .lineLimit(1)
                    HStack(spacing: Space.sm - 2) {
                        Text("\(collector.sourceCount.formatted()) source\(collector.sourceCount == 1 ? "" : "s")")
                            .font(.caption2.monospacedDigit())
                            .foregroundStyle(theme.textMuted)
                        if let cat = collector.topCategory {
                            Text("· \(cat)").font(.caption2).foregroundStyle(theme.textMuted)
                        }
                    }
                }
                Spacer()
                healthChips
                Image(systemName: isExpanded ? "chevron.up" : "chevron.down")
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
                    .accessibilityHidden(true)   // state is the button's value
            }
            .padding(10)
        }
        .buttonStyle(.plain)
        .accessibilityLabel(collector.displayName)
        .accessibilityValue(isExpanded ? "Expanded" : "Collapsed")
        .accessibilityHint("Double tap to \(isExpanded ? "collapse" : "expand") this collector")
    }

    private var healthChips: some View {
        HStack(spacing: Space.xs) {
            if collector.onlineCount > 0 {
                Pill(text: "\(collector.onlineCount)", tone: .success,
                     icon: "checkmark.circle.fill", solid: true)
            }
            if collector.degradedCount > 0 {
                Pill(text: "\(collector.degradedCount)", tone: .warning,
                     icon: "exclamationmark.triangle.fill", solid: true)
            }
            if collector.offlineCount > 0 {
                Pill(text: "\(collector.offlineCount)", tone: .danger,
                     icon: "xmark.octagon.fill", solid: true)
            }
        }
    }

    /// `LazyVStack`, not `VStack`: a large collector holds thousands of
    /// sources, so only the pages the user has scrolled to are materialised,
    /// and each next page is requested as the footer scrolls into view.
    private var expandedBody: some View {
        LazyVStack(spacing: 1) {
            ForEach(rows) { src in
                SourceRow(row: src) { selectSource(src) }
            }
            PagedFooter(shown: rows.count,
                        total: page?.total ?? collector.sourceCount,
                        hasMore: page?.has_more == true,
                        loading: isLoadingRows,
                        error: rowsError,
                        noun: "sources",
                        loadMore: loadMore)
            refreshFooter
        }
        .padding(.horizontal, 6)
        .padding(.bottom, 6)
    }

    private var refreshFooter: some View {
        VStack(spacing: 4) {
            if let err = refreshError {
                Text(err)
                    .font(.caption2)
                    .foregroundStyle(theme.danger)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.horizontal, 8)
            }
            HStack {
                Spacer()
                Button(action: refresh) {
                    HStack(spacing: 6) {
                        if isRefreshing {
                            ProgressView().scaleEffect(0.7)
                        } else {
                            Image(systemName: "arrow.clockwise")
                                .accessibilityHidden(true)   // button title follows
                        }
                        Text(runLabel)
                    }
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.small)
                .disabled(isRefreshing || collector.sourceCount > SourceDashboardTab.runAllMax)
                Spacer()
            }
            .padding(.horizontal, 8)
            .padding(.top, 4)
            if collector.pivotCount > 0 && collector.sourceCount <= SourceDashboardTab.runAllMax {
                Text("Entity pivots are skipped: open one to run it with an entity.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
            }
        }
    }

    private var runLabel: String {
        if isRefreshing { return "Running…" }
        if collector.sourceCount > SourceDashboardTab.runAllMax {
            return "Run all: too many (\(collector.sourceCount) > \(SourceDashboardTab.runAllMax)) — run one from its sheet"
        }
        return "Run all \(collector.sourceCount) source\(collector.sourceCount == 1 ? "" : "s") now"
    }

    private var statusColor: Color {
        switch collector.aggregateStatus {
        case .online:   return theme.success
        case .degraded: return theme.warning
        case .offline:  return theme.danger
        default:        return theme.textMuted
        }
    }
}

extension CollectorSummary {
    /// Sources in a collector that are entity pivots (the "run all" skips them).
    var pivotCount: Int { pivots ?? 0 }
}

struct SourceRow: View {
    let row: SourceSearchRow
    let onTap: () -> Void
    @Environment(\.theme) private var theme

    var body: some View {
        Button(action: onTap) {
            HStack(spacing: 8) {
                Circle().fill(statusColor)
                    .frame(width: 10, height: 10)
                VStack(alignment: .leading, spacing: 2) {
                    Text(row.displayName).font(.subheadline).lineLimit(1)
                        .foregroundStyle(theme.text)
                    HStack(spacing: 6) {
                        if let c = row.category {
                            Text(c).font(.caption2).foregroundStyle(theme.textMuted)
                        }
                        if let k = row.kind {
                            Text("· \(k)").font(.caption2).foregroundStyle(theme.textMuted)
                        }
                        if let n = row.item_count, n > 0 {
                            Text("· \(n.formatted()) items").font(.caption2.monospacedDigit())
                                .foregroundStyle(theme.textMuted)
                        }
                    }
                    if let note = reasonLine {
                        Text(note)
                            .font(.caption2)
                            .foregroundStyle(row.healthState == "gated" ? theme.warning : theme.textMuted)
                            .lineLimit(2)
                    }
                }
                Spacer()
                if let ms = row.response_time_ms, row.healthState != "gated" {
                    Text("\(Int(ms)) ms")
                        .font(.system(.caption2, design: .monospaced))
                        .foregroundStyle(theme.textMuted)
                }
            }
            .padding(8)
            .background(theme.surfaceElevated)
        }
        .buttonStyle(.plain)
    }

    /// The server's own reason a source is not healthy (a missing credential,
    /// a refused fetch), shown under the name; nothing is composed here.
    private var reasonLine: String? {
        guard row.healthState != "ok", row.healthState != "pending" else { return nil }
        if let creds = row.health?.credentials_required, !creds.isEmpty {
            return "needs " + creds.joined(separator: ", ")
        }
        if let note = row.health?.note, !note.isEmpty { return note }
        return nil
    }

    private var statusColor: Color {
        switch row.statusKind {
        case .online:   return theme.success
        case .degraded: return theme.warning
        case .offline:  return theme.danger
        default:        return theme.textMuted
        }
    }
}
