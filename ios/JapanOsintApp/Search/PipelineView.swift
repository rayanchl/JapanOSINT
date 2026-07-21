import SwiftUI
import Combine

// Pipeline queue / diagram for a single OSINT run. The C pipeline
// (native/core/pipeline.c) walks an ordered set of phases, recurses through
// follow-up rounds, runs a per-service queue, and emits gpt_thinking +
// synthesis. This screen renders it as a real stage diagram + service queue,
// matching the OSINTsaas web search layout: every stage is held on screen for
// at least one second (StageAnimator), the in-progress stage shows a spinner,
// services drill into their per-entity calls, and entities cross-link to the
// services that called for / discovered them.
//
// Shared verbatim by the iOS and macOS targets (one multiplatform target).

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

    /// Lower-bound progress % for this stage (pipeline.c thresholds). Used to
    /// drive the header bar from the *displayed* stage so the bar and the
    /// walked timeline never disagree.
    var percentFloor: Double {
        switch self {
        case .queued:             return 5
        case .gptAnalyzing:       return 15
        case .servicesAssigned:   return 20
        case .agentsWorking:      return 25
        case .preliminaryResults: return 60
        case .followupAnalyzing:  return 75
        case .aggregating:        return 90
        case .completed:          return 100
        }
    }
}

enum StageState { case done, current, pending, error }

/// Pure map from a live snapshot to the diagram's per-stage state. Lives here
/// (not in SearchModels) so the model layer stays untouched. Used by the
/// collapsed run card; the detail screen drives state off `StageAnimator`.
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

/// The stage index the backend currently implies (the `.current`/`.error` row,
/// or the last stage when everything is done).
private func targetStageIndex(for snap: SearchSnapshot?) -> Int {
    let rows = pipelineStages(for: snap)
    if let i = rows.firstIndex(where: { $0.state == .current || $0.state == .error }) { return i }
    return rows.count - 1
}

private func roundSuffix(_ s: SearchSnapshot?) -> String {
    guard let r = s?.current_round, r > 0 else { return "" }
    return " · round \(r)/\(s?.max_rounds ?? 5)"
}

// MARK: - Stage animator

/// Walks the *displayed* stage forward one step at a time, holding each for at
/// least `stepInterval`, so every stage is legible even when the backend jumps
/// several phases between snapshots. Forward-only; a run opened from history
/// (already finished) seeds straight to "done" instead of replaying.
@MainActor
final class StageAnimator: ObservableObject {
    @Published private(set) var shown = 0
    @Published private(set) var finished = false
    @Published private(set) var errored = false

    private var target = 0
    private var runner: Task<Void, Never>?
    private let last = PipelineStage.allCases.count - 1
    static let stepInterval: TimeInterval = 1.0

    /// Historical / already-complete run: show the whole pipeline as done with
    /// no animation.
    func seedDone() {
        shown = last; target = last; finished = true; errored = false
    }

    /// Reopened mid-flight run: jump straight to the stage it's already on with
    /// no walk, so the steps it finished before the card was closed don't
    /// visually replay from "Queued". Forward motion from here is still
    /// animated by `update`.
    func seedAt(_ index: Int, errored isError: Bool) {
        let resolved = min(max(index, 0), last)
        shown = resolved; target = resolved
        finished = false; errored = isError
    }

    /// Live update from the latest snapshot. Monotonic: the target only moves
    /// forward, so out-of-order or regressing frames can't rewind the diagram.
    func update(target newTarget: Int, finished isFinished: Bool, errored isError: Bool) {
        errored = isError
        finished = isFinished
        let resolved = (isFinished && !isError) ? last : newTarget
        target = max(target, resolved)
        ensureRunning()
    }

