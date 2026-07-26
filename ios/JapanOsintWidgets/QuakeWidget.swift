import WidgetKit
import SwiftUI

/// Latest earthquake + any active JMA warnings (roadmap 35).
struct QuakeWidget: Widget {
    var body: some WidgetConfiguration {
        StaticConfiguration(kind: "QuakeWidget",
                            provider: SnapshotProvider()) { entry in
            QuakeWidgetView(entry: entry)
                .containerBackground(.fill.tertiary, for: .widget)
        }
        .configurationDisplayName("Latest quake")
        .description("Most recent earthquake and active warnings.")
        .supportedFamilies([.systemSmall, .systemMedium])
    }
}

struct QuakeWidgetView: View {
    let entry: SnapshotEntry
    @Environment(\.widgetFamily) private var family

    var body: some View {
        if !entry.snapshot.connected {
            WidgetDisconnected()
        } else if let q = entry.snapshot.latestQuake {
            VStack(alignment: .leading, spacing: 5) {
                HStack(alignment: .firstTextBaseline, spacing: 5) {
                    if let m = q.magnitude {
                        Text(String(format: "M%.1f", m))
                            .font(.system(.title2, design: .rounded).weight(.semibold))
                            .monospacedDigit()
                            .foregroundStyle(magColor(m))
                    }
                    if let i = q.intensity {
                        Text("震度 \(i)")
                            .font(.caption2)
                            .foregroundStyle(WidgetTheme.muted)
                    }
                }
                Text(q.title)
                    .font(.caption)
                    .lineLimit(2)
                    .foregroundStyle(WidgetTheme.text)
                Text(q.at, style: .relative)
                    .font(.system(size: 9))
                    .foregroundStyle(WidgetTheme.muted)

                if family == .systemMedium, !entry.snapshot.activeWarnings.isEmpty {
                    Divider().opacity(0.3)
                    ForEach(entry.snapshot.activeWarnings.prefix(2), id: \.self) { w in
                        HStack(spacing: 4) {
                            Image(systemName: "exclamationmark.triangle.fill")
                                .font(.system(size: 8))
                                .foregroundStyle(WidgetTheme.danger)
                            Text(w).font(.caption2).lineLimit(1)
                                .foregroundStyle(WidgetTheme.text)
                        }
                    }
                }
                Spacer(minLength: 0)
            }
            .widgetURL(quakeURL(q))
        } else {
            VStack(spacing: 3) {
                Image(systemName: "waveform.path.ecg")
                    .foregroundStyle(WidgetTheme.muted)
                Text("No recent quake").font(.caption2)
                    .foregroundStyle(WidgetTheme.muted)
            }
        }
    }

    /// JMA-ish banding. Kept coarse on purpose — a widget implying precision
    /// it does not have is worse than one that just flags "big".
    private func magColor(_ m: Double) -> Color {
        if m >= 6.0 { return WidgetTheme.danger }
        if m >= 4.5 { return WidgetTheme.accent }
        return WidgetTheme.text
    }

    private func quakeURL(_ q: WidgetQuake) -> URL? {
        guard let lat = q.lat, let lon = q.lon else {
            return URL(string: "japanosint://map")
        }
        return URL(string: "japanosint://map?lat=\(lat)&lon=\(lon)")
    }
}
