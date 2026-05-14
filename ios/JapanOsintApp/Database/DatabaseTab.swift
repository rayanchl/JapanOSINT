import SwiftUI

/// Mounted inside Console's NavigationStack (RootView/ConsoleHub). The
/// `NavigationStack` used to be here; Console now owns the stack so its
/// destinations can chain without nesting nav chrome.
struct DatabaseTab: View {
    @Environment(\.theme) private var theme

    var body: some View {
        TableBrowser()
            .background(theme.surface.ignoresSafeArea())
            .navigationTitle("Database")
    }
}
