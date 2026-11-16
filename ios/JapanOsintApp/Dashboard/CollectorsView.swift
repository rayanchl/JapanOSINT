import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// The registry grouped by collector (GET /api/collectors), one collector or
// group with a paged list of its sources (GET /api/collectors/:key), and a
// server-side search of every source (GET /api/sources/search). Each is a
// bounded read: the dashboard's fleet view pulls the whole of /api/status,
// and a phone that only wants one collector, or "every source whose name
// contains X", should not have to download 58,000 rows to get it.
// ─────────────────────────────────────────────────────────────────────────────

struct CollectorsView: View {
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var index: CollectorsEnvelope?
    @State private var errorText: String?
    @State private var query = ""
    @State private var hits: [SourceSearchRow] = []
    @State private var hitsPage: OffsetPage?
    @State private var searching = false
    @State private var searchError: String?
    @State private var openGroups: Set<String> = []

    private var isSearching: Bool { !query.trimmingCharacters(in: .whitespaces).isEmpty }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Space.md) {
                if isSearching { searchResults } else { groupsList }
            }
            .padding()
        }
        .themedScreenBackground(theme)
        .navigationTitle("Collectors")
        .compatInlineTitle()
        .searchable(text: $query, prompt: "Search every source on the server")
        .onSubmit(of: .search) { Task { await search(reset: true) } }
        .onChange(of: query) { _, q in
            if q.trimmingCharacters(in: .whitespaces).isEmpty { hits = []; hitsPage = nil; searchError = nil }
        }
        .toolbar {
            ToolbarItem(placement: .compatPrimary) {
                NavigationLink { CadencePolicyView() } label: { Image(systemName: "clock.arrow.2.circlepath") }
                    .accessibilityLabel("System-wide cadence policy")
            }
        }
        .task { if index == nil { await load() } }
        .refreshable { await load() }
    }

    // MARK: Groups → collectors

    @ViewBuilder private var groupsList: some View {
        if let idx = index {
            Text("\(idx.total ?? idx.collectors.count) collectors in \(idx.groups.count) groups · \(idx.sources.map { $0.formatted() } ?? "?") sources")
                .font(.caption.monospacedDigit()).foregroundStyle(theme.textMuted)
            if let un = idx.unassignedCollectors, un > 0 {
                Text("\(un) collector\(un == 1 ? "" : "s") have no group in collectors.def")
                    .font(.caption2).foregroundStyle(theme.danger)
            }
            ForEach(idx.groups) { g in
                VStack(alignment: .leading, spacing: 1) {
                    Button {
                        if openGroups.contains(g.key) { openGroups.remove(g.key) } else { openGroups.insert(g.key) }
                    } label: {
                        CollectorTallyRow(c: g, chevron: openGroups.contains(g.key) ? "chevron.down" : "chevron.right")
                    }
                    .buttonStyle(.plain)
                    if openGroups.contains(g.key) {
                        NavigationLink { CollectorDetailView(key: g.key) } label: {
                            Text("All \(g.sources.formatted()) sources in this group")
                                .font(.caption).frame(maxWidth: .infinity, alignment: .leading)
                                .padding(8).background(theme.surfaceElevated)
                        }
                        .buttonStyle(.plain)
                        ForEach(members(of: g, in: idx)) { c in
                            NavigationLink { CollectorDetailView(key: c.key) } label: {
                                CollectorTallyRow(c: c, chevron: "chevron.right").padding(.leading, Space.md)
                            }
                            .buttonStyle(.plain)
                        }
                    }
                }
                .clipShape(RoundedRectangle(cornerRadius: 8))
            }
        } else if let errorText {
            Text(errorText).font(.caption).foregroundStyle(theme.danger)
            Button("Retry") { Task { await load() } }
        } else {
            ProgressView().frame(maxWidth: .infinity)
        }
    }

    /// A group's members, in the order /api/collectors serves them (by size).
    private func members(of g: CollectorSummary, in idx: CollectorsEnvelope) -> [CollectorSummary] {
        let keys = Set(g.memberKeys)
        if !keys.isEmpty { return idx.collectors.filter { keys.contains($0.key) } }
        return idx.collectors.filter { $0.group == g.key }
    }

    // MARK: Server search

    @ViewBuilder private var searchResults: some View {
        if searching && hits.isEmpty {
            ProgressView().frame(maxWidth: .infinity)
        } else if let searchError {
            Text(searchError).font(.caption).foregroundStyle(theme.danger)
        } else if let page = hitsPage {
            Text("\(hits.count.formatted()) of \(page.total.map { $0.formatted() } ?? "?") matching sources")
                .font(.caption.monospacedDigit()).foregroundStyle(theme.textMuted)
            VStack(spacing: 1) {
                ForEach(hits) { r in
                    NavigationLink { SourceDetailView(sourceId: r.id) } label: { SourceSearchRowView(row: r) }
                        .buttonStyle(.plain)
                }
            }
            .clipShape(RoundedRectangle(cornerRadius: 8))
            if page.has_more == true {
                Button(searching ? "Loading…" : "Load more") { Task { await search(reset: false) } }
                    .disabled(searching)
            }
        } else {
            Text("Press Search to ask the server — this searches every registered source by id, name, description, collector and host.")
                .font(.caption).foregroundStyle(theme.textMuted)
        }
    }

    private func load() async {
        do { index = try await apiClient.api.collectorsIndex(); errorText = nil }
        catch { if index == nil { errorText = error.localizedDescription } }
    }

    private func search(reset: Bool) async {
        let q = query.trimmingCharacters(in: .whitespaces)
        guard !q.isEmpty else { return }
        searching = true; defer { searching = false }
        let offset = reset ? 0 : (hitsPage?.nextOffset(fallback: hits.count) ?? hits.count)
        do {
            let p = try await apiClient.api.sourceSearch(q: q, limit: 50, offset: offset)
            if reset { hits = p.rows } else { hits.append(contentsOf: p.rows) }
            hitsPage = p.page
            searchError = nil
        } catch { searchError = error.localizedDescription }
    }
}