    private func ensureRunning() {
        guard runner == nil, shown < target else { return }
        runner = Task { @MainActor [weak self] in
            guard let self else { return }
            while shown < target {
                try? await Task.sleep(nanoseconds: UInt64(Self.stepInterval * 1_000_000_000))
                if shown < target { shown += 1 }
            }
            runner = nil
            if shown < target { ensureRunning() }   // target advanced mid-walk
        }
    }
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
                // The in-progress stage shows a live spinner before its name so
                // "working" reads at a glance (mirrors the web ClaudeLoader).
                // Each state's glyph crossfades into the next (empty circle →
                // spinner → checkmark) instead of hard-swapping, so a row
                // advancing reads as a soft fade rather than a pop.
                ZStack {
                    if state == .current {
                        ProgressView().controlSize(.small).tint(theme.accentAlt)
                            .transition(.opacity)
                    } else {
                        Image(systemName: glyph)
                            .font(.system(size: 16))
                            .foregroundStyle(tint)
                            .transition(.opacity)
                            .id(glyph)   // circle→checkmark also crossfades
                    }
                }
                .frame(width: 22, height: 22)
                .animation(.easeInOut(duration: 0.32), value: state)
                if !isLast {
                    Rectangle()
                        .fill(connectorDone ? theme.success : theme.textMuted.opacity(0.3))
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
        case .current: return stage.systemImage   // unused (spinner shown)
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
    let shown: Int
    let finished: Bool
    let errored: Bool
    let roundSuffix: String

    private var rows: [(stage: PipelineStage, state: StageState)] {
        let last = PipelineStage.allCases.count - 1
        return PipelineStage.allCases.map { s in
            if errored && s.rawValue == shown { return (s, .error) }
            if s.rawValue < shown { return (s, .done) }
            if s.rawValue == shown {
                return (s, (finished && shown == last) ? .done : .current)
            }
            return (s, .pending)
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            ForEach(Array(rows.enumerated()), id: \.element.stage.id) { i, r in
                StageRow(
                    stage: r.stage,
                    state: r.state,
                    isLast: i == rows.count - 1,
                    connectorDone: r.state == .done
                        && i + 1 < rows.count && rows[i + 1].state != .pending,
                    roundSuffix: r.stage == .followupAnalyzing ? roundSuffix : ""
                )
            }
        }
        // Fade the whole diagram's state forward (icons + connector colors)
        // as the animator walks `shown`, so each step settles softly.
        .animation(.easeInOut(duration: 0.32), value: shown)
        .animation(.easeInOut(duration: 0.32), value: finished)
        .animation(.easeInOut(duration: 0.32), value: errored)
    }
}

// MARK: - Entity ↔ service cross-linking

/// SF Symbol for an entity type (mirrors the web `getEntityIcon`).
func entityIcon(for type: String) -> String {
    switch type.lowercased() {
    case "ip":                                   return "server.rack"
    case "email":                                return "envelope"
    case "domain", "url":                        return "globe"
    case "phone":                                return "phone"
    case "person", "name":                       return "person"
    case "company", "organization":              return "building.2"
    case "address", "location", "coordinates", "coords":
                                                 return "mappin.and.ellipse"
    case "username":                             return "at"
    case "asn":                                  return "network"
    case "timezone":                             return "clock"
    case "hash":                                 return "number"
    case "crypto", "cryptocurrency":             return "bitcoinsign.circle"
    case "cik":                                  return "number.square"
    case "keyword":                              return "magnifyingglass"
    default:                                     return "tag"
    }
}

/// Read-only cross-link queries over a single snapshot. All matching is
/// case-insensitive; service `data` is the JSON string the backend sent.
enum SearchLinks {
    /// Service names whose result `data` referenced this entity (i.e. the
    /// service that surfaced it). Prefers the explicit `discovered_by`.
    static func discoveredBy(_ e: EntityRef, in snap: SearchSnapshot?) -> String? {
        if let d = e.discovered_by, !d.isEmpty { return d }
        return snap?.discovered_entities?.first {
            $0.type.lowercased() == e.type.lowercased() && $0.value == e.value
        }?.discovered_by
    }

    /// Services that were *called for* this entity value.
    static func servicesCalledFor(_ value: String, in snap: SearchSnapshot?) -> [String] {
        let v = value.lowercased()
        var names: [String] = []
        for s in snap?.services ?? [] where (s.entities ?? "").lowercased().contains(v) {
            names.append(s.displayName)
        }
        for r in snap?.results?.services ?? [] where (r.entity ?? "").lowercased() == v {
            let n = displayName(r.name)
            if !names.contains(n) { names.append(n) }
        }
        return names
    }

    /// Services whose result payload mentioned this entity again (besides the
    /// one that first discovered it).
    static func encounteredAgainBy(_ value: String, in snap: SearchSnapshot?) -> [String] {
        let v = value.lowercased()
        var names: [String] = []
        for r in snap?.results?.services ?? [] {
            guard let data = r.data, data.lowercased().contains(v) else { continue }
            let n = displayName(r.name)
            if !names.contains(n) { names.append(n) }
        }
        return names
    }

