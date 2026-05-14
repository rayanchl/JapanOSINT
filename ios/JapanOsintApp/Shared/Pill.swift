import SwiftUI

/// Single source of truth for the pill / chip pattern.
///
/// Tone picks the color from the active palette; `solid` switches between the
/// tinted-translucent style (status pills) and the filled style (count chips
/// over status bars). `maxWidth` caps the pill so an inadvertently long tag
/// (e.g. `host:meta-id17616.invoice-ads-manager.com`) truncates with an
/// ellipsis instead of wrapping the row across four lines.
struct Pill: View {
    enum Tone { case neutral, accent, success, warning, danger, info }
    enum Size { case sm, md }

    let text: String
    var tone: Tone = .neutral
    var size: Size = .sm
    var icon: String? = nil
    var solid: Bool = false
    /// Hard cap on the pill's outer width. Text inside is single-line and
    /// truncates at the tail when it would exceed this width. `nil` lets the
    /// pill take its intrinsic size (default for short status / role pills).
    var maxWidth: CGFloat? = nil

    @Environment(\.theme) private var theme

    var body: some View {
        HStack(spacing: 3) {
            if let icon {
                Image(systemName: icon).font(.caption2)
            }
            Text(text)
                .font(.caption2.bold())
                .lineLimit(1)
                .truncationMode(.tail)
        }
        .padding(.horizontal, size == .sm ? Space.sm - 2 : Space.sm)
        .padding(.vertical, size == .sm ? 2 : 3)
        .foregroundStyle(solid ? Color.white : color)
        .background(
            solid ? color : color.opacity(0.18),
            in: Capsule()
        )
        .frame(maxWidth: maxWidth, alignment: .leading)
    }

    private var color: Color {
        switch tone {
        case .neutral: return theme.textMuted
        case .accent:  return theme.accent
        case .success: return theme.success
        case .warning: return theme.warning
        case .danger:  return theme.danger
        case .info:    return theme.accentAlt
        }
    }
}
