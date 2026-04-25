#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Marble {

  class Shader {
  public:
    // Tag type — distinguishes source-string construction from file-path construction.
    // Usage: std::make_unique<Shader>(Shader::FromSource, vertSrc, fragSrc)
    struct SourceTag { explicit SourceTag() = default; };
    static constexpr SourceTag FromSource{};

    // Tag type — compute-only shader (single stage, no vert/frag).
    // Usage: std::make_unique<Shader>(Shader::Compute, computeSrc)
    struct ComputeTag { explicit ComputeTag() = default; };
    static constexpr ComputeTag Compute{};

    // Compile from GLSL source code embedded as string literals.
    Shader(SourceTag, const char* vertSrc, const char* fragSrc);
    // Compile a compute-only program from an embedded GLSL source string.
    Shader(ComputeTag, const char* computeSrc);
    // Compile from shader files on disk.
    Shader(const std::string& vertPath, const std::string& fragPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void Bind()   const;
    void Unbind() const;

    // Dispatch the compute program. Must be called after Bind().
    // Blocks the current thread until the dispatch is enqueued (not completed).
    // Use glMemoryBarrier after dispatching before reading the written data.
    void Dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) const;

    void SetInt      (const char* name, int value)              const;
    void SetIntArray (const char* name, const int* v, int n)    const;
    void SetFloat    (const char* name, float value)            const;
    void SetFloat2   (const char* name, const glm::vec2& v)     const;
    void SetFloat3   (const char* name, const glm::vec3& v)     const;
    void SetFloat4   (const char* name, const glm::vec4& v)     const;
    void SetMat4     (const char* name, const glm::mat4& m)     const;

    uint32_t GetID() const { return m_ID; }

  private:
    // Returns the cached uniform location, querying the driver only on first access.
    int GetLocation(const char* name) const;

    // Links two compiled shader objects into m_ID. Deletes both objects afterwards.
    // Throws on link failure (and deletes m_ID before throwing).
    void Link(uint32_t vert, uint32_t frag);

    uint32_t m_ID = 0;
    mutable std::unordered_map<std::string, int> m_LocationCache;

    static std::string ReadFile(const std::string& path);
    static uint32_t    CompileShader(uint32_t type, const std::string& src);
  };

} // namespace Marble
