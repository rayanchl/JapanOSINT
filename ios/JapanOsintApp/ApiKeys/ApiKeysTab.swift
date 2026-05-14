import SwiftUI

/// Mounted inside Console's NavigationStack (RootView/ConsoleHub) — Console
/// owns the surrounding stack so its destinations don't nest nav chrome.
struct ApiKeysTab: View {
    var body: some View { ApiKeysView() }
}
