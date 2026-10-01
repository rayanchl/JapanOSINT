import SwiftUI

// MARK: - Palette

struct ThemePalette: Equatable {
    let accent: Color
    let accentAlt: Color
    let surface: Color
    let surfaceElevated: Color
    let text: Color
    let textMuted: Color
    let success: Color
    let warning: Color
    let danger: Color
    /// Whether the WHOLE UI should use a monospaced font design (display,
    /// labels, prose — everything). When false, prose is the system default
    /// and only data values (counts, timestamps, IDs, coordinates) are
    /// monospaced via `.monospacedDigit()` at the call site.
    let monospaceAll: Bool

    /// Situation-room palette. Carbon surface, amber primary, cyan secondary.
    /// Reads like an instrument: every glyph is monospaced, every digit
    /// tabular — the screen looks like a console even when it's labels.
    static let cyberpunk = ThemePalette(
        accent:           Color(hex: "FFB347"),  // amber
        accentAlt:        Color(hex: "5BE7F1"),  // cyan
        surface:          Color(hex: "0B0F14"),  // carbon
        surfaceElevated:  Color(hex: "151B23"),  // lifted carbon
        text:             Color(hex: "E8EEF5"),  // paper
        textMuted:        Color(hex: "6E7E94"),  // slate
        success:          Color(hex: "5BE7A0"),  // mint
        warning:          Color(hex: "FFB347"),  // amber (intentional: brand = signal)
        danger:           Color(hex: "FF4D5E"),  // vermilion
        monospaceAll:     true
    )

    static let system = ThemePalette(
        accent:           .accentColor,
        accentAlt:        .green,
        surface:          .compatSurface,
        surfaceElevated:  .compatSurfaceElevated,
        text:             .compatLabel,
        textMuted:        .compatSecondaryLabel,
        success:          .green,
        warning:          .orange,
        danger:           .red,
        monospaceAll:     false
    )
}

extension View {
    /// Unified full-bleed screen background for every tab / detail / sheet.
    ///
    /// Paints `theme.surface` edge-to-edge AND hides the scroll content
    /// background of any enclosed `Form`/`List`/`ScrollView` so they don't punch
    /// through with the platform's own grouped backdrop (the mismatched grey
    /// that made some tabs look different from the dark surface ones). Apply to a
    /// screen's root view so every window reads the same.
    func themedScreenBackground(_ theme: ThemePalette) -> some View {
        self
            .scrollContentBackground(.hidden)
            .background(theme.surface.ignoresSafeArea())
    }

    /// Floating bar surface for the Map's top bar and time-select bar.
    ///
    /// Uses the real Liquid Glass material (`.glassEffect`) — the SAME material
    /// the system `TabView` tab bar (Map / Intel / Search …) renders in at our
    /// iOS 26 / macOS 26 deployment target. The old `.bar` blur was a flat
    /// frosted material: it can't match the refractive, self-shadowing Liquid
    /// Glass the OS draws for the tab bar no matter how it's tinted, which is
    /// why the map's bars always read as a different colour/depth than the tab
    /// bar. `.glassEffect` brings the tab bar's own edge highlight + shadow, so
    /// we don't layer a manual stroke or shadow on top (they'd double up and
    /// shift the look away from the system bar). One definition, so both bars
    /// are provably identical to each other and to the tab bar.
    func mapBarSurface(cornerRadius: CGFloat = 18) -> some View {
        self
            .glassEffect(
                .regular,
                in: RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
            )
    }

    /// Same surface, arbitrary shape — for the controls that sit ON those bars
    /// (LIVE pill, window chips, the expand chevron, the step buttons, the
    /// scrub bubble, the map's layer/geocode/more buttons).
    ///
    /// Those all reached for `.thinMaterial` / `.thickMaterial`, which is a
    /// *different* material from the bar underneath them and from the system
    /// tab bar — the exact mismatch `mapBarSurface` exists to eliminate. One
    /// definition, so every map chrome element is provably the same material.
    func mapBarSurface<S: Shape>(in shape: S) -> some View {
        self.glassEffect(.regular, in: shape)
    }
}

private struct ThemeKey: EnvironmentKey {
    static let defaultValue: ThemePalette = .system
}

extension EnvironmentValues {
    var theme: ThemePalette {
        get { self[ThemeKey.self] }
        set { self[ThemeKey.self] = newValue }
    }
}

// MARK: - Design tokens

/// Spacing scale. One canonical value per role — eliminates the 12-distinct-
/// padding chaos. Use these in `.padding(...)`, `VStack(spacing:)`, etc.
enum Space {
    static let xs:  CGFloat = 4
    static let sm:  CGFloat = 8
    static let md:  CGFloat = 12
    static let lg:  CGFloat = 16
    static let xl:  CGFloat = 24
    static let xxl: CGFloat = 32
}

