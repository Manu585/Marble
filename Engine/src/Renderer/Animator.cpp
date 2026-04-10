#include "Animator.h"

namespace Marble {

  void Animator::AddClip(std::string name, Clip clip) {
    m_Clips[std::move(name)] = std::move(clip);
  }

  void Animator::Play(const std::string& name, bool restart) {
    auto it = m_Clips.find(name);
    if (it == m_Clips.end()) return; // unknown clip — no-op

    // Same clip, already playing, and the caller doesn't want a restart.
    if (!restart && m_Current == &it->second) return;

    m_Current     = &it->second;
    m_CurrentName = name;
    m_Timer       = 0.0f;
    m_FrameIndex  = 0;
    m_Finished    = false;
  }

  void Animator::Update(float dt) {
    if (!m_Current || m_Finished) return;
    if (m_Current->Frames.empty()) return;

    m_Timer += dt;

    const float frameDuration = m_Current->FrameDuration > 0.0f
        ? m_Current->FrameDuration
        : 0.016f; // guard against zero to avoid infinite loop

    while (m_Timer >= frameDuration) {
      m_Timer -= frameDuration;
      ++m_FrameIndex;

      const int frameCount = static_cast<int>(m_Current->Frames.size());
      if (m_FrameIndex >= frameCount) {
        if (m_Current->Loop) {
          m_FrameIndex = 0;
        } else {
          m_FrameIndex = frameCount - 1; // stay on last frame
          m_Finished   = true;
          return;
        }
      }
    }
  }

  const TextureRegion* Animator::GetCurrentFrame() const {
    if (!m_Current || m_Current->Frames.empty()) return nullptr;
    return &m_Current->Frames[static_cast<std::size_t>(m_FrameIndex)];
  }

} // namespace Marble
