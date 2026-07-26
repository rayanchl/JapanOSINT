import WidgetKit
import SwiftUI

/// Unread alert count + the most recent matches (roadmap 35).
/// Deep-links into the app's inbox via the existing `japanosint://` scheme.
struct AlertInboxWidget: Widget {
    var body: some WidgetConfiguration {
        StaticConfiguration(kind: "AlertInboxWidget",
                            provider: SnapshotProvider()) { entry in
            AlertInboxWidgetView(entry: entry)
                .containerBackground(.fill.tertiary, for: .widget)
        }
        .configurationDisplayName("Alert inbox")
        .description("Unread matches from your alert rules.")
        .supportedFamilies([.systemSmall, .systemMedium])
    }
}

struct AlertInboxWidgetView: View {
    let entry: SnapshotEntry
    @Environment(\.widgetFamily) private var family

    var body: some View {
        if !entry.snapshot.connected {
            WidgetDisconnected()
        } else {
            VStack(alignment: .leading, spacing: 6) {
                header
                if family == .systemMedium { matches }
                Spacer(minLength: 0)
                WidgetStaleness(generatedAt: entry.snapshot.generatedAt)
            }
            .widgetURL(URL(string: "japanosint://inbox"))
        }
    }

    private var header: some View {
        HStack(alignment: .firstTextBaseline, spacing: 6) {
            Image(systemName: entry.snapshot.unreadAlerts > 0
                  ? "tray.full.fill" : "tray")
                .foregroundStyle(entry.snapshot.unreadAlerts > 0
                                 ? WidgetTheme.accent : WidgetTheme.muted)
            Text("\(entry.snapshot.unreadAlerts)")
                .font(.system(.title, design: .rounded).weight(.semibold))
                .monospacedDigit()
                .foregroundStyle(WidgetTheme.text)
            Text(entry.snapshot.unreadAlerts == 1 ? "unread" : "unread")
                .font(.caption2)
                .foregroundStyle(WidgetTheme.muted)
        }
    }

    private var matches: some View {
        VStack(alignment: .leading, spacing: 3) {
            ForEach(entry.snapshot.latestAlerts.prefix(3)) { a in
                HStack(spacing: 4) {
                    Circle()
                        .fill(WidgetTheme.accent)
                        .frame(width: 4, height: 4)
                    Text(a.title ?? a.ruleName ?? "Match")
                        .font(.caption2)
                        .lineLimit(1)
                        .foregroundStyle(WidgetTheme.text)
                }
            }
            if entry.snapshot.latestAlerts.isEmpty {
                Text("Nothing new")
                    .font(.caption2)
                    .foregroundStyle(WidgetTheme.muted)
            }
        }
    }
}