/// Corner radius scale. `md` is the default card radius.
enum Radius {
    static let sm:   CGFloat = 6
    static let md:   CGFloat = 10
    static let lg:   CGFloat = 14
    static let pill: CGFloat = 999
}

/// Stroke weights. Hairlines for instrument rules, regular for emphasis.
enum Stroke {
    static let hairline: CGFloat = 0.5
    static let regular:  CGFloat = 1
}

// MARK: - Typography

/// Typography tokens. System fonts only — no custom TTF registration.
///
/// `display(...)` returns monospaced (SF Mono). `body(...)` returns the system
/// default (SF). Use these for explicit text styling; for the situation-room
/// "everything monospaced" effect, the cyberpunk palette toggles
/// `monospaceAll = true` and the root applies `.fontDesign(.monospaced)`
/// globally — individual call sites don't need to do anything.
enum Typography {
    /// Apple's smallest legible size. `.caption2` is 11 pt at the default
    /// Dynamic Type setting, so anything the call sites ask for below that is
    /// raised rather than rendered at a size the HIG says is unreadable.
    static let minimumSize: CGFloat = 11

    /// Map a requested point size onto the Dynamic Type ramp.
    ///
    /// SwiftUI has no `Font.system(size:relativeTo:)` — the only system-font
    /// constructor that participates in Dynamic Type is the text-style one
    /// (`Font.system(_:design:)`). So every token resolves to the text style
    /// whose *default* size is nearest the requested size, which both gives us
    /// scaling for free and snaps the app onto Apple's ramp instead of a
    /// hand-rolled one. Default sizes, for reference:
    ///
    ///   caption2 11 · caption 12 · footnote 13 · subheadline 15 · callout 16
    ///   body 17 · title3 20 · title2 22 · title 28 · largeTitle 34
    ///
    /// Sizes below `minimumSize` land on `.caption2` by construction, which is
    /// what enforces the 11 pt floor for every `Typography` call site at once.
    static func textStyle(for size: CGFloat) -> Font.TextStyle {
        switch size {
        case ..<11.5: return .caption2      // ≤ 11
        case ..<12.5: return .caption       // 12
        case ..<14:   return .footnote      // 13
        case ..<15.5: return .subheadline   // 14–15
        case ..<16.5: return .callout       // 16
        case ..<18.5: return .body          // 17–18
        case ..<21:   return .title3        // 19–20
        case ..<25:   return .title2        // 21–24
        case ..<31:   return .title         // 25–30
        default:      return .largeTitle    // 31+
        }
    }

    /// Monospaced display — for headers, counts, timestamps, IDs.
    static func display(_ size: CGFloat, weight: Font.Weight = .semibold) -> Font {
        Font.system(textStyle(for: size), design: .monospaced).weight(weight)
    }

    /// Body / sans — for prose, labels, descriptions.
    static func body(_ size: CGFloat, weight: Font.Weight = .regular) -> Font {
        Font.system(textStyle(for: size), design: .default).weight(weight)
    }

    // Semantic display ramp.
    static let h1 = display(24, weight: .bold)
    static let h2 = display(18, weight: .semibold)
    static let h3 = display(14, weight: .semibold)

    // Monospaced inline — always tabular for shifting digits.
    static let mono      = Font.system(.body,    design: .monospaced)
    static let monoSmall = Font.system(.caption, design: .monospaced)
}

// MARK: - Hex helper

extension Color {
    /// Hex strings like "#00f0ff", "00f0ff", or "0f1729".
    init(hex: String) {
        let s = hex.trimmingCharacters(in: .whitespacesAndNewlines)
                   .replacingOccurrences(of: "#", with: "")
        var v: UInt64 = 0
        Scanner(string: s).scanHexInt64(&v)
        let r, g, b, a: Double
        switch s.count {
        case 6:
            r = Double((v >> 16) & 0xFF) / 255
            g = Double((v >> 8)  & 0xFF) / 255
            b = Double(v         & 0xFF) / 255
            a = 1
        case 8:
            r = Double((v >> 24) & 0xFF) / 255
            g = Double((v >> 16) & 0xFF) / 255
            b = Double((v >> 8)  & 0xFF) / 255
            a = Double(v         & 0xFF) / 255
        default:
            r = 0.5; g = 0.5; b = 0.5; a = 1
        }
        self.init(.sRGB, red: r, green: g, blue: b, opacity: a)
    }
}
