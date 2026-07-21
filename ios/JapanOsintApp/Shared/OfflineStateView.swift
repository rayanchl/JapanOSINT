import SwiftUI

/// Centered full-area state used when a backend-dependent tab can't reach
/// the server, or when a tab has nothing to show. Theme-aware so it matches
/// the surrounding tab's palette. Used by Saved / Intel / Cameras /
/// Scheduler / Follow / DB Tables.
struct OfflineStateView: View {
    enum Kind { case offline, empty, error }

    var kind: Kind = .offline
    var title: String? = nil
    var message: String? = nil
    var systemImage: String? = nil
    var retry: (() -> Void)? = nil

    @Environment(\.theme) private var theme

    private var defaults: (title: String, message: String, icon: String) {
        switch kind {
        case .offline:
            return ("Backend offline",
                    "Couldn't reach the server. Check your connection and try again.",
                    "wifi.exclamationmark")
        case .empty:
            return ("No data yet",
                    "There's nothing to show here.",
                    "tray")
        case .error:
            return ("Something went wrong",
                    "We couldn't load this. Pull to refresh or try again.",
                    "exclamationmark.triangle")
        }
    }

    var body: some View {
        // HIG-native empty/error state. `ContentUnavailableView` handles
        // Dynamic Type, centering and safe-area layout for us; we keep the
        // theme's own colors on the glyph/title/message so it still reads as
        // part of the cyberpunk palette rather than plain system chrome.
        ContentUnavailableView {
            Label {
                Text(title ?? defaults.title)
                    .foregroundStyle(theme.text)
            } icon: {
                Image(systemName: systemImage ?? defaults.icon)
                    .foregroundStyle(theme.textMuted)
            }
        } description: {
            Text(message ?? defaults.message)
                .foregroundStyle(theme.textMuted)
        } actions: {
            if let retry {
                Button {
                    retry()
                } label: {
                    Label("Retry", systemImage: "arrow.clockwise")
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.regular)
                .tint(theme.accent)
            }
        }
    }
}
