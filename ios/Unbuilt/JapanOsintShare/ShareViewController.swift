import UIKit
import UniformTypeIdentifiers
import Social

// ─────────────────────────────────────────────────────────────────────────────
// Share extension (roadmap 36).
//
// NOT part of the main app target — see the note in
// JapanOsintWidgetsBundle.swift and docs/ios-extensions-plan.md.
// Give ios/JapanOsintApp/Widgets/SharedSnapshot.swift membership in this
// target too; ShareQueue lives there.
//
// This does NO networking. It writes the shared payload into the App Group
// queue and hands off to the app. An extension gets roughly 15 seconds and
// cannot reach the Keychain-backed bearer token, so doing the OSINT work here
// would mean a second auth path, a second error surface, and a decent chance
// of being killed mid-request. The app drains the queue on next launch.
// ─────────────────────────────────────────────────────────────────────────────

final class ShareViewController: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        handleInput()
    }

    private func handleInput() {
        guard let item = extensionContext?.inputItems.first as? NSExtensionItem,
              let providers = item.attachments, !providers.isEmpty else {
            return finish(error: "Nothing to share")
        }
        // Only the first attachment is taken: the activation rule admits
        // exactly one item, and silently swallowing extras would be worse
        // than not accepting them.
        let provider = providers[0]

        if provider.hasItemConformingToTypeIdentifier(UTType.url.identifier) {
            load(provider, UTType.url) { [weak self] obj in
                guard let url = obj as? URL else {
                    return self?.finish(error: "Could not read that link") ?? ()
                }
                self?.enqueue(kind: .url, value: url.absoluteString)
            }
        } else if provider.hasItemConformingToTypeIdentifier(UTType.image.identifier) {
            load(provider, UTType.image) { [weak self] obj in
                self?.enqueueImage(obj)
            }
        } else if provider.hasItemConformingToTypeIdentifier(UTType.plainText.identifier) {
            load(provider, UTType.plainText) { [weak self] obj in
                guard let s = obj as? String, !s.isEmpty else {
                    return self?.finish(error: "Empty text") ?? ()
                }
                self?.enqueue(kind: .text, value: s)
            }
        } else {
            finish(error: "Unsupported content")
        }
    }

    private func load(_ p: NSItemProvider, _ type: UTType,
                      _ done: @escaping (Any?) -> Void) {
        p.loadItem(forTypeIdentifier: type.identifier, options: nil) { obj, _ in
            DispatchQueue.main.async { done(obj) }
        }
    }

    private func enqueueImage(_ obj: Any?) {
        var data: Data?
        if let url = obj as? URL { data = try? Data(contentsOf: url) }
        else if let img = obj as? UIImage { data = img.jpegData(compressionQuality: 0.9) }
        else if let d = obj as? Data { data = d }
        guard let data else { return finish(error: "Could not read that image") }
        // Cap what we copy into the shared container. A multi-hundred-MB
        // burst-mode image would blow the extension's memory budget and be
        // killed mid-write.
        guard data.count <= 25 * 1024 * 1024 else {
            return finish(error: "Image is too large (max 25 MB)")
        }
        let id = UUID().uuidString
        enqueue(kind: .image, value: "\(id).jpg", id: id, imageData: data)
    }

    private func enqueue(kind: SharedInboundItem.Kind, value: String,
                         id: String = UUID().uuidString, imageData: Data? = nil) {
        let item = SharedInboundItem(id: id, kind: kind, value: value,
                                     receivedAt: Date())
        do {
            try ShareQueue.enqueue(item, imageData: imageData)
            openHostApp(id: id)
        } catch {
            finish(error: "Could not save — is the App Group configured?")
        }
    }

    /// Hand off to the app. `openURL` from an extension is only available via
    /// the responder chain; if it fails we still completed the queue write, so
    /// the item is picked up next time the app launches rather than lost.
    private func openHostApp(id: String) {
        guard let url = URL(string: "japanosint://share?id=\(id)") else {
            return finish(error: nil)
        }
        var responder: UIResponder? = self
        while let r = responder {
            if let app = r as? UIApplication {
                app.open(url, options: [:], completionHandler: nil)
                break
            }
            responder = r.next
        }
        finish(error: nil)
    }

    private func finish(error: String?) {
        guard let error else {
            extensionContext?.completeRequest(returningItems: nil)
            return
        }
        let a = UIAlertController(title: "JapanOSINT", message: error,
                                  preferredStyle: .alert)
        a.addAction(UIAlertAction(title: "OK", style: .default) { _ in
            self.extensionContext?.completeRequest(returningItems: nil)
        })
        present(a, animated: true)
    }
}