    /// Entities a given service discovered (these feed follow-up rounds).
    static func entitiesDiscoveredBy(_ service: String, in snap: SearchSnapshot?) -> [EntityRef] {
        (snap?.discovered_entities ?? []).filter {
            displayName($0.discovered_by ?? "") == service
        }
    }

    /// Per-entity result calls of a service (entity, success, error).
    static func callsOf(_ service: String, in snap: SearchSnapshot?) -> [ServiceResult] {
        (snap?.results?.services ?? []).filter { displayName($0.name) == service }
    }

    /// Underlying sources/providers a service hit, aggregated across all its
    /// per-entity calls (deduped by source name, records summed, any non-ok
    /// status wins).
    static func sourcesOf(_ service: String, in snap: SearchSnapshot?) -> [ServiceSource] {
        var order: [String] = []
        var acc: [String: (status: String, records: Int, detail: String?)] = [:]
        for r in snap?.results?.services ?? [] where displayName(r.name) == service {
            for s in r.sources ?? [] {
                let st = s.status ?? "ok"
                if var cur = acc[s.name] {
                    cur.records += s.records ?? 0
                    if st != "ok" { cur.status = st }
                    if cur.detail == nil { cur.detail = s.detail }
                    acc[s.name] = cur
                } else {
                    order.append(s.name)
                    acc[s.name] = (st, s.records ?? 0, s.detail)
                }
            }
        }
        return order.map { ServiceSource(name: $0, status: acc[$0]?.status,
                                         records: acc[$0]?.records, detail: acc[$0]?.detail) }
    }

    /// JP_CORPUS_LOOKUP surfaces as "DB SEARCH" everywhere.
    static func displayName(_ raw: String) -> String {
        raw == "JP_CORPUS_LOOKUP" ? "DB SEARCH" : raw
    }
}

// MARK: - Two-phase disclosure

/// Sequenced expand/collapse used by the service + entity cards. On expand the
/// card grows to full height first (ease-in-out) and only once it has settled
/// does the content fade in; on collapse the content fades out first, then the
/// card shrinks. The content is clipped to the (animating) bounds throughout,
/// so it never spills past the card edge mid-animation ("no content over
/// bounds"). Shared verbatim by the iOS and macOS targets.
private struct ExpandReveal<Content: View>: View {
    let isExpanded: Bool
    @ViewBuilder var content: Content

    private static var dur: Double { 0.22 }

    /// Content occupies layout (drives the card height). On collapse it lags
    /// `isExpanded` so the fade-out completes before the height shrinks.
    @State private var present = false
    /// Content opacity. On expand it lags `present` so the grow completes
    /// before the fade-in.
    @State private var visible = false

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            if present {
                content.opacity(visible ? 1 : 0)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .clipped()                                   // never draw past the bounds
        .onAppear { present = isExpanded; visible = isExpanded }   // seed, no replay
        .onChange(of: isExpanded) { _, exp in sequence(to: exp) }
    }

    private func sequence(to exp: Bool) {
        let a = Animation.easeInOut(duration: Self.dur)
        if exp {
            // 1) grow the card (content present but transparent) …
            withAnimation(a) { present = true } completion: {
                guard isExpanded else { return }     // tapped closed mid-grow
                withAnimation(a) { visible = true }  // 2) … then fade content in
            }
        } else {
            // 1) fade the content out …
            withAnimation(a) { visible = false } completion: {
                guard !isExpanded else { return }    // tapped open mid-fade
                withAnimation(a) { present = false } // 2) … then collapse the card
            }
        }
    }
}

// MARK: - Service queue (expandable)

private struct ServiceQueueSection: View {
    let services: [ServiceProgress]
    let snapshot: SearchSnapshot?
    @Binding var expanded: Set<String>
    @Environment(\.theme) private var theme

