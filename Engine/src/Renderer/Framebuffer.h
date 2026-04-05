#pragma once
#include <cstdint>
#include "Texture.h"

namespace Marble {

  class Framebuffer {
  public:
    Framebuffer(int width, int height, TextureFilter filter = TextureFilter::Nearest);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void Bind()   const; // render into this framebuffer
    void Unbind() const; // restore default framebuffer

    void Resize(int width, int height);

    uint32_t GetColorAttachment() const { return m_ColorAttachment; }
    int      GetWidth()           const { return m_Width;           }
    int      GetHeight()          const { return m_Height;          }

  private:
    void Rebuild();

    uint32_t      m_FBO             = 0;
    uint32_t      m_ColorAttachment = 0;
    uint32_t      m_RBO             = 0;
    int           m_Width           = 0;
    int           m_Height          = 0;
    TextureFilter m_Filter          = TextureFilter::Nearest;
  };

} // namespace Marble
