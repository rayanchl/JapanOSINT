import SwiftUI

/// Full detect → triage → repair pipeline for a single collector source.
/// Reads `GET /api/admin/maintenance/source/:id` and renders, newest first:
/// the source's live health, recent fetch runs, anomalies (with LLM triage),
/// and repair propositions. Actionable: approve/reject staged fixes, requeue a
/// stuck anomaly, re-run the collector, and clear a quarantine.
struct SourcePipelineView: View {
    let sourceId: String
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var pipeline: SourcePipeline?
    @State private var loading = false
    @State private var errorMessage: String?
    @State private var actionError: String?
    @State private var rerunning = false
    @State private var clearing = false
    @State private var busyRepairs: Set<Int> = []
    @State private var busyAnomalies: Set<Int> = []

    var body: some View {
        Group {
            if pipeline == nil && !loading && errorMessage != nil {
                OfflineStateView(retry: { Task { await load() } })
            } else {
                content
            }
        }
        .navigationTitle(sourceId)
        .compatInlineTitle()
        .themedScreenBackground(theme)
        .toolbar { toolbarMenu }
        .task { await load() }
    }

    private var content: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: Space.lg) {
                if let actionError {
                    RepairUI.banner(actionError, color: theme.danger, theme: theme)
                }
                if let p = pipeline {
                    header(p.source)
                    fetchSection(p.fetchLog)
                    anomalySection(p.anomalies, repairs: p.repairs)
                    if !p.repairs.isEmpty { repairSection(p.repairs, anomalies: p.anomalies) }
                } else if loading {
                    ProgressView().frame(maxWidth: .infinity).padding(.top, Space.xxl)
                }
            }
            .padding()
        }
        .refreshable { await load() }
    }

    // MARK: Toolbar

    @ToolbarContentBuilder private var toolbarMenu: some ToolbarContent {
        ToolbarItem(placement: .primaryAction) {
            Menu {
                Button {
                    Task { await rerun() }
                } label: { Label("Re-run collector", systemImage: "play.circle") }
                .disabled(rerunning)
                if pipeline?.source.quarantine?.active == true {
                    Button {
                        Task { await clearQuarantine() }
                    } label: { Label("Clear quarantine", systemImage: "lock.open") }
                    .disabled(clearing)
                }
                Button { Task { await load() } } label: { Label("Refresh", systemImage: "arrow.clockwise") }
            } label: {
                if rerunning || clearing { ProgressView().controlSize(.small) }
                else { Image(systemName: "ellipsis.circle") }
            }
        }
    }

    // MARK: Header (live health)

    @ViewBuilder private func header(_ s: PipelineSource) -> some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            HStack {
                statusPill(s.status)
                if let cat = s.category {
                    Text(cat).font(.caption2).foregroundStyle(theme.textMuted)
                }
                Spacer()
                if let t = s.type { Text(t).font(.caption2).foregroundStyle(theme.textMuted) }
            }
            if let n = s.name { Text(n).font(Typography.display(14, weight: .semibold)).foregroundStyle(theme.text) }
            if let q = s.quarantine, q.active == true {
                Label("Quarantined\(q.reason.map { " — \($0)" } ?? "")", systemImage: "lock.fill")
                    .font(.caption).foregroundStyle(theme.danger)
            }
            HStack(spacing: Space.lg) {
                metric("records", s.recordsCount.map(String.init))
                metric("latency", s.responseTimeMs.map { "\($0)ms" })
                metric("ok'd", s.lastSuccess.map(RepairUI.shortTime))
            }
            if let e = s.errorMessage, !e.isEmpty {
                Text(e).font(.caption2.monospaced()).foregroundStyle(theme.danger).lineLimit(3)
            }
            if let u = s.url {
                Text(u).font(.caption2.monospaced()).foregroundStyle(theme.textMuted).lineLimit(1)
            }
        }
        .padding(Space.md)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.md))
    }

    private func metric(_ label: String, _ value: String?) -> some View {
        VStack(alignment: .leading, spacing: 0) {
            Text(value ?? "—").font(Typography.display(12, weight: .medium)).foregroundStyle(theme.text)
            Text(label).font(.caption2).foregroundStyle(theme.textMuted)
        }
    }

    private func statusPill(_ status: String?) -> some View {
        let s = status ?? "unknown"
        let color: Color = {
            switch s {
            case "online": return theme.success
            case "degraded": return theme.warning
            case "offline": return theme.danger
            default: return theme.textMuted
            }
        }()
        return Text(s.uppercased())
            .font(.caption2.weight(.bold))
            .padding(.horizontal, Space.sm).padding(.vertical, 2)
            .background(color.opacity(0.15), in: Capsule()).foregroundStyle(color)
    }

    // MARK: Fetch runs

    @ViewBuilder private func fetchSection(_ runs: [FetchRun]) -> some View {
        RepairUI.sectionHeader("Fetch runs", count: runs.count, theme: theme)
        if runs.isEmpty {
            RepairUI.emptyRow("No recorded runs yet.", theme: theme)
        } else {
            // Health strip — oldest→newest ticks.
            HStack(spacing: 3) {
                ForEach(Array(runs.reversed().enumerated()), id: \.offset) { _, r in
                    RoundedRectangle(cornerRadius: 1)
                        .fill(r.ok ? theme.success : theme.danger)
                        .frame(width: 8, height: 18)
                }
            }
            .padding(.bottom, Space.xs)
            VStack(spacing: 0) {
                ForEach(Array(runs.prefix(12).enumerated()), id: \.offset) { i, r in
                    HStack {
                        Circle().fill(r.ok ? theme.success : theme.danger).frame(width: 6, height: 6)
                        Text(r.timestamp.map(RepairUI.shortTime) ?? "—")
                            .font(.caption2.monospacedDigit()).foregroundStyle(theme.textMuted)
                        Spacer()
                        Text("\(r.recordsFetched ?? 0) rec").font(.caption2).foregroundStyle(theme.textMuted)
                        if let d = r.durationMs {
                            Text("\(d)ms").font(.caption2.monospacedDigit()).foregroundStyle(theme.textMuted)
                        }
                    }
                    .padding(.vertical, 5)
                    if i < min(runs.count, 12) - 1 { Divider().overlay(theme.textMuted.opacity(0.12)) }
                }
            }
            .padding(.horizontal, Space.md)
            .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.md))
        }
    }

    // MARK: Anomalies + triage

    @ViewBuilder private func anomalySection(_ anomalies: [AnomalyRow], repairs: [RepairRow]) -> some View {
        RepairUI.sectionHeader("Anomalies & triage", count: anomalies.count, theme: theme)
        if anomalies.isEmpty {
            RepairUI.emptyRow("No anomalies — pipeline healthy.", theme: theme)
        } else {
            ForEach(anomalies) { a in
                AnomalyCard(anomaly: a, theme: theme,
                            busy: busyAnomalies.contains(a.id),
                            canRequeue: a.isOpen,
                            onRequeue: { await requeue(a) })
            }
        }
    }

    // MARK: Repairs

    @ViewBuilder private func repairSection(_ repairs: [RepairRow], anomalies: [AnomalyRow]) -> some View {
        RepairUI.sectionHeader("Repair propositions", count: repairs.count, theme: theme)
        ForEach(repairs) { r in
            // RepairCard shows Approve/Reject only when `row.isApprovable`, so
            // passing the handlers unconditionally is safe for display-only rows.
            RepairCard(row: r, theme: theme,
                       busy: busyRepairs.contains(r.id),
                       onApprove: { await approve(r) },
                       onReject: { await reject(r) })
        }
    }

    // MARK: Data + actions

    private func load() async {
        loading = true; defer { loading = false }
        errorMessage = nil
        do { pipeline = try await apiClient.api.maintenanceSourceDetail(sourceId) }
        catch { if pipeline == nil { errorMessage = error.localizedDescription } }
    }

    private func rerun() async {
        rerunning = true; defer { rerunning = false }
        actionError = nil
        do { _ = try await apiClient.api.intelRunSource(sourceId); Haptics.success(); await load() }
        catch { actionError = "Re-run failed: \(error.localizedDescription)"; Haptics.error() }
    }

    private func clearQuarantine() async {
        clearing = true; defer { clearing = false }
        actionError = nil
        do { try await apiClient.api.unquarantineSource(sourceId); Haptics.success(); await load() }
        catch { actionError = "Clear failed: \(error.localizedDescription)"; Haptics.error() }
    }

    private func approve(_ r: RepairRow) async {
        busyRepairs.insert(r.id); defer { busyRepairs.remove(r.id) }
        actionError = nil
        do { try await apiClient.api.approveRepair(r.id); Haptics.success(); await load() }
        catch { actionError = "Approve failed: \(error.localizedDescription)"; Haptics.error() }
    }

    private func reject(_ r: RepairRow) async {
        busyRepairs.insert(r.id); defer { busyRepairs.remove(r.id) }
        actionError = nil
        do { try await apiClient.api.rejectRepair(r.id); Haptics.tap(.medium); await load() }
        catch { actionError = "Reject failed: \(error.localizedDescription)"; Haptics.error() }
    }

    private func requeue(_ a: AnomalyRow) async {
        busyAnomalies.insert(a.id); defer { busyAnomalies.remove(a.id) }
        actionError = nil
        do { try await apiClient.api.requeueAnomaly(a.id); Haptics.tap(.medium); await load() }
        catch { actionError = "Requeue failed: \(error.localizedDescription)"; Haptics.error() }
    }
}

