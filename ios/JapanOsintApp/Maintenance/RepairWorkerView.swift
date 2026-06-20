import SwiftUI

/// Operator window into the collector self-healing pipeline (detect → triage →
/// repair). Surfaces the host-wide digest from `GET /api/admin/maintenance`
/// and lets the operator act on it: approve / reject staged URL-swap fixes,
/// clear circuit-breaker quarantines, and drill into any source's full
/// pipeline via `SourcePipelineView`.
///
/// Gated to the platform operator (same as the rest of `AdminPanel`). Every
/// list is server-sourced — empty sections render an honest "nothing here"
/// rather than placeholder rows.
struct RepairWorkerView: View {
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var digest: MaintenanceDigest?
    @State private var loading = false
    @State private var errorMessage: String?
    @State private var window: RepairWindow = .day

    /// Repair/source IDs with an action in flight, so we can disable + spin.
    @State private var busyRepairs: Set<Int> = []
    @State private var busySources: Set<String> = []
    @State private var actionError: String?

    enum RepairWindow: Int, CaseIterable, Identifiable {
        case day = 24, threeDays = 72, week = 168
        var id: Int { rawValue }
        var label: String {
            switch self {
            case .day: return "24h"
            case .threeDays: return "3d"
            case .week: return "7d"
            }
        }
    }

    var body: some View {
        Group {
            if digest == nil && !loading && errorMessage != nil {
                OfflineStateView(retry: { Task { await load() } })
            } else {
                content
            }
        }
        .navigationTitle("Repair worker")
        .themedScreenBackground(theme)
        .task(id: window) { await load() }
    }

