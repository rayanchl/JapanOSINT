import SwiftUI
import Charts
import Combine

struct SourceDashboardTab: View {
    @EnvironmentObject var settings: AppSettings
    @EnvironmentObject var favs: CollectorFavorites
    @EnvironmentObject var nav: MapNavigation
    @Environment(\.theme) private var theme

    @State private var summary: StatusSummary?
    @State private var apis: [StatusRow] = []
    @State private var loading = false
    @State private var errorMessage: String?
    @State private var search = ""
    @State private var selected: StatusRow?
    @State private var expanded: Set<String> = []
    @State private var refreshing: Set<String> = []
    @State private var refreshError: [String: String] = [:]

    var body: some View {
        Group {
            if apis.isEmpty && !loading && errorMessage != nil {
                // Matches the Database tab's offline state exactly — full-
                // area `OfflineStateView` with no outline / no red banner.
                OfflineStateView(retry: { Task { await load() } })
            } else {
                ScrollView {
                    VStack(alignment: .leading, spacing: Space.lg - 2) {
                        if apis.isEmpty && !loading {
                            emptyState
                        }
                        // While searching, surface matched collectors at
                        // the top so the user sees results above charts.
                        if isSearching {
                            collectorsList
                            summaryCards
                            if !apis.isEmpty {
                                statusChart
                                categoryChart
                            }
                        } else {
                            summaryCards
                            if !apis.isEmpty {
                                statusChart
                                categoryChart
                            }
                            collectorsList
                        }
                    }
                    .padding()
                }
                .refreshable { await load() }
            }
        }
        .background(theme.surface.ignoresSafeArea())
        .navigationTitle("Sources")
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                Button { Task { await load() } } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .disabled(loading)
            }
        }
        .searchable(text: $search, prompt: "Filter collectors")
        .task {
            if apis.isEmpty { await load() }
            // Cross-tab deep link: when Console pushed us in response to a
            // `showSource(_:)` call, the value was published before we mounted
            // — `.onReceive` would miss it. Consume the current value here.
            if let id = nav.pendingSourceId { await openSource(id) }
        }
        .onReceive(nav.$pendingSourceId.compactMap { $0 }) { id in
            Task { await openSource(id) }
        }
        .sheet(item: $selected) { row in
            NavigationStack {
                SourceDetail(row: row, onUpdate: { updated in
                    if let i = apis.firstIndex(where: { $0.id == updated.id }) {
                        apis[i] = updated
                    }
                    selected = updated
                })
            }
            .presentationDetents([.medium, .large])
        }
    }

    // MARK: - Summary cards

    private var summaryCards: some View {
        let s = summary
        let cols = [GridItem(.flexible(), spacing: 8), GridItem(.flexible(), spacing: 8)]
        return LazyVGrid(columns: cols, spacing: 8) {
            statCard("Total",    s?.total ?? apis.count, color: theme.accent,  icon: "square.stack.3d.up.fill")
            statCard("Online",   s?.online ?? 0,         color: theme.success, icon: "checkmark.circle.fill")
            statCard("Degraded", s?.degraded ?? 0,       color: theme.warning, icon: "exclamationmark.triangle.fill")
            statCard("Offline",  s?.offline ?? 0,        color: theme.danger,  icon: "xmark.octagon.fill")
            statCard("Gated",    s?.gated ?? 0,          color: theme.textMuted, icon: "lock.fill")
        }
    }

    private var emptyState: some View {
        VStack(spacing: 8) {
            Image(systemName: "chart.pie")
                .font(.largeTitle)
                .foregroundStyle(theme.textMuted)
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
                Text(label).font(.caption2).foregroundStyle(theme.textMuted)
            }
            Text("\(value)")
                .font(.title2.bold().monospacedDigit())
                .foregroundStyle(color)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(Space.md - 2)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.md))
    }

    // MARK: - Charts

    private var sourcesByStatus: [(OsintSource.StatusKind, [StatusRow], Color, String)] {
        let groups = Dictionary(grouping: apis) { row -> OsintSource.StatusKind in
            let k = row.statusKind
            return (k == .unknown) ? .pending : k
        }
        let order: [(OsintSource.StatusKind, Color, String)] = [
            (.online,   theme.success,   "checkmark.circle.fill"),
            (.degraded, theme.warning,   "exclamationmark.triangle.fill"),
            (.offline,  theme.danger,    "xmark.octagon.fill"),
            (.gated,    theme.textMuted, "lock.fill"),
            (.pending,  theme.textMuted, "clock"),
        ]
        return order.compactMap { (status, color, icon) in
            let bucket = groups[status] ?? []
            return bucket.isEmpty ? nil : (status, bucket, color, icon)
        }
    }

    private var statusChart: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Status").font(.headline).foregroundStyle(theme.text)
            if sourcesByStatus.isEmpty {
                Text("No sources").font(.caption).foregroundStyle(theme.textMuted)
            } else {
                ForEach(Array(sourcesByStatus.enumerated()), id: \.offset) { _, entry in
                    let (status, rows, color, icon) = entry
                    bucketRect(status: status, rows: rows, color: color, icon: icon)
                }
            }
        }
        .padding(12)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 12))
    }

    private func bucketRect(
        status: OsintSource.StatusKind,
        rows: [StatusRow],
        color: Color,
        icon: String
    ) -> some View {
        let cols = [GridItem(.adaptive(minimum: 10, maximum: 10), spacing: 4, alignment: .leading)]
        return VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 6) {
                Image(systemName: icon).font(.caption2).foregroundStyle(color)
                Text(status.rawValue.capitalized)
                    .font(.caption.bold()).foregroundStyle(theme.text)
                Text("\(rows.count)")
                    .font(.caption2.monospacedDigit()).foregroundStyle(theme.textMuted)
                Spacer()
            }
            LazyVGrid(columns: cols, alignment: .leading, spacing: 4) {
                ForEach(rows) { r in
                    Circle()
                        .fill(color)
                        .frame(width: 10, height: 10)
                        .help(r.name ?? r.id)
                        .accessibilityLabel("\(r.name ?? r.id) — \(status.rawValue)")
                }
            }
        }
        .padding(8)
        .background(color.opacity(0.10), in: RoundedRectangle(cornerRadius: 10))
    }

    /// (category, total collectors, fav collectors). Sort key:
    ///   1. fav count descending  → favoriting any collector in a category
    ///      bumps that category to the top of the chart immediately
    ///   2. total count descending → bigger categories above smaller
    ///   3. name ascending → stable tie-breaker so equal counts don't
    ///      flicker around on every re-render (Dictionary iteration order
    ///      is not guaranteed in Swift)
    private var categoryBuckets: [(String, Int, Int)] {
        var totals: [String: Int] = [:]
        var favCounts: [String: Int] = [:]
        for c in apis.groupedAsCollectors() {
            let cat = c.category ?? "Other"
            totals[cat, default: 0] += 1
            if favs.contains(c.id) { favCounts[cat, default: 0] += 1 }
        }
        return totals
            .map { (cat, total) in (cat, total, favCounts[cat] ?? 0) }
            .sorted { lhs, rhs in
                if lhs.2 != rhs.2 { return lhs.2 > rhs.2 }      // favs ↓
                if lhs.1 != rhs.1 { return lhs.1 > rhs.1 }      // total ↓
                return lhs.0.lowercased() < rhs.0.lowercased()  // name ↑
            }
            .prefix(12)
            .map { ($0.0, $0.1, $0.2) }
    }

    private var categoryChart: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("Collectors by category (top 12)").font(.headline).foregroundStyle(theme.text)
            Chart(categoryBuckets, id: \.0) { (cat, count, favs) in
                BarMark(
                    x: .value("Count", count),
                    y: .value("Category", cat)
                )
                .foregroundStyle(favs > 0 ? theme.warning : theme.accent)
            }
            .frame(height: CGFloat(max(140, categoryBuckets.count * 18)))
        }
        .padding(12)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 12))
    }

    // MARK: - Collectors list

    private var isSearching: Bool {
        !search.trimmingCharacters(in: .whitespaces).isEmpty
    }

    private var collectors: [Collector] {
        let s = search.trimmingCharacters(in: .whitespaces).lowercased()
        let all = apis.groupedAsCollectors()
        let filtered = s.isEmpty ? all : all.filter { c in
            c.name.lowercased().contains(s)
                || c.id.lowercased().contains(s)
                || (c.category ?? "").lowercased().contains(s)
                || c.sources.contains { ($0.name ?? "").lowercased().contains(s) }
        }
        return filtered.sorted { lhs, rhs in
            let lf = favs.contains(lhs.id)
            let rf = favs.contains(rhs.id)
            if lf != rf { return lf && !rf }   // favs first
            return lhs.name.lowercased() < rhs.name.lowercased()
        }
    }

    private var collectorsList: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text("Collectors (\(collectors.count))")
                    .font(.headline.monospacedDigit())
                    .foregroundStyle(theme.text)
                Spacer()
                Text("\(apis.count) sources")
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(theme.textMuted)
            }
            VStack(spacing: 8) {
                ForEach(collectors) { c in
                    CollectorRow(
                        collector: c,
                        isFavorite: favs.contains(c.id),
                        isExpanded: expanded.contains(c.id),
                        isRefreshing: refreshing.contains(c.id),
                        refreshError: refreshError[c.id],
                        toggleFavorite: { favs.toggle(c.id) },
                        toggleExpand: { toggleExpand(c.id) },
                        refresh: { Task { await refresh(c.id) } },
                        selectSource: { selected = $0 }
                    )
                }
            }
        }
    }

    /// Cross-tab nav target: open the source detail for `id`, expanding
    /// its parent collector so the row is in view if the user dismisses
    /// the sheet. If the status data hasn't loaded yet, load first.
    private func openSource(_ id: String) async {
        if apis.isEmpty { await load() }
        guard let row = apis.first(where: { $0.id == id }) else {
            nav.pendingSourceId = nil
            return
        }
        if let parent = collectors.first(where: { c in
            c.sources.contains(where: { $0.id == id })
        }) {
            expanded.insert(parent.id)
        }
        selected = row
        nav.pendingSourceId = nil
    }

    private func toggleExpand(_ id: String) {
        if expanded.contains(id) { expanded.remove(id) }
        else { expanded.insert(id) }
    }

    private func refresh(_ collectorId: String) async {
        guard collectorId != "(no layer)" else { return }
        refreshing.insert(collectorId)
        refreshError.removeValue(forKey: collectorId)
        defer { refreshing.remove(collectorId) }
        do {
            let layer = LayerDef(id: collectorId, name: collectorId, category: nil, sources: nil, temporal: nil, liveOnly: nil)
            _ = try await API(baseURL: settings.backendBaseURL).data(for: layer)
            await load()
        } catch {
            refreshError[collectorId] = error.localizedDescription
        }
    }

    // MARK: - Loading

    private func load() async {
        let api = API(baseURL: settings.backendBaseURL)
        loading = true
        defer { loading = false }
        do {
            let env = try await api.status()
            summary = env.summary
            apis = env.apis
            errorMessage = nil
        } catch {
            errorMessage = error.localizedDescription
        }
    }
}

