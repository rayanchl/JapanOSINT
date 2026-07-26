import ActivityKit
import WidgetKit
import SwiftUI

// ─────────────────────────────────────────────────────────────────────────────
// Live Activity for a running OSINT search (roadmap 35).
//
// This maps onto machinery that already exists: core/progress.c tracks the
// pipeline round-by-round and Search/PipelineView.swift already renders it.
// The Live Activity is the same content state on the Lock Screen.
//
// `SearchActivityAttributes` lives in
// ios/JapanOsintApp/Widgets/SearchActivityAttributes.swift and needs
// membership in BOTH targets — the app requests/updates the activity, this
// file only renders it. The Widget type itself stays extension-only.
//
// Requires NSSupportsLiveActivities = YES in the APP's Info.plist, not just
// the extension's — otherwise Activity.request throws at runtime.
// Live Activities do not render in the simulator; test on a device.
// ─────────────────────────────────────────────────────────────────────────────

struct SearchLiveActivity: Widget {
    var body: some WidgetConfiguration {
        ActivityConfiguration(for: SearchActivityAttributes.self) { context in
            // Lock Screen / banner presentation.
            VStack(alignment: .leading, spacing: 6) {
                HStack {
                    Image(systemName: "magnifyingglass")
                        .foregroundStyle(WidgetTheme.accent)
                    Text(context.attributes.query)
                        .font(.caption).lineLimit(1)
                        .foregroundStyle(WidgetTheme.text)
                    Spacer()
                    Text("\(context.state.resultsSoFar)")
                        .font(.caption).monospacedDigit()
                        .foregroundStyle(WidgetTheme.accent)
                }
                ProgressView(value: progress(context.state))
                    .tint(WidgetTheme.accent)
                Text(subtitle(context.state))
                    .font(.system(size: 10))
                    .foregroundStyle(WidgetTheme.muted)
            }
            .padding()
            .activityBackgroundTint(Color.black.opacity(0.6))

        } dynamicIsland: { context in
            DynamicIsland {
                DynamicIslandExpandedRegion(.leading) {
                    Image(systemName: "magnifyingglass")
                        .foregroundStyle(WidgetTheme.accent)
                }
                DynamicIslandExpandedRegion(.trailing) {
                    Text("\(context.state.resultsSoFar)")
                        .monospacedDigit()
                        .foregroundStyle(WidgetTheme.accent)
                }
                DynamicIslandExpandedRegion(.bottom) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(context.attributes.query)
                            .font(.caption).lineLimit(1)
                        ProgressView(value: progress(context.state))
                            .tint(WidgetTheme.accent)
                        Text(subtitle(context.state))
                            .font(.system(size: 10))
                            .foregroundStyle(WidgetTheme.muted)
                    }
                }
            } compactLeading: {
                Image(systemName: "magnifyingglass")
                    .foregroundStyle(WidgetTheme.accent)
            } compactTrailing: {
                Text("\(context.state.resultsSoFar)").monospacedDigit()
            } minimal: {
                Image(systemName: "magnifyingglass")
                    .foregroundStyle(WidgetTheme.accent)
            }
            .widgetURL(URL(string:
                "japanosint://search?id=\(context.attributes.requestId)"))
        }
    }

    /// Blend round progress with per-round service progress so the bar moves
    /// during a long round instead of sitting still and looking hung.
    private func progress(_ s: SearchActivityAttributes.ContentState) -> Double {
        guard s.maxRounds > 0 else { return 0 }
        let roundShare = 1.0 / Double(s.maxRounds)
        let done = Double(max(0, s.round - 1)) * roundShare
        let within = s.servicesTotal > 0
            ? Double(s.servicesDone) / Double(s.servicesTotal) * roundShare
            : 0
        return min(1.0, done + within)
    }

    private func subtitle(_ s: SearchActivityAttributes.ContentState) -> String {
        if s.phase == "done" { return "Finished · \(s.resultsSoFar) results" }
        if s.servicesTotal > 0 {
            return "Round \(s.round)/\(s.maxRounds) · \(s.servicesDone)/\(s.servicesTotal) sources"
        }
        return "Round \(s.round)/\(s.maxRounds) · \(s.phase)"
    }
}