    private var content: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Space.lg) {
                windowPicker
                if let actionError {
                    RepairUI.banner(actionError, color: theme.danger, theme: theme)
                }
                if let d = digest {
                    totals(d)
                    awaitingSection(d)
                    needsHumanSection(d)
                    quarantinedSection(d)
                    autoFixedSection(d)
                    classSection(d)
                    worstSourcesSection(d)
                    footer(d)
                } else if loading {
                    ProgressView().frame(maxWidth: .infinity).padding(.top, Space.xxl)
                }
            }
            .padding()
        }
        .refreshable { await load() }
    }

    // MARK: Window

    private var windowPicker: some View {
        Picker("Window", selection: $window) {
            ForEach(RepairWindow.allCases) { w in Text(w.label).tag(w) }
        }
        .pickerStyle(.segmented)
    }

    // MARK: Totals

    @ViewBuilder private func totals(_ d: MaintenanceDigest) -> some View {
        let t = d.totals
        RepairUI.sectionHeader("Last \(d.windowHours ?? window.rawValue)h", theme: theme)
        LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible()), GridItem(.flexible())],
                  spacing: Space.sm) {
            RepairUI.statCard("Auto-fixed", t?.merged ?? 0, color: theme.success, theme: theme)
            RepairUI.statCard("Awaiting", (d.awaitingReview?.all.count ?? 0), color: theme.warning, theme: theme)
            RepairUI.statCard("Needs human", t?.needsHuman ?? 0, color: theme.accentAlt, theme: theme)
            RepairUI.statCard("Verified", t?.verified ?? 0, color: theme.warning, theme: theme)
            RepairUI.statCard("Rejected", t?.rejected ?? 0, color: theme.textMuted, theme: theme)
            RepairUI.statCard("Errors", t?.error ?? 0, color: theme.danger, theme: theme)
        }
    }

    // MARK: Awaiting review (actionable)

    @ViewBuilder private func awaitingSection(_ d: MaintenanceDigest) -> some View {
        let rows = d.awaitingReview?.all ?? []
        RepairUI.sectionHeader("Awaiting your review", count: rows.count, theme: theme)
        if rows.isEmpty {
            RepairUI.emptyRow("No staged fixes waiting.", theme: theme)
        } else {
            ForEach(rows) { r in
                RepairCard(row: r, theme: theme,
                           busy: busyRepairs.contains(r.id),
                           onApprove: { await approve(r) },
                           onReject:  { await reject(r) })
            }
        }
    }

    // MARK: Needs human

    @ViewBuilder private func needsHumanSection(_ d: MaintenanceDigest) -> some View {
        if !d.needsHuman.isEmpty {
            RepairUI.sectionHeader("Needs human", count: d.needsHuman.count, theme: theme)
            ForEach(d.needsHuman) { r in
                RepairCard(row: r, theme: theme, busy: false, onApprove: nil, onReject: nil)
            }
        }
    }

    // MARK: Quarantined (actionable)

    @ViewBuilder private func quarantinedSection(_ d: MaintenanceDigest) -> some View {
        if !d.quarantined.isEmpty {
            RepairUI.sectionHeader("Quarantined", count: d.quarantined.count, theme: theme)
            ForEach(d.quarantined) { q in
                QuarantineCard(row: q, theme: theme,
                               busy: busySources.contains(q.sourceId),
                               onClear: { await clearQuarantine(q.sourceId) })
            }
        }
    }

    // MARK: Recently auto-fixed

    @ViewBuilder private func autoFixedSection(_ d: MaintenanceDigest) -> some View {
        if !d.autoFixed.isEmpty {
            RepairUI.sectionHeader("Recently auto-fixed", count: d.autoFixed.count, theme: theme)
            ForEach(Array(d.autoFixed.prefix(8))) { r in
                RepairCard(row: r, theme: theme, busy: false, onApprove: nil, onReject: nil)
            }
            if !d.urlOverrides.isEmpty {
                DisclosureGroup {
                    ForEach(d.urlOverrides) { o in
                        VStack(alignment: .leading, spacing: 2) {
                            Text(o.sourceId).font(Typography.display(11, weight: .semibold))
                                .foregroundStyle(theme.text)
                            RepairUI.urlDiff(old: o.oldURL, new: o.newURL, theme: theme)
                        }
                        .padding(.vertical, 4)
                    }
                } label: {
                    Text("Active URL overrides (\(d.urlOverrides.count))")
                        .font(.caption).foregroundStyle(theme.textMuted)
                }
                .tint(theme.accent)
                .padding(.top, 2)
            }
        }
    }

    // MARK: Success by class

    @ViewBuilder private func classSection(_ d: MaintenanceDigest) -> some View {
        if !d.successByClass.isEmpty {
            RepairUI.sectionHeader("Success by triage class", theme: theme)
            VStack(spacing: Space.sm) {
                ForEach(d.successByClass) { c in
                    HStack {
                        Text(c.klass).font(Typography.display(12, weight: .medium))
                            .foregroundStyle(theme.text)
                        Spacer()
                        if let rate = c.successRate {
                            Text("\(Int(rate * 100))%")
                                .font(Typography.display(12, weight: .semibold))
                                .foregroundStyle(rate >= 0.5 ? theme.success : theme.warning)
                        }
                        Text("\(c.success ?? 0)✓ / \(c.fail ?? 0)✗")
                            .font(.caption).foregroundStyle(theme.textMuted)
                    }
                }
            }
            .padding(Space.md)
            .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.md))
        }
    }

    // MARK: Worst sources (navigation)

    @ViewBuilder private func worstSourcesSection(_ d: MaintenanceDigest) -> some View {
        if !d.worstSources.isEmpty {
            RepairUI.sectionHeader("Worst sources", theme: theme)
            VStack(spacing: 0) {
                ForEach(d.worstSources) { s in
                    NavigationLink {
                        SourcePipelineView(sourceId: s.sourceId)
                    } label: {
                        HStack {
                            VStack(alignment: .leading, spacing: 1) {
                                Text(s.sourceId).font(Typography.display(12, weight: .medium))
                                    .foregroundStyle(theme.text)
                                Text("\(s.success ?? 0) fixed · \(s.fail ?? 0) failed")
                                    .font(.caption2).foregroundStyle(theme.textMuted)
                            }
                            Spacer()
                            Image(systemName: "chevron.right")
                                .font(.caption2).foregroundStyle(theme.textMuted)
                        }
                        .padding(.vertical, Space.sm)
                    }
                    Divider().overlay(theme.textMuted.opacity(0.15))
                }
            }
            .padding(.horizontal, Space.md)
            .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.md))
        }
    }

    @ViewBuilder private func footer(_ d: MaintenanceDigest) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text("Auto-repair runs only while the worker (LLM) is enabled on the host. Approving a fix writes a live runtime URL override; rejecting resolves the anomaly so the worker stops re-deriving it.")
            if let h = d.concurrency?.heavy?.limit, let m = d.concurrency?.mid?.limit {
                Text("Configured LLM concurrency — heavy \(h), mid \(m) (the C runtime reports no live in-flight gauge).")
            }
        }
        .font(.caption2)
        .foregroundStyle(theme.textMuted)
        .padding(.top, Space.sm)
    }

    // MARK: Data + actions

    private func load() async {
        loading = true; defer { loading = false }
        errorMessage = nil
        do { digest = try await apiClient.api.maintenanceDigest(hours: window.rawValue) }
        catch { if digest == nil { errorMessage = error.localizedDescription } }
    }

    private func approve(_ r: RepairRow) async {
        busyRepairs.insert(r.id); defer { busyRepairs.remove(r.id) }
        actionError = nil
        do {
            try await apiClient.api.approveRepair(r.id)
            Haptics.success()
            await load()
        } catch { actionError = "Approve failed: \(error.localizedDescription)"; Haptics.error() }
    }

    private func reject(_ r: RepairRow) async {
        busyRepairs.insert(r.id); defer { busyRepairs.remove(r.id) }
        actionError = nil
        do {
            try await apiClient.api.rejectRepair(r.id)
            Haptics.tap(.medium)
            await load()
        } catch { actionError = "Reject failed: \(error.localizedDescription)"; Haptics.error() }
    }

    private func clearQuarantine(_ id: String) async {
        busySources.insert(id); defer { busySources.remove(id) }
        actionError = nil
        do {
            try await apiClient.api.unquarantineSource(id)
            Haptics.success()
            await load()
        } catch { actionError = "Clear failed: \(error.localizedDescription)"; Haptics.error() }
    }
}

