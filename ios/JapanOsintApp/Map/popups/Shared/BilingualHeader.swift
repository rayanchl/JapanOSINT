import SwiftUI

/// Canonical title row for every popup card. Renders
/// `<primary> · <secondary> · <trailing>` inline when both languages are
/// present and distinct, with no Translate capsule (the translation is
/// already on screen — surfacing one would be redundant). Falls through
/// to `TranslatableHeader(text: primary)` when only one language is
/// present; that view auto-renders an in-place Translate / 日本語 capsule
/// if and only if the title contains Japanese characters.
struct BilingualHeader<Trailing: View>: View {
    let primary: String
    let secondary: String?
    @ViewBuilder let trailing: () -> Trailing
    @Environment(\.theme) private var theme

    init(
        primary: String,
        secondary: String? = nil,
        @ViewBuilder trailing: @escaping () -> Trailing = { EmptyView() }
    ) {
        self.primary = primary
        self.secondary = secondary
        self.trailing = trailing
    }

    /// Convenience: derive primary/secondary from a feature's standard
    /// `displayName` and `name_ja` properties. Suppresses `secondary` when
    /// the primary is itself Japanese (i.e. `displayName` already fell
    /// through to `name_ja` because `name` was missing) so the fallback
    /// path can offer translation instead of duplicating the same string.
    init(
        feature: GeoFeature,
        @ViewBuilder trailing: @escaping () -> Trailing = { EmptyView() }
    ) {
        let primary = feature.displayName
        let ja = feature.properties["name_ja"]?.value as? String
        let secondary: String? = {
            guard let ja, !ja.isEmpty, ja != primary, !isJapanese(primary) else { return nil }
            return ja
        }()
        self.init(primary: primary, secondary: secondary, trailing: trailing)
    }

    var body: some View {
        if let secondary, !secondary.isEmpty, secondary != primary, !isJapanese(primary) {
            HStack(alignment: .firstTextBaseline, spacing: 6) {
                Text(primary)
                    .font(.title3.bold())
                    .foregroundStyle(theme.text)
                    .textSelection(.enabled)
                Text("·").font(.title3).foregroundStyle(theme.textMuted)
                Text(secondary)
                    .font(.subheadline)
                    .foregroundStyle(theme.textMuted)
                    .textSelection(.enabled)
                trailing()
                Spacer()
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        } else {
            TranslatableHeader(text: primary, trailing: trailing)
        }
    }
}
