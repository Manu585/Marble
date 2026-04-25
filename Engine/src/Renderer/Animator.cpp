#include "Animator.h"

namespace Marble {

  void Animator::AddClip(std::string name, Clip clip) {
    m_Clips[std::move(name)] = std::move(clip);
  }

  const Animator::Clip* Animator::CurrentClip() const {
    if (m_CurrentName.empty()) return nullptr;
    auto it = m_Clips.find(m_CurrentName);
    return it != m_Clips.end() ? &it->second : nullptr;
  }

  void Animator::Play(const std::string& name, bool restart) {
    if (m_Clips.find(name) == m_Clips.end()) return; // unknown clip — no-op

    // Same clip, already playing, and the caller doesn't want a restart.
    if (!restart && m_CurrentName == name) return;

    m_CurrentName = name;
    m_Timer       = 0.0f;
    m_FrameIndex  = 0;
    m_Finished    = false;
  }

  void Animator::Update(float dt) {
    const Clip* current = CurrentClip();
    if (!current || m_Finished) return;
    if (current->Frames.empty()) return;

    m_Timer += dt;

    static constexpr float k_MinFrameDuration = 1.0f / 60.0f;
    const float frameDuration = current->FrameDuration > 0.0f
        ? current->FrameDuration
        : k_MinFrameDuration; // guard against zero to avoid infinite loop

    while (m_Timer >= frameDuration) {
      m_Timer -= frameDuration;
      ++m_FrameIndex;

      const int frameCount = static_cast<int>(current->Frames.size());
      if (m_FrameIndex >= frameCount) {
        if (current->Loop) {
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
    const Clip* current = CurrentClip();
    if (!current || current->Frames.empty()) return nullptr;
    return &current->Frames[static_cast<std::size_t>(m_FrameIndex)];
  }

} // namespace Marble
