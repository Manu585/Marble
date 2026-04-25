#include "Framebuffer.h"
#include <glad/glad.h>
#include <cstdio>
#include <stdexcept>

namespace Marble {

  Framebuffer::Framebuffer(int width, int height, TextureFilter filter)
    : m_Width(width), m_Height(height), m_Filter(filter)
  {
    Rebuild();
  }

  Framebuffer::~Framebuffer() {
    glDeleteFramebuffers(1, &m_FBO);
    glDeleteTextures(1, &m_ColorAttachment);
    glDeleteRenderbuffers(1, &m_RBO);
  }

  void Framebuffer::Rebuild() {
    if (m_FBO) {
      glDeleteFramebuffers(1, &m_FBO);
      glDeleteTextures(1, &m_ColorAttachment);
      glDeleteRenderbuffers(1, &m_RBO);
    }

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    glGenTextures(1, &m_ColorAttachment);
    glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    const GLint glFilter = (m_Filter == TextureFilter::Nearest) ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

    glGenRenderbuffers(1, &m_RBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_Width, m_Height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_RBO);

    const GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fbStatus != GL_FRAMEBUFFER_COMPLETE) {
      char msg[64];
      std::snprintf(msg, sizeof(msg), "Framebuffer is incomplete (status 0x%X)", fbStatus);
      throw std::runtime_error(msg);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  void Framebuffer::Bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Width, m_Height);
  }

  void Framebuffer::Unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  void Framebuffer::Resize(int width, int height) {
    if (width == 0 || height == 0) return;
    m_Width  = width;
    m_Height = height;
    Rebuild();
  }

} // namespace Marble