    var body: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            Text("SERVICE QUEUE").font(Typography.h3).foregroundStyle(theme.textMuted)
            VStack(alignment: .leading, spacing: Space.xs) {
                ForEach(services) { s in serviceEntry(s) }
            }
        }
    }

    /// One service row, plus its expanded results. When expanded the whole
    /// entry sits on a dark-gray card so the row and its results read as a
    /// single group. The header keeps the stable 4-"column" shape: name |
    /// status | message | meta — the message flexes to fill, and the trailing
    /// meta cluster reserves fixed slots (follow-up tag, count, chevron) so
    /// streaming rows never reflow (no flicker).
    @ViewBuilder
    private func serviceEntry(_ s: ServiceProgress) -> some View {
        let sources = SearchLinks.sourcesOf(s.displayName, in: snapshot)
        let calls = SearchLinks.callsOf(s.displayName, in: snapshot)
        let found = SearchLinks.entitiesDiscoveredBy(s.displayName, in: snapshot)
        let canExpand = !sources.isEmpty || !calls.isEmpty || !found.isEmpty
        let isExpanded = expanded.contains(s.id)
        VStack(alignment: .leading, spacing: Space.sm) {
            HStack(spacing: Space.sm) {
                Pill(text: s.displayName, tone: .neutral)
                    .fixedSize(horizontal: true, vertical: false)
                statusPill(s.status)
                Text((s.status_message?.isEmpty == false) ? s.status_message! : " ")
                    .font(Typography.body(11)).foregroundStyle(theme.textMuted)
                    .lineLimit(1)
                    .frame(maxWidth: .infinity, alignment: .leading)
                metaCell(for: s, canExpand: canExpand)
            }
            // The whole header row is the tap target (not just the chevron), so
            // tapping anywhere on a service expands/collapses it.
            .contentShape(Rectangle())
            .onTapGesture { if canExpand { toggle(s.id) } }
            ExpandReveal(isExpanded: isExpanded) {
                detail(sources: sources, calls: calls, found: found)
            }
        }
        // Pad in both states so the header row (name | status | message | meta)
        // keeps its expanded-mode position when collapsed — toggling never
        // shifts the service name or chevron. Only the background appears on
        // expand, so the card visual is still expansion-only.
        .padding(Space.sm)
        .background(
            RoundedRectangle(cornerRadius: Radius.md)
                .fill(isExpanded ? theme.surfaceElevated : Color.clear)
                // Card grows first (ExpandReveal). On collapse, hold the card
                // background until the content has faded out AND the height has
                // finished shrinking (≈2×0.22s), so the content never sits on a
                // bare background mid-collapse; on expand the card appears at once.
                .animation(.easeInOut(duration: 0.22).delay(isExpanded ? 0 : 0.44),
                           value: isExpanded)
        )
    }

    /// Trailing meta column: follow-up tag + result count + expand chevron.
    /// Each slot is always present (the follow-up pill is laid out but
    /// transparent when the row isn't a follow-up, the count/chevron reserve a
    /// fixed width) so the column's width never changes — no reflow flicker as
    /// rows stream in.
    @ViewBuilder
    private func metaCell(for s: ServiceProgress, canExpand: Bool) -> some View {
        HStack(spacing: Space.xs) {
            Pill(text: "follow-up", tone: .accent)
                .fixedSize(horizontal: true, vertical: false)
                .opacity(s.is_followup == true ? 1 : 0)
                .accessibilityHidden(s.is_followup != true)

            Group {
                if let n = s.results_count, n > 0 {
                    Pill(text: "\(n)", tone: .info, solid: true)
                }
            }
            .frame(minWidth: 26, alignment: .trailing)

            // Chevron is a plain indicator — the whole row handles the tap.
            // It is ALWAYS in the layout (fixed 14pt slot) and only its opacity
            // flips when the row becomes expandable — inserting/removing the
            // view would reflow the row mid-stream, so we fade instead (same
            // pattern as the follow-up pill above).
            Image(systemName: expanded.contains(s.id) ? "chevron.up" : "chevron.down")
                .font(.caption2).foregroundStyle(theme.textMuted)
                .frame(width: 14)
                .opacity(canExpand ? 1 : 0)
                .accessibilityHidden(!canExpand)
        }
    }

    private func toggle(_ id: String) {
        if expanded.contains(id) { expanded.remove(id) } else { expanded.insert(id) }
    }

    @ViewBuilder
    private func detail(sources: [ServiceSource],
                        calls: [ServiceResult], found: [EntityRef]) -> some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            // Sources the service hit + each one's response.
            linkLabel(icon: "point.3.connected.trianglepath.dotted", "Sources")
            if sources.isEmpty {
                Text("No source attribution (populates when the run completes).")
                    .font(Typography.body(11)).foregroundStyle(theme.textMuted)
            } else {
                ForEach(sources) { src in
                    HStack(spacing: Space.sm) {
                        Image(systemName: src.ok ? "checkmark.circle.fill" : "xmark.circle.fill")
                            .font(.caption2).foregroundStyle(src.ok ? theme.success : theme.danger)
                        Text(src.name).font(Typography.body(11)).foregroundStyle(theme.text).lineLimit(1)
                        if let d = src.detail, !d.isEmpty {
                            Text(d).font(Typography.body(10)).foregroundStyle(theme.danger).lineLimit(1)
                        }
                        Spacer(minLength: 0)
                        if let n = src.records, n > 0 {
                            Text("\(n) rec").font(Typography.monoSmall).foregroundStyle(theme.textMuted)
                        }
                    }
                }
            }
            if !calls.isEmpty {
                linkLabel(icon: "arrow.triangle.branch", "Calls")
                ForEach(calls) { c in
                    VStack(alignment: .leading, spacing: Space.xs) {
                        HStack(spacing: Space.sm) {
                            Image(systemName: (c.success ?? false) ? "checkmark.circle.fill" : "xmark.circle.fill")
                                .font(.caption2)
                                .foregroundStyle((c.success ?? false) ? theme.success : theme.danger)
                            Text(c.entity ?? "—").font(Typography.body(11)).foregroundStyle(theme.text)
                            if let e = c.error, !e.isEmpty {
                                Text(e).font(Typography.body(10)).foregroundStyle(theme.danger).lineLimit(1)
                            }
                            Spacer(minLength: 0)
                            if let conf = c.confidence, conf > 0 {
                                Text("\(Int(conf))%").font(Typography.monoSmall).foregroundStyle(theme.textMuted)
                            }
                        }
                        // The actual fetched payload from this service call.
                        // The client already receives it; show it so a call's
                        // result is visible, not just its success/confidence.
                        if let payload = prettyPayload(c.data) {
                            Text(payload)
                                .font(Typography.monoSmall)
                                .foregroundStyle(theme.textMuted)
                                .textSelection(.enabled)
                                .lineLimit(12)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .padding(Space.sm)
                                .background(RoundedRectangle(cornerRadius: Radius.sm).fill(theme.surface))
                        }
                    }
                }
            }
            if !found.isEmpty {
                linkLabel(icon: "sparkles", "Discovered (fed follow-up)")
                FlowLayout(spacing: 6) {
                    ForEach(found) { e in
                        HStack(spacing: 3) {
                            Image(systemName: entityIcon(for: e.type)).font(.caption2)
                            Text(e.value).font(.caption2).lineLimit(1)
                        }
                        .padding(.horizontal, Space.sm - 2).padding(.vertical, 2)
                        .foregroundStyle(theme.accentAlt)
                        .background(theme.accentAlt.opacity(0.15), in: Capsule())
                    }
                }
            }
        }
        // Sits directly on the entry's dark-gray card; spacing alone sets it off
        // from the header row, so it reads as one smooth group (no divider).
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(.top, Space.xs)
    }

    private func linkLabel(icon: String, _ text: String) -> some View {
        Label(text, systemImage: icon)
            .font(Typography.body(10, weight: .semibold))
            .foregroundStyle(theme.textMuted)
    }

    /// Pretty-print a service call's `data` payload for display. Returns nil for
    /// empty / null / unusable payloads so callers can skip the block entirely.
    private func prettyPayload(_ raw: String?) -> String? {
        guard let raw, !raw.isEmpty, raw != "null" else { return nil }
        let trimmed = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return nil }
        if let d = trimmed.data(using: .utf8),
           let obj = try? JSONSerialization.jsonObject(with: d),
           let pretty = try? JSONSerialization.data(
               withJSONObject: obj, options: [.prettyPrinted, .sortedKeys]),
           let out = String(data: pretty, encoding: .utf8) {
            return out
        }
        return trimmed   // not JSON — show as-is
    }

    /// Status chip. Terminal states (completed / failed / error) collapse to an
    /// icon-only pill — their meaning reads from the glyph + color alone, and
    /// dropping the word frees row width so the service name and follow-up tag
    /// aren't truncated. In-progress states keep their label.
    @ViewBuilder
    private func statusPill(_ status: String) -> some View {
        let iconOnly = status == "completed" || status == "failed" || status == "error"
        Pill(text: iconOnly ? "" : status,
             tone: tone(status),
             icon: statusIcon(status))
            .fixedSize(horizontal: true, vertical: false)
    }

    private func statusIcon(_ status: String) -> String {
        switch status {
        case "running":         return "bolt.fill"
        case "completed":       return "checkmark"
        case "failed", "error": return "xmark"
        case "skipped":         return "arrow.right.to.line"
        default:                return "ellipsis"
        }
    }
    private func tone(_ status: String) -> Pill.Tone {
        switch status {
        case "running":         return .info
        case "completed":       return .success
        case "failed", "error": return .danger
        default:                return .neutral
        }
    }
}

