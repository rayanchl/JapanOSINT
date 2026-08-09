import SwiftUI

/// OSINT Search tab — the ported entity-pivot search. Query + 9 suggestions,
/// live SSE progress cards, completed history. Tap an entity chip to open its
/// profile; "pivot" starts a fresh investigation.
struct SearchTab: View {
    @EnvironmentObject var apiClient: APIClient
    @EnvironmentObject var nav: MapNavigation
    @Environment(\.theme) private var theme
    @StateObject private var store = SearchStore()

    @State private var query = ""
    @State private var suggestions: [String] = []
    @State private var suggestTask: Task<Void, Never>?
    /// True from the instant a query is submitted until the run is registered
    /// in `store.active` — see `auraIntensity`.
    @State private var launching = false

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    if !suggestions.isEmpty {
                        FlowChips(items: suggestions) { s in Task { await run(s) } }
                    }
                    if let e = store.lastError {
                        Text(e).font(.caption).foregroundColor(theme.danger)
                    }

                    if !store.active.isEmpty {
                        sectionHeader("Active")
                        ForEach(store.active) { item in runLink(item) }
                    }
                    if !store.completed.isEmpty {
                        sectionHeader("Completed")
                        ForEach(store.completed) { item in runLink(item) }
                    }
                    if store.active.isEmpty && store.completed.isEmpty {
                        Text("No investigations yet.")
                            .font(.footnote).foregroundColor(theme.textMuted)
                            .frame(maxWidth: .infinity).padding(.top, 40)
                    }
                }
                .padding(16)
            }
            // The living background (see SearchAuraBackground). Not
            // `themedScreenBackground`, which paints a flat surface: the aura
            // needs that surface UNDER it (it composites onto it and its
            // corners fall back to it), so the two are stacked here instead.
            .scrollContentBackground(.hidden)
            .background {
                ZStack {
                    theme.surface
                    SearchAuraBackground(intensity: auraIntensity)
                }
                .ignoresSafeArea()
            }
            .navigationTitle("Search")
            .toolbar {
                // Roadmap 38 — reach the (server-synced) search history. Re-run
                // a past query straight from the row.
                ToolbarItem(placement: .compatPrimary) {
                    NavigationLink {
                        SearchHistoryView(onRerun: { entry in
                            if let q = (entry.params?["q"]?.value as? String)
                                    ?? (entry.params?["query"]?.value as? String),
                               !q.isEmpty {
                                Task { await run(q) }
                            }
                        })
                    } label: {
                        Image(systemName: "clock.arrow.circlepath")
                    }
                    .accessibilityLabel("Search history")
                }
            }
            .searchable(
                text: $query,
                placement: .compatDrawer,
                prompt: "email, IP, domain, name, vessel IMO…"
            )
            .autocorrectionDisabled()
            .onSubmit(of: .search) { Task { await run(query) } }
        }
        .onChange(of: query) { _, q in
            // Debounce: only ask the LLM to suggest once the entry has settled
            // for 3s. Cancel any in-flight wait on each keystroke so a new one
            // restarts the clock; short/empty queries clear immediately.
            suggestTask?.cancel()
            guard q.trimmingCharacters(in: .whitespaces).count >= 3 else { suggestions = []; return }
            suggestTask = Task {
                try? await Task.sleep(nanoseconds: 3_000_000_000)
                if Task.isCancelled { return }
                let result = (try? await apiClient.api.searchSuggest(q)) ?? []
                if Task.isCancelled { return }
                suggestions = result
            }
        }
        // Query handed in by an App Intent / Siri / Shortcuts / the share-queue
        // drain (via IntentRouter → MapNavigation). Consume on arrival and on
        // any later hand-off while the tab is already showing.
        .onChange(of: nav.pendingSearchQuery) { _, q in consumePendingSearch(q) }
        .task { consumePendingSearch(nav.pendingSearchQuery) }
    }

    /// Kick off a handed-in query exactly once, clearing the hand-off slot so it
    /// can't replay on the next view update.
    private func consumePendingSearch(_ q: String?) {
        guard let q, !q.trimmingCharacters(in: .whitespaces).isEmpty else { return }
        nav.pendingSearchQuery = nil
        Task { await run(q) }
    }

    private func sectionHeader(_ t: String) -> some View {
        Text(t.uppercased()).font(.caption2).foregroundColor(theme.textMuted)
    }

    private func runLink(_ item: SearchStore.Run) -> some View {
        NavigationLink {
            SearchRunDetailView(id: item.id, store: store,
                                onPivot: { q in Task { await run(q) } })
        } label: {
            SearchRunCard(run: item)
        }
        .buttonStyle(.plain)
    }

    /// How lit the background is. A run in flight drives it to full; otherwise
    /// it idles low but never off, so the tab still breathes.
    ///
    /// `launching` covers the gap between the user hitting return and the
    /// server's first progress frame landing in `store.active` — a second or
    /// two of network on a cold call. Without it the tab sits inert at exactly
    /// the moment the user is waiting for a sign that anything happened.
    private var auraIntensity: Double {
        (launching || !store.active.isEmpty) ? 1.0 : 0.0
    }

    private func run(_ q: String) async {
        suggestions = []
        launching = true
        defer { launching = false }
        await store.start(q, api: apiClient.api)
        query = ""
    }
}

