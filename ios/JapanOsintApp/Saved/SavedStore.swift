import Foundation
import CoreLocation
import Combine
import SwiftData

/// Codable snapshot of a `GeoFeature`. Stores a single representative
/// coordinate (the feature's anchor) since popups only read
/// `feature.geometry.anchor` for display — full geometry isn't needed.
///
/// Kept as a value type so view code (`SavedTab`, `SavedCard`) can stay
/// agnostic of SwiftData. Conversions to/from the persistent
/// `SavedFeature` row happen inside `SavedStore`.
struct SavedItem: Codable, Identifiable, Hashable {
    let id: String
    let layerId: String
    let displayName: String
    let lat: Double
    let lon: Double
    let imageURL: String?
    let properties: [String: AnyCodable]
    let savedAt: Date

    var coordinate: CLLocationCoordinate2D {
        CLLocationCoordinate2D(latitude: lat, longitude: lon)
    }

    func toFeature() -> GeoFeature {
        GeoFeature(
            id: id,
            layerId: layerId,
            geometry: .point(coordinate),
            properties: properties
        )
    }

    static func from(_ f: GeoFeature) -> SavedItem? {
        guard let coord = f.geometry.anchor else { return nil }
        return SavedItem(
            id: f.id,
            layerId: f.layerId,
            displayName: f.displayName,
            lat: coord.latitude,
            lon: coord.longitude,
            imageURL: f.imageURL,
            properties: f.properties,
            savedAt: Date()
        )
    }
}

/// SwiftData-backed bookmark store. Persists each saved feature as its own
/// row in the shared `ModelContainer`, replacing the previous unbounded
/// UserDefaults JSON blob. The published `items` array is a sorted snapshot
/// kept in sync with the database after every mutation so SwiftUI views can
/// observe changes without holding a `@Query`.
@MainActor
final class SavedStore: ObservableObject {
    @Published private(set) var items: [SavedItem] = []

    private let context: ModelContext

    init(container: ModelContainer) {
        self.context = container.mainContext
        migrateFromUserDefaultsIfNeeded()
        refresh()
    }

    func contains(id: String) -> Bool {
        items.contains { $0.id == id }
    }

    /// Add or remove based on current state. Returns the new state (true = saved).
    @discardableResult
    func toggle(_ feature: GeoFeature) -> Bool {
        if contains(id: feature.id) {
            remove(id: feature.id)
            return false
        }
        if let item = SavedItem.from(feature) {
            add(item)
            return true
        }
        return false
    }

    func add(_ item: SavedItem) {
        deleteRows(matching: item.id)
        context.insert(SavedFeature(from: item))
        save()
        refresh()
    }

    func remove(id: String) {
        deleteRows(matching: id)
        save()
        refresh()
    }

    func clearAll() {
        try? context.delete(model: SavedFeature.self)
        save()
        refresh()
    }

    // MARK: - Internals

    private func deleteRows(matching id: String) {
        let descriptor = FetchDescriptor<SavedFeature>(
            predicate: #Predicate { $0.id == id }
        )
        if let existing = try? context.fetch(descriptor) {
            for row in existing { context.delete(row) }
        }
    }

    private func refresh() {
        let descriptor = FetchDescriptor<SavedFeature>(
            sortBy: [SortDescriptor(\.savedAt, order: .reverse)]
        )
        let rows = (try? context.fetch(descriptor)) ?? []
        items = rows.map { $0.toSavedItem() }
    }

    private func save() {
        do { try context.save() }
        catch { /* swallow — single-row failure shouldn't crash the UI */ }
    }

    /// One-shot import of the legacy UserDefaults blob. The migration flag
    /// stays set forever even if no data was found, so we don't repeatedly
    /// re-decode an empty key on every launch.
    private func migrateFromUserDefaultsIfNeeded() {
        let migrationKey = "savedFeatures.migratedToSwiftData.v1"
        let defaults = UserDefaults.standard
        guard !defaults.bool(forKey: migrationKey) else { return }
        defer { defaults.set(true, forKey: migrationKey) }

        guard let data = defaults.data(forKey: "savedFeatures.v1"),
              let decoded = try? JSONDecoder().decode([SavedItem].self, from: data),
              !decoded.isEmpty
        else { return }

        for item in decoded {
            context.insert(SavedFeature(from: item))
        }
        save()
        // Leave the legacy key in place so a downgrade can still read it.
    }
}