// MARK: - Repair card (shared with SourcePipelineView)

/// One repair attempt with optional inline Approve/Reject. Approve carries a
/// confirmation because it rewrites a live fetch URL.
struct RepairCard: View {
    let row: RepairRow
    let theme: ThemePalette
    let busy: Bool
    let onApprove: (() async -> Void)?
    let onReject: (() async -> Void)?
    @State private var confirmApprove = false

    var body: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            HStack(spacing: Space.sm) {
                RepairUI.statusBadge(row.status, theme: theme)
                if let a = row.action {
                    Text(a).font(.caption2.weight(.medium))
                        .foregroundStyle(theme.textMuted)
                }
                Spacer()
                if let c = row.createdAt {
                    Text(RepairUI.shortTime(c)).font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
            }
            Text(row.sourceId).font(Typography.display(12, weight: .semibold))
                .foregroundStyle(theme.text)
            if let cls = row.triageClass {
                Text("class: \(cls)").font(.caption2).foregroundStyle(theme.textMuted)
            }
            if let swap = row.proposedSwap {
                RepairUI.urlDiff(old: swap.oldURL, new: swap.newURL, theme: theme)
                if swap.runtimeOverride == false {
                    Label("old_url isn't matchable by a runtime override — applying records the swap but may not rewrite live traffic.",
                          systemImage: "exclamationmark.triangle")
                        .font(.caption2).foregroundStyle(theme.warning)
                }
            }
            if let pr = row.prURL, let u = URL(string: pr) {
                Link(destination: u) {
                    Label("Open PR", systemImage: "arrow.up.right.square").font(.caption2)
                }.tint(theme.accent)
            }
            if row.isApprovable, onApprove != nil || onReject != nil {
                HStack(spacing: Space.sm) {
                    if onApprove != nil {
                        Button {
                            confirmApprove = true
                        } label: {
                            Label("Approve", systemImage: "checkmark.circle.fill")
                                .font(.caption.weight(.semibold))
                        }
                        .buttonStyle(.borderedProminent).tint(theme.success)
                        .disabled(busy)
                    }
                    if let onReject {
                        Button(role: .destructive) {
                            Task { await onReject() }
                        } label: {
                            Label("Reject", systemImage: "xmark.circle").font(.caption.weight(.semibold))
                        }
                        .buttonStyle(.bordered).tint(theme.danger)
                        .disabled(busy)
                    }
                    if busy { ProgressView().controlSize(.small) }
                }
                .confirmationDialog("Apply this fix?", isPresented: $confirmApprove, titleVisibility: .visible) {
                    Button("Approve & apply") { Task { await onApprove?() } }
                    Button("Cancel", role: .cancel) {}
                } message: {
                    Text("Writes a live runtime URL override for \(row.sourceId) and marks the repair merged.")
                }
            }
        }
        .padding(Space.md)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.md))
        .overlay(RoundedRectangle(cornerRadius: Radius.md)
            .stroke(theme.textMuted.opacity(0.12), lineWidth: Stroke.hairline))
    }
}

// MARK: - Quarantine card

