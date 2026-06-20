#if canImport(UIKit)
import UIKit
#elseif canImport(AppKit)
import AppKit
#endif

/// Centralized haptic helpers. Used from button taps and value callbacks where
/// SwiftUI's `.sensoryFeedback` modifier (iOS 17+) is awkward because there's
/// no surrounding View. Generators are short-lived — Apple recommends
/// recreating them per-event rather than holding state.
///
/// macOS has no per-event impact/notification generators (the only API,
/// `NSHapticFeedbackManager`, drives Force Touch trackpads on alignment
/// gestures, not arbitrary button taps), so the Mac path is a no-op. The
/// call sites stay identical across platforms.
enum Haptics {
    #if canImport(UIKit)
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
    #else
    /// FeedbackStyle stand-in so iOS call sites like `Haptics.tap(.light)`
    /// compile unchanged on macOS.
    enum FeedbackStyle { case light, medium, heavy, soft, rigid }

    static func tap(_ style: FeedbackStyle = .light) {}
    static func selection() {}
    static func success() {}
    static func warning() {}
    static func error() {}
    #endif
}
