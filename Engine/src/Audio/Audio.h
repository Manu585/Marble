#pragma once

namespace Marble {

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
