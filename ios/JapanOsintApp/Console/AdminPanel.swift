import SwiftUI

/// Single home for every platform-operator-only surface. These all hit
/// server endpoints gated by `requirePlatformOperator`
/// (server/src/middleware/keyAccess.js) and affect every workspace on the
/// host, so the whole panel is gated to the platform operator (as the server
/// reports it) via
/// `AuthSession.isPlatformAdmin`. The Console row that pushes this is gated
/// the same way; the inner `isPlatformAdmin` check is belt-and-suspenders.
///
/// Pushed inside Console's `NavigationStack(path:)` (phone) or RootView's
/// detail stack (iPad), so it carries no `NavigationStack` of its own and the
/// destinations use plain value-less `NavigationLink`s — the same pattern the
/// old `SettingsTab.adminSection` used for Platform API keys.
struct AdminPanel: View {
    @EnvironmentObject var auth: AuthSession
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    var body: some View {
        Group {
            if auth.isPlatformAdmin {
                panel
            } else {
                ContentUnavailableView(
                    "Operator access only",
                    systemImage: "lock.shield",
                    description: Text("This panel is restricted to the platform operator account.")
                )
            }
        }
        .navigationTitle("Admin")
        .themedScreenBackground(theme)
    }

    private let operatorFooter =
        "Platform keys are the server-wide defaults every workspace falls back to."

    private var panel: some View {
        #if os(macOS)
        // macOS: lay the sections out as separator-free `surfaceElevated`
        // cards in a ScrollView. A grouped `List` on macOS draws hairline
        // rules between every row under the inset style, and hiding them on
        // the List itself isn't reliable — cards give the grouped look with
        // zero system separators.
        ScrollView {
            VStack(alignment: .leading, spacing: Space.lg) {
                card(header: "Operator tools", footer: operatorFooter) {
                    operatorRows
                }
            }
            .padding(Space.lg)
            .frame(maxWidth: 680, alignment: .leading)
            .frame(maxWidth: .infinity, alignment: .top)
        }
        .themedScreenBackground(theme)
        #else
        List {
            Section {
                operatorRows
            } header: {
                sectionLabel("Operator tools")
            } footer: {
                Text(operatorFooter).font(.caption2)
            }
        }
        .compatInsetGroupedListStyle()
        .themedScreenBackground(theme)
        #endif
    }

    /// Operator-tool navigation rows, shared by the iOS `List` and macOS cards.
    @ViewBuilder
    private var operatorRows: some View {
        navRow(icon: "cylinder.split.1x2.fill",
               title: "Database",
               subtitle: "Browse collected tables") { DatabaseTab() }
        navRow(icon: "calendar.badge.clock",
               title: "Scheduler",
               subtitle: "Collection cadence") { SchedulerTab() }
        navRow(icon: "slider.horizontal.3",
               title: "Source scheduling",
               subtitle: "Per-source map-cron vs search-only") { SourceScheduleSettingsView() }
        navRow(icon: "scroll",
               title: "Follow log",
               subtitle: "Live activity stream") { FollowLogTab() }
        navRow(icon: "wrench.and.screwdriver.fill",
               title: "Repair worker",
               subtitle: "Self-healing pipeline & fixes") { RepairWorkerView() }
        navRow(icon: "lock.trianglebadge.exclamationmark",
               title: "Breach corpus",
               subtitle: "Request, parse, ingest datasets") { BreachCorpusView() }
        navRow(icon: "dial.medium",
               title: "Similarity thresholds",
               subtitle: "Embedding cut-offs, tunable live") { SimilarityThresholdsView() }
        navRow(icon: "key.horizontal.fill",
               title: "Platform API keys",
               subtitle: "Server-wide default credentials") {
            ApiKeysView(scope: .platform)
        }
    }

    #if os(macOS)
    /// A titled, separator-free card used by the macOS panel layout.
    @ViewBuilder
    private func card<Content: View>(
        header: String,
        footer: String,
        @ViewBuilder content: () -> Content
    ) -> some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            sectionLabel(header)
            VStack(alignment: .leading, spacing: Space.xs) {
                content()
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(Space.sm)
            .background(theme.surfaceElevated,
                        in: RoundedRectangle(cornerRadius: Radius.md))
            Text(footer)
                .font(.caption2)
                .foregroundStyle(theme.textMuted)
        }
    }
    #endif

    // MARK: - Row styling (mirrors ConsoleHub.row)

    private func navRow<Destination: View>(
        icon: String,
        title: String,
        subtitle: String,
        @ViewBuilder destination: @escaping () -> Destination
    ) -> some View {
        let link = NavigationLink {
            destination()
        } label: {
            HStack(spacing: Space.md) {
                Image(systemName: icon)
                    .font(.body)
                    .foregroundStyle(theme.accent)
                    .frame(width: 28, height: 28)
                    .accessibilityHidden(true)   // title + subtitle follow
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
                #if os(macOS)
                Spacer(minLength: 0)
                Image(systemName: "chevron.right")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                    .accessibilityHidden(true)   // navigation affordance
                #endif
            }
            .padding(.vertical, 2)
        }

        #if os(macOS)
        // Outside a `List` the NavigationLink would render as blue link text;
        // `.plain` restores the row look. Full-width tap target + hover fill.
        return link
            .buttonStyle(.plain)
            .padding(.vertical, Space.xs)
            .padding(.horizontal, Space.sm)
            .frame(maxWidth: .infinity, alignment: .leading)
            .contentShape(Rectangle())
            .compatHoverHighlight(cornerRadius: Radius.sm)
        #else
        return link
        #endif
    }

    private func sectionLabel(_ text: String) -> some View {
        Text(text.uppercased())
            .font(Typography.display(10, weight: .semibold))
            .tracking(1.2)
            .foregroundStyle(theme.textMuted)
    }
}
