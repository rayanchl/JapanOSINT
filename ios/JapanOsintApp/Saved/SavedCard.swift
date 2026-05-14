import SwiftUI

struct SavedCard: View {
    let item: SavedItem

    @EnvironmentObject var registry: LayerRegistry
    @Environment(\.theme) private var theme

    /// Image band height. Every card uses the same band whether or not it
    /// carries a thumbnail, so grids align cleanly across rows.
    private static let imageHeight: CGFloat = 110
    /// Reserved text-section height: 2 lines title + 1 line subtitle + padding.
    /// Picked so the card total height never changes when text wraps.
    private static let textHeight: CGFloat = 56

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            imageView

            VStack(alignment: .leading, spacing: 2) {
                Text(item.displayName)
                    .font(.caption.bold())
                    .foregroundStyle(theme.text)
                    .lineLimit(2, reservesSpace: true)
                Text(LayerRegistry.displayName(forId: item.layerId))
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(1, reservesSpace: true)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(Space.sm)
            .frame(height: Self.textHeight, alignment: .top)
        }
        .frame(height: Self.imageHeight + Self.textHeight)
        .background(theme.surfaceElevated)
        .clipShape(RoundedRectangle(cornerRadius: Radius.md))
    }

    /// Image band — exactly `imageHeight` tall regardless of whether a remote
    /// image is present, so cards align across grid rows.
    @ViewBuilder
    private var imageView: some View {
        if let raw = item.imageURL, let url = URL(string: raw) {
            AsyncImage(url: url) { phase in
                switch phase {
                case .empty:
                    placeholder.overlay(ProgressView().tint(.white))
                case .success(let img):
                    // Pin the layout box first, then overlay the image and
                    // clip. `.scaledToFill()` directly on the image inflates
                    // its intrinsic size to the scaled bitmap dimensions, and
                    // `.clipped()` only clips drawing — not layout — so a wide
                    // thumbnail would push the card wider than its grid cell
                    // and overlap the neighbouring column.
                    Color.clear
                        .frame(maxWidth: .infinity)
                        .frame(height: Self.imageHeight)
                        .overlay(
                            img.resizable().scaledToFill()
                        )
                        .clipped()
                case .failure:
                    placeholder.overlay(layerIcon)
                @unknown default:
                    placeholder
                }
            }
        } else {
            placeholder.overlay(layerIcon)
        }
    }

    /// Top band uses the entity's raw layer color so each card is identifiable
    /// by type at a glance. Always exactly `imageHeight` tall.
    private var placeholder: some View {
        Rectangle()
            .fill(registry.color(for: item.layerId))
            .frame(maxWidth: .infinity)
            .frame(height: Self.imageHeight)
    }

    private var layerIcon: some View {
        Image(systemName: registry.symbol(for: item.layerId))
            .font(.title2.weight(.semibold))
            .foregroundStyle(.white.opacity(0.9))
            .shadow(color: .black.opacity(0.25), radius: 1, y: 1)
    }
}

/// Compact list row used by SavedTab's list mode. Icon · name · type.
struct SavedListRow: View {
    let item: SavedItem
    @EnvironmentObject var registry: LayerRegistry
    @Environment(\.theme) private var theme

    var body: some View {
        HStack(spacing: Space.md) {
            Image(systemName: registry.symbol(for: item.layerId))
                .font(.callout)
                .foregroundStyle(registry.color(for: item.layerId))
                .frame(width: 22)
            VStack(alignment: .leading, spacing: 1) {
                Text(item.displayName)
                    .font(.subheadline.weight(.medium))
                    .foregroundStyle(theme.text)
                    .lineLimit(1)
                Text(LayerRegistry.displayName(forId: item.layerId))
                    .font(.caption2)
                    .foregroundStyle(theme.textMuted)
                    .lineLimit(1)
            }
            Spacer()
        }
        .padding(.vertical, 2)
        .contentShape(Rectangle())
    }
}
