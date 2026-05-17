import SwiftUI

// Pipeline queue / diagram for a single OSINT run. The C pipeline
// (native/core/pipeline.c) walks an ordered set of phases, recurses through
// follow-up rounds, runs a per-service queue, and emits gpt_thinking +
// synthesis. SearchRunCard flattened all of that into one bar; this screen
// renders it as a real stage diagram + service queue.

/// The ordered pipeline phases, in the exact order pipeline.c emits them.
/// `error` is terminal and not a row — it marks the stage it failed at.
enum PipelineStage: Int, CaseIterable, Identifiable {
    case queued, gptAnalyzing, servicesAssigned, agentsWorking,
         preliminaryResults, followupAnalyzing, aggregating, completed

    var id: Int { rawValue }

    /// The exact `phase` string the backend sends for this stage.
    var phaseKey: String {
        switch self {
        case .queued:             return "queued"
        case .gptAnalyzing:       return "gpt_analyzing"
        case .servicesAssigned:   return "services_assigned"
        case .agentsWorking:      return "agents_working"
        case .preliminaryResults: return "preliminary_results"
        case .followupAnalyzing:  return "followup_analyzing"
        case .aggregating:        return "aggregating"
        case .completed:          return "completed"
        }
    }

    var title: String {
        switch self {
        case .queued:             return "Queued"
        case .gptAnalyzing:       return "Analyzing query"
        case .servicesAssigned:   return "Services assigned"
        case .agentsWorking:      return "Running services"
        case .preliminaryResults: return "Preliminary results"
        case .followupAnalyzing:  return "Follow-up analysis"
        case .aggregating:        return "Aggregating"
        case .completed:          return "Completed"
        }
    }

    var systemImage: String {
        switch self {
        case .queued:             return "tray"
        case .gptAnalyzing:       return "brain"
        case .servicesAssigned:   return "list.bullet.rectangle"
        case .agentsWorking:      return "antenna.radiowaves.left.and.right"
        case .preliminaryResults: return "doc.text.magnifyingglass"
        case .followupAnalyzing:  return "arrow.triangle.2.circlepath"
        case .aggregating:        return "square.stack.3d.up"
        case .completed:          return "checkmark.seal"
        }
    }
}

enum StageState { case done, current, pending, error }

/// Pure map from a live snapshot to the diagram's per-stage state. Lives here
/// (not in SearchModels) so the model layer stays untouched.
func pipelineStages(for snap: SearchSnapshot?)
    -> [(stage: PipelineStage, state: StageState)] {
    let all = PipelineStage.allCases

    guard let snap else {
        return all.map { ($0, $0 == .queued ? .current : .pending) }
    }
    if snap.done == true || snap.phase == "completed" {
        return all.map { ($0, .done) }
    }
    if snap.phase == "error" {
        // `error` carries no fixed %, so derive where it died from the bar.
        let k = stageOrdinal(forPercent: snap.progress_percent ?? 0)
        return all.map { s in
            if s.rawValue < k { return (s, .done) }
            if s.rawValue == k { return (s, .error) }
            return (s, .pending)
        }
    }
    let k = all.first { $0.phaseKey == snap.phase }?.rawValue
        ?? stageOrdinal(forPercent: snap.progress_percent ?? 0)
    return all.map { s in
        if s.rawValue < k { return (s, .done) }
        if s.rawValue == k { return (s, .current) }
        return (s, .pending)
    }
}

/// Fallback ordinal from progress_percent, using pipeline.c's thresholds.
private func stageOrdinal(forPercent p: Double) -> Int {
    switch p {
    case ..<15:  return PipelineStage.queued.rawValue
    case ..<20:  return PipelineStage.gptAnalyzing.rawValue
    case ..<25:  return PipelineStage.servicesAssigned.rawValue
    case ..<60:  return PipelineStage.agentsWorking.rawValue
    case ..<85:  return PipelineStage.preliminaryResults.rawValue
    case ..<90:  return PipelineStage.followupAnalyzing.rawValue
    case ..<100: return PipelineStage.aggregating.rawValue
    default:     return PipelineStage.completed.rawValue
    }
}

private func roundSuffix(_ s: SearchSnapshot?) -> String {
    guard let r = s?.current_round, r > 0 else { return "" }
    return " · round \(r)/\(s?.max_rounds ?? 5)"
}

// MARK: - Stage diagram

private struct StageRow: View {
    let stage: PipelineStage
    let state: StageState
    let isLast: Bool
    let connectorDone: Bool
    let roundSuffix: String
    @Environment(\.theme) private var theme

    var body: some View {
        HStack(alignment: .top, spacing: Space.md) {
            VStack(spacing: 0) {
                Image(systemName: glyph)
                    .font(.system(size: 16))
                    .foregroundStyle(tint)
                    .frame(width: 22, height: 22)
                if !isLast {
                    Rectangle()
                        .fill(connectorDone
                              ? theme.success
                              : theme.textMuted.opacity(0.3))
                        .frame(width: 2, height: 18)
                }
            }
            Text(stage.title + roundSuffix)
                .font(state == .pending ? Typography.body(13) : Typography.body(13, weight: .semibold))
                .foregroundStyle(state == .pending ? theme.textMuted : theme.text)
                .padding(.top, 2)
            Spacer(minLength: 0)
        }
    }

