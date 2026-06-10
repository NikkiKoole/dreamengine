// Throwaway AudioWorklet feasibility spike (see docs/design/audio-timing.md).
// A 440Hz sine generated on the dedicated audio-worklet thread. If you hear a
// clean tone after tapping START, the worklet path works in this context.
// Built with: emcc spike.c -sAUDIO_WORKLET=1 -sWASM_WORKERS=1 ...
#include <emscripten/webaudio.h>
#include <emscripten/emscripten.h>
#include <math.h>
#include <stdbool.h>

static uint8_t audioStack[8192];
static EMSCRIPTEN_WEBAUDIO_T g_ctx = 0;

// A sample-counted CLICK ROLL — the "straight to the bone" test. Spacing is a fixed
// number of samples, counted on the AUDIO THREAD, so it is even BY CONSTRUCTION: if the
// worklet's clock is solid you hear a dead-steady roll; any warble or gap = the audio
// thread missing a render quantum (a dropout). ~20Hz @44.1k — a brisk, clearly-discrete
// roll where unevenness is obvious.
#define CLICK_INTERVAL 2205   // samples between clicks
#define CLICK_LEN       800   // samples a click rings (~18ms)
static int g_count    = 0;    // counts down to the next click
static int g_clickPos = -1;   // -1 = silent, else samples into the current click

static bool ProcessAudio(int numInputs, const AudioSampleFrame *inputs,
                         int numOutputs, AudioSampleFrame *outputs,
                         int numParams, const AudioParamFrame *params, void *userData) {
    for (int i = 0; i < 128; ++i) {
        if (g_count <= 0) { g_clickPos = 0; g_count = CLICK_INTERVAL; }
        g_count--;
        float s = 0.f;
        if (g_clickPos >= 0) {
            float env = expf(-(float)g_clickPos / 120.0f);     // ~3ms exp decay
            s = sinf((float)g_clickPos * 0.21f) * 0.35f * env; // ~1.5kHz tick
            if (++g_clickPos >= CLICK_LEN) g_clickPos = -1;
        }
        for (int ch = 0; ch < outputs[0].numberOfChannels; ++ch)
            outputs[0].data[ch * 128 + i] = s;
    }
    return true;   // keep the processor alive
}

static void ProcessorCreated(EMSCRIPTEN_WEBAUDIO_T ctx, bool success, void *userData) {
    if (!success) { EM_ASM({ window.__spikeStatus && window.__spikeStatus('processor FAILED'); }); return; }
    int counts[1] = { 1 };
    EmscriptenAudioWorkletNodeCreateOptions o = {
        .numberOfInputs = 0, .numberOfOutputs = 1, .outputChannelCounts = counts
    };
    EMSCRIPTEN_AUDIO_WORKLET_NODE_T node =
        emscripten_create_wasm_audio_worklet_node(ctx, "sine", &o, &ProcessAudio, 0);
    emscripten_audio_node_connect(node, ctx, 0, 0);
    EM_ASM({ window.__spikeStatus && window.__spikeStatus('worklet ready — tap START'); });
}

static void ThreadInit(EMSCRIPTEN_WEBAUDIO_T ctx, bool success, void *userData) {
    if (!success) { EM_ASM({ window.__spikeStatus && window.__spikeStatus('audio thread FAILED'); }); return; }
    WebAudioWorkletProcessorCreateOptions opts = { .name = "sine" };
    emscripten_create_wasm_audio_worklet_processor_async(ctx, &opts, ProcessorCreated, 0);
}

// called from a user-gesture (tap) in JS → resumes the suspended context
EMSCRIPTEN_KEEPALIVE void resume_audio(void) {
    if (g_ctx) emscripten_resume_audio_context_sync(g_ctx);
}

int main(void) {
    g_ctx = emscripten_create_audio_context(0);
    emscripten_start_wasm_audio_worklet_thread_async(
        g_ctx, audioStack, sizeof(audioStack), ThreadInit, 0);
    EM_ASM({ window.__spikeStatus && window.__spikeStatus('wasm loaded (shared memory OK) — starting audio thread'); });
    return 0;
}
