#include "VertexArray.h"
#include <glad/glad.h>

namespace Marble {

  VertexArray::VertexArray(const float* vertices, uint32_t size, uint32_t stride, std::initializer_list<BufferElement> elements) {
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);

    uint32_t index = 0;
    for (const auto& el : elements) {
      glVertexAttribPointer(index, el.Count, GL_FLOAT, GL_FALSE, stride, (void*)(uintptr_t)el.Offset);
      glEnableVertexAttribArray(index);
      index++;
    }

    glBindVertexArray(0);
  }

  VertexArray::~VertexArray() {
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
  }

  void VertexArray::Bind()   const { glBindVertexArray(m_VAO); }
  void VertexArray::Unbind() const { glBindVertexArray(0);     }

} // namespace Marble