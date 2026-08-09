import SwiftUI

/// The Search tab's background: a calm vertical fade — a soft glow at the top
/// that dissolves into the dark surface toward the bottom, in the spirit of the
/// Gemini/Claude chat backdrop. It replaced an animated mesh "aura": that read
/// as a coloured blob floating mid-screen, where what the tab wants is a quiet
/// top-lit gradient that never competes with the content.
///
/// COLOUR is a single hue — the theme's primary accent (`accent`, amber on the
/// carbon theme) — laid over `theme.surface` (painted underneath). Stating one hue at
/// decreasing opacity down to `.clear` keeps the fade monochromatic and smooth;
/// the lower ~40% is pure surface, i.e. near-black on the carbon theme, matching
/// the reference where the bottom of the screen goes dark.
///
/// MOVEMENT is deliberately minimal. A slow breath keeps the glow barely alive
/// when idle; `intensity` (0 = idle, 1 = a run is in flight) blooms the glow
/// while an investigation runs, animated at the call site. Reduce Motion freezes
/// the breath and shows the still fade — the composition survives, only the
/// movement stops.
struct SearchAuraBackground: View {
    /// 0 = idle, 1 = a run is in flight. Animate changes at the call site.
    var intensity: Double

    @Environment(\.theme) private var theme
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Environment(\.colorScheme) private var colorScheme

    /// Clock origin, measured from first appearance so the `sin` argument stays
    /// small. `@State` so a parent rebuild doesn't reset the phase.
    @State private var epoch = Date()

    var body: some View {
        TimelineView(.animation(minimumInterval: 1.0 / 30.0,
                                paused: reduceMotion)) { ctx in
            let t: TimeInterval = reduceMotion
                ? 0
                : ctx.date.timeIntervalSince(epoch)

            // Idle breath: never fully still, never attention-seeking.
            let breath = 0.5 + 0.5 * sin(t * 0.13)

            // Top-glow strength. Calm at idle, blooms with `intensity`. Halved
            // in light mode, where a strong tint over a light surface muddies
            // the whole page instead of reading as a glow.
            let base = (0.22 + 0.08 * breath) + 0.34 * intensity
            let glow = base * (colorScheme == .dark ? 1.0 : 0.5)

            LinearGradient(
                stops: [
                    .init(color: theme.accent.opacity(glow),        location: 0.00),
                    .init(color: theme.accent.opacity(glow * 0.35), location: 0.30),
                    .init(color: .clear,                            location: 0.62),
                ],
                startPoint: .top,
                endPoint: .bottom
            )
        }
        // Bloom the fade in/out as a run starts and finishes rather than snapping.
        .animation(.easeInOut(duration: 1.4), value: intensity)
        .ignoresSafeArea()
        .allowsHitTesting(false)
        .accessibilityHidden(true)
    }
}
