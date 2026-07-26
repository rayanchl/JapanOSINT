import WidgetKit
import SwiftUI

/// Lock Screen / StandBy accessory: unread alert count (roadmap 35).
///
/// Accessory families render in a monochrome vibrant context — colour is
/// mostly ignored by the system, so this leans on glyph + number rather than
/// tint to carry the state.
struct InboxAccessoryWidget: Widget {
    var body: some WidgetConfiguration {
        StaticConfiguration(kind: "InboxAccessoryWidget",
                            provider: SnapshotProvider()) { entry in
            InboxAccessoryView(entry: entry)
                .containerBackground(.clear, for: .widget)
        }
        .configurationDisplayName("Unread alerts")
        .description("Unread alert matches at a glance.")
        .supportedFamilies([.accessoryCircular, .accessoryInline,
                            .accessoryRectangular])
    }
}

struct InboxAccessoryView: View {
    let entry: SnapshotEntry
    @Environment(\.widgetFamily) private var family

    var body: some View {
        switch family {
        case .accessoryInline:
            // Inline gets one line of text and no custom layout.
            Label("\(entry.snapshot.unreadAlerts) unread",
                  systemImage: "tray.full.fill")

        case .accessoryCircular:
            Gauge(value: Double(min(entry.snapshot.unreadAlerts, 99)),
                  in: 0...99) {
                Image(systemName: "tray.full.fill")
            } currentValueLabel: {
                Text("\(entry.snapshot.unreadAlerts)").monospacedDigit()
            }
            .gaugeStyle(.accessoryCircular)

        default:
            VStack(alignment: .leading, spacing: 1) {
                Label("\(entry.snapshot.unreadAlerts) unread",
                      systemImage: "tray.full.fill")
                    .font(.headline)
                if let a = entry.snapshot.latestAlerts.first {
                    Text(a.title ?? a.ruleName ?? "Match")
                        .font(.caption2).lineLimit(1)
                } else {
                    Text("Nothing new").font(.caption2)
                }
            }
        }
    }
}
