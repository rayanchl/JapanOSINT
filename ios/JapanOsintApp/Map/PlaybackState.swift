import Foundation
import Combine

/// Single global time window applied uniformly to every active time-coded
/// layer. The user picks the window once via the slider chip; the server
/// receives `?window=<seconds>` alongside `?at=<iso>` and filters with
/// `COALESCE(<layer event field>, fetched_at) BETWEEN at − window AND at`.
enum TimeWindow: Int, CaseIterable, Identifiable, Sendable {
    case m5  = 300
    case m15 = 900
    case h1  = 3600
    case h6  = 21600
    case d1  = 86400
    case d3  = 259200
    case d7  = 604800

    var id: Int { rawValue }
    var seconds: Int { rawValue }

    var label: String {
        switch self {
        case .m5:  return "5m"
        case .m15: return "15m"
        case .h1:  return "1h"
        case .h6:  return "6h"
        case .d1:  return "24h"
        case .d3:  return "3d"
        case .d7:  return "7d"
        }
    }
}

/// Time-travel state for the map. `at == nil` means LIVE; any non-nil value
/// puts every active time-coded layer into replay at that instant, looking
/// back `window` seconds. `isScrubbing` is a transient signal from the
/// slider drag so refetches can be deferred until release.
@MainActor
final class PlaybackState: ObservableObject {
    @Published var at: Date? = nil
    @Published var window: TimeWindow = .h1
    @Published var isScrubbing: Bool = false

    /// Earliest selectable instant. Defaults to now − 7 days; updated when
    /// availability data arrives. nil means "no bound yet".
    @Published var availableMin: Date? = nil
    @Published var availableMax: Date? = nil

    var isReplaying: Bool { at != nil }

    /// Snap back to LIVE. Use after the user taps the LIVE pill so callers
    /// can also fire haptics / animation alongside.
    func resumeLive() {
        at = nil
    }
}