// MARK: - Entities (icon pills + cross-link expansion)

private struct EntitySection: View {
    let entities: [EntityRef]
    let snapshot: SearchSnapshot?
    @Binding var expanded: Set<String>
    let onPivot: (String) -> Void
    @Environment(\.theme) private var theme

    var body: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            Text("ENTITIES").font(Typography.h3).foregroundStyle(theme.textMuted)
            VStack(alignment: .leading, spacing: Space.sm) {
                ForEach(dedup(entities)) { e in
                    EntityCrossLinkRow(
                        entity: e,
                        snapshot: snapshot,
                        isExpanded: expanded.contains(e.id),
                        toggle: {
                            if expanded.contains(e.id) { expanded.remove(e.id) }
                            else { expanded.insert(e.id) }
                        },
                        onPivot: onPivot
                    )
                }
            }
        }
    }

    private func dedup(_ a: [EntityRef]) -> [EntityRef] {
        var seen = Set<String>(); return a.filter { seen.insert($0.id).inserted }
    }
}

private struct EntityCrossLinkRow: View {
    let entity: EntityRef
    let snapshot: SearchSnapshot?
    let isExpanded: Bool
    let toggle: () -> Void
    let onPivot: (String) -> Void
    @Environment(\.theme) private var theme

    private var discoveredBy: String? { SearchLinks.discoveredBy(entity, in: snapshot) }
    private var calledFor: [String] { SearchLinks.servicesCalledFor(entity.value, in: snapshot) }
    private var againBy: [String] { SearchLinks.encounteredAgainBy(entity.value, in: snapshot) }
    private var canExpand: Bool { discoveredBy != nil || !calledFor.isEmpty || !againBy.isEmpty }

