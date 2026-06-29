import SwiftUI

// spike 1 — software-canvas framebuffer drawn by C, hosted on iOS via CanvasView.
@main
struct TinyjamHelloApp: App {
    var body: some Scene {
        WindowGroup { ContentView() }
    }
}

struct ContentView: View {
    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            CanvasViewRep().ignoresSafeArea()
        }
        .onAppear {
            AudioEngine.shared.start()   // spike 2 — start the synth
            Store_Init()                 // spike 4 — load products + entitlements
        }
    }
}
