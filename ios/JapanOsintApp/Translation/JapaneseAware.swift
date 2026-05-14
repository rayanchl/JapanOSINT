import SwiftUI
import Translation

/// True when the string contains at least one hiragana, katakana, or CJK ideograph.
/// Pure ASCII / romaji is rejected so we don't show a tag on `osm_overpass`.
func isJapanese(_ s: String) -> Bool {
    s.unicodeScalars.contains { scalar in
        let v = scalar.value
        return (0x3040...0x309F).contains(v)    // Hiragana
            || (0x30A0...0x30FF).contains(v)    // Katakana
            || (0x4E00...0x9FFF).contains(v)    // CJK Unified Ideographs
            || (0x3400...0x4DBF).contains(v)    // CJK Extension A
            || (0xFF66...0xFF9F).contains(v)    // Halfwidth katakana
    }
}

/// Drop-in replacement for `Text(_:)` that appends a small "Translate" capsule
/// when the string is detected as Japanese. Tapping the capsule translates the
/// text *in place* (replacing the Japanese with English on the same line) via
/// Apple's on-device Translation framework — no overlay sheet. A small "JA"
/// capsule then flips it back to the original.
struct JapaneseAware: View {
    let text: String
    var font: Font = .body
    var foregroundStyle: AnyShapeStyle = AnyShapeStyle(.primary)
    var alignment: HorizontalAlignment = .leading

    @State private var translatedText: String?
    @State private var translationConfig: TranslationSession.Configuration?
    @Environment(\.theme) private var theme
    @EnvironmentObject var settings: AppSettings

    private var displayText: String { translatedText ?? text }

    /// Inline romaji rendered as a `Text` concatenation so it joins the same
    /// line as the field value (the user's "after for smaller text fields"
    /// requirement). Empty when the toggle is off, the field is currently
    /// translated, or no transcription is available.
    private var romajiSuffix: Text {
        guard settings.showRomaji,
              translatedText == nil,
              let r = Romaji.transcribe(text)
        else { return Text("") }
        return Text(" (\(r))")
            .font(.caption2)
            .italic()
            .foregroundStyle(.secondary)
    }

    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 6) {
            (
                Text(displayText)
                    .font(font)
                    .foregroundStyle(foregroundStyle)
                + romajiSuffix
            )
                .textSelection(.enabled)
                .frame(maxWidth: .infinity,
                       alignment: alignment == .leading ? .leading : .trailing)

            if isJapanese(text) && settings.translateButtonEnabled {
                if translatedText == nil {
                    translateButton
                } else {
                    revertButton
                }
            }
        }
        .translationTask(translationConfig) { session in
            do {
                let response = try await session.translate(text)
                await MainActor.run { translatedText = response.targetText }
            } catch {
                // Silent: most likely a missing language pack on first run —
                // Apple's translationTask will surface its own download prompt.
            }
        }
        // Clear stale translation if the upstream text changes (e.g. switching
        // popups in the same view hierarchy).
        .onChange(of: text) { _, _ in
            translatedText = nil
        }
    }

    private func startTranslation() {
        // Apple's documented re-trigger: calling `invalidate()` on the existing
        // configuration is what actually re-fires `.translationTask`. Reassigning
        // a fresh Configuration with the same source/target is a no-op (value
        // equality), so on the second translate after a revert nothing happens
        // unless we go through `invalidate()`.
        if translationConfig != nil {
            translationConfig?.invalidate()
        } else {
            let targetRaw = settings.translateTargetLanguageRaw
            let target: Locale.Language? = targetRaw.isEmpty
                ? nil  // nil = device default per Apple's Translation framework
                : Locale.Language(identifier: targetRaw)
            translationConfig = TranslationSession.Configuration(
                source: Locale.Language(identifier: "ja"),
                target: target
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
