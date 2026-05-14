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

    func record(layerId: String, total: Int, bySource: [String: Int]?) {
        counts[layerId] = total
        if let bySource { self.bySource[layerId] = bySource }
        else            { self.bySource.removeValue(forKey: layerId) }
    }

    func clear(layerId: String) {
        counts.removeValue(forKey: layerId)
        bySource.removeValue(forKey: layerId)
    }

    func clearAll() {
        counts.removeAll()
        bySource.removeAll()
    }

    func count(for layerId: String) -> Int? { counts[layerId] }
    func sources(for layerId: String) -> [String: Int]? { bySource[layerId] }
}
