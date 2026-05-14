import SwiftUI

/// Bottom-edge map slider. Compact by default; tap the chevron to expand
/// into the window selector + step controls. LIVE pill snaps `at = nil`.
///
/// Refetching is gated by `PlaybackState.isScrubbing` — MapTab's onChange
/// handlers only fire when scrubbing ends, so the map never redraws mid-drag.
struct TimeSliderView: View {
    @EnvironmentObject var playback: PlaybackState
    @Environment(\.theme) private var theme

    @State private var expanded: Bool = false
    @State private var sliderValue: Double = 1.0   // 0 = oldest, 1 = LIVE
    @State private var dragging: Bool = false
    @State private var pulseScale: CGFloat = 1.0
    @State private var snapTick: Int = 0           // bump to trigger haptics
    @State private var returnedToLive: Int = 0

    /// Slider range — 7 days back from now to now. Updated when availability
    /// data lands; until then we use the static 7-day default.
    private var lowerBound: Date {
        playback.availableMin ?? Date().addingTimeInterval(-7 * 86400)
    }
    private var upperBound: Date {
        playback.availableMax ?? Date()
    }

    var body: some View {
        VStack(spacing: expanded ? 10 : 4) {
            compactRow
            if expanded {
                Divider().opacity(0.4)
                windowChips
                stepAndPickerRow
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 10)
        .background(.bar, in: RoundedRectangle(cornerRadius: 18, style: .continuous))
        .overlay(
            ZStack {
                RoundedRectangle(cornerRadius: 18, style: .continuous)
                    .strokeBorder(Color.primary.opacity(0.08), lineWidth: 0.5)
                if playback.isReplaying {
                    RoundedRectangle(cornerRadius: 18, style: .continuous)
                        .strokeBorder(theme.accent.opacity(0.6), lineWidth: 1)
                }
            }
        )
        .shadow(color: .black.opacity(0.18), radius: 12, y: 4)
        .sensoryFeedback(.selection, trigger: playback.window)
        .sensoryFeedback(.success, trigger: returnedToLive)
        .sensoryFeedback(.impact(weight: .light), trigger: snapTick)
        .animation(.spring(response: 0.36, dampingFraction: 0.82), value: expanded)
        .onChange(of: playback.at) { _, new in
            // Keep the slider thumb in sync if external code (LIVE pill,
            // step buttons, DatePicker) changed `at`.
            if let at = new {
                sliderValue = normalise(at)
            } else {
                sliderValue = 1.0
            }
        }
        .onAppear { startPulse() }
    }

    // ── Compact row ────────────────────────────────────────────────────────
    // Two thin rows: status (LIVE pill + time) on top, controls (slider +
    // window chip) on bottom. A single horizontal row crammed everything
    // and SwiftUI fell back to per-character wrapping ("LI / V / E",
    // "2026-0 / 5-11"). The split lets the slider fill the width and each
    // capsule's text get its intrinsic width via `.fixedSize`.

    private var compactRow: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 8) {
                livePill
                    .fixedSize()
                timeLabel
                    .layoutPriority(1)
                Spacer(minLength: 0)
                Button {
                    expanded.toggle()
                } label: {
                    Image(systemName: expanded ? "chevron.down" : "chevron.up")
                        .font(.system(size: 13, weight: .semibold))
                        .frame(width: 24, height: 24)
                        .background(.thinMaterial, in: Circle())
                }
                .buttonStyle(.plain)
            }
            HStack(spacing: 14) {
                slider
                    .frame(maxWidth: .infinity)
                    // Padding reserves room so the slider's thumb (8pt
                    // radius idle, 11pt while dragging) never butts up
                    // against the trailing window chip.
                    .padding(.trailing, 4)
                windowChipCompact
                    .fixedSize()
            }
        }
    }

    // ── LIVE pill ──────────────────────────────────────────────────────────

    private var livePill: some View {
        Button {
            if playback.isReplaying {
                withAnimation(.spring(response: 0.3, dampingFraction: 0.78)) {
                    playback.resumeLive()
                }
                returnedToLive &+= 1
            }
        } label: {
            HStack(spacing: 6) {
                Circle()
                    .fill(playback.isReplaying ? Color.secondary : Color.red)
                    .frame(width: 8, height: 8)
                    .scaleEffect(playback.isReplaying ? 1.0 : pulseScale)
                Text(playback.isReplaying ? relativeAgo(playback.at) : "LIVE")
                    .font(.caption.weight(.bold))
                    .monospacedDigit()
                    .foregroundStyle(playback.isReplaying ? theme.textMuted : theme.danger)
                    .lineLimit(1)
                    .fixedSize(horizontal: true, vertical: false)
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 6)
            .background(.thinMaterial, in: Capsule())
            // Red border in LIVE state matches the pulsing red dot; secondary
            // gray during replay so it visually steps back while the replay
            // window indicator (the outer card glow) carries the accent.
            .overlay(
                Capsule().stroke(playback.isReplaying ? Color.secondary.opacity(0.3)
                                                      : Color.red.opacity(0.55),
                                  lineWidth: 1)
            )
        }
        .buttonStyle(.plain)
    }

    private func startPulse() {
        guard !playback.isReplaying else { return }
        withAnimation(.easeInOut(duration: 1.0).repeatForever(autoreverses: true)) {
            pulseScale = 1.18
        }
    }

    // ── Time label ─────────────────────────────────────────────────────────

    private var timeLabel: some View {
        let dt = playback.at ?? Date()
        return VStack(alignment: .leading, spacing: 1) {
            Text(timeFormatter.string(from: dt))
                .font(.caption.monospacedDigit())
                .foregroundStyle(playback.isReplaying ? .primary : .secondary)
                .lineLimit(1)
                .truncationMode(.tail)
                .contentTransition(.numericText())
                .animation(.snappy, value: dt)
            Text(playback.isReplaying ? "Replay · \(playback.window.label) window" : "Now · JST")
                .font(.caption2)
                .foregroundStyle(.tertiary)
                .lineLimit(1)
                .truncationMode(.tail)
        }
    }

    // ── Slider ─────────────────────────────────────────────────────────────
    // Custom scrubber: capsule track + circular thumb + floating time bubble
    // during drag (Music-app-style). The map does NOT refetch mid-drag —
    // PlaybackState.isScrubbing gates that in MapTab. Bubble + accent fill
    // give the live visual feedback during scrubbing.

    private var slider: some View {
        TimeScrubber(
            value: $sliderValue,
            isDragging: $dragging,
            bubbleText: bubbleTimeFormatter.string(from: denormalise(sliderValue)),
            tint: theme.accent,
            onBegan: {
                playback.isScrubbing = true
                UIImpactFeedbackGenerator(style: .soft).impactOccurred()
            },
            onEnded: {
                playback.isScrubbing = false
                dragging = false
                let snapped = denormalise(sliderValue)
                playback.at = (sliderValue >= 0.999) ? nil : snapped
                if sliderValue >= 0.999 { returnedToLive &+= 1 }
                snapTick &+= 1
            }
        )
        .frame(height: 28)
    }

    private var bubbleTimeFormatter: DateFormatter {
        let f = DateFormatter()
        // Compact bubble: MM-dd HH:mm in JST — enough resolution while
        // scrubbing without overflowing the bubble.
        f.dateFormat = "MM-dd  HH:mm"
        f.timeZone = TimeZone(identifier: "Asia/Tokyo")
        return f
    }

    // ── Window chip (compact) ──────────────────────────────────────────────

    private var windowChipCompact: some View {
        Menu {
            ForEach(TimeWindow.allCases) { w in
                Button {
                    playback.window = w
                } label: {
                    if w == playback.window {
                        Label(w.label, systemImage: "checkmark")
                    } else {
                        Text(w.label)
                    }
                }
            }
        } label: {
            Text(playback.window.label)
                .font(.caption.weight(.semibold).monospacedDigit())
                .lineLimit(1)
                .fixedSize(horizontal: true, vertical: false)
                .padding(.horizontal, 10)
                .padding(.vertical, 6)
                .background(.thinMaterial, in: Capsule())
                .overlay(
                    Capsule().stroke(Color.secondary.opacity(0.25), lineWidth: 1)
                )
        }
        .buttonStyle(.plain)
    }

    // ── Window chip strip (expanded) ───────────────────────────────────────

    private var windowChips: some View {
        // Centered chip strip. On every iPhone width currently in production
        // (down to SE) the seven chips comfortably fit inside the card; the
        // surrounding Spacers center them horizontally. If the strip ever
        // needs to grow past the available width, wrap this HStack in
        // ScrollView(.horizontal) again — but that breaks centering.
        HStack(spacing: 6) {
            Spacer(minLength: 0)
            ForEach(TimeWindow.allCases) { w in
                Button {
                    withAnimation(.spring(response: 0.25, dampingFraction: 0.85)) {
                        playback.window = w
                    }
                } label: {
                    Text(w.label)
                        .font(.caption.weight(playback.window == w ? .bold : .regular).monospacedDigit())
                        .lineLimit(1)
                        .fixedSize(horizontal: true, vertical: false)
                        .padding(.horizontal, 10)
                        .padding(.vertical, 5)
                        .background(
                            Capsule()
                                .fill(playback.window == w ? theme.accent.opacity(0.18) : Color.clear)
                        )
                        .overlay(
                            Capsule().stroke(
                                playback.window == w ? theme.accent.opacity(0.7)
                                                     : Color.secondary.opacity(0.25),
                                lineWidth: 1
                            )
                        )
                }
                .buttonStyle(.plain)
            }
            Spacer(minLength: 0)
        }
    }

    // ── Step row (expanded) ────────────────────────────────────────────────

    /// Step buttons flank the date+time picker in one row:
    ///   [VStack: −1d / −1h]   [DatePicker centered]   [VStack: +1h / +1d]
    /// Picker stays centered via Spacers; step buttons stack vertically so
    /// the row stays compact even on narrow widths.
    private var stepAndPickerRow: some View {
        HStack(alignment: .center, spacing: 10) {
            VStack(spacing: 6) {
                stepButton("−1d", seconds: -86400)
                stepButton("−1h", seconds: -3600)
            }
            Spacer(minLength: 0)
            DatePicker(
                "",
                selection: Binding(
                    get: { playback.at ?? Date() },
                    set: { playback.at = $0 }
                ),
                in: lowerBound ... upperBound,
                displayedComponents: [.date, .hourAndMinute]
            )
            .labelsHidden()
            .font(.caption)
            Spacer(minLength: 0)
            VStack(spacing: 6) {
                stepButton("+1h", seconds: 3600)
                stepButton("+1d", seconds: 86400)
            }
        }
    }

    private func stepButton(_ label: String, seconds: TimeInterval) -> some View {
        Button {
            let base = playback.at ?? Date()
            let next = base.addingTimeInterval(seconds)
            // Clamp to selectable range; if we'd cross "now" going forward,
            // snap back to LIVE so the user can step out of replay cleanly.
            if next >= upperBound.addingTimeInterval(-1) {
                playback.resumeLive()
                returnedToLive &+= 1
            } else if next < lowerBound {
                playback.at = lowerBound
            } else {
                playback.at = next
            }
        } label: {
            Text(label)
                .font(.caption.weight(.semibold).monospacedDigit())
                .lineLimit(1)
                .fixedSize(horizontal: true, vertical: false)
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
                .background(.thinMaterial, in: Capsule())
        }
        .buttonStyle(.plain)
    }

    // ── Helpers ────────────────────────────────────────────────────────────

    /// Map slider 0…1 → Date along [lower, upper].
    private func denormalise(_ v: Double) -> Date {
        let lo = lowerBound.timeIntervalSinceReferenceDate
        let hi = upperBound.timeIntervalSinceReferenceDate
        let t  = lo + (hi - lo) * v.clamped(0, 1)
        return Date(timeIntervalSinceReferenceDate: t)
    }

    /// Map Date → slider 0…1.
    private func normalise(_ d: Date) -> Double {
        let lo = lowerBound.timeIntervalSinceReferenceDate
        let hi = upperBound.timeIntervalSinceReferenceDate
        guard hi > lo else { return 1.0 }
        return ((d.timeIntervalSinceReferenceDate - lo) / (hi - lo)).clamped(0, 1)
    }

    private var timeFormatter: DateFormatter {
        let f = DateFormatter()
        f.dateFormat = "yyyy-MM-dd  HH:mm:ss"
        f.timeZone = TimeZone(identifier: "Asia/Tokyo")
        return f
    }

    private func relativeAgo(_ at: Date?) -> String {
        guard let at else { return "—" }
        let s = -at.timeIntervalSinceNow
        if s < 60        { return "\(Int(s))s ago" }
        if s < 3600      { return "\(Int(s / 60))m ago" }
        if s < 86400     { return "\(Int(s / 3600))h ago" }
        return "\(Int(s / 86400))d ago"
    }
}

