#include "Shader.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Marble {

  Shader::Shader(const std::string& vertPath, const std::string& fragPath) {
    const std::string vertSrc = ReadFile(vertPath);
    const std::string fragSrc = ReadFile(fragPath);

    const uint32_t vert = CompileShader(GL_VERTEX_SHADER,   vertSrc);
    const uint32_t frag = CompileShader(GL_FRAGMENT_SHADER, fragSrc);

    m_ID = glCreateProgram();
    glAttachShader(m_ID, vert);
    glAttachShader(m_ID, frag);
    glLinkProgram(m_ID);

    int success;
    glGetProgramiv(m_ID, GL_LINK_STATUS, &success);
    if (!success) {
      int logLen = 0;
      glGetProgramiv(m_ID, GL_INFO_LOG_LENGTH, &logLen);
      std::string log(logLen, '\0');
      glGetProgramInfoLog(m_ID, logLen, nullptr, log.data());
      glDeleteShader(vert);
      glDeleteShader(frag);
      throw std::runtime_error("Shader link error: " + log);
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
  }

  Shader::~Shader() {
    glDeleteProgram(m_ID);
  }

  void Shader::Bind()   const { glUseProgram(m_ID); }
  void Shader::Unbind() const { glUseProgram(0);    }

  int Shader::GetLocation(const char* name) const {
    auto it = m_LocationCache.find(name);
    if (it != m_LocationCache.end()) return it->second;
    const int loc = glGetUniformLocation(m_ID, name);
    m_LocationCache.emplace(name, loc);
    return loc;
  }

  void Shader::SetInt   (const char* name, int value)          const { glUniform1i (GetLocation(name), value); }
  void Shader::SetFloat (const char* name, float value)        const { glUniform1f (GetLocation(name), value); }
  void Shader::SetFloat2(const char* name, const glm::vec2& v) const { glUniform2f (GetLocation(name), v.x, v.y); }
  void Shader::SetFloat3(const char* name, const glm::vec3& v) const { glUniform3f (GetLocation(name), v.x, v.y, v.z); }
  void Shader::SetFloat4(const char* name, const glm::vec4& v) const { glUniform4f (GetLocation(name), v.x, v.y, v.z, v.w); }
  void Shader::SetMat4  (const char* name, const glm::mat4& m) const { glUniformMatrix4fv(GetLocation(name), 1, GL_FALSE, glm::value_ptr(m)); }

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
      src = src.substr(3);

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

} // namespace Marble