    private var glyph: String {
        switch state {
        case .done:    return "checkmark.circle.fill"
        case .current: return stage.systemImage
        case .pending: return "circle"
        case .error:   return "xmark.octagon.fill"
        }
    }
    private var tint: Color {
        switch state {
        case .done:    return theme.success
        case .current: return theme.accentAlt
        case .pending: return theme.textMuted
        case .error:   return theme.danger
        }
    }
}

private struct StageDiagram: View {
    let snapshot: SearchSnapshot?
    var body: some View {
        let rows = pipelineStages(for: snapshot)
        let suffix = roundSuffix(snapshot)
        VStack(alignment: .leading, spacing: 0) {
            ForEach(Array(rows.enumerated()), id: \.element.stage.id) { i, r in
                StageRow(
                    stage: r.stage,
                    state: r.state,
                    isLast: i == rows.count - 1,
                    connectorDone: r.state == .done
                        && i + 1 < rows.count && rows[i + 1].state == .done,
                    roundSuffix: r.stage == .followupAnalyzing ? suffix : ""
                )
            }
        }
    }
}

// MARK: - Service queue

private struct ServiceQueueSection: View {
    let services: [ServiceProgress]
    @Environment(\.theme) private var theme

    var body: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            Text("SERVICE QUEUE").font(Typography.h3).foregroundStyle(theme.textMuted)
            ForEach(services) { s in
                HStack(spacing: Space.sm) {
                    Pill(text: s.name, tone: .neutral, maxWidth: 160)
                    Pill(text: s.status, tone: tone(s.status))
                    if let msg = s.status_message, !msg.isEmpty {
                        Text(msg)
                            .font(Typography.body(11))
                            .foregroundStyle(theme.textMuted)
                            .lineLimit(2)
                    }
                    Spacer(minLength: 0)
                    if let n = s.results_count, n > 0 {
                        Pill(text: "\(n)", tone: .info, solid: true)
                    }
                }
            }
        }
    }

    private func tone(_ status: String) -> Pill.Tone {
        switch status {
        case "running":             return .info
        case "completed":           return .success
        case "failed", "error":     return .danger
        default:                    return .neutral
        }
    }
}

// MARK: - Reasoning + synthesis

private struct ThinkingSection: View {
    let text: String
    @Environment(\.theme) private var theme
    var body: some View {
        DisclosureGroup {
            Text(text)
                .font(Typography.body(12))
                .foregroundStyle(theme.textMuted)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(Space.md)
                .background(RoundedRectangle(cornerRadius: Radius.md)
                    .fill(theme.surfaceElevated))
                .padding(.top, Space.xs)
        } label: {
            Text("LLM reasoning").font(Typography.h3).foregroundStyle(theme.textMuted)
        }
        .tint(theme.textMuted)
    }
}

private struct SynthesisSection: View {
    let text: String
    @Environment(\.theme) private var theme
    var body: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            Text("SYNTHESIS").font(Typography.h3).foregroundStyle(theme.textMuted)
            Text(text).font(Typography.body(13)).foregroundStyle(theme.text)
        }
    }
}

// MARK: - Detail screen

struct SearchRunDetailView: View {
    let id: String
    @ObservedObject var store: SearchStore
    let onPivot: (String) -> Void
    @Environment(\.theme) private var theme

    private var run: SearchStore.Run? {
        store.active.first { $0.id == id } ?? store.completed.first { $0.id == id }
    }

    var body: some View {
        let snap = run?.snapshot
        ScrollView {
            VStack(alignment: .leading, spacing: Space.lg) {
                header(snap)
                StageDiagram(snapshot: snap)

                if let svcs = snap?.services, !svcs.isEmpty {
                    ServiceQueueSection(services: svcs)
                }

                let ents = (snap?.entities ?? []) + (snap?.discovered_entities ?? [])
                if !ents.isEmpty {
                    VStack(alignment: .leading, spacing: Space.sm) {
                        Text("ENTITIES").font(Typography.h3).foregroundStyle(theme.textMuted)
                        FlowEntities(entities: ents, onPivot: onPivot)
                    }
                }

                if let t = snap?.gpt_thinking, !t.isEmpty {
                    ThinkingSection(text: t)
                }
                if let syn = snap?.results?.synthesis, !syn.isEmpty {
                    SynthesisSection(text: syn)
                }
            }
            .padding(Space.lg)
        }
        .background(theme.surface.ignoresSafeArea())
        .navigationTitle(snap?.query ?? run?.query ?? "Investigation")
        .navigationBarTitleDisplayMode(.inline)
    }

    private func header(_ s: SearchSnapshot?) -> some View {
        let pct = s?.progress_percent ?? 0
        let isError = s?.phase == "error"
        return VStack(alignment: .leading, spacing: Space.sm) {
            HStack {
                Text(s?.query ?? run?.query ?? "")
                    .font(Typography.body(15, weight: .semibold)).lineLimit(2)
                Spacer()
                Text("\(Int(pct))%")
                    .font(Typography.monoSmall).foregroundStyle(theme.accentAlt)
            }
            ProgressView(value: pct / 100.0)
                .tint(isError ? theme.danger : theme.accentAlt)
        }
    }
}
