import SwiftUI

/// Level-2 surface: paginated items for a single source.
struct IntelSourceItemsView: View {
    let source: IntelSource

    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var intelCache: IntelCache
    @Environment(\.theme) private var theme

    @State private var items: [IntelItem] = []
    @State private var nextCursor: String?
    @State private var loading = false
    @State private var error: String?
    @State private var searchText = ""
    @State private var running = false

    var body: some View {
        Group {
            if loading && items.isEmpty {
                ProgressView("Loading items…")
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if items.isEmpty && error == nil {
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
        }
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
            ForEach(items) { item in
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
        OfflineStateView(
            kind: .empty,
            title: "No items yet.",
            message: error,
            systemImage: "tray"
        )
    }

    // ── Data ───────────────────────────────────────────────────────────────

    private func reload() async {
        loading = true
        defer { loading = false }
        do {
            let env = try await API(baseURL: settings.backendBaseURL)
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
            _ = try await API(baseURL: settings.backendBaseURL).intelRunSource(source.id)
            await reload()
        } catch { /* leave existing items in place */ }
    }

    private func loadMore() async {
        guard let cursor = nextCursor, !loading else { return }
        loading = true
        defer { loading = false }
        do {
            let env = try await API(baseURL: settings.backendBaseURL)
                .intelItems(source: source.id, q: searchText.isEmpty ? nil : searchText, limit: 50, cursor: cursor)
            items.append(contentsOf: env.data)
            nextCursor = env.page?.next_cursor
        } catch { /* keep existing items */ }
    }
}
