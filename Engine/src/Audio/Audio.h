#pragma once

namespace Marble {

  // Global audio engine control. Initialized by Application; usable from any
  // game callback between OnStart and OnStop.
  //
  // For individual sound playback, create Sound objects. This class exposes only
  // the settings that apply to the entire audio output.
  class Audio {
  public:
    // Master volume applied to all sounds. Range [0.0, 1.0]. Default: 1.0.
    static void  SetMasterVolume(float volume);
    static float GetMasterVolume();

  private:
    friend class Application; // calls Init / Shutdown

    static void Init();
    static void Shutdown();
  };

} // namespace Marble
