import SwiftUI

/// The fourth tab. Hosts every admin / operations surface that used to be a
/// separate top-level tab. iOS's TabView truncates at 5 tabs into a generic
/// "More" list — collapsing here keeps each destination styled and reachable.
///
/// Provides the surrounding `NavigationStack` so each destination
/// (`SourceDashboardTab`, `DatabaseTab`, etc.) can drop its own NavigationStack
/// wrapper and avoid nested nav chrome.
struct ConsoleHub: View {
    @EnvironmentObject var mapNav: MapNavigation
    @Environment(\.theme) private var theme

    var body: some View {
        NavigationStack(path: $mapNav.consolePath) {
            List {
                Section {
                    row(.sources,   icon: "chart.pie.fill",
                        title: "Sources",
                        subtitle: "Status, charts, collectors")
                    row(.database,  icon: "cylinder.split.1x2.fill",
                        title: "Database",
                        subtitle: "Browse collected tables")
                    row(.scheduler, icon: "calendar.badge.clock",
                        title: "Scheduler",
                        subtitle: "Collection cadence")
                    row(.followLog, icon: "scroll",
                        title: "Follow log",
                        subtitle: "Live activity stream")
                } header: {
                    sectionLabel("Operations")
                }

                Section {
                    row(.cameras, icon: "video.fill",
                        title: "Camera discovery",
                        subtitle: "Probe public webcams")
                    row(.alerts, icon: "bell.badge.fill",
                        title: "Alerts",
                        subtitle: "Rules, channels, history")
                } header: {
                    sectionLabel("Discovery")
                }

                Section {
                    row(.apiKeys,  icon: "key.fill",
                        title: "API keys",
                        subtitle: "Credentials & overlays")
                    row(.settings, icon: "gearshape.fill",
                        title: "Settings",
                        subtitle: "Backend, appearance, limits")
                } header: {
                    sectionLabel("Configuration")
                }
            }
            .listStyle(.insetGrouped)
            .scrollContentBackground(.hidden)
            .background(theme.surface.ignoresSafeArea())
            .navigationTitle("Console")
            .navigationDestination(for: ConsoleDestination.self) { dest in
                destinationView(for: dest)
            }
        }
    }

    @ViewBuilder
    private func destinationView(for dest: ConsoleDestination) -> some View {
        switch dest {
        case .sources:    SourceDashboardTab()
        case .database:   DatabaseTab()
        case .scheduler:  SchedulerTab()
        case .cameras:    CameraDiscoveryTab()
        case .followLog:  FollowLogTab()
        case .apiKeys:    ApiKeysTab()
        case .alerts:     AlertsTab()
        case .settings:   SettingsTab()
        }
    }

    private func row(_ dest: ConsoleDestination,
                     icon: String,
                     title: String,
                     subtitle: String) -> some View {
        NavigationLink(value: dest) {
            HStack(spacing: Space.md) {
                Image(systemName: icon)
                    .font(.body)
                    .foregroundStyle(theme.accent)
                    .frame(width: 28, height: 28)
                    .background(
                        theme.accent.opacity(0.12),
                        in: RoundedRectangle(cornerRadius: Radius.sm)
                    )
                VStack(alignment: .leading, spacing: 1) {
                    Text(title)
                        .font(.body.weight(.medium))
                        .foregroundStyle(theme.text)
                    Text(subtitle)
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }
            }
            .padding(.vertical, 2)
        }
    }

    private func sectionLabel(_ text: String) -> some View {
        Text(text.uppercased())
            .font(Typography.display(10, weight: .semibold))
            .tracking(1.2)
            .foregroundStyle(theme.textMuted)
    }
}