    var body: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            Button { if canExpand { toggle() } } label: {
                HStack(spacing: Space.sm) {
                    // Discovered entities use the alt tone; query entities the accent.
                    HStack(spacing: 3) {
                        Image(systemName: entityIcon(for: entity.type)).font(.caption2)
                        Text(entity.type.uppercased()).font(.caption2.bold())
                    }
                    .padding(.horizontal, Space.sm - 2).padding(.vertical, 2)
                    .foregroundStyle(discoveredBy != nil ? theme.accentAlt : theme.accent)
                    .background((discoveredBy != nil ? theme.accentAlt : theme.accent).opacity(0.18),
                                in: Capsule())

                    Text(entity.value).font(Typography.body(13)).foregroundStyle(theme.text)
                        .lineLimit(1)
                    Spacer(minLength: 0)
                    // Chevron is always in the layout (fixed 14pt slot); only
                    // its opacity flips when the row becomes expandable, so the
                    // value never shifts when it appears mid-stream.
                    Image(systemName: isExpanded ? "chevron.up" : "chevron.down")
                        .font(.caption2).foregroundStyle(theme.textMuted)
                        .frame(width: 14)
                        .opacity(canExpand ? 1 : 0)
                        .accessibilityHidden(!canExpand)
                }
                // Tap anywhere on the row (incl. the empty spacer) to toggle.
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)

