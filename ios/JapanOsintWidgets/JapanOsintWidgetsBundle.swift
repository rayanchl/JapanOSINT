import WidgetKit
import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Widget extension entry point (roadmap 35).
//
// THIS FOLDER IS NOT PART OF THE MAIN APP TARGET and must never be.
// ios/JapanOsintApp/ is a PBXFileSystemSynchronizedRootGroup, so anything
// under it compiles into the app — and an `@main struct …: WidgetBundle` in
// an app target is a build error. Hence this lives one level up.
//
// Add this folder to the JapanOsintWidgets target ONLY, plus give
// ios/JapanOsintApp/Widgets/SharedSnapshot.swift membership in both.
// See docs/ios-extensions-plan.md.
// ─────────────────────────────────────────────────────────────────────────────

@main
struct JapanOsintWidgetsBundle: WidgetBundle {
    var body: some Widget {
        AlertInboxWidget()
        QuakeWidget()
        InboxAccessoryWidget()
        // Live Activities are registered by declaring the ActivityConfiguration
        // in the same bundle; the app starts them via Activity.request(...).
        SearchLiveActivity()
    }
}

// ── Shared chrome ──────────────────────────────────────────────────────────

/// Widgets cannot read the app's `@Environment(\.theme)`, so the palette is
/// duplicated here — deliberately minimal, only the four colours the widgets
/// actually use. Keep in sync with ThemePalette.cyberpunk by hand; a shared
/// file would have to be added to both targets for four constants.
enum WidgetTheme {
    static let accent  = Color(red: 1.00, green: 0.70, blue: 0.28)  // amber
    static let text    = Color(red: 0.91, green: 0.93, blue: 0.96)
    static let muted   = Color(red: 0.43, green: 0.49, blue: 0.58)
    static let danger  = Color(red: 1.00, green: 0.30, blue: 0.37)
}

/// Shown whenever the app has never successfully reached the backend. Saying
/// "not connected" is honest; rendering a confident 0 unread is not — the user
/// would read it as "nothing is happening" rather than "I don't know".
struct WidgetDisconnected: View {
    var body: some View {
        VStack(spacing: 4) {
            Image(systemName: "wifi.exclamationmark")
                .foregroundStyle(WidgetTheme.muted)
            Text("Open JapanOSINT")
                .font(.caption2)
                .foregroundStyle(WidgetTheme.muted)
        }
    }
}

/// Relative age of the snapshot. A widget whose data is hours old should say
/// so rather than presenting it as current.
struct WidgetStaleness: View {
    let generatedAt: Date
    var body: some View {
        if generatedAt > .distantPast {
            Text(generatedAt, style: .relative)
                .font(.system(size: 9))
                .foregroundStyle(WidgetTheme.muted)
        }
    }
}