private extension Double {
    func clamped(_ lo: Double, _ hi: Double) -> Double { Swift.max(lo, Swift.min(hi, self)) }
}

// MARK: - Custom scrubber

/// Apple-style scrubber: capsule track, scaling thumb, floating time bubble
/// during drag. Emits drag start / end callbacks so the parent can fire
/// haptics + gate map refetch (only on release).
private struct TimeScrubber: View {
    @Binding var value: Double
    @Binding var isDragging: Bool
    let bubbleText: String
    let tint: Color
    let onBegan: () -> Void
    let onEnded: () -> Void

    @GestureState private var startValue: Double? = nil

    var body: some View {
        GeometryReader { geo in
            let w = geo.size.width
            let h = geo.size.height
            let thumbX = max(0, min(w, CGFloat(value) * w))
            ZStack(alignment: .leading) {
                // Inactive (right) track
                Capsule()
                    .fill(Color.secondary.opacity(0.22))
                    .frame(height: 3)
                    .frame(maxWidth: .infinity)
                // Active (left) fill from leading edge to thumb
                Capsule()
                    .fill(tint.opacity(0.85))
                    .frame(width: thumbX, height: 3)
                // Thumb
                ZStack {
                    Circle()
                        .fill(.white)
                        .frame(width: isDragging ? 22 : 16, height: isDragging ? 22 : 16)
                        .shadow(color: .black.opacity(0.18), radius: isDragging ? 6 : 3, y: 1)
                    Circle()
                        .stroke(tint.opacity(0.65), lineWidth: isDragging ? 2 : 1)
                        .frame(width: isDragging ? 22 : 16, height: isDragging ? 22 : 16)
                }
                .offset(x: thumbX - (isDragging ? 11 : 8))
                .animation(.spring(response: 0.28, dampingFraction: 0.7), value: isDragging)
                // Floating bubble — only during drag, follows thumb.
                if isDragging {
                    bubble
                        .offset(x: thumbX - 50, y: -h - 4)
                        .transition(.scale(scale: 0.85).combined(with: .opacity))
                }
            }
            .frame(height: h, alignment: .center)
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .updating($startValue) { _, state, _ in
                        if state == nil { state = value }
                    }
                    .onChanged { g in
                        if !isDragging {
                            isDragging = true
                            onBegan()
                        }
                        let dx = g.translation.width / max(1, w)
                        let base = startValue ?? value
                        value = Swift.max(0, Swift.min(1, base + dx))
                    }
                    .onEnded { _ in
                        isDragging = false
                        onEnded()
                    }
            )
        }
    }

    private var bubble: some View {
        Text(bubbleText)
            .font(.caption2.weight(.semibold).monospacedDigit())
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
            .background(.thickMaterial, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
            .overlay(
                RoundedRectangle(cornerRadius: 8, style: .continuous)
                    .stroke(tint.opacity(0.4), lineWidth: 0.5)
            )
            .frame(width: 100)
            .shadow(color: .black.opacity(0.2), radius: 6, y: 2)
            .animation(.spring(response: 0.3, dampingFraction: 0.8), value: bubbleText)
    }
}