struct CollectorRow: View {
    let collector: Collector
    let isFavorite: Bool
    let isExpanded: Bool
    let isRefreshing: Bool
    let refreshError: String?
    let toggleFavorite: () -> Void
    let toggleExpand: () -> Void
    let refresh: () -> Void
    let selectSource: (StatusRow) -> Void

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
                    Text(collector.name)
                        .font(.subheadline.bold())
                        .foregroundStyle(theme.text)
                        .lineLimit(1)
                    HStack(spacing: Space.sm - 2) {
                        Text("\(collector.sourceCount) source\(collector.sourceCount == 1 ? "" : "s")")
                            .font(.caption2.monospacedDigit())
                            .foregroundStyle(theme.textMuted)
                        if let cat = collector.category {
                            Text("· \(cat)").font(.caption2).foregroundStyle(theme.textMuted)
                        }
                    }
                }
                Spacer()
                healthChips
                Image(systemName: isExpanded ? "chevron.up" : "chevron.down")
                    .font(.caption)
                    .foregroundStyle(theme.textMuted)
            }
            .padding(10)
        }
        .buttonStyle(.plain)
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
            if collector.gatedCount > 0 {
                Pill(text: "\(collector.gatedCount)", tone: .neutral,
                     icon: "lock.fill", solid: true)
            }
        }
    }

    private var expandedBody: some View {
        VStack(spacing: 1) {
            ForEach(collector.sources) { src in
                SourceRow(row: src) { selectSource(src) }
            }
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
                        }
                        Text(isRefreshing ? "Refreshing…" : "Refresh collector")
                    }
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.small)
                .disabled(isRefreshing || collector.id == "(no layer)")
                Spacer()
            }
            .padding(.horizontal, 8)
            .padding(.top, 4)
        }
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

