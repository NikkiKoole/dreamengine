// Lean fix for the -sWASM_WORKERS build (the AudioWorklet backend, DE_AUDIO_WORKLET):
// emscripten's wasm-workers libc pulls clock_nanosleep, which references
// emscripten_thread_sleep — undefined under bare WASM_WORKERS. raylib on web is
// rAF-driven and never sleeps, so a no-op is safe (verified on-device: CPU stays low).
// See docs/design/audio-threading.md Stage 0. Used via emcc --js-library.
addToLibrary({ emscripten_thread_sleep: function (ms) {} });
