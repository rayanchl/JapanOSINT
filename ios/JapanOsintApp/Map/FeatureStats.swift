import Foundation
import Combine

/// Lightweight stats store fed by MapTab whenever a layer fetch completes.
/// Surfaces:
///   - total feature count per layer
///   - per-source feature breakdown (only set for unified-* collectors that
///     emit `_meta.bySource`; absent for single-source layers)
///
/// Read by the Layers tab to show "· N features" inline and per-source
/// counts inside the expanded SOURCES section.
@MainActor
final class FeatureStats: ObservableObject {
    @Published private(set) var counts: [String: Int] = [:]
    @Published private(set) var bySource: [String: [String: Int]] = [:]
    /// The server's own `_meta.records_available` for a layer: how many rows
    /// it holds, of which `counts` were sent. Absent when the server did not
    /// say (never a guessed number).
    @Published private(set) var available: [String: Int] = [:]
    /// `_meta.truncated`: the server cut the response at its own cap.
    @Published private(set) var truncatedLayers: Set<String> = []

    func record(layerId: String, total: Int, bySource: [String: Int]?,
                available: Int? = nil, truncated: Bool = false) {
        counts[layerId] = total
        if let available { self.available[layerId] = available }
        else             { self.available.removeValue(forKey: layerId) }
        if truncated { truncatedLayers.insert(layerId) }
        else         { truncatedLayers.remove(layerId) }
        if let bySource { self.bySource[layerId] = bySource }
        else            { self.bySource.removeValue(forKey: layerId) }
    }

    func clear(layerId: String) {
        counts.removeValue(forKey: layerId)
        available.removeValue(forKey: layerId)
        truncatedLayers.remove(layerId)
        bySource.removeValue(forKey: layerId)
    }

    func clearAll() {
        counts.removeAll()
        available.removeAll()
        truncatedLayers.removeAll()
        bySource.removeAll()
    }

    func count(for layerId: String) -> Int? { counts[layerId] }

    /// "10,000 of 13,419" when the server held more than it sent, else nil.
    func serverBoundNote(for layerId: String) -> String? {
        guard let sent = counts[layerId], let held = available[layerId], held > sent else { return nil }
        return "\(sent.formatted()) of \(held.formatted())"
    }
    func sources(for layerId: String) -> [String: Int]? { bySource[layerId] }
}
