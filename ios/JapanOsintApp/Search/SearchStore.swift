import Foundation
import Combine

/// Live OSINT-search store. Port of the web searchStore: start a run, stream
/// progress_tracker snapshots over SSE (URLSession.bytes — sends the bearer
/// header, unlike a browser EventSource), surface active + completed runs.
@MainActor
final class SearchStore: ObservableObject {
    struct Run: Identifiable, Sendable {
        let id: String          // request_id
        var query: String
        var snapshot: SearchSnapshot?
        var finished: Bool
    }

    @Published private(set) var active: [Run] = []
    @Published private(set) var completed: [Run] = []
    @Published var lastError: String?

    private var tasks: [String: Task<Void, Never>] = [:]

    /// Each pipeline step's spinner is held on screen for at least this long so
    /// steps don't flash past faster than the eye can register them. The detail
    /// screen's `StageAnimator` enforces the same ≥1s floor while *also* walking
    /// through stages the backend skipped; this keeps the collapsed run card and
    /// the store's phase bookkeeping in step with that pacing.
    private static let minPhaseDwell: TimeInterval = 1.0
    private var phaseShownAt: [String: Date] = [:]      // id → when displayed phase began
    private var displayedPhase: [String: String] = [:]  // id → phase currently on screen

    func start(_ query: String, api: API) async {
        let q = query.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !q.isEmpty else { return }
        do {
            let resp = try await api.searchAnalyze(q)
            let id = resp.request_id
            active.insert(Run(id: id, query: q, snapshot: nil, finished: false), at: 0)
            displayedPhase[id] = "queued"   // the placeholder stage shown first
            phaseShownAt[id] = Date()
            tasks[id] = Task { await self.stream(id: id, api: api) }
        } catch {
            lastError = "\(error)"
        }
    }

    private func stream(id: String, api: API) async {
        let req = api.searchStreamRequest(id)
        var sawTerminal = false
        do {
            let (bytes, _) = try await URLSession.shared.bytes(for: req)
            var event = ""
            // The server frames each SSE event as `event: <type>\r\n
            // data: <single-line JSON>\n\n`. We cannot key dispatch off the
            // blank delimiter line: Swift's AsyncLineSequence (bytes.lines)
            // silently collapses empty lines, so it is *never* yielded — the
            // old `if line.isEmpty` branch never ran, handle() was never
            // called, and the run stayed pinned at the nil/"Queued"
            // placeholder forever. Dispatch on the `data:` line instead: one
            // event always pairs one `event:` line with one single-line
            // `data:` line for this server.
            for try await line in bytes.lines {
                if line.hasPrefix("event:") {
                    event = line.dropFirst(6).trimmingCharacters(in: .whitespaces)
                } else if line.hasPrefix("data:") {
                    let data = String(line.dropFirst(5).trimmingCharacters(in: .whitespaces))
                    let ev = event.isEmpty ? "progress" : event
                    await handle(event: ev, data: data, id: id)
                    if ev == "close" || ev == "error" { sawTerminal = true; break }
                    event = ""
                }
            }
        } catch {
            // fall through to the reconciling poll below.
        }
        // Stream ended (terminal event, server close, or a dropped
        // connection). If the run never reached a terminal snapshot, reconcile
        // once via the results poll so it can't wedge on the "Queued"
        // placeholder when the live channel didn't deliver a final state.
        if !sawTerminal, active.contains(where: { $0.id == id }) {
            if let snap = try? await api.searchResults(id) { finish(id: id, snap: snap) }
        }
        tasks[id] = nil
    }

    private func handle(event: String, data: String, id: String) async {
        guard event == "progress", let d = data.data(using: .utf8) else { return }
        guard let snap = try? JSONDecoder().decode(SearchSnapshot.self, from: d) else { return }
        let terminal = snap.done == true || snap.phase == "completed" || snap.phase == "error"
        // The phase whose spinner we're about to surface. A nil-phase frame is
        // a mid-stage progress tick, so it keeps the current phase (no dwell).
        let target = terminal ? "completed" : (snap.phase ?? displayedPhase[id] ?? "queued")
        await holdMinimumDwell(id: id, nextPhase: target)
        if terminal {
            finish(id: id, snap: snap)
        } else if let i = active.firstIndex(where: { $0.id == id }) {
            active[i].snapshot = snap
            if let q = snap.query { active[i].query = q }
        }
    }

    /// Suspend until the phase currently on screen has been visible for at
    /// least `minPhaseDwell`, then record `nextPhase` as the displayed phase.
    /// Same-phase frames return immediately so the progress bar still advances
    /// smoothly within a single step.
    private func holdMinimumDwell(id: String, nextPhase: String) async {
        if displayedPhase[id] == nextPhase { return }
        if let since = phaseShownAt[id] {
            let remaining = Self.minPhaseDwell - Date().timeIntervalSince(since)
            if remaining > 0 {
                try? await Task.sleep(nanoseconds: UInt64(remaining * 1_000_000_000))
            }
        }
        displayedPhase[id] = nextPhase
        phaseShownAt[id] = Date()
    }

    private func finish(id: String, snap: SearchSnapshot) {
        phaseShownAt[id] = nil
        displayedPhase[id] = nil
        active.removeAll { $0.id == id }
        completed.removeAll { $0.id == id }
        completed.insert(Run(id: id, query: snap.query ?? "", snapshot: snap, finished: true), at: 0)
        if completed.count > 30 { completed.removeLast() }
    }
}
