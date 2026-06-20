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
        if current != url {
            vc.player = makePlayer()
        }
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
        if current != url {
            view.player = makePlayer()
        }
    }
}
#endif
