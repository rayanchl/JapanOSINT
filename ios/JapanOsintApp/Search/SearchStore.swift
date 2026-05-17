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

    func start(_ query: String, api: API) async {
        let q = query.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !q.isEmpty else { return }
        do {
            let resp = try await api.searchAnalyze(q)
            let id = resp.request_id
            active.insert(Run(id: id, query: q, snapshot: nil, finished: false), at: 0)
            tasks[id] = Task { await self.stream(id: id, api: api) }
        } catch {
            lastError = "\(error)"
        }
    }

    private func stream(id: String, api: API) async {
        let req = api.searchStreamRequest(id)
        do {
            let (bytes, _) = try await URLSession.shared.bytes(for: req)
            var event = ""
            var data = ""
            for try await line in bytes.lines {
                if line.isEmpty {
                    await handle(event: event, data: data, id: id)
                    if event == "close" || event == "error" { break }
                    event = ""; data = ""
                } else if line.hasPrefix("event:") {
                    event = line.dropFirst(6).trimmingCharacters(in: .whitespaces)
                } else if line.hasPrefix("data:") {
                    data += line.dropFirst(5).trimmingCharacters(in: .whitespaces)
                }
            }
        } catch {
            // Connection ended; if we never finished, fall back to a poll.
            if active.contains(where: { $0.id == id }) {
                if let snap = try? await api.searchResults(id) { finish(id: id, snap: snap) }
            }
        }
        tasks[id] = nil
    }

    private func handle(event: String, data: String, id: String) async {
        guard event == "progress", let d = data.data(using: .utf8) else { return }
        guard let snap = try? JSONDecoder().decode(SearchSnapshot.self, from: d) else { return }
        if snap.done == true || snap.phase == "completed" || snap.phase == "error" {
            finish(id: id, snap: snap)
        } else if let i = active.firstIndex(where: { $0.id == id }) {
            active[i].snapshot = snap
            if let q = snap.query { active[i].query = q }
        }
    }

    private func finish(id: String, snap: SearchSnapshot) {
        active.removeAll { $0.id == id }
        completed.removeAll { $0.id == id }
        completed.insert(Run(id: id, query: snap.query ?? "", snapshot: snap, finished: true), at: 0)
        if completed.count > 30 { completed.removeLast() }
    }
}
