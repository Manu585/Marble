#pragma once
#include "Shader.h"
#include "Framebuffer.h"
#include "PostProcessSettings.h"
#include <cstdint>
#include <memory>

namespace Marble {

  class PostProcessPass {
  public:
    PostProcessPass();
    ~PostProcessPass();

    PostProcessPass(const PostProcessPass&) = delete;
    PostProcessPass& operator=(const PostProcessPass&) = delete;

    // Renders the framebuffer texture into the currently active glViewport
    void Apply(const Framebuffer& framebuffer, const PostProcessSettings& settings, float time);

  private:
    uint32_t m_VAO = 0;
    uint32_t m_VBO = 0;

    std::unique_ptr<Shader> m_Shader;
  };

} // namespace Marble
