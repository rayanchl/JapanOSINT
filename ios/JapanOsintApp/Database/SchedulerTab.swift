import SwiftUI

/// Mounted inside Console's NavigationStack. See `DatabaseTab` for the
/// rationale behind dropping the inner NavigationStack wrapper.
struct SchedulerTab: View {
    @Environment(\.theme) private var theme

    var body: some View {
        SchedulerView()
            .background(theme.surface.ignoresSafeArea())
            .navigationTitle("Scheduler")
    }
}
