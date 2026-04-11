#include <miniaudio.h>

#include "Sound.h"
#include "AudioInternal.h"
#include <algorithm>
#include <stdexcept>

namespace Marble {

  // ── Pimpl ────────────────────────────────────────────────────────────────────

  struct Sound::Impl {
    ma_sound Sound{};
    bool     Initialized = false;

    // Cached settings — used to restore state after move or for querying.
    float Volume = 1.0f;
    float Pitch  = 1.0f;
    bool  Loop   = false;
  };

  // ── Constructor / Destructor ──────────────────────────────────────────────────

  Sound::Sound(const std::string& path) : m_Impl(std::make_unique<Impl>()) {
    ma_engine* engine = GetAudioEngine();
    if (!engine) {
      throw std::runtime_error(
          "Sound: audio engine is not running. "
          "Ensure Application is constructed before creating Sound objects.");
    }

    // MA_SOUND_FLAG_DECODE: decode the entire clip into memory at load time.
    // This trades a bit of memory for minimal latency on Play() — correct for
    // short sound effects. For music, consider removing this flag (streaming).
    const ma_result result = ma_sound_init_from_file(
        engine,
        path.c_str(),
        MA_SOUND_FLAG_DECODE,
        nullptr, // sound group (nullptr = engine default group)
        nullptr, // async fence (nullptr = synchronous load)
        &m_Impl->Sound
    );
    if (result != MA_SUCCESS)
      throw std::runtime_error("Sound: failed to load '" + path + "'");

    m_Impl->Initialized = true;
  }

  Sound::~Sound() {
    if (m_Impl && m_Impl->Initialized)
      ma_sound_uninit(&m_Impl->Sound);
  }

  // Move: transfers ownership of the Impl heap allocation.
  // The ma_sound inside Impl stays at its original address — safe.
  Sound::Sound(Sound&& other) noexcept
    : m_Impl(std::move(other.m_Impl))
  {}

  Sound& Sound::operator=(Sound&& other) noexcept {
    if (this != &other) {
      if (m_Impl && m_Impl->Initialized)
        ma_sound_uninit(&m_Impl->Sound);
      m_Impl = std::move(other.m_Impl);
    }
    return *this;
  }

  // ── Playback ──────────────────────────────────────────────────────────────────

  void Sound::Play() {
    if (!m_Impl->Initialized) return;
    // Always seek to the beginning so Play() behaves like "restart".
    ma_sound_seek_to_pcm_frame(&m_Impl->Sound, 0);
    ma_sound_start(&m_Impl->Sound);
  }

  void Sound::Stop() {
    if (!m_Impl->Initialized) return;
    ma_sound_stop(&m_Impl->Sound);
    ma_sound_seek_to_pcm_frame(&m_Impl->Sound, 0);
  }

  void Sound::Pause() {
    // miniaudio's stop preserves the read cursor, making it equivalent to pause.
    if (!m_Impl->Initialized) return;
    ma_sound_stop(&m_Impl->Sound);
  }

  void Sound::Resume() {
    if (!m_Impl->Initialized) return;
    ma_sound_start(&m_Impl->Sound);
  }

  bool Sound::IsPlaying() const {
    return m_Impl->Initialized && ma_sound_is_playing(&m_Impl->Sound) == MA_TRUE;
  }

  // ── Settings ──────────────────────────────────────────────────────────────────

  void Sound::SetVolume(float volume) {
    m_Impl->Volume = std::clamp(volume, 0.0f, 1.0f);
    if (m_Impl->Initialized)
      ma_sound_set_volume(&m_Impl->Sound, m_Impl->Volume);
  }

  float Sound::GetVolume() const { return m_Impl->Volume; }

  void Sound::SetLoop(bool loop) {
    m_Impl->Loop = loop;
    if (m_Impl->Initialized)
      ma_sound_set_looping(&m_Impl->Sound, loop ? MA_TRUE : MA_FALSE);
  }

  bool Sound::GetLoop() const { return m_Impl->Loop; }

  void Sound::SetPitch(float pitch) {
    m_Impl->Pitch = pitch;
    if (m_Impl->Initialized)
      ma_sound_set_pitch(&m_Impl->Sound, pitch);
  }

  float Sound::GetPitch() const { return m_Impl->Pitch; }

} // namespace Marble
