import SwiftUI
import CoreLocation

// ─────────────────────────────────────────────────────────────────────────────
// Roadmap 18 — the timeline screen.
//
// Histogram strip on top (one stacked bar per bucket: intel / mentions /
// alerts, tap a bar to narrow the list to it), keyset-paged event list below.
//
// ── NAMING, PLEASE READ ─────────────────────────────────────────────────────
// The type is `TimelineScreen`, NOT `TimelineView`. `TimelineView` is
// SwiftUI's own type and this app already uses it unqualified in
// `Map/MapTab.swift` (`TimelineView(.periodic…)`) and five places in
// `Onboarding/OnboardingFlow.swift` (`TimelineView(.animation)`). Declaring a
// module-scope `TimelineView` would shadow SwiftUI's and break all six call
// sites — in files this file's author does not own. The file keeps the
// requested name; the type does not shadow.
//
// ── Time is shared with the map ─────────────────────────────────────────────
// The upper bound of the window is `PlaybackState.at` (nil == LIVE), the same
// instant the map's `TimeSliderView` scrubs. Moving one moves the other, and
// "Show on map" sets the map's clock to the event's timestamp before flying
// there — so the map you land on is the map as it was when the thing happened,
// not the map as it is now with an old pin on it.
// ─────────────────────────────────────────────────────────────────────────────

struct TimelineScreen: View {
    /// Optional case scope. When set the server may answer with
    /// `case_items_table_missing`, which this screen surfaces verbatim as an
    /// explanation rather than rendering an empty timeline as "nothing here".
    var caseId: String? = nil

    @EnvironmentObject var apiClient: APIClient
    @EnvironmentObject var mapNav: MapNavigation
    @EnvironmentObject var playback: PlaybackState
    @Environment(\.theme) private var theme

    @StateObject private var model = TimelineModel()
    @State private var showFilters = false

    var body: some View {
        VStack(spacing: 0) {
            header
            Divider().opacity(0.35)
            listSection
        }
        .themedScreenBackground(theme)
        .navigationTitle("Timeline")
        .compatInlineTitle()
        .toolbar {
            ToolbarItem(placement: .compatPrimary) {
                FilterToolbarButton(isActive: model.sourceFilter != nil) {
                    showFilters = true
                    Task { await model.loadSourcesIfNeeded(using: apiClient.api) }
                }
            }
            ToolbarItem(placement: .compatPrimary) {
                Button {
                    Task { await model.reload(using: apiClient.api) }
                } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .disabled(model.loading)
                .accessibilityLabel("Reload timeline")
            }
        }
        .task {
            model.caseId = caseId
            model.anchor = playback.at
            if model.buckets.isEmpty && model.events.isEmpty {
                await model.reload(using: apiClient.api)
            }
        }
        // Follow the map's clock. Deferred while the slider is mid-drag for the
        // same reason MapTab defers: refetching on every scrub frame would
        // hammer the server and make the strip flicker.
        .onChange(of: playback.at) { _, newValue in
            guard !playback.isScrubbing else { return }
            model.anchor = newValue
            Task { await model.reload(using: apiClient.api) }
        }
        .onChange(of: playback.isScrubbing) { _, scrubbing in
            guard !scrubbing else { return }
            guard model.anchor != playback.at else { return }
            model.anchor = playback.at
            Task { await model.reload(using: apiClient.api) }
        }
        .sheet(isPresented: $showFilters) { filterSheet }
        .navigationDestination(for: TimelineItemRoute.self) { route in
            IntelDetail(uid: route.uid, fallbackTitle: route.title)
        }
        .sensoryFeedback(.selection, trigger: model.bucket)
        .sensoryFeedback(.selection, trigger: model.window)
    }

    // ── Header ─────────────────────────────────────────────────────────────

    private var header: some View {
        VStack(alignment: .leading, spacing: Space.sm) {
            controlRow
            windowChips
            TimelineHistogram(
                buckets: model.buckets,
                size: model.bucket,
                focusedKey: model.focused?.t,
                peak: model.peak
            ) { bucket in
                Task { await model.focus(bucket, using: apiClient.api) }
            }
            captionRow
            banners
        }
        .padding(.horizontal, Space.md)
        .padding(.top, Space.sm)
        .padding(.bottom, Space.sm)
        .compatReadableWidth()
    }