struct QuarantineCard: View {
    let row: QuarantineRow
    let theme: ThemePalette
    let busy: Bool
    let onClear: () async -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            HStack {
                Text(row.name ?? row.sourceId).font(Typography.display(12, weight: .semibold))
                    .foregroundStyle(theme.text)
                Spacer()
                Text((row.active ?? false) ? "ACTIVE" : "EXPIRED")
                    .font(.caption2.weight(.bold))
                    .foregroundStyle((row.active ?? false) ? theme.danger : theme.textMuted)
            }
            if row.name != nil {
                Text(row.sourceId).font(.caption2).foregroundStyle(theme.textMuted)
            }
            if let reason = row.reason {
                Text(reason).font(.caption).foregroundStyle(theme.textMuted)
            }
            HStack(spacing: Space.lg) {
                if let until = row.until {
                    Text("until \(RepairUI.shortTime(until))").font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
                Spacer()
                NavigationLink { SourcePipelineView(sourceId: row.sourceId) } label: {
                    Text("Pipeline").font(.caption2)
                }.tint(theme.accent)
                Button {
                    Task { await onClear() }
                } label: {
                    if busy { ProgressView().controlSize(.small) }
                    else { Label("Clear", systemImage: "lock.open").font(.caption.weight(.semibold)) }
                }
                .buttonStyle(.bordered).tint(theme.accent).disabled(busy)
            }
        }
        .padding(Space.md)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.md))
        .overlay(RoundedRectangle(cornerRadius: Radius.md)
            .stroke(theme.danger.opacity((row.active ?? false) ? 0.3 : 0.0), lineWidth: Stroke.hairline))
    }
}

// MARK: - Shared visual helpers

enum RepairUI {
    static func sectionHeader(_ title: String, count: Int? = nil, theme: ThemePalette) -> some View {
        HStack {
            Text(title.uppercased())
                .font(Typography.display(11, weight: .semibold)).tracking(1.1)
                .foregroundStyle(theme.textMuted)
            if let count { Text("\(count)").font(.caption2.monospacedDigit()).foregroundStyle(theme.textMuted) }
            Spacer()
        }
        .padding(.top, Space.sm)
    }

    static func statCard(_ label: String, _ value: Int, color: Color, theme: ThemePalette) -> some View {
        VStack(spacing: 2) {
            Text("\(value)").font(Typography.display(20, weight: .bold)).foregroundStyle(color)
            Text(label).font(.caption2).foregroundStyle(theme.textMuted)
                .lineLimit(1).minimumScaleFactor(0.7)
        }
        .frame(maxWidth: .infinity).padding(.vertical, Space.md)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.md))
    }

    static func statusBadge(_ status: String, theme: ThemePalette) -> some View {
        let color: Color = {
            switch status {
            case "merged": return theme.success
            case "verified": return theme.warning
            case "rejected": return theme.textMuted
            case "needs_human": return theme.accentAlt
            case "error": return theme.danger
            default: return theme.textMuted
            }
        }()
        return Text(status.uppercased())
            .font(.caption2.weight(.bold))
            .padding(.horizontal, Space.sm).padding(.vertical, 2)
            .background(color.opacity(0.15), in: Capsule())
            .foregroundStyle(color)
    }

    static func verdictBadge(_ verdict: String, theme: ThemePalette) -> some View {
        Text(verdict.uppercased())
            .font(.caption2.weight(.bold))
            .padding(.horizontal, Space.sm).padding(.vertical, 2)
            .background(theme.danger.opacity(0.13), in: Capsule())
            .foregroundStyle(theme.danger)
    }

    static func urlDiff(old: String?, new: String?, theme: ThemePalette) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            if let old {
                Text("− \(old)").font(.caption2.monospaced())
                    .foregroundStyle(theme.danger).lineLimit(2)
            }
            if let new {
                Text("+ \(new)").font(.caption2.monospaced())
                    .foregroundStyle(theme.success).lineLimit(2)
            }
        }
        .padding(Space.sm)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.surface, in: RoundedRectangle(cornerRadius: Radius.sm))
    }

    static func emptyRow(_ text: String, theme: ThemePalette) -> some View {
        Text(text).font(.caption).foregroundStyle(theme.textMuted)
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.vertical, Space.sm)
    }

    static func banner(_ text: String, color: Color, theme: ThemePalette) -> some View {
        Text(text).font(.caption)
            .foregroundStyle(color)
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(Space.sm)
            .background(color.opacity(0.1), in: RoundedRectangle(cornerRadius: Radius.sm))
    }

    /// SQLite `datetime('now')` is "YYYY-MM-DD HH:MM:SS" (UTC). Show the
    /// MM-DD HH:MM tail; ISO strings get the same trim.
    static func shortTime(_ s: String) -> String {
        let t = s.replacingOccurrences(of: "T", with: " ")
        let parts = t.split(separator: " ")
        guard parts.count >= 2 else { return s }
        let date = parts[0].split(separator: "-")
        let time = parts[1].prefix(5)
        if date.count == 3 { return "\(date[1])-\(date[2]) \(time)" }
        return "\(parts[0]) \(time)"
    }
}