// MARK: - Anomaly card

struct AnomalyCard: View {
    let anomaly: AnomalyRow
    let theme: ThemePalette
    let busy: Bool
    let canRequeue: Bool
    let onRequeue: () async -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            HStack(spacing: Space.sm) {
                if let v = anomaly.verdict { RepairUI.verdictBadge(v, theme: theme) }
                if (anomaly.escalationLevel ?? 0) > 0 {
                    Text("esc \(anomaly.escalationLevel!)").font(.caption2.weight(.bold))
                        .foregroundStyle(theme.warning)
                }
                Spacer()
                Text(anomaly.isOpen ? "OPEN" : "RESOLVED")
                    .font(.caption2.weight(.bold))
                    .foregroundStyle(anomaly.isOpen ? theme.warning : theme.textMuted)
            }
            if let r = anomaly.reason {
                Text(r).font(.caption).foregroundStyle(theme.text)
            }
            if let created = anomaly.createdAt {
                Text("opened \(RepairUI.shortTime(created))")
                    .font(.caption2.monospacedDigit()).foregroundStyle(theme.textMuted)
            }
            if let res = anomaly.resolution {
                Text("→ \(res)").font(.caption2).foregroundStyle(theme.textMuted)
            }

            // Triage block
            if anomaly.isTriaged {
                Divider().overlay(theme.textMuted.opacity(0.15))
                HStack(spacing: Space.sm) {
                    Image(systemName: "brain").font(.caption2).foregroundStyle(theme.accentAlt)
                    if let cls = anomaly.triageClass {
                        Text(cls).font(.caption.weight(.semibold)).foregroundStyle(theme.accentAlt)
                    }
                    if let conf = anomaly.triageConfidence {
                        Text("· \(Int(conf * 100))% conf").font(.caption2).foregroundStyle(theme.textMuted)
                    }
                    Spacer()
                    if let m = anomaly.triageModel {
                        Text(m).font(.caption2).foregroundStyle(theme.textMuted).lineLimit(1)
                    }
                }
                if let ev = anomaly.triageEvidence, !ev.isEmpty {
                    Text(ev).font(.caption2).foregroundStyle(theme.textMuted).lineLimit(4)
                }
                if let fix = anomaly.triageSuggestedFix, !fix.isEmpty {
                    Text("suggested fix")
                        .font(.caption2.weight(.semibold)).foregroundStyle(theme.textMuted)
                    Text(fix).font(.caption2.monospaced()).foregroundStyle(theme.text)
                        .padding(Space.sm)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .background(theme.surface, in: RoundedRectangle(cornerRadius: Radius.sm))
                        .lineLimit(4)
                }
            } else if anomaly.isOpen {
                Text("awaiting triage…").font(.caption2).foregroundStyle(theme.textMuted)
            }

            if canRequeue {
                Button {
                    Task { await onRequeue() }
                } label: {
                    if busy { ProgressView().controlSize(.small) }
                    else { Label("Requeue triage", systemImage: "arrow.triangle.2.circlepath").font(.caption.weight(.semibold)) }
                }
                .buttonStyle(.bordered).tint(theme.accent).disabled(busy)
                .padding(.top, 2)
            }
        }
        .padding(Space.md)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.md))
        .overlay(RoundedRectangle(cornerRadius: Radius.md)
            .stroke(theme.warning.opacity(anomaly.isOpen ? 0.25 : 0.0), lineWidth: Stroke.hairline))
    }
}
