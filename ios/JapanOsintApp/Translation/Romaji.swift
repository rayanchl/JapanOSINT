import Foundation

/// Best-effort Japanese → romaji transcription using the same tokenizer
/// that powers iOS's built-in furigana / IME readings. `kCFStringTokenizerAttributeLatinTranscription`
/// returns Hepburn-flavored output for kana and reads kanji via the system's
/// Japanese morphological tables, so `東京駅` → `tōkyō eki` and `しんじゅく`
/// → `shinjuku` without us shipping a dictionary.
enum Romaji {
    /// Returns a romanized string, or `nil` if the input is empty, contains
    /// no Japanese, or transcribes to itself (the OS gave up and echoed).
    static func transcribe(_ text: String) -> String? {
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty, isJapanese(trimmed) else { return nil }

        let cf = trimmed as CFString
        let range = CFRangeMake(0, CFStringGetLength(cf))
        let locale = Locale(identifier: "ja") as CFLocale
        let tokenizer = CFStringTokenizerCreate(
            kCFAllocatorDefault,
            cf,
            range,
            kCFStringTokenizerUnitWord,
            locale
        )

        var pieces: [String] = []
        while CFStringTokenizerAdvanceToNextToken(tokenizer) != [] {
            guard let attr = CFStringTokenizerCopyCurrentTokenAttribute(
                tokenizer,
                kCFStringTokenizerAttributeLatinTranscription
            ) as? String else { continue }
            let piece = attr.trimmingCharacters(in: .whitespaces)
            if !piece.isEmpty { pieces.append(piece) }
        }

        let joined = pieces.joined(separator: " ")
        guard !joined.isEmpty, joined != trimmed else { return nil }
        return joined
    }
}
