import SwiftUI

/// The Search tab's living background: a slow, organic field of colour that
/// swells and drifts while an investigation runs, in the spirit of the Gemini
/// app's loading state.
///
/// WHY MeshGradient AND NOT STACKED BLURRED CIRCLES. The obvious way to build
/// this is a ZStack of `Circle().blur()` blobs on Lissajous paths. That reads as
/// several separate bubbles crossing each other, and it leans on a large
/// per-frame blur for its softness — the most expensive thing you can put behind
/// a scroll view. A mesh gradient is one GPU primitive whose control
/// points are interpolated in the shader: moving them deforms ONE continuous
/// surface, so the field stays coherent — it bulges and folds like a single
/// body of colour rather than like overlapping discs — and it costs one draw.
///
/// SHAPE comes from the 4x4 control grid. The twelve boundary points are locked
/// to their edge (an edge point may slide ALONG the edge but never off it) so
/// the mesh always covers the frame with no gap or vignette; only the four
/// interior points travel freely. Each coordinate rides its own sine at a
/// deliberately non-harmonic frequency, so the loop never visibly repeats.
///
/// COLOUR is deliberately only two hues — the theme accent and its alt — held
/// together by one global `hueRotation` that drifts across a slow cycle. The
/// whole field therefore changes colour AS A UNIT: vivid, always shifting,
/// never a rainbow. The four corners are the plain screen surface, so the
/// colour reads as a coherent mass floating in the middle of the tab instead of
/// a wall-to-wall wash, and the content on top keeps its contrast.
///
/// INTENSITY has two independent parts. A permanent slow breath (in the
/// timeline) keeps the surface alive when the tab is idle. `intensity` is the
/// search-state bloom, applied through animatable modifiers OUTSIDE the
/// timeline — opacity, saturation, scale — so SwiftUI interpolates it and the
/// field swells when a run starts and settles when it finishes, rather than
/// snapping between two states.
///
/// Reduce Motion freezes the clock and drops to the still field. The colour and
/// composition survive; only the movement stops.
struct SearchAuraBackground: View {
    /// 0 = idle, 1 = a run is in flight. Animate changes at the call site.
    var intensity: Double

    @Environment(\.theme) private var theme
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Environment(\.colorScheme) private var colorScheme

    /// Clock origin. The timeline hands out absolute dates, and
    /// `timeIntervalSinceReferenceDate` is ~8e8 — feeding that to `sin` runs the
    /// argument reduction at a magnitude where the per-point phase offsets stop
    /// being meaningfully distinct, and the carefully de-tuned frequencies
    /// collapse toward each other. Measuring from first appearance keeps every
    /// argument small and starts the composition at a known phase.
    ///
    /// `@State`, not `let`: a plain stored property is re-initialised every time
    /// the parent rebuilds this struct, which would jump the animation back to
    /// zero on unrelated view updates.
    @State private var epoch = Date()

    /// Control-point drift, in mesh units (the grid spans 0...1). Small on
    /// purpose: past roughly 0.12 the surface folds through itself and the
    /// gradient creases instead of flowing.
    private let drift: Float = 0.085

