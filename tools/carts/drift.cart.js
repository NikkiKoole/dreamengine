// DRIFT — primitives only (no sprite sheet / tile map). A timing-diagnostic
// cart: three drum voices on three different clocks, drawn as scrolling error
// scopes. Default 320×200 is plenty; settings here just pin the bake config.
module.exports = {
  screenW: 320,
  screenH: 200,
  scale: 4,
  // opt into the AudioWorklet backend (audio-threading.md): build-site ships both a
  // worklet build and the ScriptProcessor fallback + a loader that auto-picks. As a
  // timing-diagnostic this is exactly the cart that wants the tight clock.
  worklet: true,
};
