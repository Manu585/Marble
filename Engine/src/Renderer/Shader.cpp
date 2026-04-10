#include "Shader.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Marble {

  // ── Compile helpers ──────────────────────────────────────────────────────────

  std::string Shader::ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
      throw std::runtime_error("Failed to open shader file: " + path);

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string src = ss.str();

    // Strip UTF-8 BOM if present (common when saving GLSL on Windows)
    if (src.size() >= 3 &&
        static_cast<unsigned char>(src[0]) == 0xEF &&
        static_cast<unsigned char>(src[1]) == 0xBB &&
        static_cast<unsigned char>(src[2]) == 0xBF)
      src.erase(0, 3);

    return src;
  }

  uint32_t Shader::CompileShader(uint32_t type, const std::string& src) {
    const uint32_t id = glCreateShader(type);
    const char* raw = src.c_str();
    glShaderSource(id, 1, &raw, nullptr);
    glCompileShader(id);

    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
      int logLen = 0;
      glGetShaderiv(id, GL_INFO_LOG_LENGTH, &logLen);
      std::string log(logLen, '\0');
      glGetShaderInfoLog(id, logLen, nullptr, log.data());
      glDeleteShader(id);
      throw std::runtime_error("Shader compile error: " + log);
    }
    return id;
  }

  // Attaches, links, and then immediately detaches vert/frag from the program.
  // Deletes both shader objects on exit regardless of success.
  // Throws (and deletes m_ID) on link failure.
  void Shader::Link(uint32_t vert, uint32_t frag) {
    m_ID = glCreateProgram();
    glAttachShader(m_ID, vert);
    glAttachShader(m_ID, frag);
    glLinkProgram(m_ID);

    // Shader objects may be deleted immediately after linking regardless of
    // outcome — the program retains its own compiled copy.
    glDeleteShader(vert);
    glDeleteShader(frag);

    int success;
    glGetProgramiv(m_ID, GL_LINK_STATUS, &success);
    if (!success) {
      int logLen = 0;
      glGetProgramiv(m_ID, GL_INFO_LOG_LENGTH, &logLen);
      std::string log(logLen, '\0');
      glGetProgramInfoLog(m_ID, logLen, nullptr, log.data());
      // Destructor will not run if this is called from a constructor, so
      // we delete the program here before throwing.
      glDeleteProgram(m_ID);
      m_ID = 0;
      throw std::runtime_error("Shader link error: " + log);
    }
  }

  // ── Constructors ─────────────────────────────────────────────────────────────

  // From disk files. UTF-8 BOM is stripped.
  Shader::Shader(const std::string& vertPath, const std::string& fragPath) {
    const uint32_t vert = CompileShader(GL_VERTEX_SHADER, ReadFile(vertPath));
    uint32_t frag = 0;
    try {
      frag = CompileShader(GL_FRAGMENT_SHADER, ReadFile(fragPath));
    } catch (...) {
      glDeleteShader(vert);
      throw;
    }
    Link(vert, frag);
  }

  // From embedded GLSL source strings (for engine-internal shaders).
  Shader::Shader(SourceTag, const char* vertSrc, const char* fragSrc) {
    const uint32_t vert = CompileShader(GL_VERTEX_SHADER, vertSrc);
    uint32_t frag = 0;
    try {
      frag = CompileShader(GL_FRAGMENT_SHADER, fragSrc);
    } catch (...) {
      glDeleteShader(vert);
      throw;
    }
    Link(vert, frag);
  }

  Shader::~Shader() {
    glDeleteProgram(m_ID);
  }

  // ── Bind ─────────────────────────────────────────────────────────────────────

  void Shader::Bind()   const { glUseProgram(m_ID); }
  void Shader::Unbind() const { glUseProgram(0);    }

  // ── Uniform setters ───────────────────────────────────────────────────────────

  int Shader::GetLocation(const char* name) const {
    auto it = m_LocationCache.find(name);
    if (it != m_LocationCache.end()) return it->second;
    const int loc = glGetUniformLocation(m_ID, name);
    m_LocationCache.emplace(name, loc);
    return loc;
  }

  void Shader::SetInt     (const char* name, int value)           const { glUniform1i (GetLocation(name), value); }
  void Shader::SetIntArray(const char* name, const int* v, int n) const { glUniform1iv(GetLocation(name), n, v); }
  void Shader::SetFloat   (const char* name, float value)         const { glUniform1f (GetLocation(name), value); }
  void Shader::SetFloat2  (const char* name, const glm::vec2& v)  const { glUniform2f (GetLocation(name), v.x, v.y); }
  void Shader::SetFloat3  (const char* name, const glm::vec3& v)  const { glUniform3f (GetLocation(name), v.x, v.y, v.z); }
  void Shader::SetFloat4  (const char* name, const glm::vec4& v)  const { glUniform4f (GetLocation(name), v.x, v.y, v.z, v.w); }
  void Shader::SetMat4    (const char* name, const glm::mat4& m)  const { glUniformMatrix4fv(GetLocation(name), 1, GL_FALSE, glm::value_ptr(m)); }

} // namespace Marble
