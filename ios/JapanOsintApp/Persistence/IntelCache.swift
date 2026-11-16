import Foundation
import Combine
import SwiftData

/// SwiftData-backed cache for the Intel tab's first page of sources and `/api/intel/items`.
///
/// Why: previously these endpoints were re-fetched from scratch on every
/// view appearance. With the cache, opening Intel paints instantly from the
/// last successful response, then the network call refreshes in-place.
/// Offline launches still show meaningful data instead of an empty list.
///
/// Scope: only the most recent first-page items per source are cached.
/// Pagination cursors continue to live on the API side; we never try to
/// reconstruct them locally.
@MainActor
final class IntelCache: ObservableObject {
    private let context: ModelContext

    init(container: ModelContainer) {
        self.context = container.mainContext
    }

    // MARK: - Sources

    func cachedSources() -> [IntelSource] {
        let descriptor = FetchDescriptor<CachedIntelSource>()
        let rows = (try? context.fetch(descriptor)) ?? []
        return rows.compactMap {
            try? JSONDecoder().decode(IntelSource.self, from: $0.rawJSON)
        }
    }

    /// Most rows the offline catalogue keeps. The registry is ~61k sources;
    /// the cache holds only what the Intel tab has on screen (its first page),
    /// never the catalogue.
    static let maxCachedSources = 100

    /// Replace the cache with the rows the Intel tab is showing — at most
    /// `maxCachedSources`, so the write is bounded whatever the registry size.
    /// (It used to delete and re-insert EVERY source on the main actor after
    /// each load and each Run tap.)
    func cacheSources(_ sources: [IntelSource]) {
        try? context.delete(model: CachedIntelSource.self)
        let now = Date()
        for source in sources.prefix(Self.maxCachedSources) {
            guard let data = try? JSONEncoder().encode(source) else { continue }
            context.insert(CachedIntelSource(id: source.id, rawJSON: data, fetchedAt: now))
        }
        save()
    }

    /// Refresh ONE cached row (after a manual run). Inserts it if absent.
    func upsertSource(_ source: IntelSource) {
        guard let data = try? JSONEncoder().encode(source) else { return }
        let id = source.id
        let descriptor = FetchDescriptor<CachedIntelSource>(
            predicate: #Predicate { $0.id == id }
        )
        if let existing = (try? context.fetch(descriptor))?.first {
            existing.rawJSON = data
            existing.fetchedAt = Date()
        } else {
            context.insert(CachedIntelSource(id: id, rawJSON: data, fetchedAt: Date()))
        }
        save()
    }

    // MARK: - Items

    func cachedItems(for sourceId: String) -> [IntelItem] {
        let descriptor = FetchDescriptor<CachedIntelItem>(
            predicate: #Predicate { $0.sourceId == sourceId },
            sortBy: [SortDescriptor(\.fetchedAt, order: .reverse)]
        )
        let rows = (try? context.fetch(descriptor)) ?? []
        return rows.compactMap {
            try? JSONDecoder().decode(IntelItem.self, from: $0.rawJSON)
        }
    }

    /// Cache the most recent first page for a source. Older rows for the
    /// same source are dropped — pagination beyond the first page is a
    /// network-only path.
    func cacheItems(_ items: [IntelItem], for sourceId: String) {
        let descriptor = FetchDescriptor<CachedIntelItem>(
            predicate: #Predicate { $0.sourceId == sourceId }
        )
        if let existing = try? context.fetch(descriptor) {
            for row in existing { context.delete(row) }
        }
        let now = Date()
        for item in items {
            guard let data = try? JSONEncoder().encode(item) else { continue }
            context.insert(CachedIntelItem(
                uid: item.uid, sourceId: sourceId,
                rawJSON: data, fetchedAt: now
            ))
        }
        save()
    }

    // MARK: -

    private func save() {
        do { try context.save() }
        catch { /* cache is best-effort; never crash the UI on a write fail */ }
    }
}