    private var controlRow: some View {
        HStack(spacing: Space.sm) {
            clockChip
            Spacer(minLength: 0)
            legend
            bucketMenu
        }
    }

    /// LIVE / replay pill. Same vocabulary as the map's slider so the two
    /// screens visibly agree about which instant is "now".
    private var clockChip: some View {
        Button {
            if playback.isReplaying {
                playback.resumeLive()
                Haptics.success()
            }
        } label: {
            HStack(spacing: 5) {
                Circle()
                    .fill(model.isReplaying ? theme.textMuted : theme.danger)
                    .frame(width: 7, height: 7)
                Text(model.isReplaying
                     ? TimelineFormat.ago(model.anchor ?? Date())
                     : "LIVE")
                    .font(.caption.weight(.bold))
                    .monospacedDigit()
                    .foregroundStyle(model.isReplaying ? theme.textMuted : theme.danger)
                    .lineLimit(1)
                    .fixedSize(horizontal: true, vertical: false)
            }
            .padding(.horizontal, Space.sm)
            .padding(.vertical, 5)
            .background(
                Capsule().fill((model.isReplaying ? theme.textMuted : theme.danger).opacity(0.14))
            )
            .overlay(
                Capsule().stroke((model.isReplaying ? theme.textMuted : theme.danger).opacity(0.5),
                                 lineWidth: Stroke.regular)
            )
        }
        .buttonStyle(.plain)
        .accessibilityLabel(model.isReplaying ? "Replaying — tap to return to live" : "Live")
    }

    private var legend: some View {
        HStack(spacing: Space.sm) {
            legendDot("Intel", theme.accent, model.totals.intel)
            legendDot("Mentions", theme.accentAlt, model.totals.mentions)
            legendDot("Alerts", theme.danger, model.totals.alerts)
        }
        .fixedSize(horizontal: true, vertical: false)
    }

    private func legendDot(_ label: String, _ color: Color, _ count: Int) -> some View {
        HStack(spacing: 3) {
            RoundedRectangle(cornerRadius: 1.5)
                .fill(color)
                .frame(width: 7, height: 7)
            Text("\(count)")
                .font(.caption2.monospacedDigit())
                .foregroundStyle(theme.textMuted)
        }
        .accessibilityLabel("\(label): \(count)")
    }