    var body: some View {
        TimelineView(.animation(minimumInterval: 1.0 / 30.0,
                                paused: reduceMotion)) { ctx in
            // Reduce Motion holds t at 0 so the field is the still composition
            // rather than whatever phase the clock happened to be at.
            let t: TimeInterval = reduceMotion
                ? 0
                : ctx.date.timeIntervalSince(epoch)

            // The idle breath: never fully still, never attention-seeking.
            let breath = 0.5 + 0.5 * sin(t * 0.11)

            MeshGradient(width: 4, height: 4,
                         points: points(t),
                         colors: meshColors,
                         smoothsColors: true)
                // The blur is NOT what makes this soft — `smoothsColors`
                // interpolates in the shader, so the field is already diffuse.
                // This is a small, fixed radius whose only job is to kill the
                // banding the accent edge shows on a dark OLED panel. It is the
                // one offscreen pass in the view; keep it small (a full-screen
                // blur re-rasterised per frame behind a scroll view is exactly
                // the cost the mesh was chosen to avoid).
                .blur(radius: 12)
                // Overscale so the blur samples real colour past the frame edge
                // instead of transparent black, which would darken the border.
                .scaleEffect(1.18)
                .opacity(0.55 + 0.20 * breath)
                .hueRotation(.degrees(sin(t * 0.031) * 34))
        }
        // ── search-state bloom (animatable; outside the timeline) ──
        .saturation(0.85 + 0.55 * intensity)
        .opacity(0.42 + 0.58 * intensity)
        .scaleEffect(1.0 + 0.06 * intensity)
        .animation(.easeInOut(duration: 1.4), value: intensity)
        // Keeps body text legible over the brightest part of the field. Dark
        // mode needs a real scrim; light mode only a whisper, or the whole
        // effect greys out.
        .overlay(theme.surface.opacity(colorScheme == .dark ? 0.30 : 0.10))
        .ignoresSafeArea()
        .allowsHitTesting(false)
        .accessibilityHidden(true)
    }

    // MARK: - Geometry

    /// Row-major 4x4 control grid. Boundary points slide along their own edge;
    /// the four interior points move in both axes.
    private func points(_ t: TimeInterval) -> [SIMD2<Float>] {
        // Distinct, non-harmonic rates so no two points ever lock into step.
        func w(_ rate: Double, _ phase: Double) -> Float {
            Float(sin(t * rate + phase))
        }
        let d = drift
        let a: Float = 1.0 / 3.0, b: Float = 2.0 / 3.0

        return [
            // top edge (y pinned to 0)
            SIMD2(0, 0),
            SIMD2(a + d * w(0.191, 0.00), 0),
            SIMD2(b + d * w(0.157, 1.13), 0),
            SIMD2(1, 0),

            // upper interior row (x pinned at the two ends)
            SIMD2(0, a + d * w(0.173, 2.31)),
            SIMD2(a + d * w(0.211, 0.77), a + d * w(0.149, 3.02)),
            SIMD2(b + d * w(0.167, 4.18), a + d * w(0.199, 1.55)),
            SIMD2(1, a + d * w(0.181, 5.06)),

            // lower interior row
            SIMD2(0, b + d * w(0.163, 3.74)),
            SIMD2(a + d * w(0.203, 2.09), b + d * w(0.187, 0.42)),
            SIMD2(b + d * w(0.179, 5.51), b + d * w(0.155, 2.88)),
            SIMD2(1, b + d * w(0.193, 1.96)),

            // bottom edge (y pinned to 1)
            SIMD2(0, 1),
            SIMD2(a + d * w(0.151, 4.63), 1),
            SIMD2(b + d * w(0.197, 0.28), 1),
            SIMD2(1, 1),
        ]
    }

    // MARK: - Colour

    /// Corners fall to the screen surface so the colour reads as a mass rather
    /// than a wash; the interior carries accent and accentAlt. Two hues only —
    /// `hueRotation` above moves them together.
    /// Alpha is stated once per stop rather than chained: `Color.opacity()`
    /// MULTIPLIES the existing alpha, so `accent.opacity(0.45).opacity(0.7)`
    /// silently lands at 0.315 and the levels stop meaning what they read as.
    private var meshColors: [Color] {
        let base = theme.surface                  // corners: dissolve into the tab
        let edge = theme.accent.opacity(0.45)     // boundary: the mass's falloff
        let mid  = theme.accent                   // interior: full strength
        let alt  = theme.accentAlt

        // Symmetric, with mid/alt on opposite diagonals so the two hues trade
        // places as the interior points swim past each other.
        return [
            base, edge, edge, base,
            edge, mid,  alt,  edge,
            edge, alt,  mid,  edge,
            base, edge, edge, base,
        ]
    }
}
