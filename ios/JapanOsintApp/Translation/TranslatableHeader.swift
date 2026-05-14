import SwiftUI
import Translation

/// Header row for a popup card: a title text followed by a `@ViewBuilder`
/// trailing slot (for the `· <type>` chunk), and — only when the title is
/// Japanese — a Translate / 日本語 capsule at the end of the line.
///
/// The title does NOT expand to fill width, so the trailing slot stays tight
/// against the title instead of being pushed to the trailing edge of the
/// container. Translation is on-device via Apple's Translation framework and
/// uses `Configuration.invalidate()` to re-fire on subsequent taps.
struct TranslatableHeader<Trailing: View>: View {
    let text: String
    var font: Font = .title3.bold()
    @ViewBuilder let trailing: () -> Trailing

    @State private var translatedText: String?
    @State private var translationConfig: TranslationSession.Configuration?
    @Environment(\.theme) private var theme
    @EnvironmentObject var settings: AppSettings

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            HStack(alignment: .firstTextBaseline, spacing: 6) {
                Text(translatedText ?? text)
                    .font(font)
                    .foregroundStyle(theme.text)
                    .textSelection(.enabled)
                trailing()
                Spacer(minLength: 6)
                if isJapanese(text) {
                    if translatedText == nil { translateButton } else { revertButton }
                }
            }
            if let r = romajiUnderTitle {
                Text(r)
                    .font(.caption)
                    .italic()
                    .foregroundStyle(theme.textMuted)
                    .textSelection(.enabled)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .translationTask(translationConfig) { session in
            do {
                let r = try await session.translate(text)
                await MainActor.run { translatedText = r.targetText }
            } catch {
                // Silent — Apple's overlay surfaces the language-pack download.
            }
        }
        .onChange(of: text) { _, _ in translatedText = nil }
    }

    /// Romaji rendered on its own line below the title (the user's "appended
    /// under for titles" requirement). Nil when the toggle is off, the title
    /// is currently translated, or no transcription is available.
    private var romajiUnderTitle: String? {
        guard settings.showRomaji, translatedText == nil else { return nil }
        return Romaji.transcribe(text)
    }

    private func startTranslation() {
        if translationConfig != nil {
            translationConfig?.invalidate()
        } else {
            translationConfig = TranslationSession.Configuration(
                source: Locale.Language(identifier: "ja"),
                target: Locale.Language(identifier: "en")
            )
        }
    }

    private var translateButton: some View {
        Button(action: startTranslation) {
            tagLabel(systemImage: "translate", title: "Translate")
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Translate Japanese in place")
    }

    private var revertButton: some View {
        Button { translatedText = nil } label: {
            tagLabel(systemImage: "arrow.uturn.backward", title: "日本語")
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Show original Japanese")
    }

    private func tagLabel(systemImage: String, title: String) -> some View {
        HStack(spacing: 3) {
            Image(systemName: systemImage)
                .font(.system(size: 9, weight: .semibold))
            Text(title)
                .font(.caption2)
        }
        .foregroundStyle(theme.textMuted)
        .padding(.horizontal, 6)
        .padding(.vertical, 2)
        .background(theme.surface, in: Capsule())
        .overlay(Capsule().strokeBorder(theme.textMuted.opacity(0.25), lineWidth: 0.5))
    }
}
