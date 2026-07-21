import SwiftUI

/// OSINT Search tab — the ported entity-pivot search. Query + 9 suggestions,
/// live SSE progress cards, completed history. Tap an entity chip to open its
/// profile; "pivot" starts a fresh investigation.
struct SearchTab: View {
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme
    @StateObject private var store = SearchStore()

    @State private var query = ""
    @State private var suggestions: [String] = []
    @State private var suggestTask: Task<Void, Never>?

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
            .themedScreenBackground(theme)
            .navigationTitle("Search")
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

    private func run(_ q: String) async {
        suggestions = []
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
