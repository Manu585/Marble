#pragma once
#include <cstdint>
#include <string>

namespace Marble {

  enum class TextureFilter {
    Nearest, // crisp pixel edges — correct default for pixel-art sprites
    Linear,  // smooth interpolation — for high-resolution or UI textures
  };

  struct TextureSpec {
    TextureFilter Filter = TextureFilter::Nearest;
  };

  class Texture {
  public:
    explicit Texture(const std::string& path, TextureSpec spec = {});
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    int      GetWidth()  const { return m_Width;  }
    int      GetHeight() const { return m_Height; }
    uint32_t GetID()     const { return m_ID;     }

  private:
    uint32_t m_ID     = 0;
    int      m_Width  = 0;
    int      m_Height = 0;
  };

} // namespace Marble