private struct SearchRunCard: View {
    let run: SearchStore.Run
    @Environment(\.theme) private var theme

    var body: some View {
        let s = run.snapshot
        let isError = s?.phase == "error"
        // A run still in `active` (not finished, not errored) is doing live
        // work — show a spinning indicator so progress reads as ongoing even
        // while the determinate bar dwells on a stage.
        let isWorking = !run.finished && !isError
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 6) {
                Text(s?.query ?? run.query).font(.subheadline).lineLimit(1)
                Spacer()
                if isWorking {
                    ProgressView().controlSize(.small)
                }
                Text("\(Int(s?.progress_percent ?? 0))%")
                    .font(.caption).foregroundColor(isError ? theme.danger : theme.accent)
            }
            ProgressView(value: (s?.progress_percent ?? 0) / 100.0)
                .tint(isError ? theme.danger : theme.accent)
                .compatThinProgress()
                // Ease the fill between backend percentage jumps instead of
                // snapping — the bar glides to each new stage.
                .animation(.easeInOut(duration: 0.6), value: s?.progress_percent ?? 0)
            HStack {
                Text(stageLabel(s) + roundSuffix(s))
                    .font(.caption2).foregroundColor(theme.textMuted)
                Spacer()
                Image(systemName: "chevron.right")
                    .font(.caption2).foregroundColor(theme.textMuted)
            }
        }
        .padding(12)
        .background(RoundedRectangle(cornerRadius: 8).fill(theme.surfaceElevated))
    }

    /// Current-stage title via the shared PipelineStage map (covers all phases,
    /// unlike the old 6-case switch).
    private func stageLabel(_ s: SearchSnapshot?) -> String {
        if s?.phase == "error" { return "Error" }
        if let cur = pipelineStages(for: s).first(where: { $0.state == .current })?.stage {
            return cur.title
        }
        return PipelineStage.completed.title
    }
    private func roundSuffix(_ s: SearchSnapshot?) -> String {
        guard let r = s?.current_round, r > 0 else { return "" }
        return " · round \(r)/\(s?.max_rounds ?? 5)"
    }
}

struct FlowChips: View {
    let items: [String]
    let onTap: (String) -> Void
    @Environment(\.theme) private var theme
    var body: some View {
        FlowLayout(spacing: 6) {
            ForEach(items, id: \.self) { s in
                Button { onTap(s) } label: {
                    Text(s).font(.caption2).lineLimit(1)
                        .padding(.horizontal, 8).padding(.vertical, 4)
                        .background(Capsule().stroke(theme.textMuted.opacity(0.4)))
                        .foregroundColor(theme.textMuted)
                }
                .buttonStyle(.plain)   // avoid macOS default button chrome behind the chip
            }
        }
    }
}

/// Minimal wrapping layout (iOS 16+ Layout) so chips flow to multiple lines.
struct FlowLayout: Layout {
    var spacing: CGFloat = 6
    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let maxW = proposal.width ?? .infinity
        var x: CGFloat = 0, y: CGFloat = 0, lineH: CGFloat = 0
        for v in subviews {
            let s = v.sizeThatFits(.unspecified)
            if x + s.width > maxW { x = 0; y += lineH + spacing; lineH = 0 }
            x += s.width + spacing; lineH = max(lineH, s.height)
        }
        return CGSize(width: maxW == .infinity ? x : maxW, height: y + lineH)
    }
    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        var x = bounds.minX, y = bounds.minY, lineH: CGFloat = 0
        for v in subviews {
            let s = v.sizeThatFits(.unspecified)
            if x + s.width > bounds.maxX { x = bounds.minX; y += lineH + spacing; lineH = 0 }
            v.place(at: CGPoint(x: x, y: y), proposal: ProposedViewSize(s))
            x += s.width + spacing; lineH = max(lineH, s.height)
        }
    }
}
