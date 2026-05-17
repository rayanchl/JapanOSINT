import Foundation
import Combine

/// Session-scoped, in-memory cache of decoded GeoJSON for **static** layers.
///
/// Static layers (`temporal == nil && liveOnly != true`) look identical at
/// every slider position and never change within a session, so re-fetching
/// their multi-MB payload every time the user toggles visibility off→on is
/// pure waste. We keep the decoded `FeatureCollection` keyed by layer id and
/// hand it straight back on the next activation.
///
/// Time-coded and liveOnly layers are intentionally NOT cached here — their
/// data genuinely changes with the playback cursor, so MapTab's
/// `invalidateAndRefetch()` path keeps owning them.
///
/// Growth is bounded by the layer catalog (a finite, server-defined list),
/// so there is no TTL or LRU eviction — the worst case is every static layer
/// resident at once, which is well within budget. Cleared wholesale when the
/// backend URL changes (the data would belong to a different deployment).
@MainActor
final class LayerFeatureCache: ObservableObject {
    private var store: [String: FeatureCollection] = [:]

    /// Cached collection for a static layer, or nil on a miss.
    func collection(for layerId: String) -> FeatureCollection? {
        store[layerId]
    }

    /// Retain a freshly-fetched static-layer collection for the session.
    func store(_ fc: FeatureCollection, for layerId: String) {
        store[layerId] = fc
    }

    /// Drop a single layer (e.g. an explicit user-driven hard refresh).
    func invalidate(_ layerId: String) {
        store.removeValue(forKey: layerId)
    }

    /// Wipe everything — the cached payloads no longer correspond to the
    /// active backend.
    func clearAll() {
        store.removeAll()
    }
}
