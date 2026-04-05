#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Marble {

  class Shader {
  public:
    Shader(const std::string& vertPath, const std::string& fragPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void Bind()   const;
    void Unbind() const;

    void SetInt   (const char* name, int value)          const;
    void SetFloat (const char* name, float value)        const;
    void SetFloat2(const char* name, const glm::vec2& v) const;
    void SetFloat3(const char* name, const glm::vec3& v) const;
    void SetFloat4(const char* name, const glm::vec4& v) const;
    void SetMat4  (const char* name, const glm::mat4& m) const;

    uint32_t GetID() const { return m_ID; }

  private:
    // Returns the cached uniform location, querying the driver only on first access.
    int GetLocation(const char* name) const;

    uint32_t m_ID = 0;
    mutable std::unordered_map<std::string, int> m_LocationCache;

    static std::string ReadFile(const std::string& path);
    static uint32_t    CompileShader(uint32_t type, const std::string& src);
  };

} // namespace Marble