            ExpandReveal(isExpanded: isExpanded) {
                expansion
            }
        }
        // Pad in both states so the entity header (type pill | value | chevron)
        // keeps its expanded-mode position when collapsed — toggling never
        // shifts the entity name or chevron. Only the background appears on
        // expand, so the card visual is still expansion-only.
        .padding(Space.sm)
        .background(
            RoundedRectangle(cornerRadius: Radius.md)
                .fill(isExpanded ? theme.surfaceElevated : Color.clear)
                // Hold the card background through the whole collapse (content
                // fade-out + height shrink, ≈2×0.22s) so the content never sits
                // on a bare background; on expand the card appears at once.
                .animation(.easeInOut(duration: 0.22).delay(isExpanded ? 0 : 0.44),
                           value: isExpanded)
        )
    }

    private var expansion: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            if let d = discoveredBy {
                chipLine(icon: "sparkles", "Discovered by", chips: [d], tone: .neutral)
            }
            if !calledFor.isEmpty {
                chipLine(icon: "gearshape", "Services called for", chips: calledFor, tone: .neutral)
            }
            if !againBy.isEmpty {
                chipLine(icon: "magnifyingglass", "Encountered again by", chips: againBy, tone: .info)
            }
            HStack(spacing: Space.md) {
                NavigationLink {
                    EntityDetailView(type: entity.type.lowercased(), lookup: entity.value)
                } label: {
                    Label("Open profile", systemImage: "arrow.up.right.square").font(.caption2)
                }
                Button { onPivot(entity.value) } label: {
                    Label("Pivot", systemImage: "arrow.triangle.branch").font(.caption2)
                }
                .buttonStyle(.plain).foregroundStyle(theme.accentAlt)
            }
            .padding(.top, 2)
        }
        // Sits directly on the entry's dark-gray card; spacing alone sets it off
        // from the header row, so it reads as one smooth group (no divider).
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(.top, Space.xs)
    }

    private func chipLine(icon: String, _ label: String, chips: [String], tone: Pill.Tone) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Label(label, systemImage: icon)
                .font(Typography.body(10, weight: .semibold)).foregroundStyle(theme.textMuted)
            FlowLayout(spacing: 6) {
                ForEach(chips, id: \.self) { Pill(text: $0, tone: tone) }
            }
        }
    }
}

// MARK: - Reasoning + synthesis

private struct ThinkingSection: View {
    let text: String
    @Environment(\.theme) private var theme
    /// Expanded by default: the reasoning is the point of the screen, not a
    /// footnote. (Frontend-only fix — the backend keeps the latest round's
    /// analysis; we no longer hide it behind a collapsed disclosure.)
    @State private var expanded = true

    var body: some View {
        VStack(alignment: .leading, spacing: Space.xs) {
            Button { expanded.toggle() } label: {
                HStack {
                    Text("LLM REASONING").font(Typography.h3).foregroundStyle(theme.textMuted)
                    Spacer()
                    Image(systemName: expanded ? "chevron.up" : "chevron.down")
                        .font(.caption2).foregroundStyle(theme.textMuted)
                }
            }
            .buttonStyle(.plain)
            if expanded {
                Text(text)
                    .font(Typography.body(12))
                    .foregroundStyle(theme.text)
                    .textSelection(.enabled)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(Space.md)
                    .background(RoundedRectangle(cornerRadius: Radius.md).fill(theme.surfaceElevated))
            }
        }
    }
}

private struct SynthesisSection: View {
    let text: String
    @Environment(\.theme) private var theme
    var body: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            Text("FINAL ANALYSIS").font(Typography.h3).foregroundStyle(theme.textMuted)
            Text(text)
                .font(Typography.body(16))
                .foregroundStyle(theme.text)
                .textSelection(.enabled)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(Space.md)
                .background(RoundedRectangle(cornerRadius: Radius.md).fill(theme.surfaceElevated))
        }
    }
}

// MARK: - Detail screen

struct SearchRunDetailView: View {
    let id: String
    @ObservedObject var store: SearchStore
    let onPivot: (String) -> Void
    @Environment(\.theme) private var theme

    @StateObject private var animator = StageAnimator()
    @State private var didSeed = false
    @State private var expandedServices: Set<String> = []
    @State private var expandedEntities: Set<String> = []
    /// Collapses the pipeline stage list (queued / analyzing / services
    /// assigned / …) to just a one-line current-stage summary.
    @State private var stagesCollapsed = false

    private var run: SearchStore.Run? {
        store.active.first { $0.id == id } ?? store.completed.first { $0.id == id }
    }

