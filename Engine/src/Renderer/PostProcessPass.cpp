#include "PostProcessPass.h"
#include <glad/glad.h>

namespace Marble {

  // Full-screen triangle-pair quad in NDC, with UV coordinates.
  // Covers [-1, 1] x [-1, 1] exactly — maps to whatever glViewport is active.
  static constexpr float s_ScreenQuad[] = {
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,

    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
  };

  PostProcessPass::PostProcessPass() {
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers     (1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_ScreenQuad), s_ScreenQuad, GL_STATIC_DRAW);

    // position (xy)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // texcoord (uv)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    m_Shader = std::make_unique<Shader>(
      "assets/shaders/postprocess.vert",
      "assets/shaders/postprocess.frag"
    );

    // u_Screen always samples texture unit 0 — set once, never changes.
    // Sampler uniforms are per-program state; they persist until the program
    // is deleted or explicitly reassigned.
    m_Shader->Bind();
    m_Shader->SetInt("u_Screen", 0);
  }

  PostProcessPass::~PostProcessPass() {
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers     (1, &m_VBO);
  }

  void PostProcessPass::Apply(const Framebuffer& framebuffer, const PostProcessSettings& settings, float time) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, framebuffer.GetColorAttachment());

    m_Shader->Bind();
    m_Shader->SetFloat("u_Time",             time);
    m_Shader->SetFloat("u_VignetteStrength", settings.VignetteStrength);
    m_Shader->SetFloat("u_Brightness",       settings.Brightness);
    m_Shader->SetFloat("u_Contrast",         settings.Contrast);
    m_Shader->SetFloat("u_Saturation",       settings.Saturation);

    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

} // namespace Marble
