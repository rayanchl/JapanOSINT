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
        surface:          Color(.systemBackground),
        surfaceElevated:  Color(.secondarySystemBackground),
        text:             Color(.label),
        textMuted:        Color(.secondaryLabel),
        success:          .green,
        warning:          .orange,
        danger:           .red,
        monospaceAll:     false
    )
}

private struct ThemeKey: EnvironmentKey {
    static let defaultValue: ThemePalette = .cyberpunk
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
    /// Monospaced display — for headers, counts, timestamps, IDs.
    static func display(_ size: CGFloat, weight: Font.Weight = .semibold) -> Font {
        .system(size: size, weight: weight, design: .monospaced)
    }

    /// Body / sans — for prose, labels, descriptions.
    static func body(_ size: CGFloat, weight: Font.Weight = .regular) -> Font {
        .system(size: size, weight: weight, design: .default)
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
