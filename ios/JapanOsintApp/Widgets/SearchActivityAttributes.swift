import Foundation
#if canImport(ActivityKit)
import ActivityKit
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Live Activity contract for a running OSINT search (roadmap 35).
//
// Split out from the widget's SearchLiveActivity.swift deliberately: the APP
// calls Activity.request/update/end, the EXTENSION renders it, so only the
// attributes are shared. Putting the `Widget` type itself in the app target
// would drag WidgetKit view code somewhere it does not belong.
//
// GIVE THIS FILE TARGET MEMBERSHIP IN BOTH the app and JapanOsintWidgets —
// same as SharedSnapshot.swift. See docs/ios-extensions-plan.md.
//
// This mirrors state the backend already produces: core/progress.c tracks the
// pipeline round-by-round and Search/PipelineView.swift renders the same thing
// in-app. Requires NSSupportsLiveActivities in the APP's Info.plist, and a
// real device — Live Activities do not render in the simulator.
// ─────────────────────────────────────────────────────────────────────────────

#if canImport(ActivityKit)
struct SearchActivityAttributes: ActivityAttributes {
    public struct ContentState: Codable, Hashable {
        var phase: String          // analyzing | dispatching | pivoting | done
        var round: Int
        var maxRounds: Int
        var servicesDone: Int
        var servicesTotal: Int
        var resultsSoFar: Int
    }

    var query: String
    var requestId: String
}
#endif
