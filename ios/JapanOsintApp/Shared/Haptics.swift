import UIKit

/// Centralized UIKit-based haptic helpers. Used from button taps and value
/// callbacks where SwiftUI's `.sensoryFeedback` modifier (iOS 17+) is awkward
/// because there's no surrounding View. Generators are short-lived — Apple
/// recommends recreating them per-event rather than holding state.
enum Haptics {
    /// Light tap for routine button presses (toggles, +/− steppers, All On/Off).
    static func tap(_ style: UIImpactFeedbackGenerator.FeedbackStyle = .light) {
        UIImpactFeedbackGenerator(style: style).impactOccurred()
    }

    /// Discrete change in a value (theme picker, tab switch, segmented control).
    static func selection() {
        UISelectionFeedbackGenerator().selectionChanged()
    }

    /// Operation completed successfully (server live, restart back online).
    static func success() {
        UINotificationFeedbackGenerator().notificationOccurred(.success)
    }

    /// Recoverable problem (timeout, partial failure).
    static func warning() {
        UINotificationFeedbackGenerator().notificationOccurred(.warning)
    }

    /// Hard failure (connection refused, restart didn't come back).
    static func error() {
        UINotificationFeedbackGenerator().notificationOccurred(.error)
    }
}
