import SwiftUI

/// Centered full-area state used when a backend-dependent tab can't reach
/// the server, or when a tab has nothing to show. Theme-aware so it matches
/// the surrounding tab's palette. Used by Saved / Intel / Cameras /
/// Scheduler / Follow / DB Tables.
struct OfflineStateView: View {
    enum Kind { case offline, empty }

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
        }
    }

    var body: some View {
        VStack(spacing: 14) {
            Image(systemName: systemImage ?? defaults.icon)
                .font(.system(size: 48, weight: .light))
                .foregroundStyle(theme.textMuted)
            VStack(spacing: 4) {
                Text(title ?? defaults.title)
                    .font(.title3.weight(.semibold))
                    .foregroundStyle(theme.text)
                Text(message ?? defaults.message)
                    .font(.callout)
                    .foregroundStyle(theme.textMuted)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal, 32)
            }
            if let retry {
                Button {
                    retry()
                } label: {
                    Label("Retry", systemImage: "arrow.clockwise")
                        .font(.callout.weight(.medium))
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.regular)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .padding()
    }
}
