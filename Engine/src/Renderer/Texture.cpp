#include "Texture.h"
#include <glad/glad.h>
#include <stb_image.h>
#include <stdexcept>

namespace Marble {

  Texture::Texture(const std::string& path, TextureSpec spec) {
    int channels;
    stbi_uc* data = stbi_load(path.c_str(), &m_Width, &m_Height, &channels, 0);
    if (!data) {
      throw std::runtime_error("Failed to load texture: " + path);
    }

    const GLenum internalFmt = (channels == 4) ? GL_RGBA8 : GL_RGB8;
    const GLenum dataFmt     = (channels == 4) ? GL_RGBA  : GL_RGB;
    const GLint  glFilter    = (spec.Filter == TextureFilter::Nearest) ? GL_NEAREST : GL_LINEAR;

    glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_2D, m_ID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_Width, m_Height, 0, dataFmt, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
  }

  Texture::~Texture() {
    glDeleteTextures(1, &m_ID);
  }

} // namespace Marble
