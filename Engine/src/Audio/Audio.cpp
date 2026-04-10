// MINIAUDIO_IMPLEMENTATION is defined in vendor/miniaudio/miniaudio.cpp.
// Here we only include the declarations.
#include <miniaudio.h>

#include "Audio.h"
#include "AudioInternal.h"
#include <algorithm>
#include <stdexcept>

namespace Marble {

  // ── Global engine state ───────────────────────────────────────────────────────

  static ma_engine s_Engine;
  static bool      s_Initialized   = false;
  static float     s_MasterVolume  = 1.0f;

  ma_engine* GetAudioEngine() {
    return s_Initialized ? &s_Engine : nullptr;
  }

  // ── Lifecycle ─────────────────────────────────────────────────────────────────

  void Audio::Init() {
    if (s_Initialized) return;

    const ma_result result = ma_engine_init(nullptr, &s_Engine);
    if (result != MA_SUCCESS)
      throw std::runtime_error("Audio::Init — failed to initialize audio engine (miniaudio)");

    s_Initialized = true;
  }

  void Audio::Shutdown() {
    if (!s_Initialized) return;
    ma_engine_uninit(&s_Engine);
    s_Initialized = false;
  }

  // ── Master volume ─────────────────────────────────────────────────────────────

  void Audio::SetMasterVolume(float volume) {
    s_MasterVolume = std::clamp(volume, 0.0f, 1.0f);
    if (s_Initialized)
      ma_engine_set_volume(&s_Engine, s_MasterVolume);
  }

  float Audio::GetMasterVolume() {
    return s_MasterVolume;
  }

} // namespace Marble
