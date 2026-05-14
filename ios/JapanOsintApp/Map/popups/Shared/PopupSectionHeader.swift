import SwiftUI

/// Standard subsection title for popup cards: SF Symbol + headline text +
/// optional trailing slot (e.g. a refresh button on Departures). Always
/// claims full horizontal width so the row visually anchors the section.
struct PopupSectionHeader<Trailing: View>: View {
    let title: String
    let icon: String
    @ViewBuilder let trailing: () -> Trailing
    @Environment(\.theme) private var theme

    init(
        _ title: String,
        icon: String,
        @ViewBuilder trailing: @escaping () -> Trailing = { EmptyView() }
    ) {
        self.title = title
        self.icon = icon
        self.trailing = trailing
    }

    var body: some View {
        HStack(spacing: 6) {
            Image(systemName: icon)
                .font(.subheadline.weight(.semibold))
                .foregroundStyle(theme.textMuted)
            Text(title)
                .font(.headline)
                .foregroundStyle(theme.text)
            Spacer()
            trailing()
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}
