import Foundation
import SwiftData

/// Single source of truth for the SwiftData stack. Built once at app launch
/// and shared across every store (SavedStore, AppSettings, IntelCache).
///
/// We crash-on-fail intentionally: the app's persistence is foundational —
/// no fallback path makes sense if the on-disk store is unreadable.
enum AppDataContainer {
    static let schema = Schema([
        SavedFeature.self,
        AppPreferences.self,
        CachedIntelSource.self,
        CachedIntelItem.self,
    ])

    static func make() -> ModelContainer {
        let config = ModelConfiguration(schema: schema, isStoredInMemoryOnly: false)
        do {
            return try ModelContainer(for: schema, configurations: [config])
        } catch {
            fatalError("Failed to create ModelContainer: \(error)")
        }
    }
}
