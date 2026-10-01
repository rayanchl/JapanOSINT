import SwiftUI
import AVKit
#if canImport(AppKit) && !canImport(UIKit)
import AppKit
#endif

/// AVPlayer-backed live stream view. Used by `CameraFeedView` for `.hls` mode
/// (m3u8 streams). Plays muted on appear; user can unmute via the inline
/// playback controls.
///
/// iOS uses `AVPlayerViewController`; macOS uses AppKit's `AVPlayerView`, the
/// equivalent player surface (there is no `AVPlayerViewController` on macOS).
struct CameraVideoPlayer {
    let url: URL

    fileprivate func makePlayer() -> AVPlayer {
        let player = AVPlayer(url: url)
        player.isMuted = true
        player.automaticallyWaitsToMinimizeStalling = true
        player.play()
        return player
    }

    /// Fully stop a player we are about to drop.
    ///
    /// `pause()` alone is not enough: the `AVPlayerItem` keeps its asset loader
    /// and buffering pipeline alive, so a grid that scrolls through a few dozen
    /// HLS cards accumulates live decoders and network readers. Clearing the
    /// current item is what actually releases them.
    fileprivate static func teardown(_ player: AVPlayer?) {
        guard let player else { return }
        player.pause()
        player.replaceCurrentItem(with: nil)
    }
}

#if canImport(UIKit)
extension CameraVideoPlayer: UIViewControllerRepresentable {
    func makeUIViewController(context: Context) -> AVPlayerViewController {
        let vc = AVPlayerViewController()
        vc.showsPlaybackControls = true
        vc.entersFullScreenWhenPlaybackBegins = false
        vc.allowsPictureInPicturePlayback = false
        vc.videoGravity = .resizeAspect
        vc.player = makePlayer()
        return vc
    }

    func updateUIViewController(_ vc: AVPlayerViewController, context: Context) {
        let current = (vc.player?.currentItem?.asset as? AVURLAsset)?.url
        guard current != url else { return }
        // Stop the outgoing player BEFORE swapping — assigning over it left the
        // old one playing and buffering with nothing on screen.
        CameraVideoPlayer.teardown(vc.player)
        vc.player = makePlayer()
    }

    /// SwiftUI calls this when the representable leaves the hierarchy. Without
    /// it the AVPlayer outlived the view and kept pulling the stream.
    static func dismantleUIViewController(_ vc: AVPlayerViewController, coordinator: ()) {
        CameraVideoPlayer.teardown(vc.player)
        vc.player = nil
    }
}
#elseif canImport(AppKit)
extension CameraVideoPlayer: NSViewRepresentable {
    func makeNSView(context: Context) -> AVPlayerView {
        let view = AVPlayerView()
        view.controlsStyle = .inline
        view.allowsPictureInPicturePlayback = false
        view.videoGravity = .resizeAspect
        view.player = makePlayer()
        return view
    }

    func updateNSView(_ view: AVPlayerView, context: Context) {
        let current = (view.player?.currentItem?.asset as? AVURLAsset)?.url
        guard current != url else { return }
        CameraVideoPlayer.teardown(view.player)
        view.player = makePlayer()
    }

    static func dismantleNSView(_ view: AVPlayerView, coordinator: ()) {
        CameraVideoPlayer.teardown(view.player)
        view.player = nil
    }
}
#endif
