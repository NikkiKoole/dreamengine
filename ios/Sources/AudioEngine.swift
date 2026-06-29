import AVFoundation

// CoreAudio render callback wiring: an AVAudioSourceNode pulls mono samples from the C
// synth (de_audio_render) once per audio quantum and feeds the engine's main mixer.
// This is where sound.h's mixer eventually plugs in, unchanged below the C boundary.
final class AudioEngine {
    static let shared = AudioEngine()
    private let engine = AVAudioEngine()
    private var src: AVAudioSourceNode?
    private var started = false

    func start() {
        guard !started else { return }
        let sr = 44100.0
        de_audio_init(Int32(sr))

        let fmt = AVAudioFormat(standardFormatWithSampleRate: sr, channels: 1)!
        let node = AVAudioSourceNode { _, _, frameCount, ablPointer -> OSStatus in
            let abl = UnsafeMutableAudioBufferListPointer(ablPointer)
            let n = Int(frameCount)
            guard let first = abl.first,
                  let ptr = first.mData?.assumingMemoryBound(to: Float.self) else { return noErr }
            de_audio_render(ptr, Int32(n))
            for i in 1..<abl.count {                       // dupe to any extra channels
                if let dst = abl[i].mData?.assumingMemoryBound(to: Float.self) {
                    memcpy(dst, ptr, n * MemoryLayout<Float>.size)
                }
            }
            return noErr
        }
        src = node
        engine.attach(node)
        engine.connect(node, to: engine.mainMixerNode, format: fmt)

        do {
            try AVAudioSession.sharedInstance().setCategory(.playback, mode: .default)
            try AVAudioSession.sharedInstance().setActive(true)
            try engine.start()
            started = true
            NSLog("[tinyjam] audio engine started @ %.0f Hz", sr)
        } catch {
            NSLog("[tinyjam] audio start failed: %@", error.localizedDescription)
        }
    }
}
