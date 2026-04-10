#pragma once
#include "TextureRegion.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Marble {

  // Frame-by-frame sprite animator built on TextureRegion.
  //
  // Basic usage:
  //   SpriteSheet sheet("player.png", 32, 32);
  //
  //   Animator anim;
  //   anim.AddClip("idle",  { { sheet.GetFrame(0) }, 0.5f, true });
  //   anim.AddClip("run",   { { sheet.GetFrame(1), sheet.GetFrame(2),
  //                             sheet.GetFrame(3), sheet.GetFrame(4) }, 0.08f, true });
  //   anim.AddClip("jump",  { { sheet.GetFrame(5), sheet.GetFrame(6) }, 0.1f, false });
  //   anim.Play("idle");
  //
  //   // In OnUpdate:
  //   anim.Update(dt);
  //
  //   // In OnRender:
  //   if (auto* frame = anim.GetCurrentFrame())
  //       r.DrawQuad(pos, size, *frame);
  class Animator {
  public:
    struct Clip {
      std::vector<TextureRegion> Frames;
      float FrameDuration = 0.1f; // seconds per frame
      bool  Loop          = true;
    };

    // Add or replace a named clip.
    void AddClip(std::string name, Clip clip);

    // Start playing a clip. If the same clip is already playing and
    // restart = false, the call is a no-op (preserves the current frame).
    void Play(const std::string& name, bool restart = false);

    // Advance the animation by dt seconds. Call once per OnUpdate.
    void Update(float dt);

    // Returns a pointer to the current frame's TextureRegion,
    // or nullptr if no clip is active or the active clip has no frames.
    const TextureRegion* GetCurrentFrame() const;

    bool               IsPlaying()          const { return m_Current != nullptr; }

    // True when a non-looping clip has played its last frame and stopped.
    // Stays true until Play() is called again.
    bool               IsFinished()         const { return m_Finished; }

    const std::string& GetCurrentClipName() const { return m_CurrentName; }

  private:
    std::unordered_map<std::string, Clip> m_Clips;
    const Clip* m_Current     = nullptr;
    std::string m_CurrentName;
    float       m_Timer       = 0.0f;
    int         m_FrameIndex  = 0;
    bool        m_Finished    = false;
  };

} // namespace Marble
