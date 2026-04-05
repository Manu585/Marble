#pragma once
#include <cstdint>
#include <initializer_list>

namespace Marble {

  struct BufferElement {
    uint32_t Count;  // 3 for vec3, 2 for vec2, etc.
    uint32_t Offset; // byte offset into the vertex
  };

  class VertexArray {
  public:
    // stride = total bytes per vertex
    // elements = list of attributes
    VertexArray(const float* vertices, uint32_t size, uint32_t stride, std::initializer_list<BufferElement> elements);
    ~VertexArray();

    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;

    void Bind()   const;
    void Unbind() const;

  private:
    uint32_t m_VAO = 0;
    uint32_t m_VBO = 0;
  };

} // namespace Marble