    private var bucketMenu: some View {
        Menu {
            ForEach(TimelineBucketSize.allCases) { size in
                Button {
                    Task { await model.setBucket(size, using: apiClient.api) }
                } label: {
                    if size == model.bucket {
                        Label(size.label, systemImage: "checkmark")
                    } else {
                        Text(size.label)
                    }
                }
            }
        } label: {
            HStack(spacing: 3) {
                Image(systemName: "chart.bar.fill").font(.caption2)
                Text(model.bucket.label).font(.caption.weight(.semibold))
            }
            .padding(.horizontal, Space.sm)
            .padding(.vertical, 5)
            .background(Capsule().fill(theme.surfaceElevated))
            .overlay(Capsule().stroke(theme.textMuted.opacity(0.3), lineWidth: Stroke.hairline))
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Histogram granularity: \(model.bucket.label)")
    }

    /// Window presets, drawn exactly like the map slider's window strip.
    private var windowChips: some View {
        HStack(spacing: Space.xs + 2) {
            ForEach(TimelineWindow.allCases) { w in
                Button {
                    Task { await model.setWindow(w, using: apiClient.api) }
                } label: {
                    Text(w.label)
                        .font(.caption.weight(model.window == w ? .bold : .regular).monospacedDigit())
                        .lineLimit(1)
                        .fixedSize(horizontal: true, vertical: false)
                        .padding(.horizontal, Space.sm + 2)
                        .padding(.vertical, 5)
                        .background(
                            Capsule().fill(model.window == w ? theme.accent.opacity(0.18) : Color.clear)
                        )
                        .overlay(
                            Capsule().stroke(model.window == w
                                             ? theme.accent.opacity(0.7)
                                             : theme.textMuted.opacity(0.3),
                                             lineWidth: Stroke.regular)
                        )
                        .foregroundStyle(model.window == w ? theme.accent : theme.textMuted)
                }
                .buttonStyle(.plain)
            }
            Spacer(minLength: 0)
            if model.loading {
                ProgressView().controlSize(.mini)
            }
        }
    }

    private var captionRow: some View {
        HStack(spacing: Space.xs) {
            Text(TimelineFormat.stamp(model.displayedSince))
                .font(.caption2.monospacedDigit())
                .foregroundStyle(theme.textMuted)
            Text("→")
                .font(.caption2)
                .foregroundStyle(theme.textMuted)
            Text(TimelineFormat.stamp(model.displayedUntil))
                .font(.caption2.monospacedDigit())
                .foregroundStyle(theme.textMuted)
            Text("JST")
                .font(.caption2)
                .foregroundStyle(theme.textMuted)
            Spacer(minLength: 0)
            if let src = model.sourceFilter {
                Pill(text: src, tone: .accent, icon: "line.3.horizontal.decrease", maxWidth: 150)
            }
        }
    }

    // ── Banners ────────────────────────────────────────────────────────────

    @ViewBuilder
    private var banners: some View {
        if model.windowTooWide {
            VStack(alignment: .leading, spacing: Space.xs) {
                banner("That range is wider than the 366 days this server will scan. "
                       + "The database is measured in gigabytes and an unbounded scan is a "
                       + "denial of service against ourselves, so the request was refused "
                       + "rather than run.",
                       tone: theme.warning, icon: "arrow.left.and.right")
                Button("Narrow to 30 days") {
                    Task { await model.narrowToLargestAllowed(using: apiClient.api) }
                }
                .font(.caption.weight(.semibold))
                .buttonStyle(.bordered)
            }
        }
        if let err = model.error {
            banner(err, tone: theme.danger, icon: "exclamationmark.triangle.fill")
        }
        if let notes = model.meta?.notes, !notes.isEmpty {
            ForEach(notes, id: \.self) { note in
                banner(TimelineModel.explain(note: note),
                       tone: TimelineModel.isNarrowing(note: note) ? theme.warning : theme.accentAlt,
                       icon: TimelineModel.isNarrowing(note: note)
                             ? "exclamationmark.bubble" : "info.circle")
            }
        }
        if let focused = model.focused {
            HStack(spacing: Space.sm) {
                Image(systemName: "scope").font(.caption2).foregroundStyle(theme.accent)
                Text("Showing \(TimelineFormat.bucketLabel(focused.t, size: model.bucket)) only · "
                     + "\(focused.total) events in that bucket")
                    .font(.caption2.monospacedDigit())
                    .foregroundStyle(theme.text)
                Spacer(minLength: 0)
                Button("Clear") {
                    Task { await model.focus(nil, using: apiClient.api) }
                }
                .font(.caption2.weight(.semibold))
                .buttonStyle(.plain)
                .foregroundStyle(theme.accent)
            }
            .padding(Space.sm)
            .background(theme.accent.opacity(0.10), in: RoundedRectangle(cornerRadius: Radius.sm))
        }
    }

    private func banner(_ text: String, tone: Color, icon: String) -> some View {
        HStack(alignment: .top, spacing: Space.sm) {
            Image(systemName: icon).font(.caption).foregroundStyle(tone)
            Text(text)
                .font(.caption2)
                .foregroundStyle(theme.text)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
        }
        .padding(Space.sm)
        .background(tone.opacity(0.12), in: RoundedRectangle(cornerRadius: Radius.sm))
    }

    // ── Event list ─────────────────────────────────────────────────────────

    @ViewBuilder
    private var listSection: some View {
        if model.loading && model.events.isEmpty {
            ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
        } else if model.events.isEmpty {
            emptyState
        } else {
            List {
                ForEach(model.events) { event in
                    eventRow(event)
                }
                if model.hasMore {
                    loadMoreRow
                }
            }
            .listStyle(.plain)
            .scrollContentBackground(.hidden)
            .refreshable { await model.reload(using: apiClient.api) }
        }
    }

    private var loadMoreRow: some View {
        HStack(spacing: Space.sm) {
            Spacer(minLength: 0)
            if model.loadingMore {
                ProgressView().controlSize(.small)
                Text("Loading older events…")
                    .font(.caption2).foregroundStyle(theme.textMuted)
            } else {
                Button("Load older events") {
                    Task { await model.loadMore(using: apiClient.api) }
                }
                .font(.caption.weight(.semibold))
                .buttonStyle(.plain)
                .foregroundStyle(theme.accent)
            }
            Spacer(minLength: 0)
        }
        .padding(.vertical, Space.sm)
        .listRowBackground(theme.surface)
        // Keyset paging: appearing at the bottom pulls the next page. The
        // button above is the same action, kept visible so paging is still
        // reachable when the list is short enough not to scroll.
        .onAppear {
            Task { await model.loadMore(using: apiClient.api) }
        }
    }

    @ViewBuilder
    private func eventRow(_ event: TimelineEvent) -> some View {
        if let uid = event.uid, !uid.isEmpty {
            NavigationLink(value: TimelineItemRoute(uid: uid,
                                                    title: event.title ?? uid)) {
                rowContent(event)
            }
            .listRowBackground(theme.surface)
            .contextMenu { rowMenu(event) }
        } else {
            rowContent(event)
                .listRowBackground(theme.surface)
                .contextMenu { rowMenu(event) }
        }
    }

    private func rowContent(_ event: TimelineEvent) -> some View {
        HStack(alignment: .top, spacing: Space.md) {
            Image(systemName: glyph(for: event.kind))
                .font(.caption)
                .foregroundStyle(color(for: event.kind))
                .frame(width: 24, height: 24)
                .background(color(for: event.kind).opacity(0.14),
                            in: RoundedRectangle(cornerRadius: Radius.sm))

            VStack(alignment: .leading, spacing: 3) {
                Text(event.title ?? event.uid ?? "(untitled)")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(theme.text)
                    .lineLimit(2)
                HStack(spacing: Space.xs + 2) {
                    Text(TimelineFormat.stamp(iso: event.ts))
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                    Text(event.kind.uppercased())
                        .font(.caption2.weight(.semibold))
                        .foregroundStyle(color(for: event.kind))
                    if let src = event.source_id, !src.isEmpty {
                        Text("· \(src)")
                            .font(.caption2.monospaced())
                            .foregroundStyle(theme.textMuted)
                            .lineLimit(1)
                    }
                }
                if let c = event.coordinate {
                    Text(String(format: "%.4f, %.4f", c.latitude, c.longitude))
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(theme.textMuted)
                }
            }

            Spacer(minLength: Space.xs)

            if event.coordinate != nil {
                // Independent hit target so tapping "map" does not fire the
                // row's NavigationLink (same construction as IntelSourceRow's
                // Run button).
                Button {
                    showOnMap(event)
                } label: {
                    Image(systemName: "mappin.and.ellipse")
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(theme.accentAlt)
                        .frame(width: 30, height: 30)
                        .background(theme.accentAlt.opacity(0.14), in: Circle())
                }
                .frame(minWidth: 44, minHeight: 44)
                .contentShape(Rectangle())
                .buttonStyle(.plain)
                .accessibilityLabel("Show on map")
            }
        }
        .padding(.vertical, 2)
    }

    @ViewBuilder
    private func rowMenu(_ event: TimelineEvent) -> some View {
        if event.coordinate != nil {
            Button {
                showOnMap(event)
            } label: {
                Label("Show on map at this time", systemImage: "mappin.and.ellipse")
            }
        }
        Button {
            setMapClock(event)
        } label: {
            Label("Set map clock here", systemImage: "clock.arrow.circlepath")
        }
        if let uid = event.uid, !uid.isEmpty {
            Button {
                Clipboard.copy(uid)
                Haptics.selection()
            } label: {
                Label("Copy item uid", systemImage: "doc.on.doc")
            }
        }
    }

    private var emptyState: some View {
        ContentUnavailableView {
            Label("No events in this window", systemImage: "clock")
                .foregroundStyle(theme.text)
        } description: {
            Text(emptyDescription)
                .foregroundStyle(theme.textMuted)
        } actions: {
            if model.isFiltered {
                Button("Clear filters") {
                    Task { await model.clearFilters(using: apiClient.api) }
                }
            } else {
                Button("Widen to 30 days") {
                    Task { await model.setWindow(.d30, using: apiClient.api) }
                }
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    private var emptyDescription: String {
        if let notes = model.meta?.notes, notes.contains("case_items_table_missing") {
            return "The server could not apply the case filter on this deployment, so it "
                 + "returned nothing rather than showing you an unrelated timeline. "
                 + "This is not the same as \"the case is empty\"."
        }
        if model.focused != nil {
            return "That histogram bucket has no events at the current filters."
        }
        if model.sourceFilter != nil {
            return "No intel, entity mentions or alert firings from this source in the "
                 + "selected window."
        }
        return "No intel, entity mentions or alert firings landed in the selected window."
    }

    // ── Filter sheet ───────────────────────────────────────────────────────

    private var filterSheet: some View {
        FilterSheet(
            isActive: model.sourceFilter != nil,
            onReset: {
                Task { await model.clearFilters(using: apiClient.api) }
                showFilters = false
            },
            onDone: { showFilters = false }
        ) {
            if let err = model.sourcesError {
                Section {
                    Text(err).font(.caption).foregroundStyle(theme.warning)
                }
            }
            Section("Source") {
                Button {
                    Task { await model.setSource(nil, using: apiClient.api) }
                } label: {
                    HStack {
                        Text("All sources").foregroundStyle(theme.text)
                        Spacer()
                        if model.sourceFilter == nil {
                            Image(systemName: "checkmark").foregroundStyle(theme.accent)
                        }
                    }
                }
                ForEach(model.selectableSources) { entry in
                    Button {
                        Task { await model.setSource(entry.id, using: apiClient.api) }
                    } label: {
                        HStack {
                            VStack(alignment: .leading, spacing: 1) {
                                Text(entry.name).foregroundStyle(theme.text)
                                if entry.name != entry.id {
                                    Text(entry.id)
                                        .font(.caption2.monospaced())
                                        .foregroundStyle(theme.textMuted)
                                }
                            }
                            Spacer()
                            if model.sourceFilter == entry.id {
                                Image(systemName: "checkmark").foregroundStyle(theme.accent)
                            }
                        }
                    }
                }
            }
            Section {
                Text("The source filter is applied to the intel item behind every stream — "
                     + "intel, entity mentions and alert firings alike — so it always means "
                     + "exactly one thing.")
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
            }
        }
    }

    // ── Actions ────────────────────────────────────────────────────────────

    /// Fly the map to this event AND move the map's clock to when it happened.
    /// One clock for both screens (see `PlaybackState`).
    private func showOnMap(_ event: TimelineEvent) {
        guard let coordinate = event.coordinate else { return }
        if let when = TimelineFormat.date(from: event.ts) {
            playback.at = when
        }
        mapNav.showOnMap(coordinate)
        Haptics.success()
    }

    private func setMapClock(_ event: TimelineEvent) {
        guard let when = TimelineFormat.date(from: event.ts) else {
            Haptics.error()
            return
        }
        playback.at = when
        Haptics.selection()
    }

    private func glyph(for kind: String) -> String {
        switch kind {
        case "intel":   return "doc.text.fill"
        case "mention": return "at"
        case "alert":   return "bell.fill"
        default:        return "circle.fill"
        }
    }

    private func color(for kind: String) -> Color {
        switch kind {
        case "intel":   return theme.accent
        case "mention": return theme.accentAlt
        case "alert":   return theme.danger
        default:        return theme.textMuted
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Histogram strip
// ─────────────────────────────────────────────────────────────────────────────

/// Stacked bars — intel at the bottom, then entity mentions, then alert
/// firings — drawn into a `Canvas` rather than a chart library so each bar can
/// be hit-tested by x position (the same trick `GraphCanvasView` uses for
/// node taps). Scrolls horizontally as soon as the bars would be too thin to
/// aim at, so an hourly view over 30 days stays tappable instead of becoming
/// 720 half-point slivers.
private struct TimelineHistogram: View {
    let buckets: [TimelineBucket]
    let size: TimelineBucketSize
    let focusedKey: String?
    let peak: Int
    let onSelect: (TimelineBucket) -> Void

    @Environment(\.theme) private var theme

    private let stripHeight: CGFloat = 110
    private let axisHeight: CGFloat = 14
    private let minSlot: CGFloat = 6
    private let gap: CGFloat = 1

    var body: some View {
        GeometryReader { geo in
            let count = max(buckets.count, 1)
            let fitted = geo.size.width / CGFloat(count)
            let slot = max(minSlot, fitted)
            let contentWidth = slot * CGFloat(count)
            // Roughly one label per 64pt of strip, never more than one per bar.
            let labelStride = max(1, Int((64 / slot).rounded(.up)))

            ScrollView(.horizontal, showsIndicators: false) {
                Canvas { ctx, canvasSize in
                    let baseline = canvasSize.height - axisHeight
                    // Baseline so an all-empty window still reads as an axis
                    // rather than as a broken view.
                    var axis = Path()
                    axis.move(to: CGPoint(x: 0, y: baseline - 0.5))
                    axis.addLine(to: CGPoint(x: canvasSize.width, y: baseline - 0.5))
                    ctx.stroke(axis, with: .color(theme.textMuted.opacity(0.35)),
                               lineWidth: 0.5)

                    for (index, bucket) in buckets.enumerated() {
                        let x = CGFloat(index) * slot
                        let barWidth = max(1, slot - gap)
                        let isFocused = bucket.t == focusedKey

                        if isFocused {
                            let hi = Path(CGRect(x: x, y: 0, width: slot, height: baseline))
                            ctx.fill(hi, with: .color(theme.accent.opacity(0.16)))
                        }

                        if index % labelStride == 0 {
                            ctx.draw(
                                Text(TimelineFormat.bucketLabel(bucket.t, size: size))
                                    .font(.system(size: 8, design: .monospaced))
                                    .foregroundColor(theme.textMuted),
                                at: CGPoint(x: x + slot / 2, y: baseline + axisHeight / 2)
                            )
                        }

                        guard bucket.total > 0 else { continue }
                        let full = CGFloat(bucket.total) / CGFloat(peak) * (baseline - 6)
                        var cursorY = baseline - 1

                        // Stack bottom-up so the intel baseline stays put and
                        // the eye reads growth as alerts stacking on top.
                        let segments: [(Int, Color)] = [
                            (bucket.intel, theme.accent),
                            (bucket.mentions, theme.accentAlt),
                            (bucket.alerts, theme.danger)
                        ]
                        for (value, color) in segments where value > 0 {
                            let share = CGFloat(value) / CGFloat(bucket.total)
                            let segmentHeight = max(1, full * share)
                            let rect = CGRect(x: x + gap / 2,
                                              y: cursorY - segmentHeight,
                                              width: barWidth,
                                              height: segmentHeight)
                            ctx.fill(Path(rect),
                                     with: .color(isFocused ? color : color.opacity(0.85)))
                            cursorY -= segmentHeight
                        }
                    }
                }
                .frame(width: contentWidth, height: stripHeight)
                .contentShape(Rectangle())
                .onTapGesture(coordinateSpace: .local) { point in
                    let index = Int(point.x / slot)
                    guard index >= 0, index < buckets.count else { return }
                    Haptics.selection()
                    onSelect(buckets[index])
                }
                .accessibilityLabel("Event histogram, \(buckets.count) buckets")
            }
            .scrollDisabled(fitted >= minSlot)
            .overlay(alignment: .center) {
                if buckets.isEmpty {
                    Text("No histogram for this window")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }
            }
        }
        .frame(height: stripHeight)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: Radius.sm))
    }
}
