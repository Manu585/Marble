#pragma once
// Engine-internal header — do NOT include from Marble.h or game code.
// Provides access to the global ma_engine instance for use by Sound.cpp.

struct ma_engine; // forward declaration — keeps miniaudio.h out of here

namespace Marble {
  // Returns the active audio engine, or nullptr if Audio::Init() has not been
  // called yet (i.e., Application does not exist).
  ma_engine* GetAudioEngine();
} // namespace Marble
