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
                // Chrome: the on/off state is published as the control's
                // accessibility value below, not as an SF Symbol name.
                Image(systemName: configuration.isOn ? "checkmark.square.fill" : "square")
                    .font(.body)
                    .foregroundStyle(configuration.isOn ? theme.accent : theme.textMuted)
                    .accessibilityHidden(true)
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        // A custom `ToggleStyle` built out of a `Button` announces itself as a
        // plain button, so nothing tells VoiceOver whether the box is ticked.
        // These two lines restore what the stock `.switch` style gives free.
        .accessibilityAddTraits(.isToggle)
        .accessibilityValue(configuration.isOn ? "On" : "Off")
    }
}

extension ToggleStyle where Self == ThemedCheckboxToggleStyle {
    static var themedCheckbox: ThemedCheckboxToggleStyle { .init() }
}
