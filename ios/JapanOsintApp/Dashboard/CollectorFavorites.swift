import Foundation
import Combine

@MainActor
final class CollectorFavorites: ObservableObject {
    @Published private(set) var ids: Set<String> = []

    private let key = "collectorFavorites.v1"

    init() { load() }

    func contains(_ id: String) -> Bool { ids.contains(id) }

    func toggle(_ id: String) {
        if ids.contains(id) { ids.remove(id) } else { ids.insert(id) }
        persist()
    }

    private func persist() {
        if let data = try? JSONEncoder().encode(ids) {
            UserDefaults.standard.set(data, forKey: key)
        }
    }

    private func load() {
        guard let data = UserDefaults.standard.data(forKey: key),
              let decoded = try? JSONDecoder().decode(Set<String>.self, from: data)
        else { return }
        ids = decoded
    }
}

/// View-model wrapping a layer with all the sources reporting under it.
///
/// The five status counts are STORED, not computed. They used to be `filter`
/// properties — five O(n) passes recomputed on every access — and
/// `aggregateStatus` plus `healthChips` read them roughly fifteen times per row
/// per body pass. For the "(no layer)" collector, which groups the whole ~1,168
/// row breach catalog for operators, that is ~17k row scans on every keystroke
/// in the search field. `groupedAsCollectors()` now counts each bucket once.
struct Collector: Identifiable, Hashable {
    let id: String                 // = layer id
    let name: String
    let category: String?
    let sources: [StatusRow]
    let onlineCount: Int
    let degradedCount: Int
    let offlineCount: Int
    let pendingCount: Int
    let gatedCount: Int

    var sourceCount: Int { sources.count }

    /// Aggregate health: worst-of any non-gated source dominates. A collector
    /// whose every source is gated reports `.gated` so it isn't mis-coloured
    /// as offline.
    var aggregateStatus: OsintSource.StatusKind {
        if gatedCount == sourceCount, sourceCount > 0 { return .gated }
        if offlineCount == sourceCount, sourceCount > 0 { return .offline }
        if degradedCount > 0 || offlineCount > 0       { return .degraded }
        if onlineCount > 0                              { return .online }
        return .pending
    }
}

extension Array where Element == StatusRow {
    /// Group sources by their `layer` field. Sources with no layer are bucketed
    /// under "(no layer)" so they're still discoverable.
    func groupedAsCollectors() -> [Collector] {
        var byLayer: [String: [StatusRow]] = [:]
        for row in self {
            let key = (row.layer?.isEmpty == false ? row.layer! : "(no layer)")
            byLayer[key, default: []].append(row)
        }
        return byLayer.map { (layerId, rows) in
            let name = layerId == "(no layer)"
                ? "Uncategorised"
                : layerId.replacingOccurrences(of: "-", with: " ")
                    .split(separator: " ")
                    .map { $0.prefix(1).uppercased() + $0.dropFirst() }
                    .joined(separator: " ")
            let category = rows.compactMap(\.category).first
            // One pass for all five tallies.
            var online = 0, degraded = 0, offline = 0, pending = 0, gated = 0
            for row in rows {
                switch row.statusKind {
                case .online:   online   += 1
                case .degraded: degraded += 1
                case .offline:  offline  += 1
                case .pending:  pending  += 1
                case .gated:    gated    += 1
                default:        break
                }
            }
            return Collector(id: layerId, name: name, category: category, sources: rows,
                             onlineCount: online, degradedCount: degraded,
                             offlineCount: offline, pendingCount: pending,
                             gatedCount: gated)
        }
    }
}
