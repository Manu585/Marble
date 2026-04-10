#pragma once
#include <memory>
#include <string>

namespace Marble {

  // A loaded audio clip. Supports full playback control (play, stop, pause, resume)
  // and per-sound volume, pitch, and looping.
  //
  // A single Sound plays one instance at a time. To layer the same clip
  // (e.g. rapid-fire sound effects), load it into multiple Sound objects.
  //
  // Lifecycle requirement: Sound must be destroyed before Application is
  // destroyed. The MARBLE_MAIN macro guarantees this — the GameLayer (which
  // owns Sounds) is declared after Application and therefore destructs first.
  class Sound {
  public:
    explicit Sound(const std::string& path);
    ~Sound();

    Sound(const Sound&)            = delete;
    Sound& operator=(const Sound&) = delete;

    // Move-safe: pimpl stays on the heap, ma_sound is never relocated.
    Sound(Sound&&)            noexcept;
    Sound& operator=(Sound&&) noexcept;

    // ── Playback ─────────────────────────────────────────────────────
    // Seek to the beginning and start playing. Safe to call if already playing.
    void Play();
    // Stop playback and reset to the beginning.
    void Stop();
    // Pause without rewinding. Resume() continues from the same position.
    void Pause();
    void Resume();

    bool IsPlaying() const;

    // ── Per-sound settings ───────────────────────────────────────────
    // Volume multiplier in [0.0, 1.0], independent of master volume.
    void  SetVolume(float volume);
    float GetVolume() const;

    // When true, playback wraps back to the beginning on completion.
    void SetLoop(bool loop);
    bool GetLoop() const;

    // Pitch / playback speed multiplier. 1.0 = normal. 0.5 = half speed, 2.0 = double.
    void  SetPitch(float pitch);
    float GetPitch() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
  };

} // namespace Marble
