import SwiftUI

/// Compact checkbox-styled toggle for dense feature lists where `.switch`
/// produces visual overlap (LayersTab FEATURES section). Theme-aware.
///
/// Named `Themed…` (and exposed as `.themedCheckbox`) to avoid colliding with
/// SwiftUI's own `CheckboxToggleStyle` / `.checkbox`, which ships on macOS — we
/// want this custom theme-aware look on both platforms for visual parity.
struct ThemedCheckboxToggleStyle: ToggleStyle {
    @Environment(\.theme) private var theme

    func makeBody(configuration: Configuration) -> some View {
        Button {
            configuration.isOn.toggle()
        } label: {
            HStack(spacing: 8) {
                configuration.label
                Spacer(minLength: 8)
                Image(systemName: configuration.isOn ? "checkmark.square.fill" : "square")
                    .font(.body)
                    .foregroundStyle(configuration.isOn ? theme.accent : theme.textMuted)
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

extension ToggleStyle where Self == ThemedCheckboxToggleStyle {
    static var themedCheckbox: ThemedCheckboxToggleStyle { .init() }
}
