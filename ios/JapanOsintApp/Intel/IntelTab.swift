import SwiftUI

/// Level-1 surface for non-spatial OSINT sources. Always lists every
/// catalogued `kind:'intel'` source — even ones with zero ingested items.
/// Each row has a Run button to trigger its collector on demand. A toolbar
/// "Run all" action fires every source in parallel (capped). A search bar
/// runs cross-source FTS via `/api/intel/items`.
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
    @State private var runningAll = false
    @State private var runAllProgress: (done: Int, total: Int)?

    @State private var showFilters = false
    @State private var selectedCategories: Set<String> = []
    @State private var itemPresence: ItemPresence = .all

    /// "data feed or no" for non-spatial sources: a source can be catalogued
    /// yet have ingested zero items, so filtering on presence is the intel
    /// equivalent of the cameras tab's feed-availability toggle.
    enum ItemPresence: String, FilterChoice {
        case all, hasItems, empty
        var id: String { rawValue }
        var label: String {
            switch self {
            case .all:      return "All"
            case .hasItems: return "Has items"
            case .empty:    return "Empty"
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
            .background(theme.surface.ignoresSafeArea())
            .navigationTitle("Intel")
            .searchable(
                text: $searchText,
                placement: .navigationBarDrawer(displayMode: .always),
                prompt: "Search across all sources"
            )
            .modifier(BilingualSearchModifier(query: searchText, bilingual: $bilingual))
            .refreshable { await reload() }
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    FilterToolbarButton(isActive: filtersAreActive) {
                        showFilters = true
                    }
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button {
                        Task { await runAll() }
                    } label: {
                        if runningAll {
                            HStack(spacing: 6) {
                                if let p = runAllProgress {
                                    Text("\(p.done)/\(p.total)")
                                        .font(.caption.monospacedDigit())
                                }
                                ProgressView().controlSize(.mini)
                            }
                        } else {
                            Image(systemName: "play.circle.fill")
                        }
                    }
                    .disabled(runningAll)
                    .accessibilityLabel(runningAll ? "Running all sources…" : "Run all sources")
                }
            }
            .sheet(isPresented: $showFilters) { filtersSheet }
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
            List {
                if !sources.isEmpty && filteredSources.isEmpty {
                    Section {
                        Text("No sources match the current filters.")
                            .font(.caption)
                            .foregroundStyle(theme.textMuted)
                    }
                } else if filteredSources.allSatisfy({ $0.item_count == 0 }) {
                    Section {
                        VStack(alignment: .leading, spacing: 4) {
                            Label("No items collected yet", systemImage: "info.circle")
                                .font(.caption.weight(.semibold))
                                .foregroundStyle(theme.textMuted)
                            Text("Tap the play button on any source to fetch its data, or use Run all in the top bar.")
                                .font(.caption2)
                                .foregroundStyle(theme.textMuted)
                        }
                        .padding(.vertical, 4)
                    }
                }
                Section {
                    ForEach(filteredSources) { src in
                        ZStack {
                            // Tap-to-drill: NavigationLink wraps the row (with no
                            // visible chevron via opacity 0) — separate hit-target
                            // from the Run button which uses .buttonStyle(.plain).
                            NavigationLink(value: src) { EmptyView() }.opacity(0)
                            IntelSourceRow(source: src, onRunComplete: {
                                Task { await reload() }
                            })
                        }
                    }
                }
            }
            .listStyle(.insetGrouped)
            .navigationDestination(for: IntelSource.self) { src in
                IntelSourceItemsView(source: src)
            }
        }
    }

    // ── Filtering ───────────────────────────────────────────────────────────

    private var filtersAreActive: Bool {
        !selectedCategories.isEmpty || itemPresence != .all
    }

    /// Category + item-presence filter. Search is handled separately by the
    /// cross-source FTS branch, so this only narrows the catalogue list.
    private var filteredSources: [IntelSource] {
        sources.filter { src in
            if !selectedCategories.isEmpty {
                guard let cat = src.category, selectedCategories.contains(cat) else { return false }
            }
            switch itemPresence {
            case .all:      break
            case .hasItems: if src.item_count == 0 { return false }
            case .empty:    if src.item_count > 0  { return false }
            }
            return true
        }
    }

    /// Categories present in the catalogue, by descending source count then
    /// alphabetical — same ordering rule the Saved/Cameras chips use.
    private var availableCategories: [String] {
        var counts: [String: Int] = [:]
        for s in sources {
            guard let c = s.category else { continue }
            counts[c, default: 0] += 1
        }
        return counts.keys.sorted {
            counts[$0]! == counts[$1]! ? $0 < $1 : counts[$0]! > counts[$1]!
        }
    }

    private func categoryCount(_ c: String) -> Int {
        sources.filter { $0.category == c }.count
    }

    private func toggleCategory(_ c: String) {
        if selectedCategories.contains(c) { selectedCategories.remove(c) }
        else { selectedCategories.insert(c) }
    }

    private var filtersSheet: some View {
        FilterSheet(
            isActive: filtersAreActive,
            onReset: {
                selectedCategories.removeAll()
                itemPresence = .all
            },
            onDone: { showFilters = false }
        ) {
            FilterSegmentedSection(title: "Data", selection: $itemPresence)
            FilterMultiSelectSection(
                title: "Category",
                items: availableCategories,
                emptyText: "No sources yet",
                label: { $0.capitalized },
                count: { categoryCount($0) },
                isSelected: { selectedCategories.contains($0) },
                toggle: { toggleCategory($0) }
            )
        }
    }

    // ── Loading + actions ───────────────────────────────────────────────────

    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            let env = try await apiClient.api.intelSources()
            sources = env.data
            intelCache.cacheSources(env.data)
            error = nil
        } catch let err {
            error = err.localizedDescription
            // Leave previously-rendered cached sources on screen so the
            // user isn't stranded by a flaky network.
        }
    }

    /// Fire every source's collector with a concurrency cap. Reloads after
    /// completion so item counts + last_fetched reflect the new state.
    private func runAll() async {
        runningAll = true
        runAllProgress = (0, sources.count)
        defer {
            runningAll = false
            runAllProgress = nil
        }
        let api = apiClient.api
        let cap = 4
        var index = 0
        let total = sources.count

        await withTaskGroup(of: Void.self) { group in
            // Seed `cap` initial tasks.
            for _ in 0..<min(cap, total) {
                if index >= total { break }
                let id = sources[index].id
                index += 1
                group.addTask { _ = try? await api.intelRunSource(id) }
            }
            // Replace finished tasks with new ones until exhausted.
            for await _ in group {
                await MainActor.run {
                    runAllProgress = ((runAllProgress?.done ?? 0) + 1, total)
                }
                if index < total {
                    let id = sources[index].id
                    index += 1
                    group.addTask { _ = try? await api.intelRunSource(id) }
                }
            }
        }
        await reload()
    }
}
