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
    /// How many features the server says the layer holds
    /// (`_meta.records_available`, core/dataapi.c), when that is MORE than
    /// were loaded — so the Layers tab can say "N of M" instead of presenting
    /// a bounded load as the whole layer. Absent when the load is complete or
    /// the server did not count.
    @Published private(set) var available: [String: Int] = [:]

    func record(layerId: String, total: Int, bySource: [String: Int]?, available: Int? = nil) {
        counts[layerId] = total
        if let bySource { self.bySource[layerId] = bySource }
        else            { self.bySource.removeValue(forKey: layerId) }
        if let available, available > total { self.available[layerId] = available }
        else                                 { self.available.removeValue(forKey: layerId) }
    }

    func clear(layerId: String) {
        counts.removeValue(forKey: layerId)
        bySource.removeValue(forKey: layerId)
        available.removeValue(forKey: layerId)
    }

    func clearAll() {
        counts.removeAll()
        bySource.removeAll()
        available.removeAll()
    }

    func count(for layerId: String) -> Int? { counts[layerId] }
    func availableCount(for layerId: String) -> Int? { available[layerId] }
    func sources(for layerId: String) -> [String: Int]? { bySource[layerId] }
}
