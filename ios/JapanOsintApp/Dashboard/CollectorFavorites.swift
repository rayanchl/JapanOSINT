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

/// View-model wrapping a COLLECTOR with all the sources reporting under it.
/// (It wrapped a map layer until 2026-10-12; a collector is the beat a source
/// was authored and probed in, and every source has one.)
///
/// The five status counts are STORED, not computed. They used to be `filter`
/// properties — five O(n) passes recomputed on every access — and
/// `aggregateStatus` plus `healthChips` read them roughly fifteen times per row
/// per body pass. For the "(no layer)" collector, which groups the whole ~1,168
/// row breach catalog for operators, that is ~17k row scans on every keystroke
/// in the search field. `groupedAsCollectors()` now counts each bucket once.
struct Collector: Identifiable, Hashable {
    let id: String                 // = collector key
    let name: String
    let category: String?
    /// Group key and label from the server (grp_*); "grp_unassigned" when the
    /// server has none for this collector, so a hole in collectors.def is a
    /// visible heading rather than a missing one.
    let group: String
    let groupLabel: String
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
    /// Group sources by their `collector`. Every registered source carries
    /// one, so nothing lands in a catch-all; a row from a server predating the
    /// field (or a cached envelope) groups under its own id, alone, which is
    /// still visible rather than hidden. The name is the server's label from
    /// core/collectors.def when it has one, else the key itself — never a
    /// prettified guess. The category is the most common one among the
    /// sources, so a mixed beat is labelled by its majority, not its first row.
    func groupedAsCollectors() -> [Collector] {
        var byKey: [String: [StatusRow]] = [:]
        var labels: [String: String] = [:]
        var groupOf: [String: (String, String)] = [:]
        for row in self {
            let key = (row.collector?.isEmpty == false) ? row.collector! : row.id
            byKey[key, default: []].append(row)
            if let l = row.collectorLabel, !l.isEmpty { labels[key] = l }
            if let g = row.collectorGroup, !g.isEmpty {
                groupOf[key] = (g, row.collectorGroupLabel ?? g)
            }
        }
        return byKey.map { (key, rows) in
            let name = labels[key] ?? key
            var catCounts: [String: Int] = [:]
            for r in rows { if let c = r.category { catCounts[c, default: 0] += 1 } }
            let category = catCounts.max { a, b in a.value != b.value ? a.value < b.value : a.key > b.key }?.key
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
            let (group, groupLabel) = groupOf[key] ?? ("grp_unassigned", "Unassigned")
            return Collector(id: key, name: name, category: category, group: group, groupLabel: groupLabel, sources: rows,
                             onlineCount: online, degradedCount: degraded,
                             offlineCount: offline, pendingCount: pending,
                             gatedCount: gated)
        }
    }
}
