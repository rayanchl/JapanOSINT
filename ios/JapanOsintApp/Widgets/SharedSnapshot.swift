import Foundation
#if canImport(WidgetKit)
import WidgetKit
#endif

// ─────────────────────────────────────────────────────────────────────────────
// The App Group bridge between the app and the widget extension (roadmap 35).
//
// THIS FILE MUST HAVE TARGET MEMBERSHIP IN BOTH the app and the widget
// extension. It is the only shared file; everything else is one side or the
// other. See docs/ios-extensions-plan.md.
//
// Why a snapshot file instead of the widget fetching for itself: a widget
// extension cannot reach the Keychain-backed bearer token, and it is killed
// for slow work. A widget that authenticates and fetches renders "—" most of
// the time. So the APP writes what it already knows, and the widget only
// reads. The cost is that widget freshness is bounded by how often the app
// runs — which is what item 37's background refresh exists to raise.
// ─────────────────────────────────────────────────────────────────────────────

enum AppGroup {
    /// Change here and nowhere else if you register a different group id.
    /// A mistyped id makes `containerURL` return nil and every widget falls
    /// back to placeholder data — a failure that looks like "widgets are
    /// broken" rather than "the entitlement is wrong", so it is worth
    /// checking first when something is empty.
    static let identifier = "group.com.rayanchl.japanosint"

    /// Log-once diagnostic. A `static let` initialiser runs exactly once per
    /// process and is thread-safe, which is precisely the semantics we want:
    /// the container is queried on every scene activation and every background
    /// task, and a per-call log would drown the console.
    private static let warnUnavailable: Void = {
        print("""
        [AppGroup] "\(AppGroup.identifier)" is not available to this process. \
        The App Groups capability is not wired into the Xcode project yet \
        (CODE_SIGN_ENTITLEMENTS appears nowhere in project.pbxproj), so the \
        widget snapshot, the share queue and background refresh are all \
        disabled rather than writing into a nil container. \
        See ios/EXTENSION_TARGETS_TODO.md.
        """)
    }()

    static var containerURL: URL? {
        guard let url = FileManager.default.containerURL(
            forSecurityApplicationGroupIdentifier: identifier) else {
            _ = warnUnavailable
            return nil
        }
        return url
    }

    /// Whether the App Group is actually reachable from this process.
    ///
    /// Every consumer gates on this instead of discovering nil deep inside a
    /// write path. Until the capability is added in Xcode this is `false` on
    /// every device, and the widget/share/background features must no-op
    /// rather than run work whose only possible outcome is a dropped write.
    static var isConfigured: Bool { containerURL != nil }

    static var snapshotURL: URL? {
        containerURL?.appendingPathComponent("widget-snapshot.json")
    }

    /// Queue directory the share extension writes into and the app drains.
    static var shareQueueURL: URL? {
        containerURL?.appendingPathComponent("share-queue", isDirectory: true)
    }
}

// ── What the widgets render ────────────────────────────────────────────────

struct WidgetSnapshot: Codable, Hashable {
    var generatedAt: Date
    var unreadAlerts: Int
    var latestAlerts: [WidgetAlert]
    var latestQuake: WidgetQuake?
    var activeWarnings: [String]
    /// False when the app has never successfully talked to the backend, so
    /// the widget can say "not connected" instead of showing a confident 0.
    var connected: Bool

    static let empty = WidgetSnapshot(
        generatedAt: .distantPast, unreadAlerts: 0, latestAlerts: [],
        latestQuake: nil, activeWarnings: [], connected: false)
}

struct WidgetAlert: Codable, Hashable, Identifiable {
    var id: String
    var ruleName: String?
    var title: String?
    var sourceId: String?
    var matchedAt: Date
}

struct WidgetQuake: Codable, Hashable {
    var title: String
    var magnitude: Double?
    var intensity: String?
    var at: Date
    var lat: Double?
    var lon: Double?
}

// ── Writing (app side) ─────────────────────────────────────────────────────

enum SharedSnapshot {