/// A collector or group with its tallies (coll_json, sourceapi.c:794).
struct CollectorTallyRow: View {
    let c: CollectorSummary
    var chevron: String? = nil
    @Environment(\.theme) private var theme
    var body: some View {
        HStack(spacing: Space.sm) {
            VStack(alignment: .leading, spacing: 2) {
                Text(c.label ?? c.key).font(.subheadline.bold()).foregroundStyle(theme.text).lineLimit(1)
                Text("\(c.sources.formatted()) sources · \(c.scheduled ?? 0) scheduled · \(c.pivots ?? 0) pivots · \((c.items ?? 0).formatted()) items")
                    .font(.caption2.monospacedDigit()).foregroundStyle(theme.textMuted).lineLimit(1)
            }
            Spacer()
            HStack(spacing: Space.xs) {
                if let n = c.online, n > 0 { Pill(text: "\(n)", tone: .success, icon: "checkmark.circle.fill", solid: true) }
                if let n = c.degraded, n > 0 { Pill(text: "\(n)", tone: .warning, icon: "exclamationmark.triangle.fill", solid: true) }
                if let n = c.offline, n > 0 { Pill(text: "\(n)", tone: .danger, icon: "xmark.octagon.fill", solid: true) }
                if let n = c.pending, n > 0 { Pill(text: "\(n)", tone: .neutral, icon: "clock", solid: true) }
            }
            if let chevron {
                Image(systemName: chevron).font(.caption2).foregroundStyle(theme.textMuted).accessibilityHidden(true)
            }
        }
        .padding(10)
        .background(theme.surfaceElevated)
        .accessibilityElement(children: .combine)
    }
}

