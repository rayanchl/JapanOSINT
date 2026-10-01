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
            openStream(id: id, api: api)
        } catch {
            lastError = "\(error)"
        }
    }

    /// Open (or re-open) the progress stream for one run. Cancels whatever was
    /// in the slot first: nothing in this type ever cancelled a task, so a
    /// reassignment used to leave the previous `URLSession.bytes` loop running
    /// forever — that loop only ends when the server closes, which a half-open
    /// connection never does. `[weak self]` breaks the
    /// `self → tasks → Task → self` cycle that kept the whole store alive with
    /// it.
    private func openStream(id: String, api: API) {
        tasks[id]?.cancel()
        tasks[id] = Task { [weak self] in await self?.stream(id: id, api: api) }
    }

    /// Re-open streams for runs that are still active but no longer streaming —
    /// ones `cancelAll()` tore down while the view was off screen. Without this
    /// a `cancelAll()` on disappear would pin a live run on whatever phase it
    /// had reached. A re-opened stream that has nothing left to say still ends
    /// in `stream`'s reconciling poll, so the run finishes either way.
    func resumeActive(api: API) {
        for run in active where tasks[run.id] == nil {
            openStream(id: run.id, api: api)
        }
    }

    /// Drop every live stream — call this when the view owning the store goes
    /// away for good. Each run otherwise holds one Task and one open
    /// URLSession stream for the life of the process.
    func cancelAll() {
        for task in tasks.values { task.cancel() }
        tasks.removeAll()
    }

    /// Drop only the streams whose run already reached a terminal snapshot.
    /// That is the leak's usual shape: `handle` finishes the run the moment the
    /// backend reports `done`, but the `bytes.lines` loop keeps waiting for the
    /// server to close the connection — which a half-open one never does.
    /// Unlike `cancelAll()` this is safe to call from an `onDisappear` that
    /// also fires on a push or a tab switch, because a run still in flight
    /// keeps streaming.
    func cancelFinishedStreams() {
        let live = Set(active.map(\.id))
        for id in tasks.keys.filter({ !live.contains($0) }) {
            tasks.removeValue(forKey: id)?.cancel()
        }
    }

    private func stream(id: String, api: API) async {
        // The backend base URL is user-editable in Settings, so it can be
        // unparseable — in which case there is no stream to open. Surface it
        // and reconcile once so the run can't wedge on the "Queued"
        // placeholder. (This used to force-unwrap inside `API` and crash.)
        guard let req = api.searchStreamRequest(id) else {
            lastError = "Backend URL is not a valid URL — check Settings."
            if let snap = try? await api.searchResults(id) { finish(id: id, snap: snap) }
            tasks[id] = nil
            return
        }
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
        // Cancelled rather than ended: whoever cancelled owns this id's slot
        // now (`cancelAll` cleared it, or `openStream` handed it to a fresh
        // task), so don't poll and don't clear an entry that isn't ours.
        if Task.isCancelled { return }
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
