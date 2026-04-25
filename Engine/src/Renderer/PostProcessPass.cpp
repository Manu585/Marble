#include "PostProcessPass.h"
#include <glad/glad.h>
#include <algorithm>

namespace Marble {

  // ── Embedded post-process shaders ────────────────────────────────────────────
  // These are engine-level shaders: the post-process pipeline (vignette,
  // brightness, contrast, saturation) is generic infrastructure, not game logic.

  static constexpr const char* k_PostProcessVertSrc = R"GLSL(
#version 460 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main() {
    v_TexCoord  = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
)GLSL";

  static constexpr const char* k_PostProcessFragSrc = R"GLSL(
#version 460 core
in  vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Screen;
uniform float     u_Time;
uniform float     u_VignetteStrength;
uniform float     u_Brightness;
uniform float     u_Contrast;
uniform float     u_Saturation;

void main() {
    // v_TexCoord is [0,1] over the post-process quad, which already occupies
    // exactly the letterbox viewport, so no UV remapping is needed.
    vec4 color = texture(u_Screen, v_TexCoord);

    color.rgb *= u_Brightness;
    color.rgb  = (color.rgb - 0.5) * u_Contrast + 0.5;

    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    color.rgb  = mix(vec3(gray), color.rgb, u_Saturation);

    // Vignette: circular smooth falloff, darkest at corners.
    // dist is 0 at center, 1.0 at screen edge midpoints, ~1.41 at corners.
    // smoothstep gives a soft transition instead of a hard linear cutoff.
    float dist     = length(v_TexCoord - 0.5) * 2.0;
    float vignette = 1.0 - u_VignetteStrength * smoothstep(0.5, 1.3, dist);
    color.rgb     *= vignette;

    FragColor = color;
}
)GLSL";

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

    m_Shader = std::make_unique<Shader>(Shader::FromSource, k_PostProcessVertSrc, k_PostProcessFragSrc);

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

    // Clamp all settings to ranges that produce well-defined shader output.
    // Values outside these ranges cause visual artifacts (blown-out brightness,
    // inverted contrast, completely black vignette, etc.).
    const float vignetteStrength = std::clamp(settings.VignetteStrength, 0.0f, 1.0f);
    const float brightness       = std::clamp(settings.Brightness,       0.0f, 4.0f);
    const float contrast         = std::clamp(settings.Contrast,         0.0f, 4.0f);
    const float saturation       = std::clamp(settings.Saturation,       0.0f, 4.0f);

    m_Shader->Bind();
    m_Shader->SetFloat("u_Time",             time);
    m_Shader->SetFloat("u_VignetteStrength", vignetteStrength);
    m_Shader->SetFloat("u_Brightness",       brightness);
    m_Shader->SetFloat("u_Contrast",         contrast);
    m_Shader->SetFloat("u_Saturation",       saturation);

    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

} // namespace Marble