/// One row of a server source list (row_json, sourceapi.c:506).
struct SourceSearchRowView: View {
    let row: SourceSearchRow
    @Environment(\.theme) private var theme
    var body: some View {
        HStack(spacing: Space.sm) {
            Circle().fill(dot).frame(width: 10, height: 10).accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 2) {
                Text(row.name ?? row.id).font(.subheadline).foregroundStyle(theme.text).lineLimit(1)
                Text(subtitle).font(.caption2).foregroundStyle(theme.textMuted).lineLimit(1)
                if let h = row.health, h.working == false, let st = h.state {
                    Text("\(st)\(h.note.map { " — \($0)" } ?? "")")
                        .font(.caption2).foregroundStyle(theme.warning).lineLimit(2)
                }
            }
            Spacer()
            if let g = row.trustGrade { Pill(text: g, tone: .info) }
            Image(systemName: "chevron.right").font(.caption2).foregroundStyle(theme.textMuted).accessibilityHidden(true)
        }
        .padding(8)
        .background(theme.surfaceElevated)
        .accessibilityElement(children: .combine)
    }

    private var subtitle: String {
        var parts: [String] = [row.id]
        if let k = row.kind { parts.append(k) }
        if let e = row.engine { parts.append(e) }
        if let n = row.itemCount { parts.append("\(n.formatted()) items") }
        // The cadence that runs, not the one declared (sched_json, sourceapi.c:468).
        if let eff = row.sched?.effectiveInterval { parts.append("every \(fmtInterval(eff))") }
        else if let iv = row.interval, iv > 0 { parts.append("declared \(fmtInterval(iv))") }
        return parts.joined(separator: " · ")
    }

    private var dot: Color {
        switch row.status {
        case "online": return theme.success
        case "degraded": return theme.warning
        case "offline": return theme.danger
        default: return theme.textMuted
        }
    }
}

// MARK: - One collector (or group)

struct CollectorDetailView: View {
    let key: String
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var env: CollectorDetailEnvelope?
    @State private var rows: [SourceSearchRow] = []
    @State private var page: OffsetPage?
    @State private var errorText: String?
    @State private var loading = false
    private let pageSize = 100

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Space.md) {
                if let e = env {
                    CollectorTallyRow(c: e.collector).clipShape(RoundedRectangle(cornerRadius: 8))
                    if let d = e.collector.description, !d.isEmpty {
                        Text(d).font(.caption).foregroundStyle(theme.textMuted)
                    }
                    if !e.children.isEmpty {
                        SPSection(title: "Collectors in this group (\(e.children.count))") {
                            ForEach(e.children) { c in
                                NavigationLink { CollectorDetailView(key: c.key) } label: {
                                    CollectorTallyRow(c: c, chevron: "chevron.right")
                                }
                                .buttonStyle(.plain)
                            }
                        }
                    }
                    if let pre = e.idPrefixes, !pre.values.isEmpty {
                        SPSection(title: "Shared id prefixes") {
                            SPRow(k: "prefixes", v: pre.values.map { "\($0.key) \($0.count)" }.joined(separator: " · "), mono: true)
                        }
                    }
                    SPSection(title: "Sources — \(rows.count.formatted()) of \(page?.total.map { $0.formatted() } ?? "?")") {
                        ForEach(rows) { r in
                            NavigationLink { SourceDetailView(sourceId: r.id) } label: { SourceSearchRowView(row: r) }
                                .buttonStyle(.plain)
                        }
                    }
                    if page?.has_more == true {
                        Button(loading ? "Loading…" : "Load \(pageSize) more") { Task { await loadMore() } }
                            .disabled(loading)
                    }
                } else if let errorText {
                    Text(errorText).font(.caption).foregroundStyle(theme.danger)
                    Button("Retry") { Task { await loadFirst() } }
                } else {
                    ProgressView().frame(maxWidth: .infinity)
                }
            }
            .padding()
        }
        .themedScreenBackground(theme)
        .navigationTitle(env?.collector.label ?? key)
        .compatInlineTitle()
        .task { if env == nil { await loadFirst() } }
    }

    private func loadFirst() async {
        loading = true; defer { loading = false }
        do {
            let e = try await apiClient.api.collectorDetail(key, limit: pageSize, offset: 0)
            env = e
            rows = e.sources?.rows ?? []
            page = e.sources?.page
            errorText = nil
        } catch {
            if case APIError.http(404, _) = error { errorText = "No registered source has the collector \(key)." }
            else { errorText = error.localizedDescription }
        }
    }

    private func loadMore() async {
        loading = true; defer { loading = false }
        let offset = page?.nextOffset(fallback: rows.count) ?? rows.count
        do {
            let e = try await apiClient.api.collectorDetail(key, limit: pageSize, offset: offset)
            rows.append(contentsOf: e.sources?.rows ?? [])
            page = e.sources?.page
        } catch { errorText = error.localizedDescription }
    }
}