struct SourceRow: View {
    let row: StatusRow
    let onTap: () -> Void
    @Environment(\.theme) private var theme

    var body: some View {
        Button(action: onTap) {
            HStack(spacing: 8) {
                Circle().fill(statusColor)
                    .frame(width: 10, height: 10)
                VStack(alignment: .leading, spacing: 2) {
                    Text(row.name ?? row.id).font(.subheadline).lineLimit(1)
                        .foregroundStyle(theme.text)
                    HStack(spacing: 6) {
                        if let c = row.category {
                            Text(c).font(.caption2).foregroundStyle(theme.textMuted)
                        }
                        if let t = row.type {
                            Text("· \(t)").font(.caption2).foregroundStyle(theme.textMuted)
                        }
                        if row.requiresKey == true {
                            Text(row.configured == true ? "key✓" : "key✗")
                                .font(.caption2.bold())
                                .foregroundStyle(row.configured == true ? theme.success : theme.warning)
                        }
                    }
                    if row.gated == true {
                        Text("Probing needs API credentials — manual ping in card settings")
                            .font(.caption2)
                            .foregroundStyle(theme.warning)
                            .padding(.horizontal, 6).padding(.vertical, 3)
                            .background(theme.warning.opacity(0.15),
                                        in: RoundedRectangle(cornerRadius: 4))
                            .lineLimit(2)
                    }
                }
                Spacer()
                if let ms = row.responseTimeMs, row.gated != true {
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

    private var statusColor: Color {
        switch row.statusKind {
        case .online:   return theme.success
        case .degraded: return theme.warning
        case .offline:  return theme.danger
        default:        return theme.textMuted
        }
    }
}

private struct SourceDetail: View {
    let row: StatusRow
    var onUpdate: (StatusRow) -> Void = { _ in }
    @EnvironmentObject var nav: MapNavigation
    @EnvironmentObject var settings: AppSettings
    @Environment(\.theme) private var theme
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                header

                if let desc = row.description, !desc.isEmpty {
                    Text(desc)
                        .font(.subheadline)
                        .foregroundStyle(theme.text)
                        .padding(10)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 8))
                }

                section("Identity") {
                    kv("ID",        row.id)
                    kv("Layer",     row.layer ?? "—")
                    kv("Category",  row.category ?? "—")
                    kv("Type",      row.type ?? "—")
                    kv("Free?",     row.free.map { $0 ? "yes" : "no" } ?? "—")
                }

                section("Health") {
                    kv("Status",       row.status ?? "—",
                       valueColor: statusColor(row.status))
                    kv("Last check",   prettyDate(row.lastCheck))
                    kv("Last success", prettyDate(row.lastSuccess))
                    kv("Latency",      row.responseTimeMs.map { "\(Int($0)) ms" } ?? "—")
                    kv("Records",      row.recordsCount.map(String.init) ?? "—")
                }

                if row.requiresKey == true {
                    section("API key") {
                        if let env = row.envVars, !env.isEmpty {
                            ForEach(env, id: \.name) { v in envVarRowApiStyle(v) }
                        }
                    }
                    probeActions
                }

                if let url = row.url, let u = URL(string: url) {
                    Link(destination: u) {
                        Label("Open source URL", systemImage: "safari")
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.borderedProminent)
                }

                if let err = row.errorMessage, !err.isEmpty {
                    VStack(alignment: .leading, spacing: 4) {
                        Label("Last error", systemImage: "exclamationmark.triangle.fill")
                            .font(.caption.bold())
                            .foregroundStyle(theme.danger)
                        Text(err)
                            .font(.caption2)
                            .foregroundStyle(theme.text)
                            .textSelection(.enabled)
                    }
                    .padding(10)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(theme.danger.opacity(0.1),
                                in: RoundedRectangle(cornerRadius: 8))
                }
            }
            .padding()
        }
        .background(theme.surface.ignoresSafeArea())
        .navigationTitle(row.name ?? row.id)
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                Button("Close") { dismiss() }
            }
        }
    }

    private var probeActions: some View {
        ProbeActionsView(row: row, onUpdate: onUpdate)
            .padding(10)
            .frame(maxWidth: .infinity)
            .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 8))
    }

    private var header: some View {
        HStack(alignment: .top, spacing: 10) {
            Circle().fill(headerDotColor)
                .frame(width: 14, height: 14)
            VStack(alignment: .leading, spacing: 2) {
                Text(row.name ?? row.id)
                    .font(.title3.bold())
                    .foregroundStyle(theme.text)
                if let ja = row.nameJa, !ja.isEmpty {
                    Text(ja).font(.subheadline).foregroundStyle(theme.textMuted)
                }
            }
            Spacer()
        }
    }

    private var headerDotColor: Color {
        switch row.statusKind {
        case .online:   return theme.success
        case .degraded: return theme.warning
        case .offline:  return theme.danger
        default:        return theme.textMuted
        }
    }

    @ViewBuilder
    private func section<Content: View>(_ title: String,
                                        @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title).font(.caption.bold()).foregroundStyle(theme.textMuted)
            VStack(spacing: 1) { content() }
                .clipShape(RoundedRectangle(cornerRadius: 8))
        }
    }

    private func kv(_ k: String, _ v: String, valueColor: Color? = nil) -> some View {
        HStack(alignment: .firstTextBaseline) {
            Text(k).font(.caption.bold()).foregroundStyle(theme.textMuted)
                .frame(width: 100, alignment: .leading)
            Text(v).font(.caption).foregroundStyle(valueColor ?? theme.text)
                .textSelection(.enabled)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding(8)
        .background(theme.surfaceElevated)
    }

    /// One env-var row styled to match `ApiKeysView.row` exactly: icon →
    /// monospaced name → role pill → trailing SET/UNSET pill, on the same
    /// surfaceElevated background. Source of truth for the visual: ApiKeys/
    /// ApiKeysView.swift:105-151.
    private func envVarRowApiStyle(_ spec: EnvVarSpec) -> some View {
        Button {
            let target = spec.name
            dismiss()
            nav.showApiKey(target)
        } label: {
            HStack(spacing: 10) {
                Image(systemName: spec.set == true ? "key.fill" : "key.slash")
                    .foregroundStyle(spec.set == true ? theme.success : theme.textMuted)
                    .frame(width: 22)
                Text(spec.name)
                    .font(.system(.body, design: .monospaced).weight(.medium))
                    .foregroundStyle(theme.text)
                    .lineLimit(1)
                if let role = spec.role {
                    envRolePill(role)
                }
                Spacer()
                envStatusPill(spec.set == true)
            }
            .padding(.horizontal, 12).padding(.vertical, 8)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(theme.surfaceElevated)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Open API key page for \(spec.name)")
    }

    private func envRolePill(_ role: String) -> some View {
        Pill(text: role.uppercased(), tone: envRoleTone(role))
    }

    private func envStatusPill(_ set: Bool) -> some View {
        Pill(text: set ? "SET" : "UNSET", tone: set ? .success : .warning, size: .md)
    }

    private func envRoleTone(_ role: String) -> Pill.Tone {
        switch role {
        case "required": return .accent
        case "anyOf":    return .warning
        default:         return .neutral
        }
    }

    private func statusColor(_ s: String?) -> Color {
        switch s {
        case "online":   return theme.success
        case "degraded": return theme.warning
        case "offline":  return theme.danger
        default:         return theme.textMuted
        }
    }

    private func prettyDate(_ iso: String?) -> String {
        guard let iso else { return "—" }
        let f = ISO8601DateFormatter()
        f.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        let d = f.date(from: iso) ?? ISO8601DateFormatter().date(from: iso)
        guard let d else { return iso }
        let rel = RelativeDateTimeFormatter()
        rel.unitsStyle = .abbreviated
        return rel.localizedString(for: d, relativeTo: Date())
    }
}