    /// Atomically replace the snapshot and ask WidgetKit to reload.
    ///
    /// Atomic because the widget process may read at any moment: a torn JSON
    /// file decodes as nil and blanks the widget. `.atomic` writes to a temp
    /// file and renames, so a reader sees either the old file or the new one.
    static func write(_ snapshot: WidgetSnapshot) {
        guard let url = AppGroup.snapshotURL else {
            // Nil means the App Group is not configured for this target.
            // `AppGroup.containerURL` has already logged that, once, with the
            // remedy. This used to `assertionFailure`, which trapped every
            // debug launch for a condition that is currently the *expected*
            // state of the project — see ios/EXTENSION_TARGETS_TODO.md.
            return
        }
        do {
            let enc = JSONEncoder()
            enc.dateEncodingStrategy = .iso8601
            try enc.encode(snapshot).write(to: url, options: .atomic)
            reloadWidgets()
        } catch {
            // Never propagate: a failed widget update must not disturb the
            // app flow that happened to trigger it.
            #if DEBUG
            print("[widget] snapshot write failed: \(error)")
            #endif
        }
    }

    static func reloadWidgets() {
        #if canImport(WidgetKit)
        WidgetCenter.shared.reloadAllTimelines()
        #endif
    }

    // ── Reading (both sides) ───────────────────────────────────────────────

    static func read() -> WidgetSnapshot {
        guard let url = AppGroup.snapshotURL,
              let data = try? Data(contentsOf: url) else { return .empty }
        let dec = JSONDecoder()
        dec.dateDecodingStrategy = .iso8601
        return (try? dec.decode(WidgetSnapshot.self, from: data)) ?? .empty
    }
}

// ── Share extension hand-off ───────────────────────────────────────────────

/// One item the share sheet handed us. The extension only writes these; all
/// network work happens in the app, so there is one auth path and one error
/// surface rather than a second copy living in a process with ~15s to live.
struct SharedInboundItem: Codable, Hashable, Identifiable {
    enum Kind: String, Codable { case url, text, image }
    var id: String
    var kind: Kind
    var value: String          // URL string, text, or the image filename
    var receivedAt: Date
}

enum ShareQueue {

    static func enqueue(_ item: SharedInboundItem, imageData: Data? = nil) throws {
        guard let dir = AppGroup.shareQueueURL else {
            throw CocoaError(.fileNoSuchFile)
        }
        try FileManager.default.createDirectory(at: dir,
                                                withIntermediateDirectories: true)
        if let imageData {
            try imageData.write(to: dir.appendingPathComponent(item.value),
                                options: .atomic)
        }
        let enc = JSONEncoder()
        enc.dateEncodingStrategy = .iso8601
        try enc.encode(item).write(
            to: dir.appendingPathComponent("\(item.id).json"), options: .atomic)
    }

    /// Read every queued item, newest last. Does NOT delete — the app removes
    /// each one only after it has actually been handled, so a crash mid-handle
    /// does not lose what the user shared.
    static func pending() -> [SharedInboundItem] {
        guard let dir = AppGroup.shareQueueURL,
              let names = try? FileManager.default.contentsOfDirectory(atPath: dir.path)
        else { return [] }
        let dec = JSONDecoder()
        dec.dateDecodingStrategy = .iso8601
        return names
            .filter { $0.hasSuffix(".json") }
            .compactMap { name -> SharedInboundItem? in
                guard let d = try? Data(contentsOf: dir.appendingPathComponent(name))
                else { return nil }
                return try? dec.decode(SharedInboundItem.self, from: d)
            }
            .sorted { $0.receivedAt < $1.receivedAt }
    }

    static func remove(_ item: SharedInboundItem) {
        guard let dir = AppGroup.shareQueueURL else { return }
        try? FileManager.default.removeItem(
            at: dir.appendingPathComponent("\(item.id).json"))
        if item.kind == .image {
            try? FileManager.default.removeItem(
                at: dir.appendingPathComponent(item.value))
        }
    }
}