    var body: some View {
        let snap = run?.snapshot
        ScrollView {
            // Extra breathing room between category sections (pipeline, service
            // queue, entities, synthesis) so they read as distinct blocks.
            VStack(alignment: .leading, spacing: Space.xl) {
                header(snap)
                stageSection(snap)

                // Final analysis is the payoff — surface it first, right under
                // the pipeline steps, as soon as it has been computed.
                if let syn = snap?.results?.synthesis, !syn.isEmpty {
                    SynthesisSection(text: syn)
                }

                if let svcs = snap?.services, !svcs.isEmpty {
                    ServiceQueueSection(services: svcs, snapshot: snap,
                                        expanded: $expandedServices)
                }

                let ents = (snap?.entities ?? []) + (snap?.discovered_entities ?? [])
                if !ents.isEmpty {
                    EntitySection(entities: ents, snapshot: snap,
                                  expanded: $expandedEntities, onPivot: onPivot)
                }

                if let t = snap?.gpt_thinking, !t.isEmpty {
                    ThinkingSection(text: t)
                }
            }
            .padding(Space.lg)
        }
        .themedScreenBackground(theme)
        .navigationTitle(snap?.query ?? run?.query ?? "Investigation")
        .compatInlineTitle()
        .onAppear { seedIfNeeded() }
        .onChange(of: targetStageIndex(for: run?.snapshot)) { _, _ in pushAnimator() }
        .onChange(of: run?.finished ?? false) { _, _ in pushAnimator() }
    }

    /// First appearance. A finished run (history, or completed while the card
    /// was closed) shows as fully done; a still-running run snaps to its current
    /// stage so already-done steps don't replay, then animates forward from
    /// there as new snapshots arrive.
    private func seedIfNeeded() {
        guard !didSeed else { return }
        didSeed = true
        let snap = run?.snapshot
        let finished = run?.finished == true || snap?.done == true || snap?.phase == "completed"
        if finished {
            animator.seedDone()
        } else {
            animator.seedAt(targetStageIndex(for: snap), errored: snap?.phase == "error")
        }
    }

    private func pushAnimator() {
        guard didSeed else { return }
        let snap = run?.snapshot
        let finished = run?.finished == true || snap?.done == true || snap?.phase == "completed"
        animator.update(target: targetStageIndex(for: snap),
                        finished: finished,
                        errored: snap?.phase == "error")
    }

    /// Collapsible pipeline-stage list. The header row toggles between the full
    /// stage diagram and a compact one-line "current stage" summary, so a user
    /// who only cares about results can fold the steps away.
    @ViewBuilder
    private func stageSection(_ snap: SearchSnapshot?) -> some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            Button {
                withAnimation(.easeInOut(duration: 0.18)) { stagesCollapsed.toggle() }
            } label: {
                HStack(spacing: Space.sm) {
                    Text("PIPELINE").font(Typography.h3).foregroundStyle(theme.textMuted)
                    Spacer(minLength: Space.sm)
                    if stagesCollapsed {
                        Text(currentStageTitle())
                            .font(Typography.body(11))
                            .foregroundStyle(animator.errored ? theme.danger : theme.textMuted)
                            .lineLimit(1)
                    }
                    Image(systemName: stagesCollapsed ? "chevron.down" : "chevron.up")
                        .font(.caption2).foregroundStyle(theme.textMuted)
                }
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)

            if !stagesCollapsed {
                StageDiagram(shown: animator.shown,
                             finished: animator.finished,
                             errored: animator.errored,
                             roundSuffix: roundSuffix(snap))
            }
        }
    }

    /// Title of the stage currently on screen, for the collapsed summary.
    private func currentStageTitle() -> String {
        let last = PipelineStage.allCases.count - 1
        if animator.errored { return "Error" }
        if animator.finished && animator.shown >= last { return PipelineStage.completed.title }
        return PipelineStage(rawValue: min(animator.shown, last))?.title ?? ""
    }

    private func header(_ s: SearchSnapshot?) -> some View {
        // Drive the bar from the *displayed* stage so it never races ahead of
        // the walked timeline. Errors fall back to the raw bar.
        let last = PipelineStage.allCases.count - 1
        let stagePct = PipelineStage(rawValue: min(animator.shown, last))?.percentFloor ?? 0
        let pct = animator.errored ? (s?.progress_percent ?? stagePct) : stagePct
        return VStack(alignment: .leading, spacing: Space.sm) {
            HStack {
                Text(s?.query ?? run?.query ?? "")
                    .font(Typography.body(15, weight: .semibold)).lineLimit(2)
                Spacer()
                Text("\(Int(pct))%")
                    .font(Typography.body(13, weight: .semibold))
                    .foregroundStyle(animator.errored ? theme.danger : theme.accent)
            }
            ProgressView(value: pct / 100.0)
                .tint(animator.errored ? theme.danger : theme.accent)
                .compatThinProgress()
        }
    }
}
